# Algebra of waveforms {#sec:algebra}

This chapter defines how generators and timed waveforms are combined into
expressions: the operators, their precedence, the unary maps, the `repeat` construct, and the exact meaning of
`prev`. The abstract objects were introduced in @sec:model.

## Expressions {#sec:expressions}

An *expression* is built from the following operands:

- a primitive generator, e.g. `sine(amp=1, freq=5Hz)` (@sec:primitives);
- a numeric constant, e.g. `0.5` or `-2`, which is the constant generator;
- a *block*, i.e. a sequence of segments in braces, e.g.
  `{ 1s dc(0) ; 2s dc(1) }`, which is a timed waveform;
- a `repeat` construct (@sec:repeat), which is a timed waveform.

Operands are combined with the binary operators `+`, `-`, `*`, `/`, the
unary minus, and the functions of @tbl:maps. Round parentheses group
subexpressions.

A *segment* is an expression with a duration. It is written either as a
duration followed by an expression (`5s sine(1, 5Hz) + 0.5`), or as an
expression that already has a duration (a block, or a combination
containing a block). If both are given, they MUST be equal.

### Precedence

| Level | Operators | Associativity |
|:--:|:--|:--|
| 1 (highest) | function application: primitives and maps, e.g. `abs(...)` | — |
| 2 | unary minus | — |
| 3 | `*`, `/` | left |
| 4 (lowest) | `+`, `-` | left |

: Operator precedence. {#tbl:precedence}

These are the conventions of ordinary arithmetic. The expression
`a + b * c` means $a + (b \times c)$, and `a - b - c` means $(a - b) - c$.

## Duration rules {#sec:duration-rules}

Every expression has a *kind*: either *generator* (no duration) or *timed*
with a nominal duration $T$. The kind is determined statically, before any
sample is computed, by the rules in @tbl:kinds.

| Expression | Kind |
|:--|:--|
| primitive, constant | generator |
| block `{ s1 ; ... ; sn }` | timed, $T = \sum T_{s_i}$ |
| `repeat n { ... }` | timed, $n$ times the duration of the block |
| $e_1 \circ e_2$, both generators | generator |
| $e_1 \circ e_2$, one timed with $T$, one generator | timed, $T$ |
| $e_1 \circ e_2$, both timed with $T_1 = T_2 = T$ | timed, $T$ |
| $e_1 \circ e_2$, both timed with $T_1 \ne T_2$ | **error** |
| $h(e)$, unary map | same kind as $e$ |

: Kinds of expressions; $\circ$ is any binary operator. {#tbl:kinds}

Durations are compared exactly, on their decimal values (@sec:sampling).

When a generator is combined with a timed operand, it is bound to the same
interval as that operand: its local time starts with the first sample of the
operand, and its realised duration is that of the operand.

### Nested sequences and sampling

Blocks can be nested. Segment boundaries are always computed from absolute
nominal start times with Rule 1 of @sec:boundaries, whatever the nesting
depth. A segment nested inside a block that starts at $\tau_B$ and begins at
offset $\delta$ within the block has absolute start time $\tau_B + \delta$.
Two operands of a pointwise combination start at the same absolute time and
have the same nominal duration, so they always contain the same samples.

## Binary operators {#sec:binary}

Binary operators act sample by sample, @eq:pointwise. Arithmetic follows
IEEE 754 double precision.

**Rule 4 (finite samples).** Every realised sample MUST be a finite number.
If an operation produces an infinity or a NaN (for example, a division by a
waveform that passes through zero), realisation MUST stop with an error that
identifies the segment and the sample. Since stochastic waveforms can take
any value, this check cannot be done statically in general.

Two binary functions complete the operators: `min(e1, e2)` and
`max(e1, e2)`, which take the smaller or larger of the two values at each
sample. They follow the same duration rules.

## Unary maps {#sec:maps}

A unary map applies a real function $h$ to every sample of its argument.
The maps of @tbl:maps are defined.

| Map | Definition | Domain of the argument |
|:--|:--|:--|
| `abs(e)` | $\lvert x \rvert$ | $\mathbb{R}$ |
| `pos(e)` | $[x]_+ = \max(x, 0)$ | $\mathbb{R}$ |
| `pow(e, k)` | $x^k$ | $x \ge 0$, or any $x$ if $k$ is an integer; $x \ne 0$ if $k < 0$ |
| `spow(e, k)` | $\operatorname{sign}(x)\,\lvert x \rvert^k$ | $\mathbb{R}$; $x \ne 0$ if $k < 0$ |
| `sqrt(e)` | $\sqrt{x}$ | $x \ge 0$ |
| `exp(e)` | $e^x$ | $\mathbb{R}$ |
| `log(e)` | $\ln x$ | $x > 0$ |
| `clip(e, lo, hi)` | $\min(\max(x, lo), hi)$ | $\mathbb{R}$; requires $lo \le hi$ |

: Unary maps. {#tbl:maps}

A sample outside the domain of a map is an error, by Rule 4. The map
`spow` (signed power) is defined everywhere and is the safe choice for
compressing or expanding a waveform that takes both signs.

## Repetition inside a waveform {#sec:repeat}

The construct
```
repeat n { s1 ; s2 ; ... }
```
with a non-negative integer $n$, is a timed waveform equal to $n$ copies of
the block, concatenated. It is pure shorthand. For example, ten pulses of
1 ms at 10 Hz can be written
```
repeat 10 { 1ms dc(1) ; 99ms dc(0) }
```

Every copy is a separate part of the waveform. Stochastic generators in
different copies therefore receive different random streams and produce
different realisations, unless their seed is fixed explicitly
(@sec:stochastic). The keyword `prev` in the first segment of a copy refers
to the end value of the previous copy.

::: note
`repeat` repeats a *part of one waveform*. Repeating a whole stimulus several
times, with pauses between trials, is a matter for the protocol layer
(@sec:protocol), and it produces separate recordings.
:::

## The previous value {#sec:algebra-prev}

### End values of expressions

The end value of a segment was defined for primitives in @sec:end-value. It
extends to every expression recursively:

- $E(c) = c$ for a constant $c$;
- $E(g)$ for a primitive $g$ is given in its card (@sec:primitives);
- $E(e_1 \circ e_2) = E(e_1) \circ E(e_2)$ for a binary operator or binary
  function;
- $E(h(e)) = h(E(e))$ for a unary map;
- $E(\{ s_1 ; \dots ; s_n \}) = E(s_n)$, and $E$ of a `repeat` is the end
  value of its last copy (0 copies: see below);
- for a stochastic primitive, $E$ is its last realised sample, or, if the
  segment has no sample, the value of `prev` where it appears.

If evaluating an end value violates Rule 4 (for example, a division by zero
at $u = \hat T$), the end value is undefined, and it is an error to refer to
it with `prev`.

### Resolution of `prev`

The keyword `prev` may appear as the value of any parameter of dimension
*amplitude*. It is resolved as follows.

1. Inside a segment $s_i$ of a sequence, `prev` is the end value
   $E(s_{i-1})$ of the preceding segment *of the same sequence*.
2. Inside the first segment of a block, `prev` is the value of `prev` at the
   place where the block appears (rule 1 or 2 applied to the enclosing
   sequence). Likewise for a `repeat` with zero copies, whose end value is
   that same `prev`.
3. In the first segment of the waveform, `prev` is 0.

All occurrences of `prev` within one segment, including those inside every
operand of a combination, refer to the same value. The value depends only on
segments that come earlier in time, so it is always well defined.

## Static checks {#sec:static-checks}

Before computing any sample, an implementation MUST check that:

- every primitive and map is known, and every parameter is known, given at
  most once, and within its domain;
- every literal has a unit compatible with the dimension of its field
  (@sec:units);
- every combination of timed operands has matching durations
  (@tbl:kinds), and every segment has a duration;
- every duration is non-negative and every `repeat` count is a
  non-negative integer.

Errors found by these checks MUST be reported with the position of the
offending text. Only Rule 4 errors and file errors can occur during
realisation.

## Algebraic laws (informative) {#sec:laws}

The following identities hold for timed waveforms and are useful to
implementers, for example to simplify or normalise expressions.

- For timed waveforms of equal duration, `+` and `*` are associative and
  commutative, `*` distributes over `+`, and the constants 0 and 1 are their
  identities. Pointwise operations thus form a commutative ring, up to the
  usual rounding of floating point.
- Concatenation is associative, with the empty waveform as identity
  (@sec:concatenation).
- **Interchange law.** If $T_a = T_c$ and $T_b = T_d$, then
  $(a ; b) \circ (c ; d) = (a \circ c) ; (b \circ d)$. A combination of two
  sequences with the same segmentation can therefore be rewritten as a
  sequence of combinations, and vice versa.

Rewriting an expression gives the same deterministic samples, but it can
change the addresses of stochastic generators and therefore their
realisations, though not their statistics (@sec:stochastic).

## Examples

The following segments illustrate the operators. Each line is one segment.

```
5s  sine(1, 1Hz) + ou(mean=0, sd=0.2, tau=5ms)    # sine plus noise
5s  sine(1, 1Hz) * ou(mean=0, sd=2, tau=5ms)      # noise with a modulated amplitude
5s  abs(sine(4, 1Hz))                             # full-wave rectification
5s  pos(sine(4, 1Hz))                             # half-wave rectification
30s ramp(from=50, to=300)                         # ramp from 50 to 300
5s  (1 + 0.5*sine(1, 2Hz)) * sine(1, 40Hz)        # amplitude modulation
10s pos(ou(mean=10, sd=5, tau=3ms))               # non-negative conductance
```

