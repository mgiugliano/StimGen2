\part{Reference specification}

# Conventions {#sec:conventions}

This part of the document is normative. It defines what a StimGen 2
description means, independently of any particular implementation. Two
conforming implementations MUST produce the same sequence of samples from the
same description, the same sampling rate, and the same master seed, within the
reproducibility levels defined in @sec:stochastic.

## Requirement levels

The key words MUST, MUST NOT, SHOULD, SHOULD NOT, and MAY are used as defined
in RFC 2119 [@RFC2119]. In short, MUST marks an absolute requirement for
conformance, SHOULD marks a recommendation that may be ignored only for a
documented reason, and MAY marks a truly optional feature.

Text inside boxes labelled *Note* or *Design rationale* is
informative. It explains or motivates the normative text, but it adds no
requirement.

## Mathematical notation {#sec:notation}

We use the symbols in @tbl:notation throughout. Time is always a real number
in seconds, unless a unit is written explicitly.

| Symbol | Meaning |
|:-------|:--------|
| $t$ | global time, measured from the start of the waveform (s) |
| $u$ | local time, measured from the start of the current segment (s) |
| $T$, $T_i$ | duration of a segment, of the $i$-th segment (s) |
| $\tau_i$ | start time of segment $i$, $\tau_i = \sum_{j<i} T_j$ (s) |
| $f_s$, $\Delta t$ | sampling rate (Hz) and sampling interval $\Delta t = 1/f_s$ (s) |
| $k$ | global sample index, $k = 0, 1, \dots, N-1$ |
| $n_i$ | index of the first sample of segment $i$ |
| $x_k$ | value of the $k$-th sample of a realised waveform |
| $g$ | a generator, i.e. a function of local time and duration (@sec:model) |
| $\theta$ | the parameter vector of a generator |
| $\xi$ | a pseudo-random variate (Gaussian with zero mean and unit variance unless stated) |
| $[x]_+$ | positive part, $\max(x, 0)$ |
| $\operatorname{frac}(x)$ | fractional part, $x - \lfloor x \rfloor$ |
| $\mathbb{1}[\cdot]$ | indicator function: 1 if the condition holds, 0 otherwise |

: Notation used in the reference specification. {#tbl:notation}

Intervals are written with brackets and parentheses: $[a, b)$ contains $a$
but not $b$. All segment intervals are half-open in this way
(@sec:sampling).

## Units {#sec:units}

### Base units

Every parameter of a generator has a *dimension*, declared in its card in
@sec:primitives. The dimensions and their canonical units are:

| Dimension | Canonical unit | Accepted suffixes |
|:----------|:---------------|:------------------|
| time | s | `s`, `ms`, `us`, `ns`, `min` |
| frequency | Hz | `Hz`, `kHz`, `MHz` |
| phase (angle) | rad | `rad`, `deg` |
| fraction | 1 | none, or `%` (divides by 100) |
| count | 1 | none (integer) |
| number | 1 | none (dimensionless real) |
| amplitude | channel unit | none, or a physical unit (@tbl:unit-tokens) |

: Dimensions and unit suffixes. {#tbl:units}

Some parameters are not numbers: a *keyword* takes one of a listed set of
names (e.g. `law=exp`), a *string* is quoted text (e.g. a file path), and a
*list* is a bracketed, comma-separated sequence of values of one dimension
(e.g. `times=[10ms, 25ms, 70ms]`).

A numeric literal without a suffix is interpreted in the canonical unit of
the dimension of the field where it appears. Thus `tau=5` means 5 s, and a
user who means 5 ms MUST write `tau=5ms`. The one exception is the duration
of a segment, which MUST carry a time unit (@sec:syntax).

A suffix whose dimension does not match the field MUST be rejected. Writing
`freq=10ms` is an error, not a request for a period of 10 ms.

### Amplitudes {#sec:units-amplitude}

The waveform layer is dimensionless with respect to amplitude. A waveform
describes numbers, and the *stimulus* layer (@sec:stimulus) states what
those numbers are: picoamperes injected in current clamp, millivolts in
voltage clamp, nanosiemens of a dynamic-clamp conductance, or volts sent to
an LED driver. This keeps the same waveform reusable across contexts.

An amplitude literal MAY carry a physical unit (e.g. `amp=300pA`). In that
case the stimulus layer MUST check that the unit is compatible with the unit
declared for the channel, and convert it. A waveform with amplitude units
cannot be realised on a channel of a different dimension (for example, `pA`
on a channel declared in `mV`).

## Numbers

Numeric literals are decimal, with optional sign, fraction, and exponent
(e.g. `-2`, `0.5`, `1e-3`). Durations and boundaries are computed in exact
decimal arithmetic where @sec:sampling requires it. All other arithmetic is
IEEE 754 double precision.

## Files {#sec:files}

A StimGen 2 description is stored as a plain text file, encoded in UTF-8,
with the extension `.sg`. A `.sg` file holds one waveform, one stimulus, or
one protocol (@sec:layers). Its first non-comment line, the *header*, states
which, e.g. `sg 2 stimulus`. The header MAY be omitted in a file that holds a
waveform (@sec:syntax). The same text can also be given directly on the command line.

## Conformance

A *description* is conforming if it parses under the grammar of
@sec:syntax and passes the static checks of this part: known generators,
parameters within their domains, matching dimensions, and matching durations
in pointwise combinations (@sec:algebra).

An *implementation* is conforming if, for every conforming description, it
either produces the samples defined by this specification or reports an
error in the cases this specification declares erroneous. An implementation
MUST NOT silently replace an erroneous value (such as a non-finite sample)
with another one.
