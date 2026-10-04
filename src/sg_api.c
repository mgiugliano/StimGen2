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
 * sg_api.c -- library interface of sg, used by the WebAssembly build (web/).
 *
 * Every function returns 0 on success and -1 on error; sg_api_error() then
 * gives the message (with the line number), exactly as the command line
 * would print it after "sg: error: ".  Results stay valid until the next
 * call.  Nothing is printed: warnings are returned by sg_api_warnings().
 *
 *   sg_api_render(text, rate, seed, unit)  -> the complete .sgb file
 *   sg_api_check(text, unit)               -> summary as JSON
 *   sg_api_canon(text, unit)               -> canonical form
 *   sg_api_kind(text)                      -> waveform | stimulus | protocol | index
 *   sg_api_expand(text, seed)              -> trials of a protocol, as JSON
 *   sg_api_help(topic), sg_api_prims()     -> help text, generators as JSON
 *
 * Empty strings mean "not given" (no -r, no -s, no -u).  The text is read
 * as a file named "editor" in the current directory, so that 'use' and
 * file() find files written there (in the browser: Emscripten's MEMFS).
 */
#include <setjmp.h>
#include <stdlib.h>
#include <string.h>
#include "sg.h"

static uint8_t *res;  static size_t res_len;   /* binary result (.sgb)     */
static char *res_text;                          /* text result              */
static char *warn_text;                         /* warnings, JSON array     */
static const char *NAME = "editor";

typedef struct { char *s; size_t n, cap; } Buf;

static void put(Buf *b, const char *t)
{
    size_t k = strlen(t);
    if (b->n + k + 1 > b->cap) { b->cap = 2 * (b->n + k + 1); b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, t, k + 1);
    b->n += k;
}

static void put_json(Buf *b, const char *t)        /* quoted, escaped */
{
    char u[8];
    put(b, "\"");
    for (; *t; t++) {
        unsigned char c = (unsigned char)*t;
        if (c == '"' || c == '\\') { u[0] = '\\'; u[1] = (char)c; u[2] = 0; put(b, u); }
        else if (c == '\n') put(b, "\\n");
        else if (c == '\t') put(b, "\\t");
        else if (c < 0x20) { sprintf(u, "\\u%04x", c); put(b, u); }
        else { u[0] = (char)c; u[1] = 0; put(b, u); }
    }
    put(b, "\"");
}

static void reset(void)
{
    free(res); res = NULL; res_len = 0;
    free(res_text); res_text = NULL;
    free(warn_text); warn_text = NULL;
    free(g_warnings); g_warnings = NULL;
    free(g_check_warnings); g_check_warnings = NULL;
    g_quiet = 1;
}

static void collect_warnings(void)                 /* checks + realisation */
{
    Buf b = { NULL, 0, 0 };
    put(&b, "[");
    if (g_check_warnings) put(&b, g_check_warnings);
    if (g_check_warnings && g_warnings) put(&b, ", ");
    if (g_warnings) put(&b, g_warnings);
    put(&b, "]");
    warn_text = b.s;
}

static void options(SgOpts *o, const char *rate, const char *seed, const char *unit)
{
    memset(o, 0, sizeof *o);
    o->unit = unit && *unit ? unit : NULL;
    o->rate_s = rate && *rate ? rate : NULL;
    if (seed && *seed) {
        if (strspn(seed, "0123456789") != strlen(seed)) die("seed must be a non-negative integer");
        o->have_seed = 1;
        o->seed = strtoull(seed, NULL, 10);
    }
}

static void free_samples(Stimulus *s)              /* the large buffers */
{
    int i;
    for (i = 0; i < s->nch; i++) { free(s->ch[i].x); s->ch[i].x = NULL; }
}

/* Render a waveform or stimulus: the complete .sgb file, as sg render
 * writes it (trial and protocol may be NULL; they fill the provenance).  */
int sg_api_render(const char *text, const char *rate, const char *seed, const char *unit)
{
    return sg_api_render_trial(text, rate, seed, unit, NULL, NULL);
}

int sg_api_render_trial(const char *text, const char *rate, const char *seed, const char *unit,
                        const char *trial_json, const char *protocol_text)
{
    jmp_buf jb;
    SgOpts o;
    Stimulus *s;
    double fs;
    uint64_t used;
    char *canon;
    reset();
    if (setjmp(jb)) { collect_warnings(); return -1; }
    sg_set_trap(&jb);
    options(&o, rate, seed, unit);
    s = render_stimulus(NAME, text, "", &o, 0, 0, 0, &fs, &used, &canon);
    res = sgb_build(s, fs, used, canon, trial_json && *trial_json ? trial_json : NULL,
                    protocol_text && *protocol_text ? protocol_text : NULL, &res_len);
    free_samples(s);
    sg_set_trap(NULL);
    collect_warnings();
    return 0;
}

/* Check (at the stated rate, or any rate): a short summary as JSON. */
int sg_api_check(const char *text, const char *unit)
{
    jmp_buf jb;
    SgOpts o;
    Stimulus *s;
    double fs;
    uint64_t used;
    char *canon, t[160];
    Buf b = { NULL, 0, 0 };
    int i;
    reset();
    if (setjmp(jb)) { collect_warnings(); return -1; }
    sg_set_trap(&jb);
    options(&o, NULL, "0", unit);
    s = render_stimulus(NAME, text, "", &o, 0, 0, 1, &fs, &used, &canon);
    sprintf(t, "{\"channels\": %d, \"samples\": %lld, \"rate\": %.17g, \"duration\": %.17g, \"names\": [",
            s->nch, (long long)s->ch[0].n, fs, (double)s->ch[0].n / fs);
    put(&b, t);
    for (i = 0; i < s->nch; i++) { if (i) put(&b, ", "); put_json(&b, s->ch[i].name); }
    put(&b, "]}");
    res_text = b.s;
    free_samples(s);
    sg_set_trap(NULL);
    collect_warnings();
    return 0;
}

int sg_api_canon(const char *text, const char *unit)
{
    jmp_buf jb;
    Stimulus *s;
    reset();
    if (setjmp(jb)) return -1;
    sg_set_trap(&jb);
    s = parse_file(NAME, text, unit && *unit ? unit : NULL);
    check_stimulus(s);
    res_text = canonical(s);
    sg_set_trap(NULL);
    return 0;
}

int sg_api_kind(const char *text)
{
    jmp_buf jb;
    reset();
    if (setjmp(jb)) return -1;
    sg_set_trap(&jb);
    res_text = xstrdup(file_kind(NAME, text));
    sg_set_trap(NULL);
    return 0;
}

/* Expand a protocol: {"protocol_seed": "...", "timing": ..., "start": ...,
 * "trials": [{"name", "seed", "text", "info"}, ...]} -- render each trial
 * with sg_api_render_trial(text, rate, seed, unit, info, protocol).      */
int sg_api_expand(const char *text, const char *seed)
{
    jmp_buf jb;
    SgOpts o;
    Trials *T;
    Buf b = { NULL, 0, 0 };
    char t[64];
    long j;
    reset();
    if (setjmp(jb)) return -1;
    sg_set_trap(&jb);
    options(&o, NULL, seed, NULL);
    if (strcmp(file_kind(NAME, text), "protocol")) die("%s: not a protocol (sg 2 protocol)", NAME);
    T = expand_protocol(NAME, text, "", o.have_seed, o.seed);
    sprintf(t, "{\"protocol_seed\": \"%llu\", \"timing\": ", (unsigned long long)T->protocol_seed);
    put(&b, t); put_json(&b, T->timing);
    put(&b, ", \"start\": "); put_json(&b, T->start);
    put(&b, ", \"trials\": [");
    for (j = 0; j < T->n; j++) {
        put(&b, j ? ",\n {\"name\": " : "\n {\"name\": ");
        put_json(&b, T->t[j].name);
        sprintf(t, ", \"seed\": \"%llu\", \"text\": ", (unsigned long long)T->t[j].seed);
        put(&b, t); put_json(&b, T->t[j].text);
        put(&b, ", \"info\": "); put(&b, T->t[j].info);
        put(&b, "}");
    }
    put(&b, "]}");
    res_text = b.s;
    sg_set_trap(NULL);
    return 0;
}

/* A help topic, as printed by sg help TOPIC ("" for the overview). */
int sg_api_help(const char *topic)
{
    jmp_buf jb;
    FILE *f;
    long n;
    reset();
    if (setjmp(jb)) return -1;
    sg_set_trap(&jb);
    f = tmpfile();
    if (!f) die("cannot create a temporary file");
    sg_help(f, topic && *topic ? topic : NULL);
    n = ftell(f);
    rewind(f);
    res_text = xmalloc((size_t)n + 1);
    res_text[fread(res_text, 1, (size_t)n, f)] = 0;
    fclose(f);
    sg_set_trap(NULL);
    return 0;
}

/* The generators and functions with their parameters, as JSON (for forms). */
int sg_api_prims(void)
{
    static const char *kinds[] = { "required", "number", "optional", "amplitude-or-prev",
                                   "keyword", "string", "list", "keyword-or-amplitude", "seed", "waveform" };
    Buf b = { NULL, 0, 0 };
    char t[96];
    int i, k;
    reset();
    put(&b, "[");
    for (i = 0; prims[i].name; i++) {
        const Prim *p = &prims[i];
        put(&b, i ? ",\n {\"name\": " : "\n {\"name\": ");
        put_json(&b, p->name);
        sprintf(t, ", \"map\": %s, \"stochastic\": %s, \"periodic\": %s, \"duration_aware\": %s, \"help\": ",
                i >= nprims ? "true" : "false", p->flags & F_STOCH ? "true" : "false",
                p->flags & F_PERIODIC ? "true" : "false", p->flags & F_DURAWARE ? "true" : "false");
        put(&b, t); put_json(&b, p->help);
        put(&b, ", \"params\": [");
        for (k = 0; k < p->npar; k++) {
            const Par *q = &p->par[k];
            put(&b, k ? ", {\"name\": " : "{\"name\": ");
            put_json(&b, q->name);
            put(&b, ", \"dim\": "); put_json(&b, dim_name(q->dim));
            put(&b, ", \"kind\": "); put_json(&b, kinds[q->kind]);
            sprintf(t, ", \"default\": %.17g, \"keywords\": ", q->def);
            put(&b, t); put_json(&b, q->kws ? q->kws : "");
            put(&b, ", \"help\": "); put_json(&b, q->help);
            put(&b, "}");
        }
        put(&b, "]}");
    }
    put(&b, "]");
    res_text = b.s;
    return 0;
}

const char *sg_api_error(void)       { return sg_last_error(); }
const char *sg_api_text(void)        { return res_text ? res_text : ""; }
const char *sg_api_warnings(void)    { return warn_text ? warn_text : "[]"; }
const uint8_t *sg_api_result(void)   { return res; }
size_t sg_api_result_size(void)      { return res_len; }
const char *sg_api_version(void)     { return SG_VERSION; }
