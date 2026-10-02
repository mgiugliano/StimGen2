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
 * protocol.c -- protocols and their directory form (spec, Sec. 13).
 *
 * A protocol is expanded into an ordered list of trials, each one a plain
 * stimulus text with no variables left, plus a master seed (spec Sec. 13.3).
 * The list can be rendered directly, or written as a directory of .sg files
 * with an index file "protocol.sgi" (spec Sec. 13.4); reading that
 * directory back gives the same trials.
 *
 * Protocol files are read by a small scanner of their own, because the
 * stimulus template must be kept as raw text: substitution of $name and
 * $( expression ) is textual and happens before the template is parsed.
 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>       /* POSIX: only for directories without an index */
#include "sg.h"

/* ================================================================== */
/* Quantities with dimensions: s, A, V, S exponents (spec Sec. 13.2.1) */
/* ================================================================== */

typedef struct { double v; signed char d[4]; } Q;      /* value in SI units */

static Q q_of(double v, Dim dim)
{
    Q q;
    memset(&q, 0, sizeof q);
    q.v = v;
    if (dim == D_TIME) q.d[0] = 1;
    if (dim == D_FREQ) q.d[0] = -1;
    if (dim == D_CURR) q.d[1] = 1;
    if (dim == D_VOLT) q.d[2] = 1;
    if (dim == D_COND) q.d[3] = 1;
    return q;                                 /* phase, fraction: plain */
}

static int same_dim(Q a, Q b) { return !memcmp(a.d, b.d, sizeof a.d); }

/* Can q be written with a unit of Table 14.1 (or as a plain number)? */
static int writable(Q q)
{
    int i, nz = 0, one = 1;
    for (i = 0; i < 4; i++) if (q.d[i]) { nz++; if (q.d[i] != 1 && !(i == 0 && q.d[i] == -1)) one = 0; }
    return nz == 0 || (nz == 1 && one);
}

/* Write a computed value in engineering notation with a unit of spec
 * Table 14.1 (e.g. 4e-10 A -> "400pA").  Derived dimensions such as charge
 * have no unit and cannot be written.                                   */
static char *fmt_q(Q q)
{
    static const struct { signed char d[4]; const char *u[7]; double f[7]; } tab[] = {
        {{1,0,0,0},  {"ns","us","ms","s"},      {1e-9,1e-6,1e-3,1}},
        {{-1,0,0,0}, {"Hz","kHz","MHz"},        {1,1e3,1e6}},
        {{0,1,0,0},  {"fA","pA","nA","uA","mA","A"}, {1e-15,1e-12,1e-9,1e-6,1e-3,1}},
        {{0,0,1,0},  {"uV","mV","V"},           {1e-6,1e-3,1}},
        {{0,0,0,1},  {"pS","nS","uS","mS","S"}, {1e-12,1e-9,1e-6,1e-3,1}} };
    char t[64];
    int i, k;
    if (!q.d[0] && !q.d[1] && !q.d[2] && !q.d[3]) { sprintf(t, "%.12g", q.v); return xstrdup(t); }
    for (i = 0; i < 5; i++) {
        if (memcmp(q.d, tab[i].d, 4)) continue;
        if (q.v == 0)                                    /* 0 in the base unit */
            for (k = 0; tab[i].f[k] != 1; k++) ;
        else                                             /* largest unit <= |v| */
            for (k = 0; k < 6 && tab[i].u[k + 1] && fabs(q.v) >= tab[i].f[k + 1]; k++) ;
        sprintf(t, "%.12g%s", q.v / tab[i].f[k], tab[i].u[k]);
        return xstrdup(t);
    }
    die("a computed value has a derived dimension (e.g. a charge) and cannot be substituted");
    return NULL;
}

/* ================================================================== */
/* Scanner and expression evaluator                                    */
/* ================================================================== */

typedef struct { char *name; Q q; char *text; } Var;

typedef struct {
    const char *file, *s;
    size_t p;
    int line;
    Var *v; int nv;                 /* variables known so far */
} Scan;

static void serr(Scan *S, const char *m) { die("%s:%d: %s", S->file, S->line, m); }

/* Skip spaces, tabs and comments; with nl, also line breaks. */
static void skip(Scan *S, int nl)
{
    for (;;) {
        char c = S->s[S->p];
        if (c == ' ' || c == '\t' || c == '\r') S->p++;
        else if (c == '#') while (S->s[S->p] && S->s[S->p] != '\n') S->p++;
        else if (c == '\n' && nl) { S->p++; S->line++; }
        else return;
    }
}

static int peek(Scan *S) { skip(S, 0); return (unsigned char)S->s[S->p]; }

static char *word(Scan *S)                       /* identifier or keyword */
{
    size_t b;
    skip(S, 0);
    b = S->p;
    while (isalnum((unsigned char)S->s[S->p]) || S->s[S->p] == '_' || S->s[S->p] == '-') S->p++;
    if (S->p == b) serr(S, "expected a name");
    {
        char *w = xmalloc(S->p - b + 1);
        memcpy(w, S->s + b, S->p - b); w[S->p - b] = 0;
        return w;
    }
}

static void expect_ch(Scan *S, int c)
{
    char m[32];
    if (peek(S) == c) { S->p++; return; }
    sprintf(m, "expected '%c'", c);
    serr(S, m);
}

static Var *find_var(Scan *S, const char *name)
{
    int i;
    for (i = 0; i < S->nv; i++) if (!strcmp(S->v[i].name, name)) return &S->v[i];
    return NULL;
}

static Q p_expr(Scan *S);

static Q p_factor(Scan *S)
{
    int c = peek(S);
    if (c == '-') { Q q; S->p++; q = p_factor(S); q.v = -q.v; return q; }
    if (c == '(') { Q q; S->p++; q = p_expr(S); expect_ch(S, ')'); return q; }
    if (isdigit(c) || c == '.') {                 /* number with unit */
        char num[64], unit[16];
        size_t n = 0, u = 0;
        Dim d = D_NONE;
        double f = 1;
        while (isdigit((unsigned char)S->s[S->p]) || S->s[S->p] == '.' ||
               ((S->s[S->p] == 'e' || S->s[S->p] == 'E') &&
                (isdigit((unsigned char)S->s[S->p+1]) || S->s[S->p+1] == '-' || S->s[S->p+1] == '+'))) {
            if (n < 60) num[n++] = S->s[S->p];
            if ((S->s[S->p] == 'e' || S->s[S->p] == 'E') && n < 60) num[n++] = S->s[++S->p];
            S->p++;
        }
        num[n] = 0;
        while (isalpha((unsigned char)S->s[S->p]) || S->s[S->p] == '%')
            if (u < 15) unit[u++] = S->s[S->p++]; else serr(S, "unit too long");
        unit[u] = 0;
        if (u && !unit_lookup(unit, &d, &f)) serr(S, "unknown unit");
        return q_of(strtod(num, NULL) * f, d);
    }
    if (isalpha(c) || c == '_') {                 /* variable */
        char *w = word(S);
        Var *v = find_var(S, w);
        if (!v) { char m[128]; sprintf(m, "unknown variable '%.60s'", w); serr(S, m); }
        free(w);
        return v->q;
    }
    serr(S, "expected a number, a variable or '('");
    return q_of(0, D_NONE);
}

static Q p_term(Scan *S)
{
    Q a = p_factor(S);
    for (;;) {
        int c = peek(S), i;
        Q b;
        if (c != '*' && c != '/') return a;
        S->p++;
        b = p_factor(S);
        for (i = 0; i < 4; i++) a.d[i] = (signed char)(c == '*' ? a.d[i] + b.d[i] : a.d[i] - b.d[i]);
        a.v = c == '*' ? a.v * b.v : a.v / b.v;
    }
}

static Q p_expr(Scan *S)
{
    Q a = p_term(S);
    for (;;) {
        int c = peek(S);
        Q b;
        if (c != '+' && c != '-') return a;
        S->p++;
        b = p_term(S);
        if (!same_dim(a, b)) serr(S, "adding quantities of different dimensions");
        a.v = c == '+' ? a.v + b.v : a.v - b.v;
    }
}

/* ================================================================== */
/* Protocol statements (spec Table 13.1)                               */
/* ================================================================== */

typedef struct {                 /* sweep: names and values val[i*nnames+k] */
    int nnames; char **names;
    int nval;   Var *val;
} Sweep;

typedef struct {
    char  *tmpl;                 /* stimulus template, raw text             */
    int    tmpl_is_file;         /* from 'stimulus "file.sg"'               */
    Sweep *sw;  int nsw;
    char **let_name, **let_expr; int nlet;
    long   repeat;
    char  *order, *noise, *timing, *start, *seed;
} Proto;

/* One sweep value.  A plain literal keeps its written form; anything
 * else is written in engineering notation (spec Sec. 13.2.1).            */
static Var sweep_value(Scan *S)
{
    Var v;
    size_t b, n, k;
    char *raw;
    skip(S, 1);
    b = S->p;
    v.name = NULL;
    v.q = p_expr(S);
    n = S->p - b;
    raw = xmalloc(n + 1);
    memcpy(raw, S->s + b, n); raw[n] = 0;
    while (n && isspace((unsigned char)raw[n-1])) raw[--n] = 0;
    for (k = 0; k < n && (isalnum((unsigned char)raw[k]) || strchr(".-+%", raw[k])); k++) ;
    if (k == n && n) v.text = raw;
    else { free(raw); v.text = fmt_q(v.q); }
    return v;
}

static void add_value(Sweep *w, Q q)
{
    w->val = realloc(w->val, (size_t)(w->nval + 1) * sizeof(Var));
    w->val[w->nval].name = NULL;
    w->val[w->nval].q = q;
    w->val[w->nval++].text = fmt_q(q);
}

/* Unit suffix of the literal that starts at S->s + b, or NULL. */
static const char *literal_unit(const Scan *S, size_t b, size_t e, double *f, int *k)
{
    static char u[16];
    size_t i = e, n;
    Dim d;
    while (i > b && isspace((unsigned char)S->s[i-1])) i--;
    n = i;
    while (n > b && (isalpha((unsigned char)S->s[n-1]) || S->s[n-1] == '%')) n--;
    if (n == i || i - n >= sizeof u || !strchr("0123456789.", S->s[n-1])) return NULL;
    memcpy(u, S->s + n, i - n); u[i - n] = 0;
    if (!unit_lookup(u, &d, f) || (*k = unit_exp10(*f)) == 99) return NULL;
    return u;
}

/* Add a range value, written in the unit of the first value if it has
 * one (e.g. -300pA, -250pA, ..., 0pA), else in engineering notation.     */
static void add_in_unit(Sweep *w, Q q, const char *unit, int k)
{
    char t[64];
    if (!unit) { add_value(w, q); return; }
    sprintf(t, "%.12g%s", scale10(q.v, -k), unit);
    w->val = realloc(w->val, (size_t)(w->nval + 1) * sizeof(Var));
    w->val[w->nval].name = NULL;
    w->val[w->nval].q = q;
    w->val[w->nval++].text = xstrdup(t);
}

/* Exact decimal range "from A to B step S" (spec Sec. 13.2.2): A, B, S are
 * expressed in the unit of A (10^k) and scaled by 10^m to integers, so
 * that every value A + i*S, and whether B is reached, is exact.         */
static int decimal_range(Sweep *w, Q a, Q b, Q st, const char *unit, int k)
{
    double au = scale10(a.v, -k), bu = scale10(b.v, -k), su = scale10(st.v, -k), p = 1;
    long long am, bm, sm, i, n;
    int m;
    for (m = 0; m <= 9; m++, p *= 10)
        if (fabs(au * p - (double)llround(au * p)) < 1e-6 && fabs(bu * p - (double)llround(bu * p)) < 1e-6 &&
            fabs(su * p - (double)llround(su * p)) < 1e-6 && fabs(au * p) < 1e15 && fabs(bu * p) < 1e15)
            break;
    if (m > 9) return 0;                       /* not decimal: use doubles */
    am = llround(au * p); bm = llround(bu * p); sm = llround(su * p);
    if (sm == 0 || (bm - am) / sm < 0) return 0;
    n = (bm - am) / sm + 1;
    for (i = 0; i < n; i++) {
        long long v = am + i * sm, ip, fp;
        char t[64], frac[16];
        int d;
        ip = (v < 0 ? -v : v) / (long long)p;
        fp = (v < 0 ? -v : v) % (long long)p;
        if (m) sprintf(frac, "%0*lld", m, fp); else frac[0] = 0;
        for (d = m; d > 0 && frac[d-1] == '0'; d--) frac[d-1] = 0;
        sprintf(t, "%s%lld%s%s%s", v < 0 ? "-" : "", ip, frac[0] ? "." : "", frac, unit ? unit : "");
        w->val = realloc(w->val, (size_t)(w->nval + 1) * sizeof(Var));
        w->val[w->nval].name = NULL;
        w->val[w->nval].q = a;
        w->val[w->nval].q.v = scale10((double)v, k - m);
        w->val[w->nval++].text = xstrdup(t);
    }
    return 1;
}

static void expect_word(Scan *S, const char *w)
{
    char *t = word(S), m[64];
    if (strcmp(t, w)) { sprintf(m, "expected '%s'", w); serr(S, m); }
    free(t);
}

static void parse_sweep(Scan *S, Proto *P)
{
    Sweep w;
    memset(&w, 0, sizeof w);
    if (peek(S) == '(') {                          /* sweep (a, b) = ... */
        S->p++;
        for (;;) {
            w.names = realloc(w.names, (size_t)(w.nnames + 1) * sizeof(char *));
            w.names[w.nnames++] = word(S);
            if (peek(S) != ',') break;
            S->p++;
        }
        expect_ch(S, ')');
    } else {
        w.names = xmalloc(sizeof(char *));
        w.names[0] = word(S); w.nnames = 1;
    }
    expect_ch(S, '=');
    if (peek(S) == '[') {                          /* list of values or tuples */
        S->p++;
        for (;;) {
            int k;
            skip(S, 1);
            if (S->s[S->p] == ']') break;
            if (w.nnames > 1) expect_ch(S, '(');
            for (k = 0; k < w.nnames; k++) {
                w.val = realloc(w.val, (size_t)((w.nval + 1) * w.nnames) * sizeof(Var));
                w.val[w.nval * w.nnames + k] = sweep_value(S);
                if (k + 1 < w.nnames) expect_ch(S, ',');
            }
            if (w.nnames > 1) expect_ch(S, ')');
            w.nval++;
            skip(S, 1);
            if (S->s[S->p] == ',') S->p++;
            else if (S->s[S->p] != ']') serr(S, "expected ',' or ']' in the list");
        }
        S->p++;
    } else {                                       /* from / linspace / logspace */
        char *kw = word(S);
        Q a, b, st, nq;
        long n, i;
        size_t ab;
        const char *au;
        double af;
        int ak = 0;
        if (w.nnames != 1) serr(S, "a range gives values to one variable only");
        if (!strcmp(kw, "from")) {
            skip(S, 0); ab = S->p;
            a = p_expr(S);
            au = literal_unit(S, ab, S->p, &af, &ak);
            expect_word(S, "to");
            b = p_expr(S); expect_word(S, "step");
            st = p_expr(S);
            if (!same_dim(a, b) || !same_dim(a, st)) serr(S, "from, to and step must have the same dimension");
            if (st.v == 0 || (b.v - a.v) / st.v < -1e-9) serr(S, "the step does not lead from 'from' to 'to'");
            if (!(au || same_dim(a, q_of(0, D_NONE))) || !decimal_range(&w, a, b, st, au, ak)) {
                n = (long)floor((b.v - a.v) / st.v + 1e-9) + 1;   /* not decimal */
                for (i = 0; i < n; i++) { Q q = a; q.v = a.v + (double)i * st.v; add_in_unit(&w, q, au, ak); }
            }
        } else if (!strcmp(kw, "linspace") || !strcmp(kw, "logspace")) {
            int lg = kw[1] == 'o';
            expect_ch(S, '('); skip(S, 0); ab = S->p;
            a = p_expr(S);
            au = literal_unit(S, ab, S->p, &af, &ak);
            expect_ch(S, ',');
            b = p_expr(S); expect_ch(S, ','); nq = p_expr(S); expect_ch(S, ')');
            n = (long)nq.v;
            if (n < 1 || (double)n != nq.v) serr(S, "the number of values must be a positive integer");
            if (!same_dim(a, b)) serr(S, "both ends must have the same dimension");
            if (lg && !(a.v * b.v > 0)) serr(S, "logspace needs two ends of the same sign, not zero");
            for (i = 0; i < n; i++) {
                Q q = a;
                double f = n > 1 ? (double)i / (double)(n - 1) : 0;
                q.v = lg ? a.v * pow(b.v / a.v, f) : a.v + (b.v - a.v) * f;
                add_in_unit(&w, q, au, ak);
            }
        } else serr(S, "expected a list [...], 'from', linspace or logspace");
        free(kw);
    }
    if (w.nval == 0) serr(S, "a sweep needs at least one value");
    P->sw = realloc(P->sw, (size_t)(P->nsw + 1) * sizeof(Sweep));
    P->sw[P->nsw++] = w;
}

/* Raw text between matching braces; comments and strings are respected. */
static char *raw_block(Scan *S)
{
    size_t b, n;
    int depth = 1;
    char *t;
    expect_ch(S, '{');
    b = S->p;
    while (S->s[S->p] && depth) {
        char c = S->s[S->p];
        if (c == '#') { while (S->s[S->p] && S->s[S->p] != '\n') S->p++; continue; }
        if (c == '"') { S->p++; while (S->s[S->p] && S->s[S->p] != '"' && S->s[S->p] != '\n') S->p++; }
        if (c == '\n') S->line++;
        if (c == '{') depth++;
        if (c == '}') depth--;
        S->p++;
    }
    if (depth) serr(S, "unbalanced '{' in the stimulus template");
    n = S->p - b - 1;
    t = xmalloc(n + 1);
    memcpy(t, S->s + b, n); t[n] = 0;
    return t;
}

static char *rest_of_line(Scan *S)          /* up to a line break, ';' or '#' */
{
    size_t b, n;
    char *t;
    skip(S, 0);
    b = S->p;
    while (S->s[S->p] && S->s[S->p] != '\n' && S->s[S->p] != '#' && S->s[S->p] != ';') S->p++;
    n = S->p - b;
    t = xmalloc(n + 1);
    memcpy(t, S->s + b, n); t[n] = 0;
    while (n && isspace((unsigned char)t[n-1])) t[--n] = 0;
    return t;
}

static int one_of(const char *w, const char *set)
{
    size_t k = strlen(w);
    const char *p = set;
    for (; p; p = strchr(p, '|') ? strchr(p, '|') + 1 : NULL)
        if (!strncmp(p, w, k) && (p[k] == '|' || p[k] == 0)) return 1;
    return 0;
}

static void parse_proto(Scan *S, Proto *P, const char *dir)
{
    memset(P, 0, sizeof *P);
    P->repeat = 1;
    P->order = "sequential"; P->noise = "per-trial";
    P->timing = "gap 0s"; P->start = "immediately";
    for (;;) {
        char *kw;
        skip(S, 1);
        if (S->s[S->p] == ';') { S->p++; continue; }
        if (!S->s[S->p]) break;
        kw = word(S);
        if (!strcmp(kw, "stimulus")) {
            if (P->tmpl) serr(S, "only one stimulus per protocol");
            if (peek(S) == '"') {                  /* stimulus "file.sg" */
                size_t b = ++S->p;
                char *name, *path;
                while (S->s[S->p] && S->s[S->p] != '"') S->p++;
                name = xmalloc(S->p - b + 1);
                memcpy(name, S->s + b, S->p - b); name[S->p - b] = 0;
                S->p++;
                path = xmalloc(strlen(dir) + strlen(name) + 1);
                sprintf(path, "%s%s", name[0] == '/' ? "" : dir, name);
                P->tmpl = read_text_file(path);
                P->tmpl_is_file = 1;
            } else P->tmpl = raw_block(S);
        } else if (!strcmp(kw, "sweep")) parse_sweep(S, P);
        else if (!strcmp(kw, "let")) {
            P->let_name = realloc(P->let_name, (size_t)(P->nlet + 1) * sizeof(char *));
            P->let_expr = realloc(P->let_expr, (size_t)(P->nlet + 1) * sizeof(char *));
            P->let_name[P->nlet] = word(S);
            expect_ch(S, '=');
            P->let_expr[P->nlet++] = rest_of_line(S);
        } else if (!strcmp(kw, "repeat")) {
            char *t = rest_of_line(S);
            if (!*t || strspn(t, "0123456789") != strlen(t) || (P->repeat = atol(t)) < 1)
                serr(S, "repeat needs a positive integer");
        } else if (!strcmp(kw, "order")) {
            P->order = word(S);
            if (!one_of(P->order, "sequential|grouped|shuffled|shuffled-blocks"))
                serr(S, "order must be sequential, grouped, shuffled or shuffled-blocks");
        } else if (!strcmp(kw, "noise")) {
            P->noise = word(S);
            if (!one_of(P->noise, "per-trial|per-condition|fixed"))
                serr(S, "noise must be per-trial, per-condition or fixed");
        } else if (!strcmp(kw, "period") || !strcmp(kw, "gap")) {
            char *t = rest_of_line(S);
            Scan T = *S;
            Q q;
            T.s = t; T.p = 0;
            q = p_expr(&T);
            if (q.d[0] != 1 || q.d[1] || q.d[2] || q.d[3] || q.v < 0 || T.s[T.p])
                serr(S, "period and gap need a time, e.g. 5s");
            P->timing = xmalloc(strlen(t) + 8);
            sprintf(P->timing, "%s %s", kw, t);
        } else if (!strcmp(kw, "seed")) {
            P->seed = rest_of_line(S);
            if (!*P->seed || strspn(P->seed, "0123456789") != strlen(P->seed))
                serr(S, "seed must be a non-negative integer");
        } else if (!strcmp(kw, "start")) {
            P->start = rest_of_line(S);
            if (strcmp(P->start, "immediately") && strcmp(P->start, "on trigger"))
                serr(S, "start must be 'immediately' or 'on trigger'");
        } else {
            char m[128];
            sprintf(m, "unknown protocol statement '%.60s'", kw);
            serr(S, m);
        }
        skip(S, 0);
        if (S->s[S->p] && S->s[S->p] != '\n' && S->s[S->p] != ';')
            serr(S, "expected the end of the statement");
    }
    if (!P->tmpl) serr(S, "a protocol needs a stimulus");
}

/* ================================================================== */
/* Expansion (spec Sec. 13.3)                                          */
/* ================================================================== */

/* First 8 bytes of SHA-256(text), big-endian (spec Sec. 13.3.3). */
static uint64_t derive64(const char *text)
{
    uint8_t d[32];
    uint64_t v = 0;
    int i;
    sha256(text, strlen(text), d);
    for (i = 0; i < 8; i++) v = (v << 8) | d[i];
    return v;
}

typedef struct { char *s; size_t n, cap; } Txt;

static void put(Txt *b, const char *t, size_t k)
{
    if (b->n + k + 1 > b->cap) { b->cap = 2 * (b->n + k + 1); b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, t, k);
    b->n += k;
    b->s[b->n] = 0;
}

/* Textual substitution of $name and $( expression ) (spec Sec. 13.2.1).
 * Comments and strings are copied unchanged.                           */
static char *substitute(Scan *S, const char *t)
{
    Txt b = { NULL, 0, 0 };
    size_t i = 0;
    put(&b, "", 0);
    while (t[i]) {
        if (t[i] == '#') {                              /* comment */
            size_t j = i;
            while (t[j] && t[j] != '\n') j++;
            put(&b, t + i, j - i); i = j;
        } else if (t[i] == '"') {                       /* string  */
            size_t j = i + 1;
            while (t[j] && t[j] != '"' && t[j] != '\n') j++;
            if (t[j] == '"') j++;
            put(&b, t + i, j - i); i = j;
        } else if (t[i] == '$' && t[i+1] == '(') {      /* $( expr ) */
            size_t j = i + 2;
            int depth = 1;
            char *e, *val;
            Scan E = *S;
            Q q;
            while (t[j] && depth) { if (t[j] == '(') depth++; if (t[j] == ')') depth--; j++; }
            if (depth) serr(S, "unbalanced '$(' in the stimulus template");
            e = xmalloc(j - i - 2);
            memcpy(e, t + i + 2, j - i - 3); e[j - i - 3] = 0;
            E.s = e; E.p = 0;
            q = p_expr(&E);
            if (peek(&E)) serr(S, "unexpected text inside $( ... )");
            val = fmt_q(q);
            put(&b, val, strlen(val));
            i = j;
        } else if (t[i] == '$') {                       /* $name   */
            size_t j = i + 1;
            char name[128];
            Var *v;
            while (isalnum((unsigned char)t[j]) || t[j] == '_') j++;
            if (j == i + 1 || j - i - 1 >= sizeof name) serr(S, "expected a variable name after '$'");
            memcpy(name, t + i + 1, j - i - 1); name[j - i - 1] = 0;
            v = find_var(S, name);
            if (!v) { char m[160]; sprintf(m, "unknown variable '$%s' in the stimulus template", name); serr(S, m); }
            if (!v->text) die("%s: $%s has a derived dimension (e.g. a charge) and cannot be substituted", S->file, name);
            put(&b, v->text, strlen(v->text));
            i = j;
        } else { put(&b, t + i, 1); i++; }
    }
    return b.s;
}

/* Does an inline template describe a stimulus (or just a waveform)? */
static int is_stimulus_body(const char *t)
{
    static const char *kw[] = { "channel", "digital", "rate", "seed", "duration", "marker", NULL };
    int i;
    while (*t) {
        while (isspace((unsigned char)*t)) t++;
        if (*t == '#') { while (*t && *t != '\n') t++; continue; }
        break;
    }
    for (i = 0; kw[i]; i++)
        if (!strncmp(t, kw[i], strlen(kw[i])) && !isalnum((unsigned char)t[strlen(kw[i])])) return 1;
    return 0;
}

static void json_str(Txt *b, const char *s)
{
    put(b, "\"", 1);
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') put(b, "\\", 1);
        if (*s == '\n') { put(b, "\\n", 2); continue; }
        put(b, s, 1);
    }
    put(b, "\"", 1);
}

Trials *expand_protocol(const char *path, const char *text, const char *dir,
                        int have_seed, uint64_t seed)
{
    Scan S;
    Proto P;
    Trials *T = xcalloc(1, sizeof *T);
    long C = 1, R, c, j, n, *tc, *tr;
    int i, k;
    Var **cv;                       /* variables of each condition */
    int ncv;
    Stream ord;
    char key[96];
    uint64_t s;

    memset(&S, 0, sizeof S);
    S.file = path; S.s = text; S.line = 1;
    skip(&S, 1);
    if (strncmp(S.s + S.p, "sg", 2)) serr(&S, "a protocol file starts with 'sg 2 protocol'");
    while (S.s[S.p] && S.s[S.p] != '\n') S.p++;    /* header checked by caller */
    parse_proto(&S, &P, dir);

    /* Protocol seed: command line, else 'seed', else drawn and recorded. */
    s = have_seed ? seed : P.seed ? strtoull(P.seed, NULL, 10) : entropy_seed();
    R = P.repeat;

    /* Conditions: Cartesian product, first sweep varying slowest (13.2.3). */
    for (i = 0; i < P.nsw; i++) {
        if ((double)C * P.sw[i].nval > 1e7) die("%s: too many conditions", path);
        C *= P.sw[i].nval;
    }
    ncv = 0;
    for (i = 0; i < P.nsw; i++) ncv += P.sw[i].nnames;
    ncv += P.nlet;
    cv = xmalloc((size_t)C * sizeof *cv);
    for (c = 0; c < C; c++) {
        long rest = c;
        int m = 0;
        cv[c] = xmalloc((size_t)(ncv ? ncv : 1) * sizeof(Var));
        for (i = P.nsw - 1; i >= 0; i--) {             /* mixed radix */
            long idx = rest % P.sw[i].nval;
            rest /= P.sw[i].nval;
            for (k = 0; k < P.sw[i].nnames; k++) {
                Var v = P.sw[i].val[idx * P.sw[i].nnames + k];
                v.name = P.sw[i].names[k];
                cv[c][m++] = v;
            }
        }
        S.v = cv[c]; S.nv = m;
        for (i = 0; i < P.nlet; i++) {                 /* let, in order */
            Scan E = S;
            Var v;
            E.s = P.let_expr[i]; E.p = 0;
            v.name = P.let_name[i];
            v.q = p_expr(&E);
            if (peek(&E)) serr(&S, "unexpected text after a let expression");
            v.text = writable(v.q) ? fmt_q(v.q) : NULL;  /* NULL: e.g. a charge */
            cv[c][m++] = v;
            S.nv = m;
        }
        for (i = 0; i < m; i++) {                      /* duplicates */
            for (k = 0; k < i; k++)
                if (!strcmp(cv[c][i].name, cv[c][k].name)) die("%s: variable '%s' defined twice", path, cv[c][i].name);
        }
    }

    /* Playing order (spec Sec. 13.3.1).  Random permutations: Fisher-Yates
     * on the stream keyed "order:<seed>", j = floor(U * (i+1)).          */
    n = C * R;
    tc = xmalloc((size_t)n * sizeof *tc); tr = xmalloc((size_t)n * sizeof *tr);
    sprintf(key, "order:%llu", (unsigned long long)s);
    stream_init(&ord, key);
    for (j = 0; j < n; j++) {
        if (!strcmp(P.order, "grouped")) { tc[j] = j / R; tr[j] = j % R; }
        else { tc[j] = j % C; tr[j] = j / C; }     /* sequential, and blocks */
    }
    if (!strcmp(P.order, "shuffled") || !strcmp(P.order, "shuffled-blocks")) {
        long blk = !strcmp(P.order, "shuffled") ? n : C, b0;
        for (b0 = 0; b0 < n; b0 += blk)
            for (j = blk - 1; j > 0; j--) {
                long q = (long)floor(stream_uniform(&ord) * (double)(j + 1)), t;
                t = tc[b0 + j]; tc[b0 + j] = tc[b0 + q]; tc[b0 + q] = t;
                t = tr[b0 + j]; tr[b0 + j] = tr[b0 + q]; tr[b0 + q] = t;
            }
    }

    /* Trials. */
    T->n = n;
    T->t = xcalloc((size_t)n, sizeof(Trial));
    T->timing = P.timing; T->start = P.start;
    T->source = xstrdup(text);
    T->dir = xstrdup(dir);
    T->protocol_seed = s;
    for (j = 0; j < n; j++) {
        Trial *t = &T->t[j];
        char buf[160];
        Txt info = { NULL, 0, 0 };
        char *body;
        S.v = cv[tc[j]]; S.nv = ncv;
        body = substitute(&S, P.tmpl);
        if (P.tmpl_is_file) t->text = body;
        else {
            const char *hdr = is_stimulus_body(body) ? "sg 2 stimulus\n" : "sg 2 waveform\n";
            t->text = xmalloc(strlen(hdr) + strlen(body) + 2);
            sprintf(t->text, "%s%s\n", hdr, body);
        }
        if (!strcmp(P.noise, "per-trial")) sprintf(buf, "trial:%llu:%ld", (unsigned long long)s, j);
        else if (!strcmp(P.noise, "per-condition")) sprintf(buf, "condition:%llu:%ld", (unsigned long long)s, tc[j]);
        else sprintf(buf, "fixed:%llu", (unsigned long long)s);
        t->seed = derive64(buf);
        t->cond = tc[j]; t->rep = tr[j];
        sprintf(buf, "{\"index\": %ld, \"condition\": %ld, \"repetition\": %ld, \"variables\": {",
                j, tc[j], tr[j]);
        put(&info, buf, strlen(buf));
        for (i = 0; i < ncv; i++) {                    /* variables of the trial */
            if (i) put(&info, ", ", 2);
            json_str(&info, S.v[i].name);
            put(&info, ": ", 2);
            if (S.v[i].text) json_str(&info, S.v[i].text);
            else { sprintf(buf, "%.17g", S.v[i].q.v); put(&info, buf, strlen(buf)); }  /* SI */
        }
        sprintf(buf, "}, \"protocol_seed\": \"%llu\", \"order\": \"%s\", \"noise\": \"%s\", ",
                (unsigned long long)s, P.order, P.noise);
        put(&info, buf, strlen(buf));
        put(&info, "\"timing\": ", 10); json_str(&info, P.timing);
        put(&info, ", \"start\": ", 11); json_str(&info, P.start);
        put(&info, "}", 1);
        t->info = info.s;
        sprintf(buf, "%0*ld", n > 10000 ? (int)floor(log10((double)n - 1)) + 1 : 4, j);
        t->name = xstrdup(buf);
    }
    return T;
}

/* ================================================================== */
/* Directory form (spec Sec. 13.4)                                     */
/* ================================================================== */

#include <sys/stat.h>     /* POSIX mkdir */

/* Copy a file named in a trial (use "...", file("...")) into the
 * directory, so that the directory form is self-contained.             */
static void copy_referenced(const char *text, const char *srcdir, const char *outdir)
{
    const char *p = text;
    while ((p = strchr(p, '"'))) {
        const char *e = strchr(p + 1, '"');
        char name[512], from[1024], to[1024];
        FILE *f, *g;
        if (!e) break;
        if ((size_t)(e - p - 1) < sizeof name && e > p + 1 && p[1] != '/') {
            memcpy(name, p + 1, (size_t)(e - p - 1)); name[e - p - 1] = 0;
            sprintf(from, "%.500s%s", srcdir, name);
            sprintf(to, "%.500s/%s", outdir, name);
            if (!strchr(name, '/') && (f = fopen(from, "rb"))) {
                if ((g = fopen(to, "wb"))) {
                    char buf[8192];
                    size_t k;
                    while ((k = fread(buf, 1, sizeof buf, f)) > 0) fwrite(buf, 1, k, g);
                    fclose(g);
                }
                fclose(f);
            } else if (strchr(name, '/'))
                warn("'%s' is in a subdirectory: copy it into the expanded directory by hand", name);
        }
        p = e + 1;
    }
}

void write_directory(const Trials *T, const char *outdir)
{
    char path[1024];
    FILE *f;
    long j;
    mkdir(outdir, 0777);                            /* may already exist */
    for (j = 0; j < T->n; j++) {
        sprintf(path, "%.900s/%s.sg", outdir, T->t[j].name);
        if (!(f = fopen(path, "w"))) die("cannot write '%s'", path);
        fputs(T->t[j].text, f);
        fclose(f);
        copy_referenced(T->t[j].text, T->dir, outdir);
    }
    sprintf(path, "%.900s/protocol.sgi", outdir);
    if (!(f = fopen(path, "w"))) die("cannot write '%s'", path);
    fprintf(f, "sg 2 index\n# expanded by sg %s; protocol seed %llu\n%s\nstart %s\n",
            SG_VERSION, (unsigned long long)T->protocol_seed, T->timing, T->start);
    for (j = 0; j < T->n; j++)
        fprintf(f, "%s.sg  seed=%llu   # condition %ld, repetition %ld\n", T->t[j].name,
                (unsigned long long)T->t[j].seed, T->t[j].cond, T->t[j].rep);
    fclose(f);
}

static int cmp_str(const void *a, const void *b)
{
    return strcmp(*(char *const *)a, *(char *const *)b);
}

/* Read a directory form: protocol.sgi if present, else every .sg file in
 * lexical order.  Trials without a seed get have_seed ? seed : a drawn one. */
Trials *read_directory(const char *path, int have_seed, uint64_t seed)
{
    Trials *T = xcalloc(1, sizeof *T);
    char *dir, idx[1024], *text = NULL;
    FILE *f;
    size_t L = strlen(path);
    char **names = NULL;
    uint64_t *seeds = NULL;
    int *has = NULL;
    long n = 0, j;

    if (L > 4 && !strcmp(path + L - 4, ".sgi")) {         /* the index itself */
        char *slash;
        dir = xstrdup(path);
        slash = strrchr(dir, '/');
        if (slash) slash[1] = 0; else dir[0] = 0;
        strcpy(idx, path);
    } else {
        dir = xmalloc(L + 2);
        sprintf(dir, "%s%s", path, path[L-1] == '/' ? "" : "/");
        sprintf(idx, "%.1000sprotocol.sgi", dir);
    }
    T->dir = dir;
    T->timing = "gap 0s"; T->start = "immediately";
    if ((f = fopen(idx, "r"))) {
        char line[2048];
        int first = 1;
        fclose(f);
        text = read_text_file(idx);
        T->source = xstrdup(text);
        while (*text) {                                   /* line by line */
            char *e = strchr(text, '\n'), *h, *tok;
            size_t k = e ? (size_t)(e - text) : strlen(text);
            if (k >= sizeof line) k = sizeof line - 1;
            memcpy(line, text, k); line[k] = 0;
            text += e ? k + 1 : k;
            if ((h = strchr(line, '#'))) *h = 0;
            tok = strtok(line, " \t\r");
            if (!tok) continue;
            if (first) {
                char *a = strtok(NULL, " \t\r"), *b = strtok(NULL, " \t\r");
                if (strcmp(tok, "sg") || !a || strcmp(a, "2") || !b || strcmp(b, "index"))
                    die("%s: an index file starts with 'sg 2 index'", idx);
                first = 0;
            } else if (!strcmp(tok, "period") || !strcmp(tok, "gap")) {
                char *v = strtok(NULL, " \t\r");
                T->timing = xmalloc(strlen(tok) + (v ? strlen(v) : 0) + 2);
                sprintf(T->timing, "%s %s", tok, v ? v : "");
            } else if (!strcmp(tok, "start")) {
                char *v = strtok(NULL, "\r");
                T->start = xstrdup(v ? v + strspn(v, " \t") : "immediately");
            } else {
                char *sd = strtok(NULL, " \t\r");
                names = realloc(names, (size_t)(n + 1) * sizeof *names);
                seeds = realloc(seeds, (size_t)(n + 1) * sizeof *seeds);
                has = realloc(has, (size_t)(n + 1) * sizeof *has);
                names[n] = xstrdup(tok);
                has[n] = sd && !strncmp(sd, "seed=", 5);
                seeds[n] = has[n] ? strtoull(sd + 5, NULL, 10) : 0;
                n++;
            }
        }
    } else {                                              /* no index */
        DIR *d = opendir(dir);
        struct dirent *e;
        if (!d) die("cannot open directory '%s'", path);
        while ((e = readdir(d))) {
            size_t k = strlen(e->d_name);
            if (k > 3 && !strcmp(e->d_name + k - 3, ".sg")) {
                names = realloc(names, (size_t)(n + 1) * sizeof *names);
                names[n++] = xstrdup(e->d_name);
            }
        }
        closedir(d);
        if (n) qsort(names, (size_t)n, sizeof *names, cmp_str);
        seeds = xcalloc((size_t)(n ? n : 1), sizeof *seeds);
        has = xcalloc((size_t)(n ? n : 1), sizeof *has);
        T->source = xstrdup("(directory without index)");
    }
    if (n == 0) die("%s: no trials", path);
    T->n = n;
    T->t = xcalloc((size_t)n, sizeof(Trial));
    for (j = 0; j < n; j++) {
        char p2[1200], info[1400];
        Trial *t = &T->t[j];
        char *dot;
        sprintf(p2, "%.1000s%s", dir, names[j]);
        t->text = read_text_file(p2);
        t->seed = has[j] ? seeds[j] : have_seed ? seed : entropy_seed();
        t->cond = t->rep = -1;
        t->name = xstrdup(names[j]);
        dot = strrchr(t->name, '.');
        if (dot) *dot = 0;
        sprintf(info, "{\"index\": %ld, \"file\": \"%.1000s\", \"timing\": \"%s\", \"start\": \"%s\"}",
                j, names[j], T->timing, T->start);
        t->info = xstrdup(info);
    }
    return T;
}
