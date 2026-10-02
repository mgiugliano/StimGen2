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
 * parse.c -- reading StimGen 2 text (spec, Sec. 14).
 *
 * A hand-written lexer and recursive-descent parser that follow the EBNF
 * grammar of the specification, plus the printer of the canonical form.
 * Durations and the sampling rate are kept as exact integers (picoseconds,
 * and a fraction p/q), so that segment boundaries can be computed exactly
 * (spec Rule 3).  All other numbers are doubles in canonical units.
 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sg.h"

/* ================================================================== */
/* Units (spec Table 14.1).  Factor converts to the canonical unit:    */
/* s, Hz, rad, fraction, and A, V, S for amplitudes.                   */
/* ================================================================== */

static const struct { const char *u; Dim d; double f; } units[] = {
    {"s", D_TIME, 1}, {"ms", D_TIME, 1e-3}, {"us", D_TIME, 1e-6},
    {"ns", D_TIME, 1e-9}, {"min", D_TIME, 60},
    {"Hz", D_FREQ, 1}, {"kHz", D_FREQ, 1e3}, {"MHz", D_FREQ, 1e6},
    {"rad", D_PHASE, 1}, {"deg", D_PHASE, 0.017453292519943295769},
    {"%", D_FRAC, 0.01},
    {"fA", D_CURR, 1e-15}, {"pA", D_CURR, 1e-12}, {"nA", D_CURR, 1e-9},
    {"uA", D_CURR, 1e-6}, {"mA", D_CURR, 1e-3}, {"A", D_CURR, 1},
    {"uV", D_VOLT, 1e-6}, {"mV", D_VOLT, 1e-3}, {"V", D_VOLT, 1},
    {"pS", D_COND, 1e-12}, {"nS", D_COND, 1e-9}, {"uS", D_COND, 1e-6},
    {"mS", D_COND, 1e-3}, {"S", D_COND, 1},
    {NULL, D_NONE, 0} };

int unit_lookup(const char *u, Dim *d, double *factor)
{
    int i;
    for (i = 0; units[i].u; i++)
        if (strcmp(u, units[i].u) == 0) { *d = units[i].d; *factor = units[i].f; return 1; }
    return 0;
}

/* x * 10^k with a single correctly rounded operation: powers of ten up to
 * 10^22 are exact doubles, so we multiply or divide by one of them.
 * (Multiplying by 1e-3, which is not exact, could be off by one bit.)    */
double scale10(double x, int k)
{
    double p = 1;
    int i;
    for (i = 0; i < (k < 0 ? -k : k); i++) p *= 10;
    return k < 0 ? x / p : x * p;
}

int unit_exp10(double f)
{
    int k;
    for (k = -15; k <= 6; k++) if (scale10(1, k) == f) return k;
    return 99;                                  /* min, deg: not a power of ten */
}

const char *dim_name(Dim d)
{
    static const char *n[] = { "number", "time", "frequency", "phase",
                               "fraction", "current", "voltage", "conductance",
                               "amplitude" };
    return n[d];
}

/* ================================================================== */
/* Exact decimals                                                      */
/* ================================================================== */

/* Split a decimal literal "123.45e-6" into mantissa 12345 and exponent -8.
 * Returns 0 if it is malformed or has more than 18 significant digits.   */
static int split_decimal(const char *t, uint64_t *mant, int *exp10)
{
    uint64_t m = 0;
    int e = 0, nd = 0, frac = 0;
    for (; *t; t++) {
        if (isdigit((unsigned char)*t)) {
            if (m == 0 && *t == '0') { if (frac) e--; continue; } /* leading zeros */
            if (++nd > 18) return 0;
            m = m * 10 + (uint64_t)(*t - '0');
            if (frac) e--;
        } else if (*t == '.' && !frac) frac = 1;
        else if (*t == 'e' || *t == 'E') { e += atoi(t + 1); break; }
        else return 0;
    }
    *mant = m; *exp10 = e;
    return 1;
}

/* Duration literal -> exact picoseconds.  unit_ps is the length of the unit
 * in ps (1e12 for s).  Fails if the value is not a whole number of ps.   */
int parse_decimal_ps(const char *text, int64_t unit_ps, int64_t *ps)
{
    uint64_t m; int e;
    if (!split_decimal(text, &m, &e)) return 0;
    if (m != 0 && (uint64_t)unit_ps > (uint64_t)INT64_MAX / m) return 0;
    m *= (uint64_t)unit_ps;
    for (; e > 0; e--) { if (m > (uint64_t)INT64_MAX / 10) return 0; m *= 10; }
    for (; e < 0; e++) { if (m % 10) return 0; m /= 10; }
    *ps = (int64_t)m;
    return 1;
}

static int64_t unit_ps_of(const char *u)
{
    if (!u || !*u || !strcmp(u, "s")) return 1000000000000LL;
    if (!strcmp(u, "ms")) return 1000000000LL;
    if (!strcmp(u, "us")) return 1000000LL;
    if (!strcmp(u, "ns")) return 1000LL;
    if (!strcmp(u, "min")) return 60000000000000LL;
    return 0;
}

/* Rate literal (number and Hz/kHz/MHz) -> exact fraction p/q, q = 10^k. */
int parse_rate(const char *text, int64_t *p, int64_t *q)
{
    char num[64];
    const char *u;
    uint64_t m;
    int e, ue;
    size_t i = strspn(text, "0123456789.eE+-");
    if (i == 0 || i >= sizeof num) return 0;
    memcpy(num, text, i); num[i] = 0;
    u = text + i;
    if (!*u || !strcmp(u, "Hz")) ue = 0;
    else if (!strcmp(u, "kHz")) ue = 3;
    else if (!strcmp(u, "MHz")) ue = 6;
    else return 0;
    if (!split_decimal(num, &m, &e) || m == 0) return 0;
    *q = 1;
    for (e += ue; e > 0; e--) { if (m > (uint64_t)INT64_MAX / 10) return 0; m *= 10; }
    for (; e < 0; e++) { if (*q >= 1000000) return 0; *q *= 10; }
    *p = (int64_t)m;
    return 1;
}

/* ================================================================== */
/* Lexer                                                               */
/* ================================================================== */

enum { T_EOF = 256, T_NL, T_NUM, T_ID, T_STR };

typedef struct {
    const char *file, *s;        /* file name, text                        */
    size_t pos;
    int line;
    char stack[256]; int sp;     /* open brackets: line breaks count only  */
                                 /* at top level or directly inside { }    */
    int tok, tline;              /* current token and its line             */
    char text[512];              /* identifier, string, or number part     */
    char unit[16];               /* unit suffix of a number                */
} Lex;

static void lerr(Lex *L, const char *msg)
{
    die("%s:%d: %s", L->file, L->tline, msg);
}

static void next(Lex *L)
{
    const char *s = L->s;
    for (;;) {                                   /* skip blanks/comments */
        char ch = s[L->pos];
        if (ch == ' ' || ch == '\t' || ch == '\r') L->pos++;
        else if (ch == '#') while (s[L->pos] && s[L->pos] != '\n') L->pos++;
        else if (ch == '\n' && L->sp > 0 && L->stack[L->sp-1] != '{') { L->pos++; L->line++; }
        else break;
    }
    L->tline = L->line;
    L->text[0] = L->unit[0] = 0;
    if (!s[L->pos]) { L->tok = T_EOF; return; }
    if (s[L->pos] == '\n') { L->pos++; L->line++; L->tok = T_NL; return; }

    if (isdigit((unsigned char)s[L->pos]) ||
        (s[L->pos] == '.' && isdigit((unsigned char)s[L->pos+1]))) {
        size_t n = 0, nu = 0;                   /* number, then unit    */
        while (isdigit((unsigned char)s[L->pos]) || s[L->pos] == '.' ||
               ((s[L->pos] == 'e' || s[L->pos] == 'E') &&
                (isdigit((unsigned char)s[L->pos+1]) ||
                 ((s[L->pos+1] == '+' || s[L->pos+1] == '-') &&
                  isdigit((unsigned char)s[L->pos+2]))))) {
            if (s[L->pos] == 'e' || s[L->pos] == 'E') L->text[n++] = s[L->pos++];
            if (n < sizeof L->text - 2) L->text[n++] = s[L->pos++]; else lerr(L, "number too long");
        }
        L->text[n] = 0;
        while (isalpha((unsigned char)s[L->pos]) || s[L->pos] == '%' ||
               ((unsigned char)s[L->pos] == 0xC2 && (unsigned char)s[L->pos+1] == 0xB5)) {
            if ((unsigned char)s[L->pos] == 0xC2) { L->pos += 2; L->unit[nu++] = 'u'; }
            else L->unit[nu++] = s[L->pos++];   /* "µs" is read as "us" */
            if (nu >= sizeof L->unit - 1) lerr(L, "unit too long");
        }
        L->unit[nu] = 0;
        L->tok = T_NUM;
        return;
    }
    if (isalpha((unsigned char)s[L->pos]) || s[L->pos] == '_') {
        size_t n = 0;
        while (isalnum((unsigned char)s[L->pos]) || s[L->pos] == '_') {
            if (n < sizeof L->text - 1) L->text[n++] = s[L->pos++]; else lerr(L, "name too long");
        }
        L->text[n] = 0;
        L->tok = T_ID;
        return;
    }
    if (s[L->pos] == '"') {
        size_t n = 0;
        L->pos++;
        while (s[L->pos] && s[L->pos] != '"' && s[L->pos] != '\n') {
            if (s[L->pos] == '\\' && (s[L->pos+1] == '"' || s[L->pos+1] == '\\')) L->pos++;
            if (n < sizeof L->text - 1) L->text[n++] = s[L->pos++]; else lerr(L, "string too long");
        }
        if (s[L->pos] != '"') lerr(L, "unterminated string");
        L->pos++;
        L->text[n] = 0;
        L->tok = T_STR;
        return;
    }
    L->tok = (unsigned char)s[L->pos++];
    if (L->tok == '(' || L->tok == '[' || L->tok == '{') {
        if (L->sp >= (int)sizeof L->stack) lerr(L, "brackets nested too deeply");
        L->stack[L->sp++] = (char)L->tok;
    } else if (L->tok == ')' || L->tok == ']' || L->tok == '}') {
        char open = L->tok == ')' ? '(' : L->tok == ']' ? '[' : '{';
        if (L->sp == 0 || L->stack[L->sp-1] != open) lerr(L, "unbalanced brackets");
        L->sp--;
    } else if (!strchr("+-*/,=;@$", L->tok)) {
        char m[64];
        sprintf(m, "unexpected character '%c'", L->tok);
        lerr(L, m);
    }
}

static int is_id(Lex *L, const char *w) { return L->tok == T_ID && !strcmp(L->text, w); }
static void skip_nl(Lex *L) { while (L->tok == T_NL) next(L); }
static void expect(Lex *L, int tok, const char *what)
{
    char m[96];
    if (L->tok == tok) { next(L); return; }
    sprintf(m, "expected %s", what);
    lerr(L, m);
}

/* ================================================================== */
/* Parser: waveforms (spec Sec. 14.2.2)                                */
/* ================================================================== */

static Node *new_node(NodeKind k, int line)
{
    Node *n = xcalloc(1, sizeof *n);
    n->kind = k; n->line = line;
    return n;
}

/* A number token becomes N_NUM, with value in canonical unit. */
static Node *num_node(Lex *L)
{
    Node *n = new_node(N_NUM, L->tline);
    double f = 1;
    n->dim = D_NONE;
    if (L->unit[0] && !unit_lookup(L->unit, &n->dim, &f)) {
        char m[64]; sprintf(m, "unknown unit '%s'", L->unit); lerr(L, m);
    }
    n->mant = strtod(L->text, NULL);
    n->uexp = unit_exp10(f);
    n->num = n->uexp != 99 ? scale10(n->mant, n->uexp) : n->mant * f;   /* canonical unit */
    n->text = xmalloc(strlen(L->text) + strlen(L->unit) + 1);
    sprintf(n->text, "%s%s", L->text, L->unit);
    next(L);
    return n;
}

static Node *parse_expr(Lex *L);
static Node *parse_block_body(Lex *L);

static Node *parse_value(Lex *L)         /* argument value: expr|string|id|list */
{
    Node *n;
    if (L->tok == T_STR) {
        n = new_node(N_STR, L->tline); n->text = xstrdup(L->text); next(L);
        return n;
    }
    if (L->tok == '[') {
        n = new_node(N_LIST, L->tline);
        next(L);
        while (L->tok != ']') {
            n->items = realloc(n->items, (size_t)(n->nitems + 1) * sizeof *n->items);
            n->items[n->nitems++] = parse_value(L);
            if (L->tok != ',') break;
            next(L);
        }
        expect(L, ']', "']'");
        return n;
    }
    if (L->tok == T_ID && !is_id(L, "prev") && !is_id(L, "repeat")) {
        /* an identifier not followed by '(' is a keyword argument value */
        size_t p = L->pos;
        while (L->s[p] == ' ' || L->s[p] == '\t') p++;
        if (L->s[p] != '(') {
            n = new_node(N_IDENT, L->tline); n->text = xstrdup(L->text); next(L);
            return n;
        }
    }
    return parse_expr(L);
}

static Node *parse_primary(Lex *L)
{
    Node *n;
    if (L->tok == T_NUM) return num_node(L);
    if (L->tok == '(') {
        next(L);
        n = parse_expr(L);
        expect(L, ')', "')'");
        return n;
    }
    if (L->tok == '{') {
        next(L);
        n = parse_block_body(L);
        expect(L, '}', "'}'");
        return n;
    }
    if (L->tok == '$') lerr(L, "'$' substitutions are allowed only in protocols");
    if (is_id(L, "prev")) { n = new_node(N_PREV, L->tline); next(L); return n; }
    if (is_id(L, "repeat")) {
        n = new_node(N_REPEAT, L->tline);
        next(L);
        if (L->tok != T_NUM || L->unit[0] || strchr(L->text, '.') || strchr(L->text, 'e'))
            lerr(L, "repeat needs a non-negative integer count");
        n->count = strtoll(L->text, NULL, 10);
        next(L);
        if (L->tok != '{') lerr(L, "expected '{' after the repeat count");
        next(L);
        n->a = parse_block_body(L);
        expect(L, '}', "'}'");
        return n;
    }
    if (L->tok == T_ID) {
        n = new_node(N_CALL, L->tline);
        n->name = xstrdup(L->text);
        next(L);
        if (L->tok != '(') {
            char m[128]; sprintf(m, "'%s' must be followed by '(' (or is misplaced)", n->name);
            lerr(L, m);
        }
        next(L);
        while (L->tok != ')') {
            Arg a = { NULL, NULL };
            if (L->tok == T_ID) {                 /* named argument?   */
                size_t p = L->pos;
                while (L->s[p] == ' ' || L->s[p] == '\t') p++;
                if (L->s[p] == '=') {
                    a.name = xstrdup(L->text);
                    next(L); next(L);            /* name and '='      */
                }
            }
            a.val = parse_value(L);
            n->args = realloc(n->args, (size_t)(n->nargs + 1) * sizeof *n->args);
            n->args[n->nargs++] = a;
            if (L->tok != ',') break;
            next(L);
        }
        expect(L, ')', "')' or ','");
        return n;
    }
    lerr(L, "expected a value, a generator, a block or '('");
    return NULL;
}

static Node *parse_unary(Lex *L)
{
    if (L->tok == '-') {
        Node *n = new_node(N_NEG, L->tline);
        next(L); skip_nl(L);
        n->a = parse_primary(L);
        return n;
    }
    return parse_primary(L);
}

static Node *binop(Lex *L, Node *(*sub)(Lex *), const char *ops)
{
    Node *l = sub(L);
    while (L->tok < 256 && L->tok && strchr(ops, L->tok)) {
        Node *n = new_node(N_BIN, L->tline);
        n->op = (char)L->tok;
        next(L); skip_nl(L);                     /* line continues   */
        n->a = l; n->b = sub(L);
        l = n;
    }
    return l;
}
static Node *parse_term(Lex *L) { return binop(L, parse_unary, "*/"); }
static Node *parse_expr(Lex *L) { return binop(L, parse_term, "+-"); }

/* A sequence of items up to '}' or end of file (spec: sequence, item). */
static Node *parse_block_body(Lex *L)
{
    Node *b = new_node(N_BLOCK, L->tline);
    char *label = NULL;
    for (;;) {
        Seg sg;
        while (L->tok == T_NL || L->tok == ';') next(L);
        if (L->tok == '}' || L->tok == T_EOF) break;
        if (L->tok == '@') {                     /* label             */
            next(L);
            if (L->tok != T_ID) lerr(L, "expected a name after '@'");
            if (label) lerr(L, "two labels for one segment");
            label = xstrdup(L->text);
            next(L);
            if (L->tok == T_NL || L->tok == ';') continue;   /* labels the next */
        }
        memset(&sg, 0, sizeof sg);
        sg.line = L->tline;
        sg.label = label; label = NULL;
        if (L->tok == T_NUM && L->unit[0]) {     /* duration rule     */
            Dim d; double f;
            if (unit_lookup(L->unit, &d, &f) && d == D_TIME) {
                if (!parse_decimal_ps(L->text, unit_ps_of(L->unit), &sg.dur_ps))
                    lerr(L, "duration is not a whole number of picoseconds, or too long");
                sg.has_dur = 1;
                next(L);
            }
        }
        sg.expr = parse_expr(L);
        b->segs = realloc(b->segs, (size_t)(b->nsegs + 1) * sizeof *b->segs);
        b->segs[b->nsegs++] = sg;
        if (L->tok != T_NL && L->tok != ';' && L->tok != '}' && L->tok != T_EOF)
            lerr(L, "expected the end of the segment (line break or ';')");
    }
    if (label) lerr(L, "a label must be followed by a segment");
    return b;
}

/* ================================================================== */
/* Parser: files and stimuli (spec Secs. 12, 14.2)                     */
/* ================================================================== */

char *read_text_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long n;
    if (!f) die("cannot open '%s'", path);
    fseek(f, 0, SEEK_END); n = ftell(f); fseek(f, 0, SEEK_SET);
    buf = xmalloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) die("cannot read '%s'", path);
    buf[n] = 0;
    fclose(f);
    return buf;
}

static void lex_init(Lex *L, const char *file, const char *text)
{
    memset(L, 0, sizeof *L);
    L->file = file; L->s = text; L->line = 1;
    next(L);
}

/* Optional header "sg 2 <kind>"; returns the kind ("waveform" if absent). */
static const char *parse_header(Lex *L)
{
    static char kind[32];
    skip_nl(L);
    if (!is_id(L, "sg")) return "waveform";
    next(L);
    if (L->tok != T_NUM || strcmp(L->text, SG_SPEC_VERSION))
        lerr(L, "this program reads version 2 descriptions (\"sg 2 ...\")");
    next(L);
    if (L->tok != T_ID) lerr(L, "expected waveform, stimulus, protocol or index");
    strncpy(kind, L->text, sizeof kind - 1);
    next(L);
    if (L->tok != T_NL && L->tok != ';' && L->tok != T_EOF) lerr(L, "expected end of header line");
    return kind;
}

static void append_source(Stimulus *s, const char *path, const char *text)
{
    size_t old = s->source ? strlen(s->source) : 0;
    s->source = realloc(s->source, old + strlen(path) + strlen(text) + 32);
    sprintf(s->source + old, "%s### file: %s\n%s", old ? "\n" : "", path, text);
}

static char *dir_of(const char *path)          /* "a/b/c.sg" -> "a/b/" */
{
    char *d = xstrdup(path), *slash = strrchr(d, '/');
    if (slash) slash[1] = 0; else d[0] = 0;
    return d;
}

/* "use FILE": a waveform file, read relative to the including file. */
static Node *parse_use(Stimulus *s, const char *from, const char *name)
{
    char *dir = dir_of(from), *path, *text;
    const char *kind;
    Lex L;
    Node *b;
    path = xmalloc(strlen(dir) + strlen(name) + 1);
    sprintf(path, "%s%s", name[0] == '/' ? "" : dir, name);
    text = read_text_file(path);
    append_source(s, path, text);
    lex_init(&L, path, text);
    kind = parse_header(&L);
    if (strcmp(kind, "waveform")) die("%s: 'use' needs a waveform file, not a %s", path, kind);
    b = parse_block_body(&L);
    if (L.tok != T_EOF) lerr(&L, "unexpected '}'");
    free(dir);
    return b;
}

static void add_channel(Stimulus *s, Channel c)
{
    int i;
    for (i = 0; i < s->nch; i++)
        if (!strcmp(s->ch[i].name, c.name)) die("channel '%s' is defined twice", c.name);
    s->ch = realloc(s->ch, (size_t)(s->nch + 1) * sizeof *s->ch);
    s->ch[s->nch++] = c;
}

static void set_unit(Lex *L, Channel *c, const char *u)
{
    c->unit = xstrdup(u);
    c->udim = D_NONE; c->ufactor = 1;
    if (strcmp(u, "1") && (!unit_lookup(u, &c->udim, &c->ufactor) ||
        (c->udim != D_CURR && c->udim != D_VOLT && c->udim != D_COND)))
        lerr(L, "channel unit must be a current, voltage or conductance unit, or 1");
}

static double const_value(Lex *L, Node *n)     /* rest=... : a plain constant */
{
    if (n->kind == N_NUM) return n->num;
    if (n->kind == N_NEG) return -const_value(L, n->a);
    lerr(L, "expected a number");
    return 0;
}

static void parse_stimulus(Lex *L, Stimulus *s)
{
    for (;;) {
        while (L->tok == T_NL || L->tok == ';') next(L);
        if (L->tok == T_EOF) break;
        if (is_id(L, "rate")) {
            char t[64];
            next(L);
            if (L->tok != T_NUM) lerr(L, "expected a rate, e.g. 20kHz");
            sprintf(t, "%.30s%.10s", L->text, L->unit);
            if (!parse_rate(t, &s->rate_p, &s->rate_q)) lerr(L, "invalid rate");
            s->has_rate = 1;
            next(L);
        } else if (is_id(L, "seed")) {
            next(L);
            if (L->tok != T_NUM || L->unit[0] || strspn(L->text, "0123456789") != strlen(L->text))
                lerr(L, "seed must be a non-negative integer");
            s->seed = strtoull(L->text, NULL, 10); s->has_seed = 1;
            next(L);
        } else if (is_id(L, "duration")) {
            next(L);
            if (L->tok != T_NUM || !unit_ps_of(L->unit) || !L->unit[0] ||
                !parse_decimal_ps(L->text, unit_ps_of(L->unit), &s->dur_ps))
                lerr(L, "expected a duration with a time unit");
            s->has_dur = 1;
            next(L);
        } else if (is_id(L, "channel") || is_id(L, "digital")) {
            Channel c;
            memset(&c, 0, sizeof c);
            c.digital = is_id(L, "digital");
            c.unit = xstrdup("1"); c.ufactor = 1;
            next(L);
            if (L->tok != T_ID) lerr(L, "expected a channel name");
            c.name = xstrdup(L->text);
            next(L);
            while (is_id(L, "unit") || is_id(L, "rest")) {
                int is_unit = is_id(L, "unit");
                next(L); expect(L, '=', "'='");
                if (is_unit) {
                    if (c.digital) lerr(L, "digital channels have no unit");
                    if (L->tok == T_ID) set_unit(L, &c, L->text);
                    else if (L->tok == T_NUM && !strcmp(L->text, "1")) set_unit(L, &c, "1");
                    else lerr(L, "expected a unit");
                    next(L);
                } else c.rest = const_value(L, parse_expr(L));
            }
            if (L->tok == '{') {
                next(L);
                c.wave = parse_block_body(L);
                expect(L, '}', "'}'");
            } else if (is_id(L, "use")) {
                next(L);
                if (L->tok != T_STR) lerr(L, "expected a file name in quotes");
                c.wave = parse_use(s, L->file, L->text);
                next(L);
            } else if (is_id(L, "copy")) {
                next(L);
                if (L->tok != T_ID) lerr(L, "expected a channel name");
                c.copy_of = xstrdup(L->text);
                next(L);
            } else lerr(L, "expected '{', use or copy");
            add_channel(s, c);
        } else if (is_id(L, "marker")) {
            char *name;
            next(L);
            if (L->tok != T_ID) lerr(L, "expected a marker name");
            name = xstrdup(L->text);
            next(L);
            if (!is_id(L, "at")) lerr(L, "expected 'at'");
            do {
                int64_t ps = 0;
                next(L);
                if (L->tok != T_NUM || !L->unit[0] || !unit_ps_of(L->unit) ||
                    !parse_decimal_ps(L->text, unit_ps_of(L->unit), &ps))
                    lerr(L, "expected a time with a unit");
                s->emk_name = realloc(s->emk_name, (size_t)(s->nemk + 1) * sizeof(char *));
                s->emk_ps = realloc(s->emk_ps, (size_t)(s->nemk + 1) * sizeof(int64_t));
                s->emk_name[s->nemk] = name; s->emk_ps[s->nemk++] = ps;
                next(L);
            } while (L->tok == ',');
        } else lerr(L, "expected rate, seed, duration, channel, digital or marker");
        if (L->tok != T_NL && L->tok != ';' && L->tok != T_EOF)
            lerr(L, "expected the end of the statement");
    }
}

/* Kind stated by the header of a text ("waveform" if there is none). */
const char *file_kind(const char *path, const char *text)
{
    Lex L;
    lex_init(&L, path, text);
    return parse_header(&L);
}

/* Parse a file.  A waveform becomes a stimulus with one channel "out" whose
 * unit is def_unit (spec Sec. 12.7).                                     */
Stimulus *parse_file(const char *path, const char *text, const char *def_unit)
{
    Stimulus *s = xcalloc(1, sizeof *s);
    const char *kind;
    Lex L;
    append_source(s, path, text);
    lex_init(&L, path, text);
    kind = parse_header(&L);
    if (!strcmp(kind, "waveform")) {
        Channel c;
        memset(&c, 0, sizeof c);
        c.name = xstrdup("out");
        set_unit(&L, &c, def_unit ? def_unit : "1");
        c.wave = parse_block_body(&L);
        if (L.tok != T_EOF) lerr(&L, "unexpected '}'");
        add_channel(s, c);
    } else if (!strcmp(kind, "stimulus")) {
        parse_stimulus(&L, s);
    } else if (!strcmp(kind, "protocol") || !strcmp(kind, "index")) {
        die("%s: a %s cannot be used here (it is not a single stimulus)", path, kind);
    } else die("%s: unknown kind '%s' in header", path, kind);
    if (s->nch == 0) die("%s: no channel", path);
    return s;
}

/* ================================================================== */
/* Canonical form (spec Sec. 14.4)                                     */
/* ================================================================== */

typedef struct { char *s; size_t n, cap; } Buf;

static void put(Buf *b, const char *t)
{
    size_t k = strlen(t);
    if (b->n + k + 1 > b->cap) { b->cap = 2 * (b->n + k + 1); b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, t, k + 1);
    b->n += k;
}

/* Shortest decimal that reads back as exactly the same double. */
static void put_num(Buf *b, double v)
{
    char t[40];
    int p;
    for (p = 1; p <= 17; p++) {
        sprintf(t, "%.*g", p, v);
        if (strtod(t, NULL) == v) break;
    }
    if (strchr(t, 'e') && fabs(v) >= 1e-4 && fabs(v) < 1e16) {   /* 2e+04 -> 20000 */
        int dec = p - 1 - (int)floor(log10(fabs(v)));
        sprintf(t, "%.*f", dec > 0 ? dec : 0, v);
    }
    put(b, t);
}

static void put_ps(Buf *b, int64_t ps)               /* exact seconds */
{
    char t[48];
    int64_t ip = ps / 1000000000000LL, fp = ps % 1000000000000LL;
    int k;
    sprintf(t, "%lld.%012lld", (long long)ip, (long long)fp);
    for (k = (int)strlen(t) - 1; t[k] == '0'; k--) t[k] = 0;
    if (t[k] == '.') t[k] = 0;
    put(b, t); put(b, "s");
}

static const char *unit_of(Dim d)
{
    return d == D_TIME ? "s" : d == D_FREQ ? "Hz" : d == D_PHASE ? "rad" : "";
}

static void put_block(Buf *b, const Node *n, int inline_);

static void put_expr(Buf *b, const Node *n)
{
    int i;
    switch (n->kind) {
    case N_NUM:  put_num(b, n->num); break;
    case N_PREV: put(b, "prev"); break;
    case N_NEG:  put(b, "-"); if (n->a->kind == N_BIN) { put(b, "("); put_expr(b, n->a); put(b, ")"); }
                 else put_expr(b, n->a); break;
    case N_BIN: {
        char op[4] = { ' ', n->op, ' ', 0 };
        if (n->a->kind == N_BIN) { put(b, "("); put_expr(b, n->a); put(b, ")"); } else put_expr(b, n->a);
        put(b, op);
        if (n->b->kind == N_BIN) { put(b, "("); put_expr(b, n->b); put(b, ")"); } else put_expr(b, n->b);
        break; }
    case N_BLOCK: put_block(b, n, 1); break;
    case N_REPEAT: { char t[32]; sprintf(t, "repeat %lld ", (long long)n->count); put(b, t);
                     put_block(b, n->a, 1); break; }
    case N_CALL: {
        const Prim *p = &prims[n->fn];
        int first = 1;
        put(b, p->name); put(b, "(");
        for (i = 0; i < p->npar; i++) {
            const Par *q = &p->par[i];
            const Pv *v = &n->pv[i];
            if (!v->given) continue;
            if (!first) put(b, ", ");
            first = 0;
            if (q->kind == P_WAVE) { put_expr(b, v->wave); continue; }
            put(b, q->name); put(b, "=");
            if (v->is_prev) put(b, "prev");
            else if (v->kw) put(b, v->kw);
            else if (q->kind == P_SEED) put(b, v->str);
            else if (v->str) { put(b, "\""); put(b, v->str); put(b, "\""); }
            else if (q->kind == P_LIST) {
                int j;
                put(b, "[");
                for (j = 0; j < v->nlist; j++) { if (j) put(b, ", "); put_num(b, v->list[j]); put(b, "s"); }
                put(b, "]");
            } else { put_num(b, v->v); put(b, unit_of(q->dim)); }
        }
        put(b, ")");
        break; }
    default: break;
    }
}

static void put_seg(Buf *b, const Seg *s)
{
    if (s->label) { put(b, "@"); put(b, s->label); put(b, " "); }
    if (s->has_dur) { put_ps(b, s->dur_ps); put(b, " "); }
    put_expr(b, s->expr);
}

static void put_block(Buf *b, const Node *n, int inline_)
{
    int i;
    if (inline_) put(b, "{ ");
    for (i = 0; i < n->nsegs; i++) {
        if (i) put(b, inline_ ? " ; " : "\n");
        put_seg(b, &n->segs[i]);
    }
    put(b, inline_ ? " }" : "\n");
}

char *canonical(const Stimulus *s)
{
    Buf b = { NULL, 0, 0 };
    int i;
    char t[96];
    put(&b, "");
    if (s->nch == 1 && !strcmp(s->ch[0].name, "out") && !s->has_rate && !s->has_seed &&
        !s->has_dur && !s->nemk && !s->ch[0].digital) {
        put(&b, "sg 2 waveform\n");
        put_block(&b, s->ch[0].wave, 0);
        return b.s;
    }
    put(&b, "sg 2 stimulus\n");
    if (s->has_rate) {
        put(&b, "rate "); put_num(&b, (double)s->rate_p / (double)s->rate_q); put(&b, "Hz\n");
    }
    if (s->has_seed) { sprintf(t, "seed %llu\n", (unsigned long long)s->seed); put(&b, t); }
    if (s->has_dur) { put(&b, "duration "); put_ps(&b, s->dur_ps); put(&b, "\n"); }
    for (i = 0; i < s->nch; i++) {
        const Channel *c = &s->ch[i];
        put(&b, c->digital ? "digital " : "channel "); put(&b, c->name);
        if (!c->digital) { put(&b, " unit="); put(&b, c->unit); }
        put(&b, " rest="); put_num(&b, c->rest);
        if (c->copy_of) { put(&b, " copy "); put(&b, c->copy_of); put(&b, "\n"); continue; }
        put(&b, " {\n");
        put_block(&b, c->wave, 0);
        put(&b, "}\n");
    }
    for (i = 0; i < s->nemk; i++) {           /* one line per marker time */
        put(&b, "marker "); put(&b, s->emk_name[i]); put(&b, " at ");
        put_ps(&b, s->emk_ps[i]); put(&b, "\n");
    }
    return b.s;
}
