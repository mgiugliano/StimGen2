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
 * sg.h -- shared declarations of the StimGen 2 reference renderer.
 *
 * The program turns a StimGen 2 description (.sg text) into samples and
 * writes them to a portable binary file (.sgb).  It follows the
 * specification in docs/ (cited below as "spec, Sec. x").
 *
 * Source files:
 *   rng.c    SHA-256, Philox4x64-10 and random variates  (spec Sec. 11)
 *   parse.c  lexer, parser and canonical printer         (spec Sec. 14)
 *   prims.c  primitive generators and their parameters   (spec Sec. 9)
 *   eval.c   static checks, sampling, algebra, stimuli   (spec Secs. 8,10,12)
 *   protocol.c  protocols, trials, directory form          (spec Sec. 13)
 *   sgb.c    .sgb output and provenance record            (spec Sec. 15)
 *   render.c one stimulus: parse, check, rate, seed, realise
 *   util.c   errors, warnings, memory
 *   help.c   built-in help texts
 *   sg_api.c library interface (used by the WebAssembly build)
 *   main.c   command line
 *
 * Language: ISO C99 (needed for exact 64-bit integers, uint64_t).
 */
#ifndef SG_H
#define SG_H

#include <setjmp.h>
#include <stdint.h>
#include <stdio.h>

#define SG_VERSION      "0.2"
#define SG_SPEC_VERSION "2"

/* ------------------------------------------------------------------ */
/* Errors: every error is fatal for a command-line tool.               */
/* ------------------------------------------------------------------ */
void die(const char *fmt, ...);               /* print "sg: error: ..." and exit(1),  */
                                              /* or jump to a trap set by the library */
void sg_set_trap(jmp_buf *jb);                /* NULL: no trap (command line)          */
const char *sg_last_error(void);              /* message of the last trapped error     */
extern int g_quiet;                           /* do not print warnings                 */
void warn(const char *fmt, ...);              /* print and record a warning           */
extern char *g_warnings;                      /* all warnings, as a JSON array body    */
char *xstrdup(const char *s);
void *xmalloc(size_t n);
void *xcalloc(size_t n, size_t m);

/* ------------------------------------------------------------------ */
/* Random numbers (rng.c).                                             */
/* ------------------------------------------------------------------ */
void sha256(const void *data, size_t len, uint8_t out[32]);
void sha256_hex(const void *data, size_t len, char hex[65]);
void mulhilo64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo);
void philox4x64_10(const uint64_t ctr[4], const uint64_t key[2], uint64_t out[4]);

typedef struct {                 /* one random stream (spec Rule 5)          */
    uint64_t key[2];             /* 128-bit key, from SHA-256 (spec Rule 6)  */
    uint64_t block;              /* Philox counter of the next block         */
    uint64_t buf[4];             /* words of the current block               */
    int      nbuf;               /* words of buf not yet used                */
    int      has_gauss;          /* Box-Muller makes pairs: one is cached    */
    double   gauss;
} Stream;

void   stream_init(Stream *s, const char *keystring);
uint64_t stream_word(Stream *s);
double stream_uniform(Stream *s);        /* [0,1), top 53 bits of a word  */
double stream_gauss(Stream *s);          /* standard normal, Box-Muller  */
double stream_exponential(Stream *s);    /* unit mean                    */
void   sin_cos(double x, double *s, double *c);  /* never fused to sincos */
uint64_t entropy_seed(void);             /* master seed from the OS      */

/* ------------------------------------------------------------------ */
/* Dimensions and units (spec Sec. 6.3, Table 14.1).                   */
/* ------------------------------------------------------------------ */
typedef enum { D_NONE, D_TIME, D_FREQ, D_PHASE, D_FRAC,
               D_CURR, D_VOLT, D_COND,
               D_AMP } Dim;              /* D_AMP: "in the channel unit" */
const char *dim_name(Dim d);
int unit_lookup(const char *u, Dim *d, double *factor);  /* 1 if known */
double scale10(double x, int k);     /* x * 10^k, correctly rounded */
int    unit_exp10(double factor);    /* k if factor = 10^k, else 99 */

/* ------------------------------------------------------------------ */
/* Syntax tree (parse.c).                                              */
/* ------------------------------------------------------------------ */
typedef enum { N_NUM, N_PREV, N_IDENT, N_STR, N_LIST, N_CALL,
               N_BIN, N_NEG, N_BLOCK, N_REPEAT } NodeKind;

typedef struct Node Node;

typedef struct {                 /* one segment of a sequence                */
    char   *label;               /* "@name" before it, or NULL               */
    int     has_dur;             /* duration written explicitly?             */
    int64_t dur_ps;              /* its duration in picoseconds (exact)      */
    Node   *expr;
    int     line;
} Seg;

typedef struct {                 /* one argument of a call: [name=] value    */
    char *name;
    Node *val;
} Arg;

/* Resolved value of one parameter of a call, filled by the checker. */
typedef struct {
    int     given;               /* written by the user (or has a default)   */
    int     is_prev;             /* the keyword prev                         */
    double  v;                   /* numeric value, canonical unit            */
    const char *kw;              /* keyword value, e.g. "linear"             */
    char   *str;                 /* string value (file path)                 */
    double *list; int nlist;     /* list value (times)                       */
    Node   *wave;                /* waveform argument of a map               */
} Pv;

#define MAXPAR 16

struct Node {
    NodeKind kind;
    int      line;
    /* N_NUM: value in canonical unit, its dimension and raw text.          */
    double   num;  Dim dim;  char *text;
    double   mant; int uexp;           /* number as written, unit = 10^uexp */
    /* N_BIN: operator '+','-','*','/'; operands a,b.  N_NEG: operand a.    */
    char     op;   Node *a, *b;
    /* N_CALL: name and arguments; N_IDENT, N_STR: text in 'text'.          */
    char    *name; Arg *args; int nargs;
    /* N_LIST: items.  N_BLOCK: segments.  N_REPEAT: count, body in a.      */
    Node   **items; int nitems;
    Seg     *segs;  int nsegs;
    int64_t  count;
    /* Filled by the checker (eval.c).                                      */
    int      timed;  int64_t T_ps;   /* kind of the expression (spec 10.2)  */
    int      fn;                     /* index of primitive or map           */
    int      is_map;
    Pv       pv[MAXPAR];
};

/* A channel of a stimulus (spec Sec. 12). */
typedef struct {
    char   *name;
    char   *unit;  Dim udim;  double ufactor;   /* unit of the channel     */
    double  rest;
    int     digital;
    char   *copy_of;             /* "copy NAME", or NULL                     */
    Node   *wave;                /* N_BLOCK                                  */
    double *x;  int64_t n;       /* realised samples                         */
} Channel;

typedef struct { char *name; int64_t sample; } Marker;

typedef struct {
    Channel *ch;  int nch;
    Marker  *mk;  int nmk;
    int      has_rate;  int64_t rate_p, rate_q;   /* rate = p/q Hz (exact)  */
    int      has_seed;  uint64_t seed;
    int      has_dur;   int64_t dur_ps;
    /* explicit markers: name and time, rounded when the rate is known      */
    char   **emk_name; int64_t *emk_ps; int nemk;
    char    *source;             /* verbatim text of every file read         */
    char   **files; char (*file_sha)[65]; int nfiles;   /* file() data      */
} Stimulus;

/* parse.c */
Stimulus *parse_file(const char *path, const char *text, const char *def_unit);
int  parse_decimal_ps(const char *text, int64_t unit_ps, int64_t *ps);
int  parse_rate(const char *text, int64_t *p, int64_t *q);
char *canonical(const Stimulus *s);     /* canonical form (spec Sec. 14.4) */
char *read_text_file(const char *path); /* whole file, NUL-terminated        */

/* prims.c */
typedef struct {                 /* static description of one parameter     */
    const char *name;
    Dim         dim;
    int         kind;            /* P_REQ, P_NUM, ... see prims.c           */
    double      def;             /* default value (P_NUM)                    */
    const char *kws;             /* allowed keywords "a|b|c", default first  */
    const char *help;
} Par;

typedef struct {
    const char *name;
    int         flags;           /* F_STOCH, F_PERIODIC, F_DURAWARE          */
    int         npar;
    Par         par[MAXPAR];
    const char *help;
} Prim;

enum { P_REQ,       /* required number                                 */
       P_NUM,       /* number with a default                           */
       P_OPT,       /* optional number without default                 */
       P_AMP_PREV,  /* amplitude whose default is prev                 */
       P_KW,        /* keyword from a list, first one is the default   */
       P_STR,       /* string (file name)                              */
       P_LIST,      /* list of times                                   */
       P_INIT,      /* keyword or amplitude (ou init)                  */
       P_SEED,      /* integer seed, kept as exact text                */
       P_WAVE };    /* waveform argument of a map                      */
enum { F_STOCH = 1, F_PERIODIC = 2, F_DURAWARE = 4 };

extern const Prim prims[];       /* primitives, then maps; NULL-terminated   */
extern const int  nprims;        /* number of primitives (maps follow)       */
int prim_lookup(const char *name);
void print_prim_help(FILE *f, const Prim *p);
void prim_check(const Node *call);       /* domains of parameter values */

/* Evaluation context shared by eval.c and prims.c. */
typedef struct {
    const char *chan;            /* channel name, for addresses              */
    uint64_t    mseed;           /* master seed                              */
    int64_t     rate_p, rate_q;  /* exact rate                               */
    double      fs, dt;          /* the same as doubles                      */
    const char *srcdir;          /* directory of the main file (file())     */
    Stimulus   *stim;            /* for recording file digests               */
} Ctx;

double gen_prim(Ctx *c, Node *call, int64_t n0, int64_t N, double prev,
                const char *addr, double *out);

/* eval.c */
int64_t boundary(const Ctx *c, int64_t t_ps);    /* n = R(t * fs), exact    */
void check_stimulus(Stimulus *s);
void realise(Stimulus *s, uint64_t mseed, int64_t rate_p, int64_t rate_q,
             const char *srcdir);

/* protocol.c */
typedef struct {                 /* one trial of a protocol (spec Sec. 13.3) */
    char    *name;               /* "0000", ...                              */
    char    *text;               /* fully substituted .sg text               */
    uint64_t seed;               /* master seed of the trial                 */
    long     cond, rep;          /* condition and repetition (-1: unknown)   */
    char    *info;               /* JSON object for the provenance record    */
} Trial;

typedef struct {
    Trial   *t;  long n;
    char    *timing, *start;     /* "period 5s", "immediately"               */
    char    *source;             /* protocol (or index) text                 */
    char    *dir;                /* directory for relative file names        */
    uint64_t protocol_seed;
} Trials;

Trials *expand_protocol(const char *path, const char *text, const char *dir,
                        int have_seed, uint64_t seed);
Trials *read_directory(const char *path, int have_seed, uint64_t seed);
void    write_directory(const Trials *T, const char *outdir);
const char *file_kind(const char *path, const char *text);  /* header kind */

/* render.c: one stimulus from text to samples */
typedef struct {                 /* settings, as given on the command line   */
    const char *unit, *rate_s;   /* -u, -r (NULL if absent)                  */
    int text, have_seed;         /* -t, -s given                             */
    uint64_t seed;               /* -s                                       */
} SgOpts;
extern char *g_check_warnings;   /* warnings issued while checking           */
Stimulus *render_stimulus(const char *name, const char *text, const char *dir, const SgOpts *o,
                          int seed_fixed, uint64_t tseed, int check,
                          double *fs_out, uint64_t *seed_out, char **canon_out);

/* sg_api.c: library interface (see sg_api.c) */
int sg_api_render(const char *text, const char *rate, const char *seed, const char *unit);
int sg_api_render_trial(const char *text, const char *rate, const char *seed, const char *unit,
                        const char *trial_json, const char *protocol_text);
int sg_api_check(const char *text, const char *unit);
int sg_api_canon(const char *text, const char *unit);
int sg_api_kind(const char *text);
int sg_api_expand(const char *text, const char *seed);
int sg_api_help(const char *topic);
int sg_api_prims(void);
const char *sg_api_error(void);
const char *sg_api_text(void);
const char *sg_api_warnings(void);
const uint8_t *sg_api_result(void);
size_t sg_api_result_size(void);
const char *sg_api_version(void);

/* help.c */
extern const char *sg_about, *sg_license;
void sg_help(FILE *f, const char *topic);     /* a help topic (NULL: overview) */

/* sgb.c */
uint8_t *sgb_build(const Stimulus *s, double fs, uint64_t mseed, const char *canon,
                   const char *trial_json, const char *protocol_text, size_t *len);
void write_sgb(const char *path, const Stimulus *s, double fs,
               uint64_t mseed, const char *canon,
               const char *trial_json, const char *protocol_text);
void write_text(const char *path, const Stimulus *s, double fs);

#endif
