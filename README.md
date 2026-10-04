# StimGen 2

StimGen 2 is a compact text language for electrophysiological stimulus
waveforms, together with `sg`, its reference renderer. A stimulus is written
as it is thought of:

```
500ms  dc(0)
1s     dc(300)                          # a 300 pA step
500ms  sine(50, 8Hz) + ou(mean=0, sd=20, tau=5ms)
```

and `sg` turns it into samples in a portable binary file (`.sgb`), with a
record that allows every stimulus to be regenerated exactly.

| Path | Content |
|:--|:--|
| `docs/tldr/` | **start here**: a five-page overview, the problem, the idea, two examples (`docs/build/stimgen2-tldr.pdf`) |
| `docs/` | the specification and tutorial (Markdown, built with Pandoc into `docs/build/stimgen2-spec.pdf`) |
| `docs/access/` | a short note on reading `.sgb` files from C, Python and Julia (`docs/build/sgb-access.pdf`) |
| `docs/slides/` | two beginner's decks: `StimGen2-tutorial.pptx` (the language and `sg`; `build_deck.js`) and `StimGen2-planner-tutorial.pptx` (the web planner, step by step; `build_webapp_deck.js`, screenshots by `make_webapp_shots.mjs`) |
| `src/` | `sg`, the command-line renderer (ISO C99, no dependencies), built on a small library (`sg_api.c`) |
| `web/` | the StimGen 2 planner: a web app running the same C code as WebAssembly (`sg.wasm`), online at https://blog.giugliano.info/StimGen2/ |
| `tools/sgplot.py` | a tiny Python reader and plotter for `.sgb` files |
| `tools/sgconvert.py` | describes a `.sgb` file in words, or converts it to CSV, NPZ, MAT, HDF5, ATF (pClamp), NEURON or Brian 2 input |
| `skill/stimgen2/` | an agent skill (plain Markdown and Python, not tied to one agent) that teaches an AI agent to write, check, render, describe, plot and convert StimGen 2 stimuli |
| `tests/` | known-answer, reference, and conformance tests |
| `docs/figures/make_figures.py` | renders every figure of the document with `sg` |

## Build and test

```
make            # builds src/sg (any C99 compiler: cc, gcc, clang)
make test       # self test, 45 reference + 120 conformance tests, converters, agent skill
make docs       # figures and PDFs: spec, .sgb access note, TL;DR (needs python3/matplotlib, pandoc, xelatex)
make wasm       # rebuild web/sg.wasm (needs Emscripten; the result is committed)
make test-wasm  # WebAssembly vs native sg, and the planner in headless Chrome
```

The tests need Python 3 with numpy; plotting needs matplotlib.

## Using `sg`

```
sg render -r 20kHz step.sg                      # writes step.sgb
sg render -r 20kHz -e '500ms dc(0); 1s dc(300); 500ms dc(0)' -o step.sgb
sg render -r 20kHz -s 42 pair.sg                # multichannel, master seed 42
sg render -r 10kHz -t -o - -e '1s sine(1, 5Hz)' # text table on stdout
sg render -r 20kHz steps.sg                     # a protocol: one .sgb per trial in steps_sgb/
sg expand steps.sg steps/                       # directory form: 0000.sg ... + protocol.sgi
sg render -r 20kHz steps/                       # the same trials from the directory
sg check stim.sg                                # parse and check only
sg canon stim.sg                                # canonical form
sg help | sg help syntax | sg help sine         # built-in documentation
sg examples                                     # 40 ready-to-run example commands
sg selftest                                     # SHA-256, Philox, rounding
sg version | sg license                         # author, credits, MIT License
```

| Option | Meaning |
|:--|:--|
| `-r, --rate RATE` | sampling rate (`20kHz`, `10000`). Required unless the stimulus states `rate`; if both are given they must agree. |
| `-s, --seed N` | master seed. Overrides the stimulus `seed`; without either, one is drawn from `/dev/urandom` and recorded. For a protocol: the protocol seed. |
| `-o, --output FILE` | output file; default: input name with `.sgb` (or `.txt`). `-` is standard output (text only). For a protocol or a directory: the output directory (default `NAME_sgb/`). |
| `-t, --text` | write a text table (time, one column per channel) instead of `.sgb`. |
| `-u, --unit UNIT` | unit of the single channel of a plain waveform file (`pA`, `mV`, `nS`...). |
| `-e, --expr TEXT` | the description is given on the command line; `;` separates segments. |
| `-q, --quiet` | do not print warnings (they are still recorded in the output). |

Plotting:

```
python3 tools/sgplot.py step.sgb                 # interactive window
python3 tools/sgplot.py pair.sgb -o pair.pdf -t 0.8:1.6 -c pre
```

and from Python:

```python
from sgplot import load
h, t, x = load("pair.sgb")      # header (dict), time (s), samples[channel, k]
```

## Describing and converting `.sgb` files

`tools/sgconvert.py` needs only numpy (plus scipy for `mat`, h5py for `h5`):

```
python3 tools/sgconvert.py info  step.sgb        # in words: levels, frequencies, trial, seed, description
python3 tools/sgconvert.py csv   step.sgb        # step.csv: time and one column per channel
python3 tools/sgconvert.py npz   step.sgb        # numpy arrays t, x and the header
python3 tools/sgconvert.py mat   step.sgb        # MATLAB
python3 tools/sgconvert.py h5    step.sgb        # HDF5
python3 tools/sgconvert.py atf   step.sgb        # Axon Text File, for pClamp
python3 tools/sgconvert.py neuron step.sgb       # step_<channel>.dat for Vector.play (ms; nA, mV, uS)
python3 tools/sgconvert.py brian step.sgb        # step.brian.npz for brian2.TimedArray
python3 tools/sgconvert.py json  step.sgb        # the header
```

Every format keeps the seed and the description, so the provenance survives
the conversion. In the author's laboratory, the rendered trials are played
at the rig by Gecko, a separate command-line program that drives the
National Instruments board. `sg` only renders: it does not play stimuli in
real time.

## The agent skill

`skill/stimgen2/` packages what an AI agent needs to use StimGen 2: a
`SKILL.md` entry point (YAML front matter and Markdown, readable by any agent
that supports skills, or simply pasted into a prompt), references on the
language, the command line and the `.sgb` format, the verbatim built-in help
of `sg`, ten checked examples, and four scripts:

```
python3 skill/stimgen2/scripts/sg_run.py check fi.sg            # JSON: ok, message
python3 skill/stimgen2/scripts/sg_run.py render -r 20kHz -s 1 fi.sg
python3 skill/stimgen2/scripts/sgb_describe.py fi_sgb/          # one line per trial (or --json)
python3 skill/stimgen2/scripts/sgb_plot.py fi_sgb/ -o fi.png    # trials overlaid (or --stack)
python3 skill/stimgen2/scripts/sgconvert.py neuron fi_sgb/0003.sgb
```

`sg_run.py` finds `sg` through `$SG_BIN`, the PATH, or `src/sg` of a checkout,
and builds it if needed. To install the skill, copy the folder where your
agent looks for skills (for Claude Code: `~/.claude/skills/`). The verbatim
help and the converter are generated from `sg` and `tools/`: run
`python3 skill/build.py` after changing either. `make test` fails if they are
out of date.

## The web planner

**Try it online: https://blog.giugliano.info/StimGen2/** (published from `web/`
by `.github/workflows/pages.yml` on every push).

`web/` is a planner that runs in any modern browser: type a description on
the left, see it rendered on the right while you type. The text stays the
single source of truth; around it, everything can also be done with the
mouse, and every action writes ordinary StimGen 2 text:

* **Generator palette and segment builder**: click a generator, fill in a
  form (units chosen from menus, rarely used options folded away), watch the
  plot preview the new segment, then Insert. Click a segment in the
  **timeline** above the plot (or Edit → Edit the segment at the cursor) to
  change it in the same form; its values are read back through sg's
  canonical form.
* **Menus**: File (new, open, save `.sg` and `.sgb`); Edit (repeat the
  selected lines, combine them with a generator — multiply by an envelope,
  add noise —, add a marker, comment lines); Insert; Make (waveform →
  stimulus, add a channel, sweep a selected value to make a protocol);
  Examples; Help (all `sg help` topics).
* **Axes**: time and value ranges are automatic or fixed — type the limits,
  drag on a plot to zoom time (Shift+drag for values), double-click to go
  back to automatic. Fixed limits are kept while the text changes.
* **Protocols**: pick any trial from the drop-down; show it alone, or
  **overlay** or **stack** the next N trials (coloured by condition, the
  selected one in bold, one value scale per channel).
* **Animate**: ▶ Play moves a cursor through the stimulus at a chosen speed
  (¼× to 20×), waits the inter-trial interval (taken from the protocol's
  `period` or `gap`, editable), then goes on to the next trial; optionally
  in a loop.
* **Seeds, as in the specification**: the seed field is empty by default,
  so noise without a seed is a new realisation at every rendering (Plot ⟳
  draws again), and seeds written in the text are respected; the status line
  says whether the seed shown was drawn, given or stated in the text. Type a
  seed (like `sg -s`) or press **keep** to freeze the realisation shown.
* **Examples**: 43 complete examples in the Examples menu, from a first step
  to ZAP chirps, synaptic barrages, frozen-noise reliability, dynamic-clamp
  conductances, paired-pulse and I–V protocols (`web/examples.js`; every one
  is checked by the native `sg` in `tests/test_web.mjs`).
* **Live or on demand**: with *live* on, the plot follows every keystroke;
  with it off, the plot is marked out of date until **Plot ⟳**
  (⌘/Ctrl+Enter) — handy for long stimuli and large protocols.
* **Layout**: drag the divider between text and plots (double-click resets
  it); the plots fill the available height. Markers, a readout under the
  mouse, and dropped files for `use` and `file()` complete it. Nothing is uploaded: the renderer is `sg` itself, compiled to
WebAssembly (`sg.wasm`, about 170 KB) and run in a background worker.

```
cd web && python3 -m http.server 8000      # then open http://localhost:8000
```

(Browsers load WebAssembly only from a web server, not from `file://`; the
folder can also be published as is, e.g. with GitHub Pages.) `web/sglib.mjs`
is a small JavaScript interface to the same functions, usable in other pages
or in Node.

**One code base.** The command line and the web app share the library:
`render.c` (one stimulus from text to samples), `sgb.c`, `protocol.c` and the
rest; `main.c` only reads the command line, and `sg_api.c` is the interface
used by WebAssembly (render, check, canonical form, protocol expansion, help,
and a JSON table of the generators). Errors inside the library return to the
caller with the same message the command line prints.

**Same results.** The web app produces the same `.sgb` files as `sg`: same
header, same seeds, same canonical digest. Samples are bit-identical except
where an elementary function (`sin`, `exp`, `log`, `pow`) of Emscripten's math
library differs in its last bit from the native one; the differences stay
within reproducibility Level B (largest observed: 6·10⁻¹⁵ of the amplitude,
in a long exponential chirp).

## What `sg` implements

The whole specification (Chapters 6–15): the 16 primitives, the operators
and maps, blocks, `repeat`, `prev`, labels and markers, units, multichannel
stimuli with `use`, `copy`, digital channels and explicit durations,
protocols (sweeps, `let`, repetitions, the four orders, the three noise
policies, timing), the directory form with `expand`, the canonical form, and
the `.sgb` output with its provenance record.

`sg` renders stimuli; it does not drive acquisition hardware. Timing
statements (`period`, `gap`, `start on trigger`) are recorded in every
trial's provenance and in `protocol.sgi` for the program that plays them.

## Implementation choices

This section records every choice made in `sg`, why it was made, and where
it is in the code. The section numbers refer to the specification.

### Language and portability

* **ISO C99, no external libraries** (only the C standard library and
  `libm`). C99 rather than C89 because exact 64-bit integers
  (`uint64_t`) are needed by the random number generator and by the exact
  time arithmetic. Compiled with `-std=c99 -pedantic -Wall -Wextra` without
  warnings.
* **`-ffp-contract=off`**: forbids the compiler from fusing `a*b + c` into a
  single fused multiply-add, which would change the last bits of results
  depending on the compiler and the CPU.
* **`sin_cos()`** (`rng.c`): where sine and cosine of the same angle are
  needed (Box–Muller, FFT), they are evaluated as two separate calls through
  a `volatile`. Otherwise optimising compilers merge them into one
  `sincos()` call, and the samples would differ in the last bit between
  `-O0` and `-O2`. The test suite checks that both builds write identical
  bytes.
* **Errors are fatal**: any error prints `sg: error: ...` with the line
  number and exits with status 1; nothing is written. A stimulus is never
  altered silently (spec Rule 4). Warnings are printed and also recorded in
  the provenance record.

### Exact time (spec Sec. 8.2, Rule 3)

* **Durations are integers in picoseconds** (`parse_decimal_ps`, `parse.c`).
  Decimal literals are converted exactly; a duration that is not a whole
  number of picoseconds is rejected. The longest waveform is about 106 days.
* **The sampling rate is an exact fraction** p/q with q a power of ten
  (`parse_rate`): `44.1kHz` is 44100/1, `12.5Hz` is 125/10.
* **Boundaries** n = R(t·fs) are computed in 128-bit integer arithmetic
  (`boundary`, `eval.c`), with rounding to nearest and ties to even. No
  floating point is involved, so boundaries never drift and never depend on
  binary rounding (the tests compare 25 random sequences with Python's exact
  `Fraction`).
* **Grid-aligned local time** u = k/fs inside every segment, and the
  realised duration N/fs for duration-aware generators (spec Sec. 8.4).
* **Nested blocks** use absolute nominal times for their boundaries, so a
  combination of a generator with a block always covers the same samples.

### Random numbers (spec Sec. 11)

* **Philox4x64-10** (Salmon et al. 2011), implemented from scratch in
  `rng.c`, with a portable 64×64→128-bit multiply (no `__int128`). It is a
  counter-based generator: block j of a stream is computed directly from
  (key, j). Every noise segment therefore owns its stream, and editing one
  segment never changes the noise of another. Classic sequential generators
  (such as the "Ran" generator of *Numerical Recipes*, 3rd ed., used by the
  earlier implementation) are excellent, but they cannot give this
  locality.
* **SHA-256** (FIPS 180-4), also implemented from scratch, derives the
  128-bit key of each stream from the text `"<master seed>:<address>"`, or
  `"fixed:<n>"` for a generator with `seed=n`. Bytes 0–7 and 8–15 of the
  digest, read big-endian, are the two key words. The counter of block j is
  (j, 0, 0, 0).
* **Addresses** such as `ch=out/s1/o1` name the position of a generator in
  the description (channel, segment, operand, repeat copy). Two channels
  that `use` the same file therefore get different noise; `copy` gives
  identical samples.
* **Variates**: uniform U = (w >> 11)·2⁻⁵³ (53 random bits, exactly
  representable); Gaussian by Box–Muller on two consecutive uniforms
  (cosine branch first, sine branch cached); exponential −ln(1−U). These
  formulas are part of the specification, so another implementation can
  reproduce the samples.
* **Verification**: `sg selftest` checks SHA-256 against the FIPS test
  vectors and Philox against the Random123 known-answer vectors. The tests
  regenerate the noise samples independently with Python's `hashlib` and
  numpy's own Philox.
* **Seeds are kept as exact text** (a double cannot hold every 64-bit
  integer), and the master seed is written to JSON as a string for the same
  reason.

### Generators (spec Sec. 9)

* **Ornstein–Uhlenbeck**: exact update
  x ← μ + (x−μ)ρ + σ√(1−ρ²) ξ with ρ = e^(−Δt/τ), exact for any Δt (the
  tests check σ and the absence of correlation at Δt = 20τ). Default
  `init=stationary`: the first sample is drawn from the stationary law, so
  the realisation is stationary from its start. One Gaussian is always drawn
  for the first sample, so sample k always uses variate k.
* **Pulse trains** are a superposition of kernels at onset times. With
  `align=grid` (default) onsets are rounded to the nearest sample and widths
  to whole samples, so every pulse of a train is identical. The **width of a
  biphasic pulse is the width of each phase** (the total pulse lasts
  2·width). The first regular pulse is at `delay` (0 by default). Poisson
  onsets use intervals `dead` + Exp(mean 1/rate − dead), so the mean rate is
  exactly `rate`.
* **Kernel tails**: exponential, alpha and biexponential kernels are
  summed up to 47–52 time constants after each onset, where they are below
  10⁻²⁰ of their peak, far below double precision. This keeps long trains
  fast without any visible effect.
* **Power-law noise (`cnoise`)** follows the Timmer–König procedure of spec
  App. B.6. The inverse DFT of arbitrary length uses Bluestein's algorithm
  on a radix-2 FFT (`prims.c`), so no FFT library is needed.
* **`file()`** reads one column of a text file (`#` comments, blank lines,
  spaces/tabs/commas). Its SHA-256 is recorded in the provenance. With
  `extend=error`, a file exactly as long as its segment is accepted (the end
  value holds the last sample).
* **End values and `prev`**: deterministic generators continue to u = T
  (a ramp ends exactly at `to`); stochastic ones end at their last sample;
  a noise segment with no sample passes `prev` through. `prev` is allowed as
  a whole parameter value (`dc(prev)`) or as a term of an expression
  (`10ms prev + 1`), as the grammar specifies.

### Protocols (spec Sec. 13, `protocol.c`)

* **Raw template, textual substitution.** The stimulus template is kept as
  text; `$name` and `$( expression )` are replaced before it is parsed, so
  an expanded trial is an ordinary `.sg` file. Comments and strings are not
  substituted.
* **Values carry units** through `let` and `$( ... )` (dimensions are
  tracked as exponents of s, A, V, S). A listed value keeps its written
  form; a range is written in the unit of its first value; a computed value
  in engineering notation (`400pA`). A derived dimension such as a charge
  can be used in `let` but not substituted.
* **Exact ranges**: `from A to B step S` is computed with decimal integers,
  so `from -0.3nA to 0.1nA step 0.1nA` gives exactly `-0.3nA … 0nA, 0.1nA`.
* **Order**: Fisher–Yates on the stream `"order:<protocol seed>"`, with
  index ⌊U·(i+1)⌋; `shuffled-blocks` permutes each block of conditions in
  turn. **Trial seeds**: first 8 bytes (big-endian) of SHA-256 of
  `"trial:s:j"`, `"condition:s:c"` or `"fixed:s"`, depending on `noise`.
* **An inline template may be a plain waveform**: if it does not start with
  a stimulus statement (`channel`, `rate`, ...), it is a waveform.
* **Directory form**: `expand` writes `0000.sg`, `0001.sg`, ... (more
  digits beyond 10000 trials), `protocol.sgi` with timing, seeds and, as
  comments, condition and repetition, and copies the files that the trials
  name (`use`, `file(...)`), so the directory is self-contained. A
  directory without index is played in lexical order of its `.sg` files.
  Reading directories uses POSIX `dirent.h`/`sys/stat.h`, the only
  non-C99 headers.
* **Provenance of a trial**: `description` is the substituted trial text,
  so each trial file can be regenerated on its own; `trial` holds indices,
  variables, protocol seed, order, noise policy and timing; `protocol` holds
  the protocol text.

### Units (spec Sec. 6.3, 12.2)

* Segment durations must carry a time unit; other parameters default to
  their canonical unit (s, Hz, rad).
* An amplitude written with a unit (`0.3nA`) is converted to the channel
  unit directly between decimal units, with one correctly rounded
  operation: `-250pA` on a pA channel is exactly −250, `0.3nA` exactly 300.
  The wrong kind of unit is an error. Time and frequency literals are
  scaled the same way (`5ms` is the double nearest to 0.005). A plain waveform file has one
  channel `out`, whose unit is given by `-u` (default: no unit).

### Output (spec Sec. 15)

* **`.sgb`**: `SGB` + 5 zero bytes, a little-endian 64-bit header length,
  a UTF-8 JSON header padded with spaces so that the samples start on an
  8-byte boundary (they can be memory-mapped directly), then IEEE 754
  doubles, little-endian, channel after channel. Every byte is written explicitly, so files are identical on all
  machines.
* **Provenance**: the verbatim description (with every `use`d file), the
  SHA-256 of its canonical form, data files and their digests, rate, master
  seed, SHA-256 of the sample bytes, warnings, and a UTC time stamp. The
  tests regenerate a file from its provenance record alone.
* **Reproducibility level B** is declared: results are bit-identical for a
  given platform and C library; across platforms, the last bits of `sin`,
  `exp` and `log` may differ.
* **No quantisation**: samples are written in channel units. Conversion to
  D/A codes (spec Sec. 8.9) belongs to the acquisition software that knows
  the hardware gains, so the `clipping` field is `null`.

## Testing

| Suite | What it checks |
|:--|:--|
| `sg selftest` | SHA-256 (3 FIPS vectors), Philox4x64-10 (3 Random123 vectors), exact boundaries and tie rounding |
| `tests/run_tests.py` | 45 checks: every generator against closed forms, noise streams against an independent Python implementation, frozen noise, locality, repeat, stimuli, `.sgb` format, errors |
| `tests/test_thorough.py` | 120 checks: randomised exact boundaries, canonical round trip on 138 random waveforms (the valid ones among 150; identical samples), all options, `prev` semantics, units, stimulus rules, regeneration from provenance, noise statistics, `-O0`/`-O2` identity, 24 error cases, and protocols: the four orders and trial seeds against an independent implementation, substitution, sweeps, noise policies, protocol = directory form (byte-identical), 10 protocol errors |
| `tests/test_examples.py` | runs the 40 commands printed by `sg examples`, exactly as printed |
| `tests/compare_cli.py` | compares two builds of `sg` on 383 command lines (exit codes, stdout, stderr, every file written); used to prove that refactorings leave the command line unchanged |
| `tests/test_api.py` | the library interface against the command line: identical `.sgb` files, errors, canonical forms, expansions and help (native, byte for byte); with `--level-b`, the WebAssembly build |
| `tests/test_web.mjs` | the planner in headless Chrome: every example renders, errors mark their line, help inserts templates, browser samples equal native `sg` |
| `tests/test_convert.py` | every `sgconvert` format read back and compared with the `.sgb` samples (HDF5 if h5py is installed) |
| `tests/test_skill.py` | the agent skill: derived files up to date, every example and code block accepted by `sg`, the scripts on a stimulus and a protocol |
| `tests/test_access.py` | extracts and runs every C, Python and Julia program of `docs/access/sgb-access.md`, and checks that all languages read the same values |
| `docs/figures/make_figures.py` | renders every example figure of the document with `sg` |

## License

StimGen 2 is released under the MIT License (see `LICENSE`, or run
`sg license`). Copyright (c) 2026 Michele Giugliano. Every source file
carries the license notice.

## Credits

StimGen 2 and `sg` are written by Michele Giugliano. The idea of describing
stimuli by short parametric texts goes back to his work with Maura Arsiero
(Bern, 2001–2005) and to the LCG suite of Daniele Linaro, João Couto and
Michele Giugliano (2014). StimGen 2 is a new design. `sg help` and
`sg version` print these credits.
