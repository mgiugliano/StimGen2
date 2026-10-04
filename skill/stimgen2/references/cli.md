# `sg` command line

`sg` is a single C99 program with no dependencies (`make -C src` in a
StimGen2 checkout). Every command prints errors on stderr as
`sg: error: <where>: <what>` and exits with status 1. Success is status 0.

## Commands

| Command | Does |
|:--|:--|
| `sg render [opts] FILE.sg` | waveform or stimulus → `FILE.sgb` (or `-o NAME`) |
| `sg render [opts] -e 'TEXT'` | the description is TEXT: one line, segments separated by `;`; writes `stimulus.sgb` unless `-o NAME` |
| `sg render [opts] PROTOCOL.sg` | one `.sgb` per trial in `PROTOCOL_sgb/0000.sgb, 0001.sgb, ...` (or `-o DIR`) |
| `sg render [opts] DIR/` | renders a directory written by `sg expand` (reads `DIR/protocol.sgi`) |
| `sg expand [opts] PROTOCOL.sg DIR` | writes the trials as plain `.sg` files `DIR/0000.sg ...` and the index `DIR/protocol.sgi` (seeds, timing) |
| `sg check [opts] FILE` | parse and validate, write nothing; prints e.g. `ok, 1 channel(s), 40000 samples at 20000 Hz (2 s)` or `ok, 33 trial(s), protocol seed 2026` |
| `sg canon FILE` | prints the canonical form (every parameter named, defaults explicit) |
| `sg help [TOPIC]` | topics: `syntax generators maps units stimulus noise protocol output`, or a generator or function name |
| `sg examples` | about 40 ready-to-run commands |
| `sg selftest` | known-answer tests of SHA-256, Philox, rounding |
| `sg version`, `sg license` | version, author, credits; the MIT License |

## Options

| Option | Meaning |
|:--|:--|
| `-r, --rate RATE` | sampling rate: `20kHz`, `10000` (Hz). Required unless the stimulus states `rate`; if both, they must agree. |
| `-s, --seed N` | master seed (integer). Overrides the stimulus `seed`. For a protocol: the protocol seed, from which the order and every trial's seed derive. Without any seed, one is drawn from the OS and recorded in the output. |
| `-o, --output NAME` | output file; for a protocol or directory, the output directory. `-` with `-t` = stdout. |
| `-t, --text` | write a text table (`# t[s]  name[unit] ...`, one row per sample) instead of `.sgb`; default name `FILE.txt` |
| `-u, --unit UNIT` | unit of the single channel (`out`) of a plain waveform: `pA`, `mV`, `nS`, ... Default `1` (dimensionless). |
| `-e, --expr TEXT` | description from TEXT instead of a file |
| `-q, --quiet` | no warnings on stderr (still recorded in the file) |

## Recipes

```
sg check -r 20kHz stim.sg && sg render -r 20kHz -s 1 stim.sg
sg render -r 20kHz -u pA -e '500ms dc(0); 1s dc(300); 500ms dc(0)' -o step.sgb
sg render -q -r 1kHz -t -o - -e '10ms sine(1, 100Hz)'           # samples on stdout
sg render -r 20kHz -s 7 fi.sg            # fi_sgb/0000.sgb ... (seed 7 => same order, same noise)
sg expand -s 7 fi.sg fi/ && sg render -r 20kHz fi/   # identical samples via the directory form
for a in 100 200 300; do sg render -q -r 20kHz -u pA -e "1s dc($a)" -o step_$a.sgb; done
```

## From Python

`scripts/sg_run.py` wraps all of this:

```python
import sys; sys.path.insert(0, "path/to/skill/stimgen2/scripts")
from sg_run import check, render, load
ok, msg = check(text=open("fi.sg").read(), rate="20kHz")
files = render(file="fi.sg", rate="20kHz", seed=7)          # ['fi_sgb/0000.sgb', ...]
h, x = load(files[0])                                         # header, samples [channel, k]
```

Or as a command with JSON output: `python3 sg_run.py check FILE`,
`python3 sg_run.py render -r 20kHz -s 1 FILE [-o OUT]`, and
`python3 sg_run.py which`, which reports where `sg` was found (`$SG_BIN`, the
PATH, or `src/sg` of a checkout, built if needed).

## After rendering

| Goal | Command |
|:--|:--|
| describe in words | `python3 scripts/sgb_describe.py FILE.sgb` |
| summarise a protocol | `python3 scripts/sgb_describe.py FILE_sgb/` |
| JSON summary | `python3 scripts/sgb_describe.py FILE.sgb --json` |
| plot | `python3 scripts/sgb_plot.py FILE.sgb [-o x.png] [-t 0.5:1.5] [-c Iinj]` |
| plot a protocol | `python3 scripts/sgb_plot.py FILE_sgb/ [--stack] [-n 8]` |
| convert | `python3 scripts/sgconvert.py {csv,npz,mat,h5,atf,neuron,brian,json} FILE.sgb [-o OUT]` |

The converters: `csv` (time + one column per channel), `npz` (`t`, `x`,
header), `mat` (needs scipy), `h5` (needs h5py), `atf` (Axon Text File for
pClamp), `neuron` (one two-column file per channel, time in ms, value in nA,
mV or µS, for `Vector.play`), `brian` (`values`, `dt` for
`brian2.TimedArray`), `json` (the header).
