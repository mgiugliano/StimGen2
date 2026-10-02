# Sampling and quantisation {#sec:sampling}

A description denotes a function of continuous time (@sec:model). A D/A
converter needs a finite sequence of numbers. This chapter defines the
*sampling operator* that maps one to the other. Without it, two
implementations can disagree on the number of samples in a segment, on where
a step occurs, or on the phase of a sine wave.

## The sampling grid

Sampling takes a sampling rate $f_s > 0$ (Hz), with sampling interval
$\Delta t = 1/f_s$. The sampling rate is not part of the waveform. It is
supplied by the stimulus layer or by the program that realises the waveform.

Sample $k$ of a waveform corresponds to global time $t_k = k\,\Delta t$,
for $k = 0, 1, \dots, N-1$. The samples are *point evaluations*: no
filtering, averaging, or band-limiting is applied (@sec:aliasing).

## Segment boundaries {#sec:boundaries}

Consider a sequence of segments with nominal durations $T_1, \dots, T_m$ and
nominal start times $\tau_i = \sum_{j<i} T_j$, with $\tau_1 = 0$ and
$\tau_{m+1} = T_{\text{total}}$.

**Rule 1 (boundary index).** The first sample of segment $i$ has index
$$
n_i = \operatorname{R}\!\left(\tau_i \, f_s\right), \qquad i = 1, \dots, m+1,
$$ {#eq:boundary}
where $\operatorname{R}$ rounds to the nearest integer, with ties rounded to
the nearest even integer.

**Rule 2 (membership).** Sample $k$ belongs to segment $i$ if and only if
$n_i \le k < n_{i+1}$. Segment $i$ therefore contains
$N_i = n_{i+1} - n_i$ samples, and the waveform contains
$N = n_{m+1} = \operatorname{R}(T_{\text{total}} f_s)$ samples.

**Rule 3 (exact arithmetic).** The products $\tau_i f_s$ MUST be evaluated
so that the result of @eq:boundary equals the one obtained in exact
arithmetic on the decimal literals of the description. An implementation
MAY use rational or decimal arithmetic, or any other method with the same
result. Binary floating point is not sufficient. For instance,
$0.1 + 0.2$ is not $0.3$ in IEEE 754, and at $f_s = 10\,\text{Hz}$ this
shifts a boundary that falls exactly on a tie.

Three properties follow from these rules.

- **No drift.** Boundaries are rounded from cumulative time, not segment by
  segment. The error of every boundary is at most $\Delta t/2$, however long
  the sequence. If each segment were rounded on its own, the errors would
  add up.
- **No overlap and no gaps.** Every sample belongs to exactly one segment,
  because the intervals $[n_i, n_{i+1})$ partition $\{0, \dots, N-1\}$.
- **Half-open segments.** The sample that falls exactly on a boundary
  belongs to the segment that *starts* there. A step from `dc(0)` to
  `dc(1)` at $t = 1\,\text{s}$ with $f_s = 1\,\text{kHz}$ has $x_{999} = 0$
  and $x_{1000} = 1$.

::: note
**Example.** Forty segments of $0.15\,\text{ms}$ alternate between two
levels, at $f_s = 10\,\text{kHz}$ ($\Delta t = 0.1\,\text{ms}$). Each segment
is 1.5 samples long. Rounding each segment separately would give 2 samples
per segment, i.e. 80 samples ($8\,\text{ms}$ instead of $6\,\text{ms}$).
Rules 1–2 give boundaries $0, 2, 3, 4, 6, 8, 9, 10, 12, \dots$. The segments
are 1 or 2 samples long (1.5 on average), and the total is exactly 60
samples. @Fig:boundaries shows the first eight segments as realised by `sg`.
:::

![Segment boundaries at 10 kHz for segments of 0.15 ms that alternate between 0 and 1. Dotted lines: nominal boundaries $\tau_i$; dots: samples. Each segment holds 1 or 2 samples, and no rounding error accumulates.](figures/fig-boundaries.pdf){#fig:boundaries}

A segment may contain zero samples. This happens when its duration is
shorter than about $\Delta t$, or when it is exactly zero (zero-duration
segments are allowed, which is convenient in sweeps). An implementation
SHOULD warn when a segment of positive duration contains no sample. A
negative duration is an error.

## Local time of samples {#sec:local-time}

Inside segment $i$, sample $k$ is evaluated at the *grid-aligned local time*
$$
u_k = (k - n_i)\,\Delta t, \qquad n_i \le k < n_{i+1},
$$ {#eq:local-time}
and duration-aware generators receive the *realised duration*
$$
\hat T_i = N_i \, \Delta t
$$ {#eq:realised-duration}
in place of the nominal $T_i$. The value of sample $k$ is then
$$
x_k = g_i\big(u_k, \hat T_i; \theta_i\big).
$$ {#eq:sample-value}

In words, each segment is realised as if it started exactly on a sample and
lasted an integer number of samples. Its first sample is always at local time
$u = 0$, so a sine wave with zero phase starts at exactly zero. A ramp spans
exactly the samples of its segment. The price is a timing offset between the
continuous description and its realisation, at most $\Delta t/2$ at each
boundary. This is the best that any sampled representation can do.

::: rationale
An alternative is to evaluate each sample at its exact continuous time,
$x_k = g_i(t_k - \tau_i)$. Since $n_i \Delta t$ can be smaller than
$\tau_i$ by up to $\Delta t/2$, the first sample of a segment would then be
evaluated at a slightly negative local time, outside the domain of the
generator, and a sine wave would not start at its written phase. Grid
alignment avoids both problems and makes the realisation of every segment
depend only on its own sample count.
:::

## End value of a segment {#sec:end-value}

Each segment has an *end value* $e_i$. For a deterministic segment, it is the
value the generator would take at the first sample after the segment,
$$
e_i = g_i\big(\hat T_i, \hat T_i; \theta_i\big),
$$ {#eq:end-value}
i.e. the right-hand continuation of the generator. A ramp from $a$ to $b$ has
end value exactly $b$, even though its last sample is
$a + (b-a)(N_i - 1)/N_i$. For a stochastic segment, the end value is its last
realised sample. Combinations and unary maps propagate end values as defined
in @sec:algebra-prev. The end value is what the keyword `prev` refers to.

::: note
Because segments are half-open, a ramp never reaches its final value within
its own samples. The next segment does. The sequence

```
1s  ramp(from=0, to=1)
1s  dc(1)
```

is continuous at the boundary in the sense that matters for a D/A
converter: the sample sequence increases by equal steps up to and including
the first sample of `dc(1)`.
:::

## Global clock {#sec:clock}

By default, local time restarts at zero at the beginning of every segment.
Periodic generators (`sine`, `square`, `saw`, `triangle`) accept the
optional parameter `clock`:

- `clock=local` (default): the generator is evaluated at $u_k$.
- `clock=global`: the generator is evaluated at global time $t_k = k\,\Delta t$,
  measured from the start of the waveform.

With `clock=global`, two segments of the same sine wave separated by a pause
stay phase-locked to one continuous oscillation, as if the oscillation had
kept running during the pause. This is useful, for example, when an
oscillation is gated on and off by a sequence of segments.

## Values outside the waveform

A waveform defines samples $x_0, \dots, x_{N-1}$ only. What the output does
before the first and after the last sample is set by the *rest value* of the
channel, defined in the stimulus layer (@sec:stimulus). The default rest
value is 0.

## Aliasing and edges {#sec:aliasing}

Point sampling does not remove frequencies above the Nyquist frequency
$f_s/2$. Consequences for the user are:

- A sine or chirp whose frequency exceeds $f_s/2$ is realised as an alias at
  a lower frequency. An implementation SHOULD warn when any parameter of
  dimension *frequency* of a periodic generator exceeds $f_s/2$.
- The edges of square waves and pulses fall on the sampling grid. An edge
  at continuous time $t^*$ is realised at the first sample with
  $t_k \ge t^*$, so a pulse of width $w$ lasts $\lceil w/\Delta t \rceil$ or
  $\lfloor w/\Delta t \rfloor$ samples depending on where it starts. A pulse
  narrower than $\Delta t$ can be lost entirely. Pulse generators therefore
  follow an explicit edge rule (@sec:prim-pulses).
- Discrete white noise has a flat spectrum up to $f_s/2$ by construction.
  Its continuous-time interpretation depends on $f_s$ (@sec:prim-wnoise).

Band-limited synthesis (e.g. of square waves) is outside the scope of this
version.

## Quantisation {#sec:quantisation}

The realised samples $x_k$ are double-precision numbers in the units of the
channel. Converting them into codes for a D/A converter is the last step of
realisation, and it belongs to the stimulus layer, not to the waveform. For
completeness, we specify it here.

Let the channel have an output gain $\gamma$ (channel units per volt, e.g.
$400\,\text{pA/V}$ for a current-clamp command), and let the converter have
$B$ bits over the voltage range $[V_{\min}, V_{\max}]$. The code of sample
$k$ is
$$
c_k = \operatorname{clip}\!\left(
\operatorname{R}\!\left(
\frac{x_k/\gamma - V_{\min}}{V_{\max} - V_{\min}}\,(2^B - 1)
\right),\ 0,\ 2^B - 1 \right).
$$ {#eq:quantisation}

The quantisation step in channel units is
$q = \gamma\,(V_{\max} - V_{\min})/(2^B - 1)$. For example, a 16-bit
converter over $\pm 10\,\text{V}$ with $\gamma = 400\,\text{pA/V}$ gives
$q \approx 0.12\,\text{pA}$.

An implementation MUST report every sample that is clipped. It MUST NOT
clip silently, because a clipped stimulus is a different stimulus from the
one described.
