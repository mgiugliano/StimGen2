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
 * main.c -- command line of sg, the StimGen 2 reference renderer.
 *
 *   sg render  FILE        samples -> FILE.sgb (or text with -t)
 *   sg check   FILE        parse and check only
 *   sg canon   FILE        print the canonical form
 *   sg help    [TOPIC]     help; TOPIC may be a generator name
 *   sg selftest            known-answer tests of SHA-256, Philox, rounding
 *
 * Run "sg help" for the full description.  The work is done by the library
 * (render.c and the other files); this file only reads the command line
 * and writes the results.
 */
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>     /* POSIX: stat, mkdir (directories of trials) */
#include "sg.h"

/* ================================================================== */
/* Self test: known answers                                            */
/* ================================================================== */

static int selftest(void)
{
    static const struct { const char *in; const char *hex; } sha[] = {
        {"", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"abc", "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        {"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
         "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"} };
    /* Philox4x64-10 known-answer vectors of the Random123 distribution. */
    static const uint64_t ph[3][10] = {
        {0, 0, 0, 0, 0, 0,
         0x16554d9eca36314cu, 0xdb20fe9d672d0fdcu, 0xd7e772cee186176bu, 0x7e68b68aec7ba23bu},
        {~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull,
         0x87b092c3013fe90bu, 0x438c3c67be8d0224u, 0x9cc7d7c69cd777b6u, 0xa09caebf594f0ba0u},
        {0x243f6a8885a308d3u, 0x13198a2e03707344u, 0xa4093822299f31d0u, 0x082efa98ec4e6c89u,
         0x452821e638d01377u, 0xbe5466cf34e90c6cu,
         0xa528f45403e61d95u, 0x38c72dbd566e9788u, 0xa5a1610e72fd18b5u, 0x57bd43b5e52b7fe6u} };
    /* Segment boundaries (spec Sec. 8.2 example: 0.15 ms at 10 kHz). */
    static const int64_t bnd[9] = { 0, 2, 3, 4, 6, 8, 9, 10, 12 };
    int i, fail = 0;
    char hex[65];
    Ctx c;

    for (i = 0; i < 3; i++) {
        sha256_hex(sha[i].in, strlen(sha[i].in), hex);
        printf("SHA-256 \"%.12s%s\": %s\n", sha[i].in, strlen(sha[i].in) > 12 ? "..." : "",
               strcmp(hex, sha[i].hex) ? (fail++, "FAIL") : "ok");
    }
    for (i = 0; i < 3; i++) {
        uint64_t out[4];
        philox4x64_10(ph[i], ph[i] + 4, out);
        printf("Philox4x64-10 vector %d: %s\n", i + 1,
               memcmp(out, ph[i] + 6, sizeof out) ? (fail++, "FAIL") : "ok");
    }
    memset(&c, 0, sizeof c);
    c.rate_p = 10000; c.rate_q = 1;
    for (i = 0; i < 9; i++)
        if (boundary(&c, (int64_t)i * 150000000) != bnd[i]) fail++;
    c.rate_p = 1; c.rate_q = 1;                  /* tie at 0.5 and 1.5 s */
    if (boundary(&c, 500000000000LL) != 0 || boundary(&c, 1500000000000LL) != 2) fail++;
    printf("exact segment boundaries: %s\n", fail ? "see above / FAIL" : "ok");
    printf(fail ? "selftest FAILED\n" : "all tests passed\n");
    return fail ? 1 : 0;
}

/* ================================================================== */
/* Rendering one stimulus                                              */
/* ================================================================== */

/* Render one stimulus (render.c) and write it to 'out'. */
static Stimulus *render_one(const char *cmd, const char *name, const char *text, const char *dir,
                            const SgOpts *o, int seed_fixed, uint64_t tseed, const char *out,
                            const char *trial_json, const char *protocol_text, double *fs_out)
{
    uint64_t seed;
    char *canon;
    Stimulus *s = render_stimulus(name, text, dir, o, seed_fixed, tseed, !strcmp(cmd, "check"),
                                  fs_out, &seed, &canon);
    if (out) {
        if (o->text) write_text(out, s, *fs_out);
        else write_sgb(out, s, *fs_out, seed, canon, trial_json, protocol_text);
        if (!g_quiet && strcmp(out, "-"))
            fprintf(stderr, "sg: wrote %s (%d channel(s) x %lld samples, master seed %llu)\n", out,
                    s->nch, (long long)s->ch[0].n, (unsigned long long)seed);
    }
    return s;
}

static char *dirname_of(const char *path)          /* "a/b.sg" -> "a/" */
{
    char *d = xstrdup(path), *slash = strrchr(d, '/');
    if (slash) slash[1] = 0; else d[0] = 0;
    return d;
}

static char *strip_ext(const char *path)           /* "a/b.sg" -> "a/b" */
{
    char *d = xstrdup(path), *dot = strrchr(d, '.');
    if (dot && !strchr(dot, '/')) *dot = 0;
    return d;
}

static int is_directory(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

/* ================================================================== */
/* Main                                                                */
/* ================================================================== */

int main(int argc, char **argv)
{
    const char *cmd, *file = NULL, *file2 = NULL, *expr = NULL, *out = NULL, *seed_s = NULL;
    const char *kind;
    char *src = NULL, *dir;
    SgOpts o;
    Trials *T = NULL;
    int i;
    double fs = 0;

    memset(&o, 0, sizeof o);
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { sg_help(stdout, NULL); return 0; }
    cmd = argv[1];
    if (!strcmp(cmd, "help"))     { sg_help(stdout, argc > 2 ? argv[2] : NULL); return 0; }
    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) { fputs(sg_about, stdout); return 0; }
    if (!strcmp(cmd, "license"))  { fputs(sg_license, stdout); return 0; }
    if (!strcmp(cmd, "examples")) { sg_help(stdout, "examples"); return 0; }
    if (!strcmp(cmd, "selftest")) return selftest();
    if (strcmp(cmd, "render") && strcmp(cmd, "check") && strcmp(cmd, "canon") && strcmp(cmd, "expand"))
        die("unknown command '%s' (try: sg help)", cmd);

    for (i = 2; i < argc; i++) {                       /* options */
        const char *a = argv[i];
#define OPT(s, l) (!strcmp(a, s) || !strcmp(a, l))
#define ARG() (i + 1 < argc ? argv[++i] : (die("option %s needs a value", a), ""))
        if (OPT("-r", "--rate")) o.rate_s = ARG();
        else if (OPT("-s", "--seed")) seed_s = ARG();
        else if (OPT("-o", "--output")) out = ARG();
        else if (OPT("-u", "--unit")) o.unit = ARG();
        else if (OPT("-e", "--expr")) expr = ARG();
        else if (OPT("-t", "--text")) o.text = 1;
        else if (OPT("-q", "--quiet")) g_quiet = 1;
        else if (OPT("-h", "--help")) { sg_help(stdout, NULL); return 0; }
        else if (a[0] == '-' && a[1]) die("unknown option '%s' (try: sg help)", a);
        else if (!file) file = a;
        else if (!file2) file2 = a;
        else die("too many file names");
    }
    if (file2 && strcmp(cmd, "expand")) die("only one input can be given");
    if (!file == !expr) die("give one input (a file, a directory), or -e TEXT");
    if (seed_s) {
        if (!*seed_s || strspn(seed_s, "0123456789") != strlen(seed_s))
            die("seed must be a non-negative integer");
        o.have_seed = 1; o.seed = strtoull(seed_s, NULL, 10);
    }

    /* What is the input?  A text, a file of some kind, or a directory. */
    if (expr) { src = xstrdup(expr); file = "<command line>"; dir = xstrdup(""); kind = file_kind(file, src); }
    else if (is_directory(file)) { kind = "index"; dir = xstrdup(file); }
    else {
        src = read_text_file(file);
        dir = dirname_of(file);
        kind = file_kind(file, src);
    }

    if (!strcmp(cmd, "expand")) {                      /* spec Sec. 13.4 */
        if (strcmp(kind, "protocol")) die("expand needs a protocol file");
        if (!file2) die("usage: sg expand PROTOCOL DIRECTORY");
        T = expand_protocol(file, src, dir, o.have_seed, o.seed);
        write_directory(T, file2);
        if (!g_quiet) fprintf(stderr, "sg: wrote %ld trials and protocol.sgi to %s (protocol seed %llu)\n",
                            T->n, file2, (unsigned long long)T->protocol_seed);
        return 0;
    }

    if (!strcmp(kind, "protocol") || !strcmp(kind, "index")) {     /* many trials */
        long j;
        char *outdir;
        if (!strcmp(cmd, "canon")) die("the canonical form is defined for waveforms and stimuli only");
        T = !strcmp(kind, "protocol") ? expand_protocol(file, src, dir, o.have_seed, o.seed)
                                      : read_directory(file, o.have_seed, o.seed);
        outdir = out ? xstrdup(out) : NULL;
        if (!outdir && !strcmp(cmd, "render")) {
            char *b = strip_ext(file);
            size_t k = strlen(b);
            while (k && b[k-1] == '/') b[--k] = 0;
            outdir = xmalloc(k + 8);
            sprintf(outdir, "%s_sgb", b);
        }
        if (outdir) mkdir(outdir, 0777);               /* may already exist */
        for (j = 0; j < T->n; j++) {
            char name[1200], *path = NULL;
            if (!strcmp(kind, "index")) sprintf(name, "%.900s%s.sg", T->dir, T->t[j].name);
            else sprintf(name, "%.900s [trial %s]", file, T->t[j].name);   /* same directory */
            if (outdir) {
                path = xmalloc(strlen(outdir) + strlen(T->t[j].name) + 8);
                sprintf(path, "%s/%s.%s", outdir, T->t[j].name, o.text ? "txt" : "sgb");
            }
            render_one(cmd, name, T->t[j].text, T->dir, &o, 1, T->t[j].seed, path,
                       T->t[j].info, T->source, &fs);
            free(path);
        }
        if (!strcmp(cmd, "check"))
            printf("%s: ok, %ld trial(s), protocol seed %llu\n", file, T->n,
                   (unsigned long long)T->protocol_seed);
        else if (!g_quiet)
            fprintf(stderr, "sg: rendered %ld trial(s) into %s/\n", T->n, outdir);
        return 0;
    }

    /* A single waveform or stimulus. */
    if (!strcmp(cmd, "canon")) {
        Stimulus *s = parse_file(file, src, o.unit);
        check_stimulus(s);
        fputs(canonical(s), stdout);
        return 0;
    }
    if (!strcmp(cmd, "render") && !out) {
        char *b = strip_ext(expr ? "stimulus.sg" : file);
        char *t = xmalloc(strlen(b) + 8);
        sprintf(t, "%s.%s", b, o.text ? "txt" : "sgb");
        out = t;
    }
    {
        Stimulus *s = render_one(cmd, file, src, dir, &o, 0, 0, !strcmp(cmd, "render") ? out : NULL,
                                 NULL, NULL, &fs);
        if (!strcmp(cmd, "check"))
            printf("%s: ok, %d channel(s), %lld samples at %g Hz (%.9g s)\n", file, s->nch,
                   (long long)s->ch[0].n, fs, (double)s->ch[0].n / fs);
    }
    return 0;
}
