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
 * help.c -- the built-in documentation: sg help, sg examples, sg version,
 * sg license.  sg_help() writes a topic to any stream.
 */
#include <string.h>
#include "sg.h"

#define AUTHOR "Michele Giugliano"
#define YEAR   "2026"

const char *sg_about =
"sg " SG_VERSION " -- StimGen 2 reference renderer (specification version " SG_SPEC_VERSION ")\n"
"Copyright (c) " YEAR " " AUTHOR ". Released under the MIT License (sg license).\n"
"StimGen 2 continues the symbolic stimulus descriptions developed by\n"
"Michele Giugliano with Maura Arsiero (Bern, 2001-2005) and with Daniele\n"
"Linaro and Joao Couto in the LCG suite (2014).\n";

const char *sg_license =
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

void sg_help(FILE *f, const char *topic)
{
    int i;
    if (!topic) { fputs(help_main, f); return; }
    if (!strcmp(topic, "syntax"))   { fputs(help_syntax, f); return; }
    if (!strcmp(topic, "maps") || !strcmp(topic, "functions")) { fputs(help_maps, f); return; }
    if (!strcmp(topic, "units"))    { fputs(help_units, f); return; }
    if (!strcmp(topic, "stimulus")) { fputs(help_stimulus, f); return; }
    if (!strcmp(topic, "noise") || !strcmp(topic, "seeds")) { fputs(help_noise, f); return; }
    if (!strcmp(topic, "output"))   { fputs(help_output, f); return; }
    if (!strcmp(topic, "protocol") || !strcmp(topic, "protocols")) { fputs(help_protocol, f); return; }
    if (!strcmp(topic, "license"))  { fputs(sg_license, f); return; }
    if (!strcmp(topic, "examples")) { fputs(help_examples, f); return; }
    if (!strcmp(topic, "generators") || !strcmp(topic, "primitives")) {
        fprintf(f, "PRIMITIVE GENERATORS (sg help NAME for parameters)\n\n");
        for (i = 0; i < nprims; i++) fprintf(f, "  %-9s %s\n", prims[i].name, prims[i].help);
        fprintf(f, "\nFunctions: sg help maps\n");
        return;
    }
    i = prim_lookup(topic);
    if (i >= 0) { print_prim_help(f, &prims[i]); return; }
    die("no help on '%s' (try: sg help)", topic);
}
