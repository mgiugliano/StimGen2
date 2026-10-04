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
 * util.c -- errors, warnings and memory helpers shared by all files.
 */
#include <setjmp.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include "sg.h"

char *g_warnings = NULL;                 /* JSON array body, for provenance */
int g_quiet = 0;                         /* -q: no warnings on stderr */

/* Error trap for library use (sg_api.c): when a trap is set, die() stores
 * the message and jumps back to the caller instead of ending the program.
 * The command-line program never sets a trap, so for it die() prints and
 * exits exactly as it always did.                                        */
static jmp_buf *trap = NULL;
static char errmsg[4096];

void sg_set_trap(jmp_buf *jb) { trap = jb; }
const char *sg_last_error(void) { return errmsg; }

void die(const char *fmt, ...)
{
    va_list ap;
    if (trap) {
        jmp_buf *jb = trap;
        va_start(ap, fmt); vsnprintf(errmsg, sizeof errmsg, fmt, ap); va_end(ap);
        trap = NULL;                       /* one jump per trap */
        longjmp(*jb, 1);
    }
    fprintf(stderr, "sg: error: ");
    va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
    fprintf(stderr, "\n");
    exit(1);
}

void warn(const char *fmt, ...)
{
    char msg[512], *p;
    size_t old = g_warnings ? strlen(g_warnings) : 0;
    va_list ap;
    va_start(ap, fmt); vsnprintf(msg, sizeof msg, fmt, ap); va_end(ap);
    if (!g_quiet) fprintf(stderr, "sg: warning: %s\n", msg);
    for (p = msg; *p; p++) if (*p == '"' || *p == '\\') *p = '\'';
    g_warnings = realloc(g_warnings, old + strlen(msg) + 8);
    sprintf(g_warnings + old, "%s\"%s\"", old ? ", " : "", msg);
}

void *xmalloc(size_t n)
{
    void *p = malloc(n ? n : 1);
    if (!p) die("out of memory");
    return p;
}

void *xcalloc(size_t n, size_t m)
{
    void *p = calloc(n ? n : 1, m ? m : 1);
    if (!p) die("out of memory");
    return p;
}

char *xstrdup(const char *s)
{
    char *d = xmalloc(strlen(s) + 1);
    strcpy(d, s);
    return d;
}
