# The StimGen 2 language

Three layers: a **waveform** (one channel), a **stimulus** (several channels
on one clock), and a **protocol** (many trials). Each layer adds one idea.
The exact parameters of every generator are in `sg-help.md` (or `sg help NAME`).
Every example below passes `sg check -r 20kHz`.

## 1. Waveforms

One segment per line (or separated by `;`): a **duration with a time unit**,
then an expression. `#` starts a comment.

```
500ms dc(0)
1s    dc(300)                                  # a step
2s    ramp(to=0)                               # from the previous end value
5s    sine(50, 8Hz) + ou(mean=0, sd=20, tau=5ms)
```

Parameters go by position or by name: `sine(3, 1Hz)` = `sine(amp=3, freq=1Hz)`.
Keyword values such as `law=exp` and `shape=biexp` are bare words.

### Generators

| Generator | Positional parameters (then named options) | Notes |
|:--|:--|:--|
| `dc` | `level` | constant |
| `ramp` | `from=prev, to` | linear over the segment |
| `relax` | `from=prev, to, tau` | exponential relaxation to `to` |
| `sine` | `amp, freq` (`phase, offset, clock=local\|global`) | `clock=global` keeps phase across segments |
| `square` | `amp, freq` (`duty=0.5, phase, offset, clock`) | alternates offset ± amp |
| `saw` | `amp, freq` (`duty=1, phase, offset, clock`) | `duty=0.5` is a triangle |
| `triangle` | `amp, freq` (`phase, offset, clock`) | |
| `chirp` | `amp, f0, f1` (`law=linear\|exp, phase, offset`) | ZAP; sweeps over the segment |
| `biexp` | `amp, tau_rise, tau_decay` (`delay, offset`) | peak-normalised, synaptic-like |
| `alpha` | `amp, tau` (`delay, offset`) | peaks at `tau` after `delay` |
| `pulses` | `amp, rate` (`shape=square\|biphasic\|exp\|alpha\|biexp, width, tau, tau_rise, tau_decay, timing=regular\|poisson, times=[..], delay, dead, align, offset, seed`) | trains, regular or Poisson |
| `ou` | `mean, sd, tau` (`init, seed`) | Ornstein–Uhlenbeck, exact update |
| `wnoise` | `mean, sd` (`seed`) | Gaussian white |
| `unoise` | `mean, sd` (`seed`) | uniform white |
| `cnoise` | `mean, sd` (`alpha=1, fmin, fmax, seed`) | 1/f^alpha noise (0 white, 1 pink, 2 brown) |
| `file` | `path, rate` (`column, interp=hold\|linear, extend=error\|hold\|zero\|cycle`) | samples from a text file |

Time inside a segment restarts at 0 (`u`), unless `clock=global`.

### Operators and functions

`+ - * /` with the usual precedence, unary `-`, and parentheses. Functions:
`abs(e)`, `pos(e)` (= max(e, 0)), `sqrt`, `exp`, `log`, `pow(e, k)`,
`spow(e, k)` (sign-preserving power), `clip(e, lo, hi)`, `min(a, b)`, `max(a, b)`.
Results that are not finite stop `sg` with an error that names the line and
the sample.

```
5s  (1 + 0.5*sine(1, 2Hz)) * sine(80, 40Hz)      # amplitude modulation
10s pos(ou(mean=10, sd=5, tau=3ms))               # a conductance stays >= 0
5s  clip(sine(200, 1Hz), -100, 100)
```

### Blocks, repeat, prev, markers

```
{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(80, 8Hz)    # an envelope
repeat 10 { 2ms dc(1000) ; 48ms dc(0) }                         # 10 pulses at 20 Hz
1s dc(-50)
@probe                                                          # marker before the next segment
2s ramp(from=prev, to=200)
```

A block `{...}` is a sequence used as a value. Its duration is the sum of its
segments, and a generator combined with it takes that duration. Two blocks
combined must have the same duration. `prev` is the end value of the
previous segment and is the default `from` of `ramp` and `relax`.
`@name` creates the marker `channel.name` at the start of the next segment.

### Units

| Kind | Units | Canonical |
|:--|:--|:--|
| time | `s ms us ns min` | s |
| frequency | `Hz kHz MHz` | Hz |
| phase | `rad deg` | rad |
| fraction | `%` | 50% = 0.5 |
| current | `fA pA nA uA mA A` | channel unit |
| voltage | `uV mV V` | channel unit |
| conductance | `pS nS uS mS S` | channel unit |

A number is written with its unit attached (`500ms`, `20kHz`). Without a unit,
a number is in the canonical unit of its parameter, and an amplitude is in
the channel's unit. With a unit, it is converted: `dc(0.3nA)` on a pA channel
is 300. The wrong kind of unit is an error. Units are case-sensitive.

## 2. Stimuli (several channels)

```
sg 2 stimulus
rate 20kHz                                 # optional (else -r); must agree with -r
seed 4711                                  # optional master seed (-s overrides)

channel Iinj unit=pA rest=0 {
    1s    dc(0)
    @train
    500ms pulses(amp=2000, rate=20Hz, width=1ms)
    1s    dc(0)
}
channel G    unit=nS { 2.5s pos(ou(10, 3, 5ms)) }
channel twin unit=pA copy Iinj             # identical samples, noise included
digital cam  { 1s dc(0); 1.5s pulses(1, 100Hz, width=1ms) }
marker flash at 1s, 1.5s
duration 3s                                # optional; default = longest channel
```

Channels shorter than the stimulus hold their `rest` value. Digital channels
must contain only 0 and 1. `channel X unit=pA use "noise.sg"` takes a waveform
from a file. A file without a header is a plain waveform. It is a stimulus
with one channel named `out` (unit from `-u`).

## 3. Protocols (many trials)

```
sg 2 protocol
sweep  amp = from -300pA to 300pA step 50pA          # or [a, b, c], or linspace(a, b, n)
sweep  (amp2, dur) = [(100pA, 1s), (200pA, 500ms)]   # values that go together
let    q = amp2 * dur                                # derived variable
repeat 3
order  shuffled-blocks       # sequential | grouped | shuffled | shuffled-blocks
period 5s                    # onset to onset; or: gap 2s (pause between trials)
noise  per-trial             # per-trial | per-condition (frozen) | fixed
seed   2026                  # protocol seed; -s overrides
start  immediately           # or: on trigger

stimulus {                   # or: stimulus "file.sg"
    channel Iinj unit=pA {
        500ms dc(0)
        $dur  dc($amp + $amp2)
        500ms dc(0)
    }
}
```

- Several `sweep` lines form a Cartesian product, and the first varies slowest.
  A tuple sweep varies its variables together.
- `$name` and `$( expression )` are replaced **textually** before the
  stimulus is parsed, so `$dur` can be a duration.
- Orders: `sequential` puts the conditions in order and repeats the whole
  list. `grouped` repeats each condition back to back. `shuffled` is one
  random permutation of all trials. `shuffled-blocks` shuffles each repetition
  block separately. Random orders are reproducible from the protocol seed.
- Noise: `per-trial` gives every trial its own master seed. `per-condition`
  gives every repetition of a condition the same seed (frozen noise). `fixed`
  gives one seed to all trials.
- Number of trials = (product of sweep sizes) × repeat. `sg check` prints it.
- `sg render` writes `NAME_sgb/0000.sgb ...`. `sg expand` writes the plain
  `.sg` trials plus `protocol.sgi`, and `sg render DIR/` gives identical
  samples.

## 4. Noise and seeds

Each stochastic generator (`ou wnoise unoise cnoise`, Poisson `pulses`) has
its own random stream, keyed by the master seed and by its address in the
text. Consequences:

- Same text + rate + seed → same samples on every machine.
- Editing one segment does not change the noise of the others. Inserting a
  segment *before* a noise does change it, because the address shifts.
- `seed=n` inside a generator freezes that realisation wherever it appears,
  independently of the master seed.
- Without `-s` or `seed`, a seed is drawn from the OS and written to the output.

## 5. Common requests → descriptions

| Request | Description |
|:--|:--|
| current steps for an f–I curve | protocol: `sweep amp = from 0pA to 500pA step 50pA`, segment `2s dc($amp)` |
| hyperpolarising steps for input resistance / sag | `sweep amp = from -200pA to 0pA step 50pA`, `1s dc($amp)` |
| ramp to find rheobase | `500ms dc(0); 2s ramp(0, 400); 500ms dc(0)` |
| ZAP / impedance | `20s chirp(amp=30, f0=0.5Hz, f1=20Hz, law=exp)`, optionally times an envelope block |
| frozen noise reliability | protocol: `repeat 10`, `noise per-condition`, segment `2s ou(mean, sd, tau)` |
| synaptic-like conductance | stimulus `channel g unit=nS { 10s pos(ou(10, 3, 5ms)) }` |
| EPSC train | `pulses(amp=-50, rate=20Hz, shape=biexp, tau_rise=0.5ms, tau_decay=5ms)` |
| Poisson input | `pulses(amp=100, rate=10Hz, width=1ms, timing=poisson, dead=2ms)` (square pulses need `width`) |
| paired pulses at several intervals | `sweep isi = [20ms, 50ms, 100ms]`, `2ms dc(1000); $isi dc(0); 2ms dc(1000)` (`$isi` is the gap between the pulses) |
| TTL to a camera or LED | stimulus `digital cam { ... pulses(1, 30Hz, width=2ms) }` |
| a recorded waveform | `10s file("trace.txt", rate=10kHz, interp=linear)` |

## 6. Grammar summary

```
file      := [header] (waveform | stimulus-body | protocol-body)
header    := "sg 2 waveform" | "sg 2 stimulus" | "sg 2 protocol"
waveform  := segment { (newline | ";") segment }
segment   := ["@" name] duration expr | "repeat" N "{" waveform "}"
expr      := term { ("+" | "-") term }
term      := factor { ("*" | "/") factor }
factor    := number[unit] | "prev" | "-" factor | "(" expr ")" | "{" waveform "}"
           | name "(" [args] ")"
args      := arg { "," arg } ;  arg := expr | name "=" (expr | word | "[" list "]")
```

The normative grammar is in the specification (`docs/build/stimgen2-spec.pdf`,
chapter "Concrete syntax"). `sg canon FILE` shows how `sg` reads a file.
