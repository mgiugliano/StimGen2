/*
 * StimGen 2 -- SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Michele Giugliano
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * eval.c -- from syntax tree to samples (spec, Secs. 8, 10 and 12).
 *
 *   check_stimulus()  static checks: arguments, units, durations (spec 10.7)
 *   boundary()        exact first-sample index of a time (spec Rule 1, 3)
 *   realise()         samples of every channel, markers, padding
 *
 * Evaluation is recursive.  A sequence (block) places each segment on the
 * samples given by the boundary rule; an expression fills the samples of
 * its segment and returns the end value used by the next 'prev'.  While
 * descending, the address of each stochastic instance is built as in spec
 * Sec. 11.3 ("ch=NAME/s1/o0/r2/..."), and it keys its random stream.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sg.h"

/* ================================================================== */
/* Exact segment boundaries                                            */
/* ================================================================== */

/* n = R(t * fs) with t = t_ps * 1e-12 s and fs = p/q Hz, i.e.
 * n = R(t_ps * p / (q * 10^12)), rounded to nearest, ties to even.
 * The product is formed in 128 bits and divided by long division, so the
 * result is exact for every representable input (spec Rule 3).          */
int64_t boundary(const Ctx *c, int64_t t_ps)
{
    uint64_t hi, lo, D = (uint64_t)c->rate_q * 1000000000000ULL, qt = 0, rem = 0;
    int bit;
    if (t_ps < 0) die("internal: negative time");
    mulhilo64((uint64_t)t_ps, (uint64_t)c->rate_p, &hi, &lo);
    for (bit = 127; bit >= 0; bit--) {          /* (hi:lo) / D        */
        uint64_t b = bit >= 64 ? (hi >> (bit - 64)) & 1 : (lo >> bit) & 1;
        rem = (rem << 1) | b;                   /* rem < D < 2^60     */
        if (qt >> 63) die("stimulus too long for this sampling rate");
        qt <<= 1;
        if (rem >= D) { rem -= D; qt |= 1; }
    }
    if (2 * rem > D || (2 * rem == D && (qt & 1))) qt++;   /* half to even */
    return (int64_t)qt;
}

/* ================================================================== */
/* Static checks                                                       */
/* ================================================================== */

static const Channel *cur_ch;          /* channel being checked (units) */

/* A constant argument such as 5ms, -2, or 2*3: value and dimension. */
static double const_eval(const Node *n, Dim *d)
{
    Dim da, db;
    double a, b;
    switch (n->kind) {
    case N_NUM: *d = n->dim; return n->num;
    case N_NEG: return -const_eval(n->a, d);
    case N_BIN:
        a = const_eval(n->a, &da); b = const_eval(n->b, &db);
        if (n->op == '+' || n->op == '-') {
            if (da != db) die("line %d: cannot add a %s and a %s", n->line, dim_name(da), dim_name(db));
            *d = da; return n->op == '+' ? a + b : a - b;
        }
        if (n->op == '*' && (da == D_NONE || db == D_NONE)) { *d = da == D_NONE ? db : da; return a * b; }
        if (n->op == '/' && db == D_NONE) { *d = da; return a / b; }
        die("line %d: only plain numbers can multiply or divide quantities here", n->line);
    default:
        die("line %d: expected a constant value", n->line);
    }
    return 0;
}

/* Convert an amplitude to the channel unit (spec Sec. 12.2.1). */
static double amp_value(double v, Dim d, int line)
{
    if (d == D_NONE) return v;
    if (d != D_CURR && d != D_VOLT && d != D_COND)
        die("line %d: a %s was given where an amplitude is expected", line, dim_name(d));
    if (cur_ch->udim != d)
        die("line %d: a %s does not fit channel '%s' (unit %s)", line, dim_name(d),
            cur_ch->name, cur_ch->unit);
    return v / cur_ch->ufactor;
}

/* Value of an amplitude constant in the channel unit.  Literals with a unit
 * are converted directly between decimal units (e.g. -250pA on a pA
 * channel is exactly -250, 0.3nA is exactly 300), not through SI.        */
static double amp_const(const Node *n)
{
    switch (n->kind) {
    case N_NUM:
        if (n->dim == D_NONE || n->uexp == 99) return amp_value(n->num, n->dim, n->line);
        amp_value(0, n->dim, n->line);                     /* dimension check */
        return scale10(n->mant, n->uexp - unit_exp10(cur_ch->ufactor));
    case N_NEG: return -amp_const(n->a);
    case N_BIN: {
        double a = amp_const(n->a), b = amp_const(n->b);
        return n->op == '+' ? a + b : n->op == '-' ? a - b : n->op == '*' ? a * b : a / b; }
    default: die("line %d: expected a constant value", n->line);
    }
    return 0;
}

static int kw_in(const char *w, const char *set)    /* w in "a|b|c"? */
{
    size_t k = strlen(w);
    const char *p = set;
    while (p) {
        if (!strncmp(p, w, k) && (p[k] == '|' || p[k] == 0)) return 1;
        p = strchr(p, '|');
        if (p) p++;
    }
    return 0;
}

static const char *kw_static(const char *w, const char *set)  /* stable copy */
{
    const char *p = set;
    size_t k = strlen(w);
    while (p) {
        if (!strncmp(p, w, k) && (p[k] == '|' || p[k] == 0)) {
            char *r = xmalloc(k + 1); memcpy(r, p, k); r[k] = 0; return r;
        }
        p = strchr(p, '|'); if (p) p++;
    }
    return NULL;
}

static void check_expr(Node *e);
static void check_block(Node *b);

/* Kind of a combination of two operands (spec Table 10.2). */
static void combine_kind(Node *e, const Node *a, const Node *b)
{
    e->timed = a->timed || (b && b->timed);
    e->T_ps = a->timed ? a->T_ps : b ? b->T_ps : 0;
    if (b && a->timed && b->timed && a->T_ps != b->T_ps)
        die("line %d: combined waveforms have different durations", e->line);
}

static void resolve_call(Node *n)
{
    const Prim *p;
    int i, pos = 0;
    n->fn = prim_lookup(n->name);
    if (n->fn < 0) die("line %d: unknown generator or function '%s' (try: sg help)", n->line, n->name);
    p = &prims[n->fn];
    n->is_map = n->fn >= nprims;
    memset(n->pv, 0, sizeof n->pv);
    for (i = 0; i < n->nargs; i++) {                 /* match arguments */
        Arg *a = &n->args[i];
        int j = -1;
        const Par *q;
        Pv *v;
        if (a->name) {
            for (j = 0; j < p->npar && strcmp(p->par[j].name, a->name); j++) ;
            if (j == p->npar) die("line %d: %s has no parameter '%s'", n->line, p->name, a->name);
            pos = -1;                                /* no more positional */
        } else {
            if (pos < 0) die("line %d: positional argument after a named one", n->line);
            if (pos >= p->npar) die("line %d: too many arguments for %s", n->line, p->name);
            j = pos++;
        }
        q = &p->par[j]; v = &n->pv[j];
        if (v->given) die("line %d: parameter '%s' given twice", n->line, q->name);
        v->given = 1;
        switch (q->kind) {
        case P_WAVE: v->wave = a->val; check_expr(a->val); break;
        case P_STR:
            if (a->val->kind != N_STR) die("line %d: %s needs a file name in quotes", n->line, q->name);
            v->str = a->val->text; break;
        case P_SEED:
            if (a->val->kind != N_NUM || a->val->dim != D_NONE ||
                strspn(a->val->text, "0123456789") != strlen(a->val->text))
                die("line %d: seed must be a non-negative integer", n->line);
            v->str = a->val->text; break;
        case P_LIST: {
            int k;
            if (a->val->kind != N_LIST) die("line %d: %s needs a list, e.g. [10ms, 20ms]", n->line, q->name);
            v->nlist = a->val->nitems;
            v->list = xmalloc((size_t)(v->nlist + 1) * sizeof(double));
            for (k = 0; k < v->nlist; k++) {
                Dim d;
                v->list[k] = const_eval(a->val->items[k], &d);
                if (d != D_TIME && d != D_NONE) die("line %d: %s must contain times", n->line, q->name);
                if (v->list[k] < 0 || (k && v->list[k] <= v->list[k-1]))
                    die("line %d: %s must be increasing and >= 0", n->line, q->name);
            }
            break; }
        case P_KW: case P_INIT:
            if (a->val->kind == N_IDENT) {
                if (!kw_in(a->val->text, q->kws))
                    die("line %d: %s=%s: expected one of %s", n->line, q->name, a->val->text, q->kws);
                v->kw = kw_static(a->val->text, q->kws);
                break;
            }
            if (q->kind == P_KW) die("line %d: %s expects one of %s", n->line, q->name, q->kws);
            /* fall through: ou init can be prev or a value */
        default: {
            Dim d;
            if (a->val->kind == N_PREV) {
                if (q->dim != D_AMP) die("line %d: prev is allowed only for amplitudes", n->line);
                v->is_prev = 1; break;
            }
            if (a->val->kind == N_IDENT)
                die("line %d: %s: '%s' is not a number", n->line, q->name, a->val->text);
            v->v = const_eval(a->val, &d);                 /* checks dimensions */
            if (q->dim == D_AMP) { amp_value(0, d, n->line); v->v = amp_const(a->val); }
            else if (d != D_NONE && d != q->dim)
                die("line %d: %s expects a %s, not a %s", n->line, q->name,
                    dim_name(q->dim), dim_name(d));
            break; }
        }
    }
    for (i = 0; i < p->npar; i++) {                  /* defaults       */
        const Par *q = &p->par[i];
        Pv *v = &n->pv[i];
        if (v->given) continue;
        switch (q->kind) {
        case P_REQ: case P_STR: case P_WAVE:
            die("line %d: %s needs parameter '%s'", n->line, p->name, q->name);
        case P_NUM: v->given = 1; v->v = q->def; break;
        case P_AMP_PREV: v->given = 1; v->is_prev = 1; break;
        case P_KW: case P_INIT: {
            char w[32]; size_t k = strcspn(q->kws, "|");
            memcpy(w, q->kws, k); w[k] = 0;
            v->given = 1; v->kw = kw_static(w, q->kws); break; }
        default: break;                              /* optional       */
        }
    }
    if (!n->is_map) {
        prim_check(n);
        if (!strcmp(p->name, "pulses") && n->pv[13].given && strcmp(n->pv[7].kw, "poisson"))
            warn("line %d: pulses: seed has no effect on regular pulses", n->line);
        n->timed = 0;
    } else {
        Node *a = n->pv[0].wave, *b = p->par[1].kind == P_WAVE ? n->pv[1].wave : NULL;
        combine_kind(n, a, b);
        prim_check(n);
    }
}

static void check_expr(Node *e)
{
    switch (e->kind) {
    case N_NUM:                          /* an amplitude constant      */
        e->num = amp_const(e);
        e->dim = D_NONE;
        e->timed = 0; break;
    case N_PREV: e->timed = 0; break;
    case N_NEG:  check_expr(e->a); e->timed = e->a->timed; e->T_ps = e->a->T_ps; break;
    case N_BIN:  check_expr(e->a); check_expr(e->b); combine_kind(e, e->a, e->b); break;
    case N_CALL: resolve_call(e); break;
    case N_BLOCK: check_block(e); break;
    case N_REPEAT:
        check_block(e->a);
        if (e->count < 0 || (e->count > 0 && e->a->T_ps > INT64_MAX / e->count))
            die("line %d: repeat makes the waveform too long", e->line);
        e->timed = 1; e->T_ps = e->a->T_ps * e->count; break;
    case N_IDENT: die("line %d: unexpected name '%s'", e->line, e->text);
    default: die("line %d: a string or list cannot be used as a waveform", e->line);
    }
}

static void check_block(Node *b)
{
    int i;
    b->timed = 1; b->T_ps = 0;
    for (i = 0; i < b->nsegs; i++) {
        Seg *s = &b->segs[i];
        check_expr(s->expr);
        if (s->has_dur && s->expr->timed && s->dur_ps != s->expr->T_ps)
            die("line %d: the segment duration differs from that of its expression", s->line);
        if (!s->has_dur && !s->expr->timed)
            die("line %d: segment without duration (write e.g. '1s dc(0)')", s->line);
        if (!s->has_dur) s->dur_ps = s->expr->T_ps;
        if (s->dur_ps > INT64_MAX - b->T_ps) die("line %d: waveform too long", s->line);
        b->T_ps += s->dur_ps;
    }
}

void check_stimulus(Stimulus *s)
{
    int i, j;
    for (i = 0; i < s->nch; i++) {
        Channel *c = &s->ch[i];
        cur_ch = c;
        if (c->copy_of) {
            for (j = 0; j < s->nch && strcmp(s->ch[j].name, c->copy_of); j++) ;
            if (j == s->nch) die("channel '%s': no channel '%s' to copy", c->name, c->copy_of);
            if (s->ch[j].copy_of) die("channel '%s': cannot copy a copy", c->name);
            continue;
        }
        check_block(c->wave);
    }
}

/* ================================================================== */
/* Evaluation                                                          */
/* ================================================================== */

static Stimulus *cur_stim;

static void finite_or_die(const Ctx *c, const double *x, int64_t N, int64_t n0,
                          const char *what, int line)
{
    int64_t k;
    for (k = 0; k < N; k++)
        if (!isfinite(x[k]))
            die("line %d: %s gives a non-finite value at sample %lld (t = %.9g s) of channel '%s'"
                " (division by zero, or an argument outside the domain of a function)",
                line, what, (long long)(n0 + k), (double)(n0 + k) * c->dt, c->chan);
}

static size_t push(char *addr, const char *fmt, long long i)
{
    size_t len = strlen(addr);
    if (len + 32 > 4096) die("description nested too deeply");
    sprintf(addr + len, fmt, i);
    return len;
}

static double ev_block(Ctx *c, Node *b, int64_t tau, int64_t base, double prev, char *addr, double *out);

/* Fill out[0..N-1] (global samples n0..n0+N-1, nominal start tau) with the
 * value of expression e, and return its end value (spec Sec. 10.6).      */
static double ev_expr(Ctx *c, Node *e, int64_t n0, int64_t tau, int64_t N,
                      double prev, char *addr, double *out)
{
    int64_t k;
    size_t len;
    double ea, eb, *tmp;
    switch (e->kind) {
    case N_NUM:  for (k = 0; k < N; k++) out[k] = e->num; return e->num;
    case N_PREV:
        if (isnan(prev)) die("line %d: prev is undefined here", e->line);
        for (k = 0; k < N; k++) out[k] = prev;
        return prev;
    case N_NEG:
        len = push(addr, "/o%lld", 0);
        ea = ev_expr(c, e->a, n0, tau, N, prev, addr, out);
        addr[len] = 0;
        for (k = 0; k < N; k++) out[k] = -out[k];
        return -ea;
    case N_BIN:
        tmp = xmalloc((size_t)(N > 0 ? N : 1) * sizeof *tmp);
        len = push(addr, "/o%lld", 0);
        ea = ev_expr(c, e->a, n0, tau, N, prev, addr, out);
        addr[len] = 0; push(addr, "/o%lld", 1);
        eb = ev_expr(c, e->b, n0, tau, N, prev, addr, tmp);
        addr[len] = 0;
        for (k = 0; k < N; k++)
            switch (e->op) {
            case '+': out[k] += tmp[k]; break;
            case '-': out[k] -= tmp[k]; break;
            case '*': out[k] *= tmp[k]; break;
            default:  out[k] /= tmp[k]; break;
            }
        free(tmp);
        finite_or_die(c, out, N, n0, "an arithmetic operation", e->line);
        ea = e->op == '+' ? ea + eb : e->op == '-' ? ea - eb : e->op == '*' ? ea * eb : ea / eb;
        return isfinite(ea) ? ea : NAN;              /* NAN: undefined end value */
    case N_BLOCK:
        return ev_block(c, e, tau, n0, prev, addr, out);
    case N_REPEAT: {
        double p = prev;
        for (k = 0; k < e->count; k++) {
            len = push(addr, "/r%lld", (long long)k);
            p = ev_block(c, e->a, tau + k * e->a->T_ps, n0, p, addr, out);
            addr[len] = 0;
        }
        return p; }
    case N_CALL:
        break;
    default:
        die("internal: bad node");
    }
    /* A call: primitive generator, or map. */
    {
        const Prim *p = &prims[e->fn];
        int i;
        for (i = 0; i < p->npar; i++)
            if (e->pv[i].is_prev && isnan(prev)) die("line %d: prev is undefined here", e->line);
        if (!e->is_map) {
            /* Nyquist warning (spec Sec. 8.8) */
            if ((p->flags & F_PERIODIC) && e->pv[1].v > c->fs / 2)
                warn("line %d: %s frequency %g Hz is above fs/2 = %g Hz (aliasing)", e->line, p->name, e->pv[1].v, c->fs / 2);
            if (!strcmp(p->name, "chirp") && (e->pv[1].v > c->fs / 2 || e->pv[2].v > c->fs / 2))
                warn("line %d: chirp frequency above fs/2 = %g Hz (aliasing)", e->line, c->fs / 2);
            ea = gen_prim(c, e, n0, N, prev, addr, out);
            finite_or_die(c, out, N, n0, p->name, e->line);
            return isfinite(ea) ? ea : NAN;
        }
        len = push(addr, "/o%lld", 0);
        ea = ev_expr(c, e->pv[0].wave, n0, tau, N, prev, addr, out);
        addr[len] = 0;
        tmp = NULL; eb = 0;
        if (p->par[1].kind == P_WAVE) {              /* min, max */
            tmp = xmalloc((size_t)(N > 0 ? N : 1) * sizeof *tmp);
            push(addr, "/o%lld", 1);
            eb = ev_expr(c, e->pv[1].wave, n0, tau, N, prev, addr, tmp);
            addr[len] = 0;
        }
        for (k = -1; k < N; k++) {                   /* k = -1: end value */
            double x = k < 0 ? ea : out[k], y = k < 0 ? eb : (tmp ? tmp[k] : 0), r;
            double a1 = e->pv[1].v, a2 = e->pv[2].v;  /* k of pow; lo, hi of clip */
            switch (e->fn - nprims) {                /* order of the table   */
            case 0: r = fabs(x); break;                                    /* abs  */
            case 1: r = x > 0 ? x : 0; break;                              /* pos  */
            case 2: r = x < 0 ? NAN : sqrt(x); break;                      /* sqrt */
            case 3: r = exp(x); break;                                     /* exp  */
            case 4: r = x > 0 ? log(x) : NAN; break;                       /* log  */
            case 5: r = (x < 0 && a1 != floor(a1)) || (x == 0 && a1 < 0)   /* pow  */
                        ? NAN : pow(x, a1); break;
            case 6: r = (x == 0 && a1 < 0) ? NAN                           /* spow */
                        : (x < 0 ? -pow(-x, a1) : pow(x, a1)); break;
            case 7: r = x < a1 ? a1 : x > a2 ? a2 : x; break;              /* clip */
            case 8: r = x < y ? x : y; break;                              /* min  */
            default: r = x > y ? x : y; break;                             /* max  */
            }
            if (k < 0) ea = r; else out[k] = r;
        }
        free(tmp);
        finite_or_die(c, out, N, n0, p->name, e->line);
        return isfinite(ea) ? ea : NAN;
    }
}

/* Place the segments of block b, starting at nominal time tau.  'base' is
 * the global index of out[0].  Returns the end value of the last segment. */
static double ev_block(Ctx *c, Node *b, int64_t tau, int64_t base, double prev, char *addr, double *out)
{
    int64_t off = 0;
    int i;
    for (i = 0; i < b->nsegs; i++) {
        Seg *s = &b->segs[i];
        int64_t ns = boundary(c, tau + off), ne = boundary(c, tau + off + s->dur_ps);
        size_t len = push(addr, "/s%lld", i);
        if (s->dur_ps > 0 && ne == ns)
            warn("line %d: segment of positive duration has no sample at this rate", s->line);
        if (s->label) {                               /* marker "chan.label" */
            Marker m;
            m.name = xmalloc(strlen(c->chan) + strlen(s->label) + 2);
            sprintf(m.name, "%s.%s", c->chan, s->label);
            m.sample = ns;
            cur_stim->mk = realloc(cur_stim->mk, (size_t)(cur_stim->nmk + 1) * sizeof(Marker));
            cur_stim->mk[cur_stim->nmk++] = m;
        }
        prev = ev_expr(c, s->expr, ns, tau + off, ne - ns, prev, addr, out + (ns - base));
        addr[len] = 0;
        off += s->dur_ps;
    }
    return prev;
}

void realise(Stimulus *s, uint64_t mseed, int64_t rate_p, int64_t rate_q, const char *srcdir)
{
    Ctx c;
    int i, j, differ = 0;
    int64_t Tmax = 0, N, k;
    char addr[4096];
    memset(&c, 0, sizeof c);
    c.mseed = mseed; c.rate_p = rate_p; c.rate_q = rate_q;
    c.fs = (double)rate_p / (double)rate_q; c.dt = (double)rate_q / (double)rate_p;
    c.srcdir = srcdir; c.stim = s;
    cur_stim = s;

    for (i = 0; i < s->nch; i++) {                    /* each channel */
        Channel *ch = &s->ch[i];
        if (ch->copy_of) continue;
        c.chan = ch->name;
        ch->n = boundary(&c, ch->wave->T_ps);
        ch->x = xcalloc((size_t)(ch->n > 0 ? ch->n : 1), sizeof(double));
        sprintf(addr, "ch=%.200s", ch->name);
        ev_block(&c, ch->wave, 0, 0, 0.0, addr, ch->x);
        if (ch->digital)
            for (k = 0; k < ch->n; k++)
                if (ch->x[k] != 0 && ch->x[k] != 1)
                    die("digital channel '%s': sample %lld is %g, not 0 or 1",
                        ch->name, (long long)k, ch->x[k]);
    }
    for (i = 0; i < s->nch; i++) {                    /* copies       */
        Channel *ch = &s->ch[i];
        if (!ch->copy_of) continue;
        for (j = 0; strcmp(s->ch[j].name, ch->copy_of); j++) ;
        ch->wave = s->ch[j].wave;
        ch->n = s->ch[j].n;
        ch->x = xmalloc((size_t)(ch->n > 0 ? ch->n : 1) * sizeof(double));
        memcpy(ch->x, s->ch[j].x, (size_t)ch->n * sizeof(double));
    }
    for (i = 0; i < s->nch; i++) {                    /* duration (spec 12.3) */
        if (s->ch[i].wave->T_ps != s->ch[0].wave->T_ps) differ = 1;
        if (s->ch[i].wave->T_ps > Tmax) Tmax = s->ch[i].wave->T_ps;
    }
    if (s->has_dur) {
        if (s->dur_ps < Tmax) die("duration is shorter than the longest channel");
        Tmax = s->dur_ps;
    } else if (differ)
        warn("channels have different durations; shorter ones hold their rest value");
    N = boundary(&c, Tmax);
    for (i = 0; i < s->nch; i++) {                    /* pad with rest */
        Channel *ch = &s->ch[i];
        ch->x = realloc(ch->x, (size_t)(N > 0 ? N : 1) * sizeof(double));
        for (k = ch->n; k < N; k++) ch->x[k] = ch->rest;
        ch->n = N;
    }
    for (i = 0; i < s->nemk; i++) {                   /* explicit markers */
        Marker m;
        m.name = s->emk_name[i];
        m.sample = boundary(&c, s->emk_ps[i]);
        s->mk = realloc(s->mk, (size_t)(s->nmk + 1) * sizeof(Marker));
        s->mk[s->nmk++] = m;
    }
}
