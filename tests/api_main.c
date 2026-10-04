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
 * api_main.c -- a native driver of the library interface (src/sg_api.c),
 * used by tests/test_api.py to check that the library gives exactly what
 * the command line gives.
 *
 *   api_main render FILE RATE SEED UNIT   -> .sgb bytes on stdout
 *   api_main check|canon|kind FILE [UNIT] -> text on stdout
 *   api_main expand FILE SEED             -> JSON on stdout
 *   api_main help TOPIC | prims
 * On error: "sg: error: MESSAGE" on stderr and exit status 1.
 */
#include <stdio.h>
#include <string.h>
#include "../src/sg.h"

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "", *a2 = argc > 2 ? argv[2] : "";
    const char *a3 = argc > 3 ? argv[3] : "", *a4 = argc > 4 ? argv[4] : "", *a5 = argc > 5 ? argv[5] : "";
    char *text = NULL;
    int r;
    if (!strcmp(cmd, "help")) r = sg_api_help(a2);
    else if (!strcmp(cmd, "prims")) r = sg_api_prims();
    else {
        text = read_text_file(a2);
        if (!strcmp(cmd, "render")) r = sg_api_render(text, a3, a4, a5);
        else if (!strcmp(cmd, "check")) r = sg_api_check(text, a3);
        else if (!strcmp(cmd, "canon")) r = sg_api_canon(text, a3);
        else if (!strcmp(cmd, "kind")) r = sg_api_kind(text);
        else if (!strcmp(cmd, "expand")) r = sg_api_expand(text, a3);
        else { fprintf(stderr, "usage: api_main render|check|canon|kind|expand|help|prims ...\n"); return 2; }
    }
    if (r) { fprintf(stderr, "sg: error: %s\n", sg_api_error()); return 1; }
    if (!strcmp(cmd, "render")) fwrite(sg_api_result(), 1, sg_api_result_size(), stdout);
    else fputs(sg_api_text(), stdout);
    if (!strcmp(cmd, "check") || !strcmp(cmd, "render")) fprintf(stderr, "warnings: %s\n", sg_api_warnings());
    return 0;
}
