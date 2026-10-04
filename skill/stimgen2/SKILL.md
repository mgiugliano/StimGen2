---
name: stimgen2
description: >-
  Write, check, render, inspect and convert StimGen 2 descriptions of
  electrophysiology stimuli (.sg text files: waveforms, multichannel stimuli,
  protocols with sweeps/repetitions/shuffling/frozen noise) with the `sg`
  command-line renderer, and read the resulting .sgb sample files (describe
  in words, plot, convert to CSV/NPZ/MAT/HDF5/ATF/NEURON/Brian2). Use when a
  task mentions StimGen, sg, .sg or .sgb files, or asks to design current-,
  voltage- or conductance-clamp stimuli or stimulation protocols.
license: MIT
metadata:
  author: Michele Giugliano
  project: https://github.com/mgiugliano/StimGen2
  sg-version: "0.2"
---

# StimGen 2 (`sg`)

StimGen 2 describes an electrophysiology stimulus as a short text with an
exact meaning, independent of the sampling rate. `sg` turns the text into
samples (`.sgb` files) that a rig, a simulator, or an analysis script reads.
The same description, rate and seed give the same samples on every machine.

This skill is plain Markdown plus Python scripts. It assumes only a shell,
Python 3 with numpy (matplotlib for plots), and the `sg` binary (a single C99
program; `scripts/sg_run.py` finds it or builds it from a StimGen2 checkout).

## Files in this skill

| Path | Read it when |
|:--|:--|
| `references/language.md` | writing or editing any `.sg` text (syntax, generators, units, stimulus and protocol layers, pitfalls) |
| `references/cli.md` | running `sg` (commands, options, output names, exit codes) |
| `references/sgb-format.md` | reading `.sgb` files yourself, or explaining provenance and seeds |
| `references/sg-help.md` | you need the exact parameters of a generator: the verbatim built-in help of `sg`, the ground truth |
| `examples/*.sg` | you want a starting point; every file passes `sg check` |
| `scripts/sg_run.py` | to find `sg`, check or render from Python or with JSON output |
| `scripts/sgb_describe.py` | to describe `.sgb` files in words or JSON (no plotting needed) |
| `scripts/sgb_plot.py` | to plot `.sgb` files or a protocol's trials to a PNG/PDF |
| `scripts/sgconvert.py` | to convert `.sgb` to csv, npz, mat, h5, atf (pClamp), neuron, brian, json |

## Workflow

1. **Write** the description: start from the closest file in `examples/`
   and from `references/language.md`. Put units on every number that has
   one; durations MUST carry a time unit.
2. **Check** it, which writes nothing and reports the trial count of a protocol:
   `sg check -r 20kHz file.sg` or `python3 scripts/sg_run.py check file.sg`.
   Read the error (it names the line), fix, repeat.
3. **Render** with an explicit rate and seed, so that the result is reproducible:
   `sg render -r 20kHz -s 1 file.sg`. A waveform or stimulus gives `file.sgb`;
   a protocol gives `file_sgb/0000.sgb, 0001.sgb, ...`.
4. **Verify** the result before you report it. Describe it with
   `python3 scripts/sgb_describe.py file.sgb` (or a `file_sgb/` directory) and
   compare it with the intent: levels, durations, trial variables, channel
   units. Plot it with `python3 scripts/sgb_plot.py file.sgb` and look at the
   image if you can view images.
5. **Convert** if the next program needs it:
   `python3 scripts/sgconvert.py neuron file.sgb` (or `csv`, `mat`, `atf`, ...).

When the user gives no rate, use 20 kHz and say so. When no seed is given,
`sg` draws one from the OS and records it in the file. Pass `-s` whenever
reproducibility matters, which is almost always.

## The language in 30 seconds

```
500ms dc(0)                                 # waveform: one segment per line,
1s    dc(300) + ou(0, 20, 5ms)              #   duration + expression
repeat 5 { 2ms dc(1000) ; 98ms dc(0) }      # repeat a part
```

```
sg 2 stimulus                               # several channels on one clock
channel Iinj unit=pA { 1s dc(0); @on 2s sine(50, 8Hz); 1s dc(0) }
digital ttl { 1s dc(0); 10ms dc(1); 2.99s dc(0) }
```

```
sg 2 protocol                               # many trials
sweep  amp = from -100pA to 300pA step 50pA
repeat 3
order  shuffled-blocks                      # sequential | grouped | shuffled | shuffled-blocks
period 5s                                   # or: gap 2s
noise  per-trial                            # or per-condition (frozen) or fixed
stimulus { channel Iinj unit=pA { 500ms dc(0); 1s dc($amp); 500ms dc(0) } }
```

Generators: `dc ramp relax sine square saw triangle chirp biexp alpha file
pulses ou wnoise unoise cnoise`. Functions: `abs pos sqrt exp log pow spow
clip min max`, plus `+ - * /`. Run `sg help NAME` for any of them.

## Pitfalls (each has bitten someone)

- `-e TEXT` is one line: separate segments with `;`. Use a file for stimuli
  and protocols (or `sg_run.py`, which writes a temporary file for you).
- Durations without a unit are an error (`1 dc(0)`). Units are
  case-sensitive: `ms` is a millisecond, `mS` a millisiemens.
- A plain waveform has one channel named `out` with unit `1`. Give `-u pA`
  (or write `sg 2 stimulus` with `channel ... unit=pA`) when the unit matters,
  for example before converting to NEURON units.
- `-r` is required unless the stimulus states `rate`. If both are given,
  they must agree.
- Noise realisations depend on the generator's *position*: inserting a
  segment before a noise changes it. Use `seed=n` inside the generator to
  freeze a realisation wherever it appears.
- Operands that are blocks `{...}` must have equal durations. Non-finite
  values (division by zero, `sqrt` of a negative number) stop `sg` with an
  error; `sg` never clips silently. Use `pos()` or `clip()` explicitly.
- `$name` substitution in protocols is textual and happens before parsing.
  Use `$( expression )` for arithmetic and `let` for derived variables.
- `sg` renders trials; it does not play them in real time. Period, gap and
  trigger are recorded in each trial's provenance and in `protocol.sgi` for
  the program that plays them (in the author's lab this is a separate program,
  Gecko, which drives the National Instruments board).
- Do not hand-edit `.sgb` files. Change the `.sg` text and render again.

## Reporting back

Show the user the `.sg` text you wrote (it is short and it *is* the
stimulus), the command you ran with its rate and seed, the files produced,
and a one-paragraph description from `sgb_describe.py`. For protocols, give
the number of trials and the order of the variables.
