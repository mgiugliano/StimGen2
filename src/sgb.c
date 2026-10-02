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
 * sgb.c -- output files (spec, Sec. 15).
 *
 * The .sgb format (spec Table 15.1):
 *     8 bytes   "SGB" and five zero bytes
 *     8 bytes   length L of the JSON header, unsigned, little-endian
 *     L bytes   JSON header (UTF-8): rate, channels, markers, provenance,
 *               padded with spaces so that 16 + L is a multiple of 8
 *     8*J*N     samples, IEEE 754 binary64, little-endian, channel after channel
 * Bytes are written one by one, so the file is identical on every machine.
 */
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "sg.h"

typedef struct { char *s; size_t n, cap; } Str;

static void add(Str *b, const char *t)
{
    size_t k = strlen(t);
    if (b->n + k + 1 > b->cap) { b->cap = 2 * (b->n + k + 1); b->s = realloc(b->s, b->cap); }
    memcpy(b->s + b->n, t, k + 1);
    b->n += k;
}

static void addf(Str *b, const char *fmt, double v)
{
    char t[64];
    sprintf(t, fmt, v);
    add(b, t);
}

static void add_json_string(Str *b, const char *t)   /* quoted and escaped */
{
    char u[8];
    add(b, "\"");
    for (; *t; t++) {
        unsigned char ch = (unsigned char)*t;
        if (ch == '"' || ch == '\\') { u[0] = '\\'; u[1] = (char)ch; u[2] = 0; add(b, u); }
        else if (ch == '\n') add(b, "\\n");
        else if (ch == '\t') add(b, "\\t");
        else if (ch < 0x20) { sprintf(u, "\\u%04x", ch); add(b, u); }
        else { u[0] = (char)ch; u[1] = 0; add(b, u); }
    }
    add(b, "\"");
}

static void put_le64(uint8_t *p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i));
}

void write_sgb(const char *path, const Stimulus *s, double fs, uint64_t mseed, const char *canon,
               const char *trial_json, const char *protocol_text)
{
    int64_t N = s->ch[0].n, k;
    size_t nbytes = (size_t)(8 * N * s->nch);
    uint8_t *data = xmalloc(nbytes ? nbytes : 1), head[16];
    char hex[65], t[128];
    Str h = { NULL, 0, 0 };
    int i;
    time_t now = time(NULL);
    FILE *f;

    for (i = 0; i < s->nch; i++)                       /* sample blocks   */
        for (k = 0; k < N; k++) {
            uint64_t bits;
            memcpy(&bits, &s->ch[i].x[k], 8);           /* IEEE 754 bits  */
            put_le64(data + 8 * ((size_t)i * (size_t)N + (size_t)k), bits);
        }

    add(&h, "{\"version\": \"" SG_SPEC_VERSION "\", \"rate\": ");
    addf(&h, "%.17g", fs);
    sprintf(t, ", \"samples\": %lld,\n \"channels\": [", (long long)N); add(&h, t);
    for (i = 0; i < s->nch; i++) {
        add(&h, i ? ", {\"name\": " : "{\"name\": ");
        add_json_string(&h, s->ch[i].name);
        add(&h, ", \"unit\": "); add_json_string(&h, s->ch[i].digital ? "1" : s->ch[i].unit);
        add(&h, ", \"rest\": "); addf(&h, "%.17g", s->ch[i].rest);
        add(&h, s->ch[i].digital ? ", \"digital\": true}" : ", \"digital\": false}");
    }
    add(&h, "],\n \"markers\": [");
    for (i = 0; i < s->nmk; i++) {
        add(&h, i ? ", {\"name\": " : "{\"name\": ");
        add_json_string(&h, s->mk[i].name);
        sprintf(t, ", \"sample\": %lld}", (long long)s->mk[i].sample); add(&h, t);
    }
    /* Provenance record (spec Table 15.2). */
    add(&h, "],\n \"provenance\": {\"spec_version\": \"" SG_SPEC_VERSION "\", "
            "\"implementation\": \"sg " SG_VERSION " (StimGen 2 reference renderer)\", "
            "\"repro_level\": \"B\",\n  \"description\": ");
    add_json_string(&h, s->source);
    sha256_hex(canon, strlen(canon), hex);
    add(&h, ",\n  \"canonical_sha256\": \""); add(&h, hex); add(&h, "\", \"files\": [");
    for (i = 0; i < s->nfiles; i++) {
        add(&h, i ? ", {\"path\": " : "{\"path\": ");
        add_json_string(&h, s->files[i]);
        add(&h, ", \"sha256\": \""); add(&h, s->file_sha[i]); add(&h, "\"}");
    }
    add(&h, "],\n  \"rate\": "); addf(&h, "%.17g", fs);
    /* The seed is a 64-bit integer: written as a string, since many JSON
     * readers keep numbers as doubles and would round it.                 */
    sprintf(t, ", \"master_seed\": \"%llu\", \"trial\": ", (unsigned long long)mseed);
    add(&h, t);
    add(&h, trial_json ? trial_json : "null");      /* protocol trials (13.3) */
    if (protocol_text) { add(&h, ",\n  \"protocol\": "); add_json_string(&h, protocol_text); }
    add(&h, ",\n  \"samples_sha256\": \"");
    sha256_hex(data, nbytes, hex);
    add(&h, hex);
    add(&h, "\",\n  \"warnings\": ["); add(&h, g_warnings ? g_warnings : "");
    strftime(t, sizeof t, "%Y-%m-%dT%H:%M:%SZ", gmtime(&now));
    add(&h, "], \"clipping\": null, \"timestamp\": \""); add(&h, t); add(&h, "\"}}\n");
    while ((16 + h.n) % 8) add(&h, " ");    /* samples start on an 8-byte boundary */

    memcpy(head, "SGB\0\0\0\0\0", 8);
    put_le64(head + 8, (uint64_t)h.n);
    f = fopen(path, "wb");
    if (!f) die("cannot write '%s'", path);
    if (fwrite(head, 1, 16, f) != 16 || fwrite(h.s, 1, h.n, f) != h.n ||
        fwrite(data, 1, nbytes, f) != nbytes) die("error writing '%s'", path);
    fclose(f);
    free(data); free(h.s);
}

/* Plain text: one row per sample, time then one column per channel. */
void write_text(const char *path, const Stimulus *s, double fs)
{
    FILE *f = strcmp(path, "-") ? fopen(path, "w") : stdout;
    int64_t k;
    int i;
    if (!f) die("cannot write '%s'", path);
    fprintf(f, "# t[s]");
    for (i = 0; i < s->nch; i++) fprintf(f, "\t%s[%s]", s->ch[i].name, s->ch[i].unit);
    fprintf(f, "\n");
    for (k = 0; k < s->ch[0].n; k++) {
        fprintf(f, "%.9g", (double)k / fs);
        for (i = 0; i < s->nch; i++) fprintf(f, "\t%.17g", s->ch[i].x[k]);
        fprintf(f, "\n");
    }
    if (f != stdout) fclose(f);
}
