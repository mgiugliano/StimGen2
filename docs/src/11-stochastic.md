# Stochastic semantics and reproducibility {#sec:stochastic}

Noise stimuli are central to cellular electrophysiology. They are used to
probe input–output transfer, spike-time reliability, and synaptic
integration, and many of these experiments rely on presenting the *same*
noise realisation more than once ("frozen noise"). This chapter defines
exactly which realisation a description produces, so that any realisation
can be regenerated later from the description and a seed.

## Process and realisation

A description that contains stochastic generators denotes a *random*
waveform, i.e. a probability law over waveforms (@sec:semantics). The
statistics of this law (mean, variance, autocorrelation, spectrum) are set by
the parameters of the generators and are the same in every conforming
implementation [@CoxMiller1965; @Papoulis2002].

A *realisation* is one waveform drawn from this law. In StimGen 2 a
realisation is a deterministic function of three things:

1. the description,
2. the sampling rate $f_s$,
3. a *master seed* $s$, a non-negative integer.

Two conforming implementations given the same three inputs MUST produce the
same samples, within the reproducibility level of @sec:repro-levels.

## Random streams

Every occurrence of a stochastic generator in a waveform is called an
*instance*. Each instance draws its random numbers from its own *stream*, an
infinite sequence of 64-bit words $w_0, w_1, w_2, \dots$ The stream depends
only on a 128-bit *key*.

**Rule 5 (generator).** The stream for key $\kappa$ is the output of the
counter-based generator Philox4x64-10 [@Salmon2011] with key $\kappa$.
Block $j = 0, 1, 2, \dots$ of the stream is the output for the 256-bit counter
whose four 64-bit words are $(j, 0, 0, 0)$, and its four output words are
used in order.

A counter-based generator computes the $j$-th block of words directly from
the key and the counter $j$, without stepping through the previous ones.
Streams with different keys are statistically independent for all practical
purposes, and a stream can be read starting at any position.

## Keys and addresses {#sec:keys}

### Addresses

Every instance has an *address* that identifies its position in the
description. The address is a text string built from the path that leads
from the root of the stimulus to the instance:

- the name of the channel, e.g. `ch=Iinj`;
- for each enclosing sequence, the index of the segment, starting from 0,
  e.g. `s3`;
- for each enclosing `repeat`, the index of the copy, e.g. `r2`;
- for each enclosing operator or function, the position of the operand,
  starting from 0, e.g. `o1`.

The components are joined by `/`. For example, the noise in

```
1s  dc(0)
5s  sine(1, 1Hz) + ou(0, 0.2, 5ms)
```

on channel `Iinj` has address `ch=Iinj/s1/o1`. Addresses are taken from the
description as written, never from a rewritten form.

### Keys

**Rule 6 (key derivation).** The key of an instance is formed from the
first 16 bytes of the SHA-256 digest of a text string (encoded in UTF-8):
bytes 0–7 and bytes 8–15, each read as a big-endian unsigned integer, give
the two 64-bit key words $\kappa_0$ and $\kappa_1$. The string is:

- if the instance has an explicit parameter `seed=n`, the string is
  `fixed:n`;
- otherwise, the string is `s:a`, where $s$ is the master seed written in
  decimal and $a$ is the address.

Two consequences matter in practice.

- **Locality.** The noise of a segment does not depend on what other
  segments contain. Changing the amplitude of a step in segment 0 does not
  change the noise realisation in segment 1. (Inserting or removing a
  segment *before* it does change it, because the addresses shift. Use an
  explicit `seed` to make a realisation independent of its position.)
- **Frozen noise.** An explicit `seed` gives the same realisation
  wherever it appears, independently of the master seed and of the address.
  Two segments with the same primitive, parameters, duration, and seed
  produce identical samples.

```
200ms ou(2, 0.5, 100ms, seed=21)    # realisation A
50ms  dc(0)
200ms ou(2, 0.5, 100ms, seed=21)    # realisation A again
50ms  dc(0)
200ms ou(2, 0.5, 100ms)             # a different realisation
```

### The master seed

The master seed is set by the stimulus or protocol layer, or on the command
line. If none is given, the implementation MUST draw one from a source of
entropy of the operating system, and it MUST record the seed it used with
the output (@sec:output). Every realisation that was ever played can
therefore be regenerated exactly.

## From words to variates {#sec:variates}

Generators consume words from their stream in the order defined below. These
transformations are part of the specification, because two implementations
that transform the same words differently would produce different samples.

- **Uniform** variate on $[0, 1)$: $U = \lfloor w / 2^{11} \rfloor \cdot 2^{-53}$,
  i.e. the top 53 bits of one word.
- **Standard Gaussian** variates are produced in pairs by the Box–Muller
  transform of two consecutive uniforms $U_1, U_2$:
  $\xi = \sqrt{-2 \ln(1 - U_1)}\, \cos(2\pi U_2)$ and
  $\xi' = \sqrt{-2 \ln(1 - U_1)}\, \sin(2\pi U_2)$, used in this order.
- **Exponential** variate with unit mean: $E = -\ln(1 - U)$.

The consumption order for each stochastic primitive is:

| Primitive | Variates consumed, in order |
|:--|:--|
| `ou` | $\xi_0, \xi_1, \dots, \xi_{N_i - 1}$, one per sample; $\xi_0$ is drawn even when `init` is not `stationary` |
| `wnoise` | $\xi_0, \dots, \xi_{N_i - 1}$ |
| `unoise` | $U_0, \dots, U_{N_i - 1}$ |
| `pulses` (Poisson) | $E_1, E_2, \dots$ for the successive intervals, until an onset exceeds the segment |
| `cnoise` | for $j = 1, \dots, \lfloor N_i/2 \rfloor$: the real then the imaginary part of the coefficient at $f_j$ |

: Consumption of random variates. {#tbl:consumption}

Drawing $\xi_0$ in every case keeps sample $k$ of an OU segment tied to
variate $\xi_k$, whichever initial condition is chosen.

## Reproducibility levels {#sec:repro-levels}

Bit-for-bit equality across implementations is achievable only for
operations that IEEE 754 rounds correctly. Elementary functions such as
$\sin$, $\exp$, and $\ln$ are not correctly rounded by most mathematical
libraries, and their last bit may differ between platforms. We therefore
define two levels.

- **Level A (bit-exact).** The samples are identical bit for bit. This level
  is required for the words of every stream and for the uniform variates
  derived from them. It extends to all samples when the implementation uses
  correctly rounded elementary functions.
- **Level B (numerically equivalent).** Every elementary function may
  return a result that differs from the correctly rounded one by a few units
  in the last place, and every sample differs from the Level A result only
  by the effect of these differences through the formula of its generator.
  For most generators this is a few units in the last place relative to the
  amplitude scale of the segment. Where the formula is ill-conditioned it
  can be more: a sinusoid or chirp whose phase argument has grown to tens of
  radians inherits a few units in the last place *of the phase*, which is
  tens of units in the last place of the output (about $10^{-14}$ of the
  amplitude after 10 s of a chirp to 40 Hz). Conforming implementations MUST
  reach at least Level B, and MUST state which level they reach.

Rounding differences do not accumulate in recursive generators: in the OU
update @eq:ou-exact, an error is multiplied by $\rho < 1$ at every step.

One exception to Level B is possible. A rounding difference can move a
Poisson onset across the midpoint between two samples, so that `align=grid`
places the pulse one sample earlier or later. Such events are extremely rare.
An implementation that claims Level A excludes them.

## Repetitions and trials

Within one waveform, every copy of a `repeat` has its own address and
therefore its own realisation (@sec:repeat).

Across trials, the protocol layer derives the master seed of each trial
from a protocol seed and the index of the trial (@sec:protocol). Repeated
trials thus present different realisations, unless the description fixes
the realisation with an explicit `seed` (frozen noise) or the protocol
reuses the same master seed for several trials. Both designs are common in
experiments, and both are stated explicitly in the description, never left to
chance.
