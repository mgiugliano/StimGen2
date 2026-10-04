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
 * render.c -- one stimulus from text to samples: parse, check, choose the
 * rate and the master seed, realise (spec Secs. 8-12).  Used by the command
 * line (main.c) and by the library interface (sg_api.c).
 */
#include <stdlib.h>
#include <string.h>
#include "sg.h"

char *g_check_warnings = NULL;   /* warnings of the checks, before realisation */

/* 'seed_fixed' (protocol trials) overrides everything; otherwise the seed
 * of the options, then the stimulus 'seed', then a drawn seed.  'check'
 * allows a missing rate (any rate will do for checking).               */
Stimulus *render_stimulus(const char *name, const char *text, const char *dir, const SgOpts *o,
                          int seed_fixed, uint64_t tseed, int check,
                          double *fs_out, uint64_t *seed_out, char **canon_out)
{
    Stimulus *s = parse_file(name, text, o->unit);
    int64_t rp = 0, rq = 1;
    uint64_t seed;
    char *canon;
    check_stimulus(s);
    canon = canonical(s);
    if (o->rate_s && !parse_rate(o->rate_s, &rp, &rq)) die("invalid rate '%s'", o->rate_s);
    if (s->has_rate) {
        if (o->rate_s && rp * s->rate_q != s->rate_p * rq)
            die("%s: --rate %s differs from the rate stated in the stimulus", name, o->rate_s);
        rp = s->rate_p; rq = s->rate_q;
    } else if (!o->rate_s) {
        if (check) { rp = 10000; rq = 1; }      /* any rate will do */
        else die("no sampling rate: give -r RATE (e.g. -r 20kHz)");
    }
    seed = seed_fixed ? tseed : o->have_seed ? o->seed : s->has_seed ? s->seed : entropy_seed();
    free(g_check_warnings);
    g_check_warnings = g_warnings;                      /* kept for the library */
    g_warnings = NULL;                                  /* warnings of this stimulus */
    realise(s, seed, rp, rq, dir);
    *fs_out = (double)rp / (double)rq;
    *seed_out = seed;
    *canon_out = canon;
    return s;
}
