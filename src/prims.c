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
 * prims.c -- the primitive generators of StimGen 2 (spec, Sec. 9).
 *
 * The table prims[] lists every primitive (and, after them, the unary and
 * binary maps of spec Sec. 10.4) with its parameters in positional order.
 * The same table drives argument checking, the canonical printer, and the
 * text printed by "sg help <name>".
 *
 * A generator fills N samples out[0..N-1] of one segment and returns the
 * segment's end value (spec Sec. 8.5).  Sample k is evaluated at the
 * grid-aligned local time u = k/fs, and duration-aware generators use the
 * realised duration T = N/fs (spec Sec. 8.4).
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "sg.h"

#define PI     3.141592653589793238462643383279502884
#define TWO_PI 6.283185307179586476925286766559005768

/* Shorthands for the parameter table. */
#define AMP D_AMP
#define TIM D_TIME
#define FRQ D_FREQ
#define PHS D_PHASE
#define FRC D_FRAC
#define NUM D_NONE
#define PHASE  {"phase",  PHS, P_NUM, 0, NULL, "phase at u = 0 (rad, or deg)"}
#define OFFSET {"offset", AMP, P_NUM, 0, NULL, "constant added to the waveform"}
#define CLOCK  {"clock",  NUM, P_KW,  0, "local|global", "local: time restarts in each segment; global: time from the start of the waveform"}
#define SEED   {"seed",   NUM, P_SEED, 0, NULL, "fixed seed: the same realisation wherever it is used (frozen noise)"}
#define MEAN   {"mean",   AMP, P_REQ, 0, NULL, "mean"}
#define SD     {"sd",     AMP, P_REQ, 0, NULL, "standard deviation (>= 0)"}
#define WAVE(n) {n, NUM, P_WAVE, 0, NULL, "waveform"}

const Prim prims[] = {
  {"dc", 0, 1, {{"level", AMP, P_REQ, 0, NULL, "constant level"}},
   "Constant level."},
  {"ramp", F_DURAWARE, 2, {
     {"from", AMP, P_AMP_PREV, 0, NULL, "start value (default: prev, the end value of the previous segment)"},
     {"to",   AMP, P_REQ, 0, NULL, "value reached at the end of the segment"}},
   "Linear ramp from 'from' to 'to' over the segment."},
  {"relax", 0, 3, {
     {"from", AMP, P_AMP_PREV, 0, NULL, "start value (default: prev)"},
     {"to",   AMP, P_REQ, 0, NULL, "asymptotic value"},
     {"tau",  TIM, P_REQ, 0, NULL, "time constant (> 0)"}},
   "Exponential relaxation: to + (from - to) exp(-u/tau)."},
  {"sine", F_PERIODIC, 5, {
     {"amp",  AMP, P_REQ, 0, NULL, "amplitude (half of peak-to-peak)"},
     {"freq", FRQ, P_REQ, 0, NULL, "frequency (>= 0)"}, PHASE, OFFSET, CLOCK},
   "Sinusoid: amp sin(2 pi freq u + phase) + offset."},
  {"square", F_PERIODIC, 6, {
     {"amp",  AMP, P_REQ, 0, NULL, "the wave alternates between offset +/- amp"},
     {"freq", FRQ, P_REQ, 0, NULL, "frequency (> 0)"},
     {"duty", FRC, P_NUM, 0.5, NULL, "fraction of the period at the high level, 0..1 (or %)"},
     PHASE, OFFSET, CLOCK},
   "Square wave with duty cycle; its mean is offset + amp (2 duty - 1)."},
  {"saw", F_PERIODIC, 6, {
     {"amp",  AMP, P_REQ, 0, NULL, "the wave spans offset - amp .. offset + amp"},
     {"freq", FRQ, P_REQ, 0, NULL, "frequency (> 0)"},
     {"duty", FRC, P_NUM, 1, NULL, "fraction of the period spent rising (1: rising saw, 0.5: triangle)"},
     PHASE, OFFSET, CLOCK},
   "Sawtooth: rises during 'duty' of each period, falls during the rest."},
  {"triangle", F_PERIODIC, 5, {
     {"amp",  AMP, P_REQ, 0, NULL, "the wave spans offset - amp .. offset + amp"},
     {"freq", FRQ, P_REQ, 0, NULL, "frequency (> 0)"}, PHASE, OFFSET, CLOCK},
   "Symmetric triangle, the same as saw(..., duty=0.5)."},
  {"chirp", F_DURAWARE, 6, {
     {"amp", AMP, P_REQ, 0, NULL, "amplitude"},
     {"f0",  FRQ, P_REQ, 0, NULL, "frequency at the start of the segment"},
     {"f1",  FRQ, P_REQ, 0, NULL, "frequency at the end of the segment"},
     {"law", NUM, P_KW, 0, "linear|exp", "linear or exponential (equal time per octave) sweep"},
     PHASE, OFFSET},
   "Sinusoid whose frequency sweeps from f0 to f1 over the segment (ZAP)."},
  {"biexp", 0, 5, {
     {"amp",       AMP, P_REQ, 0, NULL, "peak value above offset"},
     {"tau_rise",  TIM, P_REQ, 0, NULL, "rise time constant (> 0)"},
     {"tau_decay", TIM, P_REQ, 0, NULL, "decay time constant (> tau_rise)"},
     {"delay",     TIM, P_NUM, 0, NULL, "onset time within the segment"}, OFFSET},
   "Difference of exponentials normalised to peak amp (synaptic-like)."},
  {"alpha", 0, 4, {
     {"amp",   AMP, P_REQ, 0, NULL, "peak value above offset"},
     {"tau",   TIM, P_REQ, 0, NULL, "time to peak (> 0)"},
     {"delay", TIM, P_NUM, 0, NULL, "onset time within the segment"}, OFFSET},
   "Alpha function amp (v/tau) exp(1 - v/tau), v = u - delay."},
  {"file", 0, 5, {
     {"path",   NUM, P_STR, 0, NULL, "text file, one sample per row"},
     {"rate",   FRQ, P_REQ, 0, NULL, "sampling rate of the file"},
     {"column", NUM, P_NUM, 1, NULL, "column to read (1 = first)"},
     {"interp", NUM, P_KW, 0, "hold|linear", "hold the last sample, or interpolate linearly"},
     {"extend", NUM, P_KW, 0, "error|hold|zero|cycle", "what to do after the last sample"}},
   "Samples read from a text file and resampled."},
  {"pulses", 0, 14, {
     {"amp",       AMP, P_REQ, 0, NULL, "pulse amplitude (peak of the kernel)"},
     {"rate",      FRQ, P_OPT, 0, NULL, "pulse rate (required unless times is given)"},
     {"shape",     NUM, P_KW, 0, "square|biphasic|exp|alpha|biexp", "kernel shape"},
     {"width",     TIM, P_OPT, 0, NULL, "pulse width (square), width of each phase (biphasic)"},
     {"tau",       TIM, P_OPT, 0, NULL, "time constant (exp, alpha)"},
     {"tau_rise",  TIM, P_OPT, 0, NULL, "rise time constant (biexp)"},
     {"tau_decay", TIM, P_OPT, 0, NULL, "decay time constant (biexp)"},
     {"timing",    NUM, P_KW, 0, "regular|poisson", "regular, or Poisson (random) onsets"},
     {"times",     TIM, P_LIST, 0, NULL, "explicit onset times, e.g. [10ms, 25ms]"},
     {"delay",     TIM, P_NUM, 0, NULL, "time of the first regular onset (or offset of all onsets)"},
     {"dead",      TIM, P_NUM, 0, NULL, "dead time after each Poisson onset (< 1/rate)"},
     {"align",     NUM, P_KW, 0, "grid|exact", "grid: onsets and widths rounded to samples, identical pulses"},
     OFFSET, SEED},
   "Train of pulses: offset + amp * sum of kernels at the onset times."},
  {"ou", F_STOCH, 5, {MEAN, SD,
     {"tau",  TIM, P_REQ, 0, NULL, "correlation time (> 0)"},
     {"init", AMP, P_INIT, 0, "stationary|mean|prev", "first sample: stationary draw, the mean, prev, or a value"},
     SEED},
   "Ornstein-Uhlenbeck process (exact update; Gaussian, exponential autocorrelation)."},
  {"wnoise", F_STOCH, 3, {MEAN, SD, SEED},
   "Gaussian white noise, independent samples."},
  {"unoise", F_STOCH, 3, {MEAN, SD, SEED},
   "Uniform white noise with the given mean and standard deviation."},
  {"cnoise", F_STOCH | F_DURAWARE, 6, {MEAN, SD,
     {"alpha", NUM, P_NUM, 1, NULL, "spectral exponent: PSD ~ 1/f^alpha (0 white, 1 pink, 2 brown)"},
     {"fmin",  FRQ, P_OPT, 0, NULL, "lowest frequency (default 1/T)"},
     {"fmax",  FRQ, P_OPT, 0, NULL, "highest frequency (default fs/2)"}, SEED},
   "Gaussian noise with a power-law spectrum, synthesised over the whole segment."},
  /* ---- maps (spec Table 10.4) and binary functions ---- */
  {"abs",  0, 1, {WAVE("e")}, "Absolute value."},
  {"pos",  0, 1, {WAVE("e")}, "Positive part, max(x, 0)."},
  {"sqrt", 0, 1, {WAVE("e")}, "Square root (argument >= 0)."},
  {"exp",  0, 1, {WAVE("e")}, "Exponential."},
  {"log",  0, 1, {WAVE("e")}, "Natural logarithm (argument > 0)."},
  {"pow",  0, 2, {WAVE("e"), {"k", NUM, P_REQ, 0, NULL, "exponent"}}, "Power x^k."},
  {"spow", 0, 2, {WAVE("e"), {"k", NUM, P_REQ, 0, NULL, "exponent"}}, "Signed power sign(x) |x|^k."},
  {"clip", 0, 3, {WAVE("e"), {"lo", AMP, P_REQ, 0, NULL, "lower bound"},
                  {"hi", AMP, P_REQ, 0, NULL, "upper bound"}}, "Clip to [lo, hi]."},
  {"min",  0, 2, {WAVE("a"), WAVE("b")}, "Smaller of two waveforms, sample by sample."},
  {"max",  0, 2, {WAVE("a"), WAVE("b")}, "Larger of two waveforms, sample by sample."},
  {NULL, 0, 0, {{NULL, NUM, 0, 0, NULL, NULL}}, NULL}
};
const int nprims = 16;

int prim_lookup(const char *name)
{
    int i;
    for (i = 0; prims[i].name; i++)
        if (!strcmp(prims[i].name, name)) return i;
    return -1;
}

void print_prim_help(FILE *f, const Prim *p)
{
    int i;
    fprintf(f, "%s: %s\n", p->name, p->help);
    if (p->flags & F_STOCH)    fprintf(f, "  (stochastic)");
    if (p->flags & F_DURAWARE) fprintf(f, "  (duration-aware)");
    if (p->flags & (F_STOCH | F_DURAWARE)) fprintf(f, "\n");
    fprintf(f, "  parameters, in positional order:\n");
    for (i = 0; i < p->npar; i++) {
        const Par *q = &p->par[i];
        char def[64] = "";
        switch (q->kind) {
        case P_REQ: case P_STR: case P_WAVE: strcpy(def, "required"); break;
        case P_NUM:      sprintf(def, "default %g", q->def); break;
        case P_AMP_PREV: strcpy(def, "default prev"); break;
        case P_KW: case P_INIT: {
            const char *bar = strchr(q->kws, '|');
            sprintf(def, "default %.*s", (int)(bar - q->kws), q->kws); break; }
        default: strcpy(def, "optional"); break;
        }
        fprintf(f, "    %-10s %-10s %-18s %s\n", q->name,
                q->kind == P_KW ? "keyword" : q->kind == P_STR ? "string" :
                q->kind == P_LIST ? "times" : q->kind == P_SEED ? "integer" :
                q->kind == P_WAVE ? "waveform" : dim_name(q->dim), def, q->help);
        if (q->kind == P_KW || q->kind == P_INIT)
            fprintf(f, "    %-10s %-10s %-18s one of: %s\n", "", "", "", q->kws);
    }
}

/* ------------------------------------------------------------------ */
/* Static domain checks (spec Sec. 10.7), called by the checker.       */
/* ------------------------------------------------------------------ */

#define V(i)   (n->pv[i].v)
#define HAS(i) (n->pv[i].given)
#define KW(i)  (n->pv[i].kw)

void prim_check(const Node *n)
{
    const char *nm = prims[n->fn].name;
    const char *e = NULL;
    if (!strcmp(nm, "relax") && !(V(2) > 0)) e = "tau must be > 0";
    if (!strcmp(nm, "sine") && V(1) < 0) e = "freq must be >= 0";
    if ((!strcmp(nm, "square") || !strcmp(nm, "saw") || !strcmp(nm, "triangle")) && !(V(1) > 0))
        e = "freq must be > 0";
    if ((!strcmp(nm, "square") || !strcmp(nm, "saw")) && (V(2) < 0 || V(2) > 1))
        e = "duty must be between 0 and 1";
    if (!strcmp(nm, "chirp") && (V(1) < 0 || V(2) < 0)) e = "f0 and f1 must be >= 0";
    if (!strcmp(nm, "chirp") && !strcmp(KW(3), "exp") && !(V(1) > 0 && V(2) > 0))
        e = "an exponential chirp needs f0 > 0 and f1 > 0";
    if (!strcmp(nm, "biexp") && !(V(1) > 0 && V(2) > V(1))) e = "need 0 < tau_rise < tau_decay";
    if ((!strcmp(nm, "biexp") || !strcmp(nm, "alpha")) && V(!strcmp(nm, "biexp") ? 3 : 2) < 0)
        e = "delay must be >= 0";
    if (!strcmp(nm, "alpha") && !(V(1) > 0)) e = "tau must be > 0";
    if (!strcmp(nm, "file") && (!(V(1) > 0) || V(2) < 1 || V(2) != floor(V(2))))
        e = "need rate > 0 and an integer column >= 1";
    if (!strcmp(nm, "ou") && !(V(2) > 0)) e = "tau must be > 0";
    if ((!strcmp(nm, "ou") || !strcmp(nm, "wnoise") || !strcmp(nm, "unoise") ||
         !strcmp(nm, "cnoise")) && V(1) < 0) e = "sd must be >= 0";
    if (!strcmp(nm, "cnoise") && V(2) < 0) e = "alpha must be >= 0";
    if (!strcmp(nm, "clip") && V(1) > V(2)) e = "need lo <= hi";
    if (!strcmp(nm, "pulses")) {
        const char *sh = KW(2);
        if (!HAS(8) && !(HAS(1) && V(1) > 0)) e = "need rate > 0, or a list of times";
        else if (HAS(8) && HAS(1)) e = "give either rate or times, not both";
        else if ((!strcmp(sh, "square") || !strcmp(sh, "biphasic")) && !(HAS(3) && V(3) > 0))
            e = "square and biphasic pulses need width > 0";
        else if ((!strcmp(sh, "exp") || !strcmp(sh, "alpha")) && !(HAS(4) && V(4) > 0))
            e = "exp and alpha pulses need tau > 0";
        else if (!strcmp(sh, "biexp") && !(HAS(5) && HAS(6) && V(5) > 0 && V(6) > V(5)))
            e = "biexp pulses need 0 < tau_rise < tau_decay";
        else if (V(9) < 0 || V(10) < 0) e = "delay and dead must be >= 0";
        else if (V(10) > 0 && !(HAS(1) && V(10) < 1.0 / V(1))) e = "dead must be < 1/rate";
        else if (V(10) > 0 && strcmp(KW(7), "poisson")) e = "dead is used only with timing=poisson";
    }
    if (e) die("line %d: %s: %s", n->line, nm, e);
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static double frac(double x) { return x - floor(x); }

/* Value of a parameter, with prev resolved. */
static double pval(const Node *n, int i, double prev)
{
    return n->pv[i].is_prev ? prev : n->pv[i].v;
}

/* Normalisation of the biexponential: its peak value (spec Eq. 9.8). */
static double biexp_K(double tr, double td)
{
    double r = tr / td;
    return pow(r, tr / (td - tr)) - pow(r, td / (td - tr));
}

/* Random stream of a stochastic instance (spec Rule 6). */
static void open_stream(Stream *s, const Ctx *c, const Node *n, int seed_par, const char *addr)
{
    char key[600];
    if (n->pv[seed_par].given) sprintf(key, "fixed:%s", n->pv[seed_par].str);
    else sprintf(key, "%llu:%.500s", (unsigned long long)c->mseed, addr);
    stream_init(s, key);
}

/* ------------------------------------------------------------------ */
/* FFT: radix-2 for powers of two, Bluestein for any other length.     */
/* Computes X_k = sum_j x_j exp(sign * 2 pi i j k / n), in place.      */
/* ------------------------------------------------------------------ */

static void fft2(double *re, double *im, size_t n, int sign)
{
    size_t i, j, len, k;
    for (i = 1, j = 0; i < n; i++) {                /* bit reversal */
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) { double t = re[i]; re[i] = re[j]; re[j] = t;
                     t = im[i]; im[i] = im[j]; im[j] = t; }
    }
    for (len = 2; len <= n; len <<= 1) {            /* butterflies  */
        double a = sign * TWO_PI / (double)len;
        for (i = 0; i < n; i += len)
            for (k = 0; k < len / 2; k++) {
                double wr, wi;
                sin_cos(a * (double)k, &wi, &wr);
                size_t p = i + k, q = i + k + len / 2;
                double xr = re[q] * wr - im[q] * wi, xi = re[q] * wi + im[q] * wr;
                re[q] = re[p] - xr; im[q] = im[p] - xi;
                re[p] += xr;        im[p] += xi;
            }
    }
}

static void dft(double *re, double *im, size_t n, int sign)
{
    size_t L = 1, j;
    double *ar, *ai, *br, *bi, *wr, *wi;
    if (n <= 1) return;
    if ((n & (n - 1)) == 0) { fft2(re, im, n, sign); return; }
    /* Bluestein: jk = (j^2 + k^2 - (k-j)^2)/2 turns the DFT into a
     * convolution with the chirp w_m = exp(sign i pi m^2 / n).          */
    while (L < 2 * n - 1) L <<= 1;
    ar = xcalloc(L, sizeof *ar); ai = xcalloc(L, sizeof *ai);
    br = xcalloc(L, sizeof *br); bi = xcalloc(L, sizeof *bi);
    wr = xmalloc(n * sizeof *wr); wi = xmalloc(n * sizeof *wi);
    for (j = 0; j < n; j++) {        /* j^2 mod 2n keeps the angle exact */
        double a = sign * PI * (double)((uint64_t)j * j % (2 * (uint64_t)n)) / (double)n;
        sin_cos(a, &wi[j], &wr[j]);
        ar[j] = re[j] * wr[j] - im[j] * wi[j];
        ai[j] = re[j] * wi[j] + im[j] * wr[j];
        br[j] = wr[j]; bi[j] = -wi[j];               /* conj(w_j)        */
        if (j) { br[L - j] = wr[j]; bi[L - j] = -wi[j]; }
    }
    fft2(ar, ai, L, -1); fft2(br, bi, L, -1);
    for (j = 0; j < L; j++) {
        double r = ar[j] * br[j] - ai[j] * bi[j], i = ar[j] * bi[j] + ai[j] * br[j];
        ar[j] = r; ai[j] = i;
    }
    fft2(ar, ai, L, +1);
    for (j = 0; j < n; j++) {
        double r = ar[j] / (double)L, i = ai[j] / (double)L;
        re[j] = r * wr[j] - i * wi[j];
        im[j] = r * wi[j] + i * wr[j];
    }
    free(ar); free(ai); free(br); free(bi); free(wr); free(wi);
}

/* ------------------------------------------------------------------ */
/* file(): read one column of a text file.                             */
/* ------------------------------------------------------------------ */

/* Sample j of a file, with the rule 'extend' beyond its end.  For the end
 * value (one sample past the segment) 'error' holds the last sample, so
 * that a file exactly as long as its segment is accepted.                */
static double file_sample(const Node *n, const double *y, int64_t m, int64_t j, int endv)
{
    const char *ext = n->pv[4].kw;
    if (j < m) return y[j];
    if (!strcmp(ext, "hold") || (endv && !strcmp(ext, "error"))) return y[m - 1];
    if (!strcmp(ext, "zero")) return 0;
    if (!strcmp(ext, "cycle")) return y[j % m];
    die("line %d: file '%s' (%lld samples) is shorter than its segment; see extend=",
        n->line, n->pv[0].str, (long long)m);
    return 0;
}

static double *read_column(Ctx *c, const char *name, int col, int64_t *m)
{
    char *path = xmalloc(strlen(c->srcdir) + strlen(name) + 1), *text, *line, hex[65];
    double *y = NULL;
    int64_t n = 0;
    int i;
    sprintf(path, "%s%s", name[0] == '/' ? "" : c->srcdir, name);
    text = read_text_file(path);
    sha256_hex(text, strlen(text), hex);                 /* provenance */
    for (i = 0; i < c->stim->nfiles && strcmp(c->stim->files[i], path); i++) ;
    if (i == c->stim->nfiles) {
        c->stim->files = realloc(c->stim->files, (size_t)(i + 1) * sizeof(char *));
        c->stim->file_sha = realloc(c->stim->file_sha, (size_t)(i + 1) * sizeof *c->stim->file_sha);
        c->stim->files[i] = xstrdup(path);
        strcpy(c->stim->file_sha[i], hex);
        c->stim->nfiles++;
    }
    for (line = strtok(text, "\n"); line; line = strtok(NULL, "\n")) {
        char *p = line, *end;
        double v = 0;
        int k;
        while (*p == ' ' || *p == '\t') p++;
        if (!*p || *p == '#' || *p == '\r') continue;           /* comments */
        for (k = 1; k <= col; k++) {
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            v = strtod(p, &end);
            if (end == p) die("%s: row %lld has fewer than %d numeric columns", path, (long long)n + 1, col);
            p = end;
        }
        y = realloc(y, (size_t)(n + 1) * sizeof *y);
        y[n++] = v;
    }
    if (n == 0) die("%s: no samples", path);
    free(text); free(path);
    *m = n;
    return y;
}

/* ------------------------------------------------------------------ */
/* Pulse kernels (spec Table 9.2); every kernel has peak 1.            */
/* ------------------------------------------------------------------ */

typedef struct { int shape; double w, tau, tr, td, K; } Kernel;
enum { K_SQUARE, K_BIPHASIC, K_EXP, K_ALPHA, K_BIEXP };

static double kernel(const Kernel *k, double v)
{
    if (v < 0) return 0;
    switch (k->shape) {
    case K_SQUARE:   return v < k->w ? 1.0 : 0.0;
    case K_BIPHASIC: return v < k->w ? 1.0 : v < 2 * k->w ? -1.0 : 0.0;
    case K_EXP:      return exp(-v / k->tau);
    case K_ALPHA:    return v / k->tau * exp(1.0 - v / k->tau);
    default:         return (exp(-v / k->td) - exp(-v / k->tr)) / k->K;
    }
}

/* Duration after which a kernel is below 1e-20 of its peak, far below the
 * resolution of a double: contributions beyond it are not computed.      */
static double kernel_extent(const Kernel *k)
{
    switch (k->shape) {
    case K_SQUARE:   return k->w;
    case K_BIPHASIC: return 2 * k->w;
    case K_EXP:      return 47 * k->tau;
    case K_ALPHA:    return 52 * k->tau;
    default:         return 47 * k->td;
    }
}

static double gen_pulses(Ctx *c, Node *n, int64_t N, double prev, const char *addr, double *out)
{
    const char *sh = KW(2);
    double T = (double)N * c->dt, amp = pval(n, 0, prev), off = pval(n, 12, prev);
    double delay = V(9), end = 0, *on = NULL, ext;
    int grid = !strcmp(KW(11), "grid");
    int64_t non = 0, i, k;
    Kernel K;
    memset(&K, 0, sizeof K);
    K.shape = !strcmp(sh, "square") ? K_SQUARE : !strcmp(sh, "biphasic") ? K_BIPHASIC :
              !strcmp(sh, "exp") ? K_EXP : !strcmp(sh, "alpha") ? K_ALPHA : K_BIEXP;
    K.w = V(3); K.tau = V(4); K.tr = V(5); K.td = V(6);
    if (K.shape == K_BIEXP) K.K = biexp_K(K.tr, K.td);

    /* 1. Onset times inside [0, T) (spec Sec. 9.5). */
    if (n->pv[8].given) {                              /* explicit list */
        for (i = 0; i < n->pv[8].nlist; i++) {
            double s = delay + n->pv[8].list[i];
            if (s >= T) break;
            on = realloc(on, (size_t)(non + 1) * sizeof *on); on[non++] = s;
        }
    } else if (!strcmp(KW(7), "poisson")) {            /* renewal process */
        Stream st;
        double s = delay, mean = 1.0 / V(1) - V(10);
        open_stream(&st, c, n, 13, addr);
        for (;;) {
            s += V(10) + mean * stream_exponential(&st);
            if (s >= T) break;
            on = realloc(on, (size_t)(non + 1) * sizeof *on); on[non++] = s;
        }
    } else {                                           /* regular       */
        for (i = 0; ; i++) {
            double s = delay + (double)i / V(1);
            if (s >= T) break;
            on = realloc(on, (size_t)(non + 1) * sizeof *on); on[non++] = s;
        }
    }

    /* 2. Superposition of kernels. */
    for (k = 0; k < N; k++) out[k] = 0;
    ext = kernel_extent(&K);
    if (grid && (K.w > 0) && (K.w < c->dt))
        warn("line %d: pulse width below the sampling interval", n->line);
    for (i = 0; i < non; i++) {
        if (grid) {                    /* onset on the grid, whole-sample widths */
            int64_t o = (int64_t)nearbyint(on[i] * c->fs), W = 0;
            if (K.shape == K_SQUARE || K.shape == K_BIPHASIC) {
                W = (int64_t)nearbyint(K.w * c->fs);
                if (W < 1) W = 1;
            }
            for (k = o < 0 ? 0 : o; k < N; k++) {
                double v = (double)(k - o) * c->dt;
                if (W) out[k] += (k - o < W) ? 1.0 : (K.shape == K_BIPHASIC && k - o < 2 * W) ? -1.0 : 0.0;
                else out[k] += kernel(&K, v);
                if ((W && k - o >= (K.shape == K_BIPHASIC ? 2 * W : W)) || (!W && v > ext)) break;
            }
            if (W) end += (N - o < W) ? 1.0 : (K.shape == K_BIPHASIC && N - o < 2 * W) ? -1.0 : 0.0;
            else end += kernel(&K, (double)(N - o) * c->dt);
        } else {                       /* exact: evaluate at sample times */
            for (k = (int64_t)ceil(on[i] * c->fs); k < N; k++) {
                double v = (double)k * c->dt - on[i];
                if (v > ext) break;
                out[k] += kernel(&K, v);
            }
            end += kernel(&K, T - on[i]);
        }
    }
    for (k = 0; k < N; k++) out[k] = off + amp * out[k];
    free(on);
    return off + amp * end;
}

/* ------------------------------------------------------------------ */
/* The generators                                                      */
/* ------------------------------------------------------------------ */

double gen_prim(Ctx *c, Node *n, int64_t n0, int64_t N, double prev,
                const char *addr, double *out)
{
    const char *nm = prims[n->fn].name;
    const double fs = c->fs, dt = c->dt, T = (double)N * dt;
    int64_t k;

    if (!strcmp(nm, "dc")) {
        double a = pval(n, 0, prev);
        for (k = 0; k < N; k++) out[k] = a;
        return a;
    }
    if (!strcmp(nm, "ramp")) {                         /* spec Eq. 9.1 */
        double a = pval(n, 0, prev), b = pval(n, 1, prev);
        for (k = 0; k < N; k++) out[k] = a + (b - a) * ((double)k / (double)N);
        return b;
    }
    if (!strcmp(nm, "relax")) {
        double a = pval(n, 0, prev), b = pval(n, 1, prev), tau = V(2);
        for (k = 0; k <= N; k++) {
            double x = b + (a - b) * exp(-(double)k * dt / tau);
            if (k == N) return x;
            out[k] = x;
        }
    }
    if (prims[n->fn].flags & F_PERIODIC) {
        int is_sine = !strcmp(nm, "sine"), is_tri = !strcmp(nm, "triangle");
        int ip = is_sine || is_tri ? 2 : 3;            /* index of phase  */
        double amp = pval(n, 0, prev), f = V(1), ph = V(ip), off = pval(n, ip + 1, prev);
        double duty = is_tri ? 0.5 : is_sine ? 0 : V(2);
        int global = !strcmp(KW(ip + 2), "global");
        for (k = 0; k <= N; k++) {
            double u = (double)(global ? n0 + k : k) / fs, x;
            if (is_sine) x = amp * sin(TWO_PI * f * u + ph);
            else {
                double p = frac(f * u + ph / TWO_PI);
                if (!strcmp(nm, "square")) x = p < duty ? amp : -amp;
                else if (p < duty) x = -amp + 2 * amp * p / duty;          /* saw */
                else x = amp - 2 * amp * (p - duty) / (1 - duty);
            }
            if (k == N) return x + off;
            out[k] = x + off;
        }
    }
    if (!strcmp(nm, "chirp")) {                        /* spec Eqs. 9.5-9.6 */
        double amp = pval(n, 0, prev), f0 = V(1), f1 = V(2), ph = V(4), off = pval(n, 5, prev);
        int ex = !strcmp(KW(3), "exp") && f0 != f1;
        for (k = 0; k <= N; k++) {
            double u = (double)k * dt, Phi;
            if (N == 0) Phi = 0;
            else if (ex) Phi = TWO_PI * f0 * T * (pow(f1 / f0, u / T) - 1) / log(f1 / f0);
            else Phi = TWO_PI * (f0 * u + (f1 - f0) * u * u / (2 * T));
            if (k == N) return amp * sin(Phi + ph) + off;
            out[k] = amp * sin(Phi + ph) + off;
        }
    }
    if (!strcmp(nm, "biexp") || !strcmp(nm, "alpha")) {
        int bi = nm[0] == 'b';
        double amp = pval(n, 0, prev), delay = V(bi ? 3 : 2), off = pval(n, bi ? 4 : 3, prev);
        double tr = V(1), td = bi ? V(2) : 0, K = bi ? biexp_K(tr, td) : 1;
        for (k = 0; k <= N; k++) {
            double v = (double)k * dt - delay, x = off;
            if (v >= 0) x += bi ? amp * (exp(-v / td) - exp(-v / tr)) / K
                                : amp * v / tr * exp(1 - v / tr);
            if (k == N) return x;
            out[k] = x;
        }
    }
    if (!strcmp(nm, "file")) {                         /* spec Sec. 9.4 */
        int64_t m;
        double *y = read_column(c, n->pv[0].str, (int)V(2), &m), r = V(1), x = 0;
        int lin = !strcmp(KW(3), "linear");
        for (k = 0; k <= N; k++) {
            double ur = (double)k * dt * r, w;
            int64_t j = (int64_t)floor(ur);
            w = ur - (double)j;
            x = file_sample(n, y, m, j, k == N);
            if (lin && w > 0) x += w * (file_sample(n, y, m, j + 1, k == N) - x);
            if (k == N) break;
            out[k] = x;
        }
        free(y);
        return x;
    }
    if (!strcmp(nm, "pulses")) return gen_pulses(c, n, N, prev, addr, out);

    /* ---- noise processes: spec Sec. 9.6, consumption order Table 11.1 ---- */
    {
        double mu = pval(n, 0, prev), sd = pval(n, 1, prev);
        Stream st;
        open_stream(&st, c, n, prims[n->fn].npar - 1, addr);
        if (!strcmp(nm, "ou")) {                       /* spec Eq. 9.12 */
            double rho = exp(-dt / V(2)), q = sd * sqrt(1 - rho * rho), x, xi0;
            const Pv *in = &n->pv[3];
            xi0 = stream_gauss(&st);                   /* always drawn  */
            if (in->is_prev) x = prev;
            else if (in->kw && !strcmp(in->kw, "mean")) x = mu;
            else if (in->kw) x = mu + sd * xi0;        /* stationary    */
            else x = in->v;
            for (k = 0; k < N; k++) {
                if (k > 0) x = mu + (x - mu) * rho + q * stream_gauss(&st);
                out[k] = x;
            }
        } else if (!strcmp(nm, "wnoise")) {
            for (k = 0; k < N; k++) out[k] = mu + sd * stream_gauss(&st);
        } else if (!strcmp(nm, "unoise")) {
            for (k = 0; k < N; k++) out[k] = mu + sd * 3.4641016151377545870548926830117 *
                                             (stream_uniform(&st) - 0.5);
        } else if (!strcmp(nm, "cnoise") && N > 0) {   /* spec App. B.6 */
            double fmin = HAS(3) ? V(3) : 1.0 / T, fmax = HAS(4) ? V(4) : fs / 2, Vsum = 0;
            double *re = xcalloc((size_t)N, sizeof *re), *im = xcalloc((size_t)N, sizeof *im);
            int64_t j;
            for (j = 1; 2 * j <= N; j++) {
                double f = (double)j / T, S = (f >= fmin && f <= fmax) ? pow(f, -V(2)) : 0;
                double a = stream_gauss(&st), b = stream_gauss(&st);
                if (2 * j == N) { re[j] = sqrt(S) * a; Vsum += S; }      /* Nyquist */
                else {
                    re[j] = sqrt(S / 2) * a; im[j] = sqrt(S / 2) * b;
                    re[N - j] = re[j]; im[N - j] = -im[j];               /* Hermitian */
                    Vsum += 2 * S;
                }
            }
            if (Vsum == 0) die("line %d: cnoise: no frequency inside [fmin, fmax]", n->line);
            dft(re, im, (size_t)N, +1);
            for (k = 0; k < N; k++) out[k] = mu + sd * re[k] / sqrt(Vsum);
            free(re); free(im);
        }
        return N > 0 ? out[N - 1] : prev;    /* end value: last sample */
    }
}
