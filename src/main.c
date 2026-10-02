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
 * Run "sg help" for the full description.
 */
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>     /* POSIX: stat, mkdir (directories of trials) */
#include "sg.h"

/* ================================================================== */
/* Utilities shared by all files                                       */
/* ================================================================== */

char *g_warnings = NULL;                 /* JSON array body, for provenance */
static int quiet = 0;

void die(const char *fmt, ...)
{
    va_list ap;
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
    if (!quiet) fprintf(stderr, "sg: warning: %s\n", msg);
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

/* ================================================================== */
/* Help                                                                */
/* ================================================================== */

#define AUTHOR "Michele Giugliano"
#define YEAR   "2026"

static const char *about =
"sg " SG_VERSION " -- StimGen 2 reference renderer (specification version " SG_SPEC_VERSION ")\n"
"Copyright (c) " YEAR " " AUTHOR ". Released under the MIT License (sg license).\n"
"StimGen 2 continues the symbolic stimulus descriptions developed by\n"
"Michele Giugliano with Maura Arsiero (Bern, 2001-2005) and with Daniele\n"
"Linaro and Joao Couto in the LCG suite (2014).\n";

static const char *license_text =
"MIT License\n\n"
"Copyright (c) " YEAR " " AUTHOR "\n\n"
"Permission is hereby granted, free of charge, to any person obtaining a copy\n"
"of this software and associated documentation files (the \"Software\"), to deal\n"
"in the Software without restriction, including without limitation the rights\n"
"to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
"copies of the Software, and to permit persons to whom the Software is\n"
"furnished to do so, subject to the following conditions:\n\n"
"The above copyright notice and this permission notice shall be included in all\n"
"copies or substantial portions of the Software.\n\n"
"THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
"IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
"FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
"AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
"LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
"OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
"SOFTWARE.\n";

static const char *help_main =
"sg - StimGen 2 reference renderer, version " SG_VERSION "\n"
"Copyright (c) " YEAR " " AUTHOR " -- MIT License (see: sg license)\n"
"\n"
"Turns a StimGen 2 description of an electrophysiological stimulus into\n"
"samples and writes them to a portable binary file (.sgb).\n"
"\n"
"USAGE\n"
"  sg render [options] FILE      realise FILE, write FILE.sgb (or -o NAME)\n"
"  sg render [options] -e TEXT   the description is TEXT (segments separated by ';')\n"
"  sg render [options] PROTOCOL  render every trial into PROTOCOL_sgb/ (or -o DIR)\n"
"  sg render [options] DIR       render the trials of a directory (protocol.sgi)\n"
"  sg expand [options] PROTOCOL DIR   write the trials as .sg files + protocol.sgi\n"
"  sg check  [options] FILE      parse and check FILE (or every trial), write nothing\n"
"  sg canon  [options] FILE      print the canonical form of FILE\n"
"  sg help   [TOPIC]             this text, or help on TOPIC (see below)\n"
"  sg selftest                   run the built-in known-answer tests\n"
"  sg examples                   many ready-to-run example commands\n"
"  sg version                    version, author and credits\n"
"  sg license                    the MIT License\n"
"\n"
"OPTIONS\n"
"  -r, --rate RATE     sampling rate, e.g. 20kHz or 10000 (Hz). Required\n"
"                      unless the stimulus states 'rate'; then they must agree.\n"
"  -s, --seed N        master seed of the noise (an integer). Overrides the\n"
"                      stimulus 'seed'. Without either, one is drawn from the\n"
"                      operating system and recorded in the output. For a\n"
"                      protocol: the protocol seed (order and trial seeds).\n"
"  -o, --output FILE   output file name ('-' with -t: standard output);\n"
"                      for a protocol or directory: the output directory\n"
"  -t, --text          write a text table (time and one column per channel)\n"
"                      instead of .sgb, e.g. for plotting\n"
"  -u, --unit UNIT     unit of the channel of a plain waveform file (pA, mV,\n"
"                      nS, ...); default: none (numbers are used as written)\n"
"  -e, --expr TEXT     read the description from TEXT instead of a file\n"
"  -q, --quiet         do not print warnings (they are still recorded)\n"
"  -h, --help          this text\n"
"\n"
"EXAMPLES\n"
"  sg render -r 20kHz -e '500ms dc(0); 1s dc(300); 500ms dc(0)' -o step.sgb\n"
"  sg render -r 10kHz -t -o - -e '1s sine(1, 5Hz) + ou(0, 0.2, 5ms, seed=1)'\n"
"  sg render -r 20kHz -s 42 pair.sg          # a multichannel stimulus\n"
"  sg help sine                              # parameters of a generator\n"
"\n"
"HELP TOPICS\n"
"  sg examples          many example commands, from a first step to protocols\n"
"  sg help syntax       how a description is written\n"
"  sg help generators   list of the primitive generators\n"
"  sg help NAME         parameters of generator or function NAME\n"
"  sg help maps         functions and operators: + - * / abs pos clip ...\n"
"  sg help units        unit suffixes\n"
"  sg help stimulus     multichannel stimuli, markers, digital lines\n"
"  sg help noise        seeds and reproducibility of noise\n"
"  sg help protocol     repetitions, sweeps, ordering, directory form\n"
"  sg help output       the .sgb file format and the provenance record\n"
"\n"
"The language is specified in docs/build/stimgen2-spec.pdf.\n"
"\n"
"AUTHOR\n"
"  " AUTHOR ". StimGen 2 continues the stimulus descriptions developed with\n"
"  Maura Arsiero (Bern, 2001-2005) and with Daniele Linaro and Joao Couto\n"
"  (LCG, 2014). Released under the MIT License: see 'sg license'.\n";

static const char *help_protocol =
"PROTOCOLS: REPETITIONS, SWEEPS, ORDER, TIMING\n"
"\n"
"    sg 2 protocol\n"
"    sweep  amp = from -300pA to 300pA step 50pA    # or [10pA, 20pA], linspace(a,b,n)\n"
"    sweep  (amp, dur) = [(100pA, 1s), (200pA, 500ms)]   # values that go together\n"
"    let    q = amp * dur                           # derived variable\n"
"    repeat 3                                       # repetitions of each condition\n"
"    order  shuffled-blocks     # sequential | grouped | shuffled | shuffled-blocks\n"
"    period 5s                  # onset to onset; or: gap 2s (pause between trials)\n"
"    noise  per-trial           # per-trial | per-condition (frozen) | fixed\n"
"    seed   2026                # protocol seed; -s overrides it\n"
"    start  immediately         # or: on trigger\n"
"\n"
"    stimulus {                 # or: stimulus \"file.sg\"\n"
"        channel Iinj unit=pA {\n"
"            500ms dc(0)\n"
"            1s    dc($amp)                         # $name, or $( expression )\n"
"            500ms dc(0)\n"
"        }\n"
"    }\n"
"\n"
"Several sweeps combine as a Cartesian product (the first varies slowest).\n"
"Substitution is textual, before the stimulus is parsed. Each trial gets\n"
"a master seed derived from the protocol seed (SHA-256 of 'trial:s:j',\n"
"'condition:s:c' or 'fixed:s'); random orders use Fisher-Yates on the\n"
"stream 'order:s'.\n"
"\n"
"  sg render steps.sg               one .sgb per trial in steps_sgb/\n"
"  sg expand steps.sg steps/        steps/0000.sg ... and steps/protocol.sgi\n"
"  sg render steps/                 the same trials, samples and seeds\n"
"\n"
"Timing (period, gap, trigger) is recorded in every trial's provenance and\n"
"in protocol.sgi; sg renders trials, it does not play them in real time.\n";

static const char *help_syntax =
"SYNTAX OF A WAVEFORM\n"
"\n"
"A waveform is a sequence of segments, one per line (or separated by ';').\n"
"A segment is a duration followed by an expression:\n"
"\n"
"    500ms  dc(0)\n"
"    1s     dc(300)                       # a 300 (pA, mV, ...) step\n"
"    500ms  dc(0)\n"
"\n"
"Durations MUST carry a time unit (s, ms, us, ns, min). Everything after '#'\n"
"is a comment. Generators take parameters by position or by name:\n"
"\n"
"    5s  sine(3, 1Hz)                      # = sine(amp=3, freq=1Hz)\n"
"    5s  sine(3, 1Hz, phase=90deg, offset=10)\n"
"\n"
"Expressions combine generators sample by sample with + - * / and\n"
"parentheses, and with functions such as abs(), pos(), clip():\n"
"\n"
"    5s  sine(50, 8Hz) + ou(mean=0, sd=20, tau=5ms)\n"
"    5s  (1 + 0.5*sine(1, 2Hz)) * sine(80, 40Hz)\n"
"    10s pos(ou(mean=10, sd=5, tau=3ms))\n"
"\n"
"Blocks { ... } are sequences used as values; their duration is the sum of\n"
"their segments, and a generator combined with a block takes its duration:\n"
"\n"
"    { 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(80, 8Hz)\n"
"\n"
"repeat N { ... } repeats a part of the waveform:\n"
"\n"
"    repeat 10 { 2ms dc(1000) ; 48ms dc(0) }\n"
"\n"
"'prev' is the end value of the previous segment:\n"
"\n"
"    1s dc(-50)\n"
"    2s ramp(from=prev, to=200)            # 'from' defaults to prev\n"
"\n"
"'@name' before a segment labels it; the label becomes a marker in the\n"
"output. A file may start with the header line 'sg 2 waveform' or\n"
"'sg 2 stimulus' (see: sg help stimulus).\n";

static const char *help_maps =
"OPERATORS AND FUNCTIONS (applied sample by sample)\n"
"\n"
"  a + b, a - b, a * b, a / b, -a      usual precedence: * / before + -\n"
"  abs(e)          |x|\n"
"  pos(e)          max(x, 0), e.g. a conductance that must stay >= 0\n"
"  sqrt(e)         square root, x >= 0\n"
"  exp(e), log(e)  exponential, natural logarithm (x > 0)\n"
"  pow(e, k)       x^k  (x >= 0 unless k is an integer)\n"
"  spow(e, k)      sign(x) |x|^k, defined for every x\n"
"  clip(e, lo, hi) limit to [lo, hi]\n"
"  min(a, b), max(a, b)\n"
"\n"
"Combined waveforms with their own durations (blocks) must have the same\n"
"duration. A value that is not finite (division by zero, sqrt of a\n"
"negative number, ...) stops the program with an error that names the\n"
"line and the sample: a stimulus is never altered silently.\n";

static const char *help_units =
"UNITS (a number followed immediately by a unit, e.g. 500ms, 20kHz)\n"
"\n"
"  time         s ms us (or µs) ns min    canonical: s\n"
"  frequency    Hz kHz MHz                canonical: Hz\n"
"  phase        rad deg                   canonical: rad\n"
"  fraction     %                         50% = 0.5\n"
"  current      fA pA nA uA mA A\n"
"  voltage      uV mV V\n"
"  conductance  pS nS uS mS S\n"
"\n"
"A number without unit is in the canonical unit of its parameter, except\n"
"segment durations, which must always carry a unit. Amplitudes without a\n"
"unit are in the unit of the channel; with a unit (e.g. dc(0.3nA) on a\n"
"channel in pA) they are converted, and a wrong kind of unit is an error.\n"
"Units are case-sensitive: ms is a millisecond, mS a millisiemens.\n";

static const char *help_stimulus =
"STIMULI: SEVERAL CHANNELS ON ONE TIME BASE\n"
"\n"
"    sg 2 stimulus\n"
"    rate 20kHz                  # optional; else give -r\n"
"    seed 4711                   # optional master seed\n"
"\n"
"    channel pre unit=pA rest=0 {\n"
"        1s     dc(0)\n"
"        @train                  # marker 'pre.train' at the next segment\n"
"        500ms  pulses(amp=2000, rate=20Hz, width=1ms)\n"
"        1s     dc(0)\n"
"    }\n"
"    channel post unit=pA { 2.5s dc(-50) }\n"
"    channel copy1 unit=pA copy pre        # identical samples, noise included\n"
"    channel B unit=pA use \"noise.sg\"      # waveform from a file\n"
"    digital camera { 1s dc(0); 1.5s pulses(1, 100Hz, width=1ms) }\n"
"    marker flash at 1s, 1.5s\n"
"    duration 3s                 # optional; default: longest channel\n"
"\n"
"Channels shorter than the stimulus hold their rest value. Digital channels\n"
"must contain only 0 and 1. A plain waveform file is a stimulus with one\n"
"channel named 'out' (unit from -u).\n";

static const char *help_noise =
"NOISE, SEEDS AND REPRODUCIBILITY\n"
"\n"
"Every stochastic generator (ou, wnoise, unoise, cnoise, Poisson pulses)\n"
"draws from its own random stream. The stream is keyed by SHA-256 of\n"
"\n"
"    \"<master seed>:<address>\"   e.g. \"42:ch=out/s1/o1\"\n"
"\n"
"where the address is the position of the generator in the description,\n"
"or by \"fixed:<n>\" when the generator has seed=n. The stream itself is\n"
"Philox4x64-10 (a counter-based generator); uniform, Gaussian (Box-Muller)\n"
"and exponential variates are derived from it exactly as the\n"
"specification prescribes. Consequences:\n"
"\n"
"  - the same description, rate and master seed give the same samples on\n"
"    every machine (Level B: equal up to the last bits of sin/exp/log);\n"
"  - changing one segment does not change the noise of the others;\n"
"  - seed=n gives the same realisation wherever it is used (frozen noise);\n"
"  - the master seed actually used is always written to the output.\n"
"\n"
"'sg selftest' checks SHA-256 and Philox against published test vectors.\n";

static const char *help_output =
"THE .sgb FILE\n"
"\n"
"  bytes 0-7    'S' 'G' 'B' and five zero bytes\n"
"  bytes 8-15   length L of the header, unsigned 64-bit, little-endian\n"
"  next L       JSON header (UTF-8), padded with spaces: 16 + L is a\n"
"               multiple of 8, so the samples are 8-byte aligned\n"
"  rest         samples: 64-bit IEEE doubles, little-endian, all samples of\n"
"               channel 1, then channel 2, ... (the order of 'channels')\n"
"\n"
"The JSON header holds: version, rate, samples (per channel), channels\n"
"(name, unit, rest, digital), markers (name, sample index), and the\n"
"provenance record: the full description text, the SHA-256 of its\n"
"canonical form, data files used and their SHA-256, the rate, the master\n"
"seed (as a string), the SHA-256 of the sample bytes, warnings and a time\n"
"stamp. From description, rate and seed the samples can be regenerated.\n"
"\n"
"Reading it in Python:\n"
"    import json, numpy as np\n"
"    b = open('x.sgb', 'rb').read()\n"
"    L = int.from_bytes(b[8:16], 'little')\n"
"    h = json.loads(b[16:16+L])\n"
"    x = np.frombuffer(b[16+L:], '<f8').reshape(len(h['channels']), -1)\n";

static const char *help_examples =
"EXAMPLES  (indented lines are shell commands: copy, paste, run; the lines\n"
"between <<'END' and END are the content of a file and go with them)\n"
"\n"
"1. A FIRST STIMULUS: a 1 s step of 300 pA, between two 500 ms pauses\n"
"  sg render -r 20kHz -e '500ms dc(0); 1s dc(300); 500ms dc(0)' -o step.sgb\n"
"  sg render -r 20kHz -u pA -e '500ms dc(0); 1s dc(0.3nA); 500ms dc(0)' -o step.sgb\n"
"  sg render -r 1kHz -t -o - -e '5ms dc(0); 5ms dc(1)'     # print the samples\n"
"\n"
"2. ONE LINE PER GENERATOR (sg help generators, sg help NAME)\n"
"  sg render -r 20kHz -e '2s ramp(from=0, to=500)' -o ramp.sgb\n"
"  sg render -r 20kHz -e '5s sine(amp=50, freq=8Hz)' -o sine.sgb\n"
"  sg render -r 20kHz -e '5s square(amp=100, freq=2Hz, duty=25%)' -o square.sgb\n"
"  sg render -r 20kHz -e '5s triangle(amp=100, freq=1Hz)' -o triangle.sgb\n"
"  sg render -r 20kHz -e '10s chirp(amp=50, f0=0.5Hz, f1=20Hz)' -o zap.sgb\n"
"  sg render -r 20kHz -e '200ms biexp(amp=30, tau_rise=1ms, tau_decay=8ms)' -o epsc.sgb\n"
"  sg render -r 20kHz -e '500ms relax(from=0, to=1, tau=50ms)' -o charge.sgb\n"
"  sg render -r 20kHz -e '1s pulses(amp=2000, rate=20Hz, width=1ms)' -o train.sgb\n"
"  sg render -r 20kHz -e '1s pulses(100, 10Hz, width=0.2ms, shape=biphasic)' -o bi.sgb\n"
"\n"
"3. NOISE: new, repeatable, frozen\n"
"  sg render -r 20kHz -e '10s ou(mean=100, sd=50, tau=5ms)' -o noise.sgb   # new each time\n"
"  sg render -r 20kHz -s 42 -e '10s ou(100, 50, 5ms)' -o noise42.sgb      # same with -s 42\n"
"  sg render -r 20kHz -e '10s ou(100, 50, 5ms, seed=17)' -o frozen.sgb    # always the same\n"
"  sg render -r 20kHz -e '5s wnoise(0, 10)' -o white.sgb\n"
"  sg render -r 20kHz -e '20s cnoise(0, 20, alpha=1)' -o pink.sgb\n"
"  sg render -r 20kHz -e '2s pulses(1, 10Hz, width=1ms, timing=poisson)' -o poisson.sgb\n"
"\n"
"4. COMBINING AND TRANSFORMING\n"
"  sg render -r 20kHz -e '5s sine(50, 8Hz) + ou(0, 20, 5ms)' -o mix.sgb\n"
"  sg render -r 20kHz -e '5s (1 + 0.5*sine(1, 2Hz)) * sine(80, 40Hz)' -o am.sgb\n"
"  sg render -r 20kHz -e '{1s ramp(0,1); 3s dc(1); 1s ramp(1,0)} * sine(80, 8Hz)' -o env.sgb\n"
"  sg render -r 20kHz -e '10s pos(ou(10, 5, 3ms))' -o conductance.sgb\n"
"  sg render -r 20kHz -e '5s abs(sine(4, 1Hz))' -o rectified.sgb\n"
"  sg render -r 20kHz -e '1s dc(0); repeat 10 {2ms dc(1000); 48ms dc(0)}; 1s dc(0)' -o ten.sgb\n"
"  sg render -r 20kHz -e '1s dc(-50); 2s ramp(to=200); 1s ramp(to=0)' -o ramps.sgb\n"
"\n"
"5. WORKING WITH FILES\n"
"  printf '500ms dc(0)\\n1s dc(300)\\n500ms dc(0)\\n' > step.sg\n"
"  sg check step.sg                         # syntax, units, durations\n"
"  sg canon step.sg                         # the canonical form\n"
"  sg render -r 20kHz -u pA step.sg         # writes step.sgb\n"
"  sg render -r 20kHz -t step.sg            # writes step.txt (a table)\n"
"\n"
"6. SEVERAL CHANNELS, MARKERS, A CAMERA TRIGGER\n"
"  cat > pair.sg <<'END'\n"
"sg 2 stimulus\n"
"rate 20kHz\n"
"channel pre  unit=pA { 1s dc(0); @train 500ms pulses(2000, 20Hz, width=1ms); 1s dc(0) }\n"
"channel post unit=pA { 2.5s dc(-50) }\n"
"digital cam  { 2.5s pulses(1, 50Hz, width=1ms) }\n"
"END\n"
"  sg render pair.sg                        # rate from the file; writes pair.sgb\n"
"\n"
"7. PROTOCOLS: a family of steps, 3 times each, shuffled, one every 5 s\n"
"  cat > steps.sg <<'END'\n"
"sg 2 protocol\n"
"sweep  amp = from -300pA to 300pA step 50pA\n"
"repeat 3\n"
"order  shuffled-blocks\n"
"period 5s\n"
"stimulus { channel Iinj unit=pA { 500ms dc(0); 1s dc($amp); 500ms dc(0) } }\n"
"END\n"
"  sg check steps.sg                        # 39 trials\n"
"  sg render -r 20kHz -s 1 steps.sg         # steps_sgb/0000.sgb ... 0038.sgb\n"
"  sg expand -s 1 steps.sg steps            # steps/0000.sg ... and steps/protocol.sgi\n"
"  sg render -r 20kHz -o steps_again steps  # the same 39 trials, from the directory\n"
"\n"
"8. IN SCRIPTS\n"
"  for a in 100 200 300; do sg render -q -r 20kHz -e \"1s dc($a)\" -o step_$a.sgb; done\n"
"  sg check step.sg > /dev/null && echo 'step.sg is valid'\n"
"  sg render -q -r 1kHz -t -o - -e '10ms sine(1, 100Hz)' | head -4\n"
"\n"
"Look at any .sgb file with:  python3 tools/sgplot.py FILE.sgb\n";

static void help(const char *topic)
{
    int i;
    if (!topic) { fputs(help_main, stdout); return; }
    if (!strcmp(topic, "syntax"))   { fputs(help_syntax, stdout); return; }
    if (!strcmp(topic, "maps") || !strcmp(topic, "functions")) { fputs(help_maps, stdout); return; }
    if (!strcmp(topic, "units"))    { fputs(help_units, stdout); return; }
    if (!strcmp(topic, "stimulus")) { fputs(help_stimulus, stdout); return; }
    if (!strcmp(topic, "noise") || !strcmp(topic, "seeds")) { fputs(help_noise, stdout); return; }
    if (!strcmp(topic, "output"))   { fputs(help_output, stdout); return; }
    if (!strcmp(topic, "protocol") || !strcmp(topic, "protocols")) { fputs(help_protocol, stdout); return; }
    if (!strcmp(topic, "license"))  { fputs(license_text, stdout); return; }
    if (!strcmp(topic, "examples")) { fputs(help_examples, stdout); return; }
    if (!strcmp(topic, "generators") || !strcmp(topic, "primitives")) {
        printf("PRIMITIVE GENERATORS (sg help NAME for parameters)\n\n");
        for (i = 0; i < nprims; i++) printf("  %-9s %s\n", prims[i].name, prims[i].help);
        printf("\nFunctions: sg help maps\n");
        return;
    }
    i = prim_lookup(topic);
    if (i >= 0) { print_prim_help(stdout, &prims[i]); return; }
    die("no help on '%s' (try: sg help)", topic);
}

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

typedef struct {                 /* command-line settings */
    const char *unit, *rate_s;
    int text, have_seed;
    uint64_t seed;
} Opts;

/* Parse, check and (unless only checking) realise one stimulus text, then
 * write it to 'out'.  'seed_fixed' (protocol trials) overrides everything;
 * otherwise -s, then the stimulus 'seed', then a drawn seed.            */
static Stimulus *render_one(const char *cmd, const char *name, const char *text, const char *dir,
                            const Opts *o, int seed_fixed, uint64_t tseed, const char *out,
                            const char *trial_json, const char *protocol_text, double *fs_out)
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
        if (!strcmp(cmd, "check")) { rp = 10000; rq = 1; }      /* any rate will do */
        else die("no sampling rate: give -r RATE (e.g. -r 20kHz)");
    }
    seed = seed_fixed ? tseed : o->have_seed ? o->seed : s->has_seed ? s->seed : entropy_seed();
    free(g_warnings); g_warnings = NULL;                /* warnings of this stimulus */
    realise(s, seed, rp, rq, dir);
    *fs_out = (double)rp / (double)rq;
    if (out) {
        if (o->text) write_text(out, s, *fs_out);
        else write_sgb(out, s, *fs_out, seed, canon, trial_json, protocol_text);
        if (!quiet && strcmp(out, "-"))
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
    Opts o;
    Trials *T = NULL;
    int i;
    double fs = 0;

    memset(&o, 0, sizeof o);
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { help(NULL); return 0; }
    cmd = argv[1];
    if (!strcmp(cmd, "help"))     { help(argc > 2 ? argv[2] : NULL); return 0; }
    if (!strcmp(cmd, "version") || !strcmp(cmd, "--version")) { fputs(about, stdout); return 0; }
    if (!strcmp(cmd, "license"))  { fputs(license_text, stdout); return 0; }
    if (!strcmp(cmd, "examples")) { fputs(help_examples, stdout); return 0; }
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
        else if (OPT("-q", "--quiet")) quiet = 1;
        else if (OPT("-h", "--help")) { help(NULL); return 0; }
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
        if (!quiet) fprintf(stderr, "sg: wrote %ld trials and protocol.sgi to %s (protocol seed %llu)\n",
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
        else if (!quiet)
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
