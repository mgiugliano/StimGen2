# The abstract model {#sec:model}

This chapter defines the objects that a StimGen 2 description denotes. The
definitions are purely mathematical. How these objects are written as text is
the subject of @sec:syntax, and how they are turned into samples is the
subject of @sec:sampling. We use a light preview of the syntax in the
examples, so that the reader can connect the two.

## Three layers {#sec:layers}

A description has three layers, each built on the previous one:

1. **Waveform.** One channel, one trial. A waveform is a real-valued
   function of time on a finite interval. It is written as a sequence of
   segments, and each segment is built from primitive generators by
   pointwise algebra. The waveform layer has no variables and no control
   flow.
2. **Stimulus.** Several waveforms played simultaneously on named output
   channels with a shared time base, together with markers (digital events)
   and the physical units of each channel (@sec:stimulus).
3. **Protocol.** An ordered collection of stimuli, with repetitions,
   inter-trial intervals, ordering rules, and parameter sweeps
   (@sec:protocol). The protocol layer is the only place where variables
   exist.

::: rationale
Keeping the layers separate keeps each one simple. A waveform remains
a short declarative text, readable at a glance, while the protocol layer
carries all the logic that changes from one trial to the next. The protocol
layer is defined by its expansion into a directory of plain `.sg` files
(@sec:protocol), so a protocol can always be inspected, or replaced, by an
ordinary folder of stimuli.
:::

## Generators {#sec:generators}

A *generator* is a rule that produces a value for every instant of a segment,
once the duration of the segment is known.

**Definition 1 (deterministic generator).** A deterministic generator is a
function
$$
g : (u, T; \theta) \mapsto g(u, T; \theta) \in \mathbb{R},
\qquad 0 \le u < T,
$$ {#eq:generator}
where $u$ is local time, $T > 0$ is the duration of the segment that hosts
the generator, and $\theta$ is a vector of parameters fixed when the
generator is written.

Most generators ignore $T$. A sine wave, for example, is
$g(u) = a \sin(2\pi f u + \varphi) + c$. Some generators need $T$: a linear
ramp from $a$ to $b$ is $g(u, T) = a + (b - a)\,u/T$, and a chirp sweeps its
frequency from $f_0$ to $f_1$ over the whole segment. We call these
*duration-aware*. The catalogue in @sec:primitives marks them.

**Definition 2 (stochastic generator).** A stochastic generator is a family
of random functions, i.e. a stochastic process $\{G(u)\}_{0 \le u < T}$ whose
law depends on $T$, $\theta$, and, for some generators, on the sampling
interval $\Delta t$. A *realisation* of a stochastic generator is one sample
path of that process. A stochastic generator becomes a definite function
only once a random stream is attached to it (@sec:stochastic).

A *scalar* constant $c$ is the deterministic generator $g(u) = c$. This lets
us write `2 * sine(1, 5Hz)` without a separate scaling operation.

## Timed waveforms {#sec:timed}

**Definition 3 (timed waveform).** A timed waveform is a pair
$w = (T, \phi)$, where $T \ge 0$ is its duration and
$\phi : [0, T) \to \mathbb{R}$ is its value as a function of time.

A generator becomes a timed waveform when it is given a duration. We call
this operation *binding* and write it $g \!\upharpoonright\! T$:
$$
g \!\upharpoonright\! T = \big(T,\ u \mapsto g(u, T; \theta)\big).
$$ {#eq:binding}
In the text syntax, binding is written by placing the duration before the
generator:

```
5s  sine(amp=3, freq=1Hz)
```

A *segment* is a timed waveform that appears as one element of a sequence.

## Concatenation {#sec:concatenation}

**Definition 4 (concatenation).** The concatenation of two timed waveforms
$w_1 = (T_1, \phi_1)$ and $w_2 = (T_2, \phi_2)$ is
$$
w_1 \mathbin{;} w_2 = \Big(T_1 + T_2,\ t \mapsto
\begin{cases}
\phi_1(t) & 0 \le t < T_1,\\
\phi_2(t - T_1) & T_1 \le t < T_1 + T_2.
\end{cases}\Big)
$$ {#eq:concat}

Concatenation is associative, $(w_1 ; w_2) ; w_3 = w_1 ; (w_2 ; w_3)$, and
the empty waveform $\varepsilon = (0, \varnothing)$ is its identity. Timed
waveforms under concatenation therefore form a monoid, and a sequence
$w_1 ; w_2 ; \dots ; w_n$ needs no parentheses. Its duration is additive,
$$
T_{\text{total}} = \sum_{i=1}^{n} T_i ,
$$ {#eq:total-duration}
which also determines the length of the recording.

Each segment keeps its own local time. Inside segment $i$, local time is
$u = t - \tau_i$, where $\tau_i = \sum_{j<i} T_j$ is the start of the
segment. A periodic generator therefore restarts its phase at the beginning
of each segment, unless it is told to follow global time instead
(@sec:clock).

A sequence written in braces is itself a timed waveform and can be used
anywhere a timed waveform is expected:

```
{ 100ms dc(0) ; 200ms ou(mean=2, sd=0.5, tau=1ms) ; 100ms dc(0) }
```

## Pointwise combination {#sec:pointwise-model}

Waveforms of equal duration can be combined sample by sample. Let
$\circ$ be one of the operations $+$, $-$, $\times$, $\div$.

**Definition 5 (pointwise combination).** For timed waveforms of equal
duration,
$$
(T, \phi_1) \circ (T, \phi_2) = \big(T,\ u \mapsto \phi_1(u) \circ \phi_2(u)\big).
$$ {#eq:pointwise}
For generators, $(g_1 \circ g_2)(u, T) = g_1(u, T) \circ g_2(u, T)$. When a
timed waveform is combined with a generator, the generator is first bound to
the duration of the timed waveform:
$$
(T, \phi) \circ g = (T, \phi) \circ (g \!\upharpoonright\! T).
$$ {#eq:mixed-binding}

Combining two timed waveforms of different durations is an error.

These three rules give the language its expressive power without extra
syntax. An envelope can be applied to a whole sequence, because the sequence
is a timed waveform and the carrier is a generator:

```
{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(amp=2, freq=8Hz)
```

Here the braces have duration 5 s, so the sine generator is bound to 5 s and
multiplied, point by point, by the trapezoidal envelope.

Unary maps such as absolute value or rectification act in the same pointwise
way, $(T, \phi) \mapsto (T, h \circ \phi)$ for a real function $h$. They are
listed in @sec:algebra.

## Context: the previous value {#sec:prev}

The value of a segment depends only on its own definition, with one
exception. A generator parameter may be given the special value `prev`,
which stands for the *last realised sample of the preceding segment in the
same sequence* (0 if there is no preceding segment). The common use is a
ramp that starts where the previous segment ended:

```
2.5s  dc(-1)
5s    ramp(from=prev, to=4)
```

`prev` is the only context-dependent construct of the waveform layer. It is
defined on realised samples (rather than on the continuous function) so that
it has a meaning also after a stochastic segment. Its exact definition,
including its behaviour inside pointwise combinations, is given in
@sec:algebra-prev.

## Meaning of a description {#sec:semantics}

We write $\llbracket D \rrbracket$ for the meaning of a description $D$.

- If $D$ contains only deterministic generators, $\llbracket D \rrbracket$
  is one timed waveform $(T, \phi)$, a definite function of continuous time.
- If $D$ contains stochastic generators, $\llbracket D \rrbracket$ is a
  random timed waveform: a probability distribution over functions. Given a
  master seed $s$, $\llbracket D \rrbracket_s$ is one realisation, and it is
  unique (@sec:stochastic).

The sampling operator of @sec:sampling turns $\llbracket D \rrbracket_s$
into a finite sequence of numbers $x_0, \dots, x_{N-1}$ for a given sampling
rate.

::: note
**What independence of the sampling rate means.** A StimGen 2 description
does not depend on the waveform length, the sampling interval, or the D/A
resolution. This holds exactly for the
deterministic part: the description denotes a function of continuous time,
and changing $f_s$ only changes where that function is sampled. For
stochastic generators it holds in distribution, not sample by sample. An
Ornstein–Uhlenbeck realisation sampled at 10 kHz and one sampled at 20 kHz
have the same mean, variance, and autocorrelation, but they are different
sample paths. Discrete white noise is a stronger case. Its variance per
sample is fixed, so its power spectral density, $\sigma^2 \Delta t$, scales
with the sampling interval (@sec:prim-wnoise).
:::

## Summary of the objects

| Object | Has duration | Built from | Example |
|:-------|:------------:|:-----------|:--------|
| generator | no | primitives, scalars, operators | `sine(1, 5Hz) + 0.5` |
| segment | yes | duration + generator, or a block | `5s sine(1, 5Hz)` |
| sequence (block) | yes | segments joined by `;` | `{ 1s dc(0) ; 2s dc(1) }` |
| waveform | yes | one top-level sequence | a `.sg` file |
| stimulus | yes | waveforms on channels + markers | @sec:stimulus |
| protocol | per trial | stimuli + repetitions + sweeps | @sec:protocol |

: The objects of the model. {#tbl:objects}
