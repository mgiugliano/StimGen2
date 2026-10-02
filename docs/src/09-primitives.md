# Catalogue of primitive generators {#sec:primitives}

This chapter lists the primitive generators of StimGen 2. Every primitive is
described by a card with the same structure:

- a one-line description;
- a parameter table: name, dimension (@tbl:units), domain, and default
  value (a dash means the parameter is required);
- the definition, as a function of grid-aligned local time $u$ and realised
  duration $\hat T$ (@sec:local-time);
- the end value $e$ (@sec:end-value).

Parameters can be given by name, or by position in the order of the table.
Parameters of dimension *amplitude* are in the units of the channel
(@sec:units-amplitude). Every stochastic primitive also accepts the
parameter `seed` (@sec:stochastic), which is not repeated in the tables.

| Name | Kind | Duration-aware | Summary |
|:-----|:-----|:--:|:--------|
| `dc` | deterministic | | constant level |
| `ramp` | deterministic | yes | linear ramp |
| `relax` | deterministic | | exponential relaxation to a level |
| `sine` | deterministic, periodic | | sinusoid |
| `square` | deterministic, periodic | | square wave with duty cycle |
| `saw` | deterministic, periodic | | asymmetric triangle (sawtooth) |
| `triangle` | deterministic, periodic | | symmetric triangle |
| `chirp` | deterministic | yes | sinusoid with swept frequency |
| `biexp` | deterministic | | difference of exponentials |
| `alpha` | deterministic | | alpha function |
| `file` | deterministic | | samples read from a file |
| `pulses` | deterministic or stochastic | | train of pulses |
| `ou` | stochastic | | Ornstein–Uhlenbeck process |
| `wnoise` | stochastic | | Gaussian white noise |
| `unoise` | stochastic | | uniform white noise |
| `cnoise` | stochastic | yes | power-law ($1/f^\alpha$) noise |

: Primitive generators. {#tbl:primitives}

![Every primitive generator, rendered by `sg` as one segment of 400 ms at 5 kHz with the parameters shown (the `file` panel plays a damped oscillation stored as text at 1 kHz).](figures/fig-gallery.pdf){#fig:gallery}

## Levels and ramps

### `dc`: constant level {#sec:prim-dc}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `level` | amplitude | $\mathbb{R}$ | — |

$$
g(u) = \texttt{level}.
$$

End value: `level`.

### `ramp`: linear ramp {#sec:prim-ramp}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `from` | amplitude | $\mathbb{R}$ or `prev` | `prev` |
| `to` | amplitude | $\mathbb{R}$ | — |

$$
g(u, \hat T) = \texttt{from} + (\texttt{to} - \texttt{from})\,\frac{u}{\hat T}.
$$ {#eq:ramp}

End value: `to`. Duration-aware.

Because the default of `from` is `prev`, the short form `ramp(to=4)` starts
from the end value of the preceding segment. (A single positional argument,
as in `ramp(4)`, is `from`, and `to` is then missing.) We recommend writing
`from` explicitly whenever the ramp is not meant to continue from the
previous segment.

### `relax`: exponential relaxation {#sec:prim-relax}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `from` | amplitude | $\mathbb{R}$ or `prev` | `prev` |
| `to` | amplitude | $\mathbb{R}$ | — |
| `tau` | time | $> 0$ | — |

$$
g(u) = \texttt{to} + (\texttt{from} - \texttt{to})\, e^{-u/\tau}.
$$

End value: $g(\hat T)$. With `from=1, to=0` this is a decaying exponential;
with `from=0, to=1` it is the charging curve of an RC circuit.

## Periodic generators

The periodic generators share the parameters `phase` (rad, default 0),
`offset` (amplitude, default 0), and `clock` (`local` or `global`, default
`local`; see @sec:clock). For square, saw, and triangle waves we use the
*cycle position*
$$
p(u) = \operatorname{frac}\!\left( \texttt{freq}\cdot u + \frac{\texttt{phase}}{2\pi} \right) \in [0, 1).
$$ {#eq:cycle-position}
With `clock=global`, $u$ is replaced by global time $t_k$ in all formulas of
this section. The end value of every periodic generator is its value at
$u = \hat T$.

### `sine`: sinusoid {#sec:prim-sine}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `freq` | frequency | $\ge 0$ | — |
| `phase` | phase | $\mathbb{R}$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u) = \texttt{amp}\,\sin\!\big(2\pi\,\texttt{freq}\,u + \texttt{phase}\big) + \texttt{offset}.
$$

`amp` is half the peak-to-peak amplitude.

### `square`: square wave {#sec:prim-square}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `freq` | frequency | $> 0$ | — |
| `duty` | fraction | $[0, 1]$ | 0.5 |
| `phase` | phase | $\mathbb{R}$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u) =
\begin{cases}
\texttt{offset} + \texttt{amp} & p(u) < \texttt{duty},\\
\texttt{offset} - \texttt{amp} & \text{otherwise}.
\end{cases}
$$ {#eq:square}

The wave alternates between $\texttt{offset} \pm \texttt{amp}$, and the
high level lasts the fraction `duty` of each period. Its mean is
$\texttt{offset} + \texttt{amp}\,(2\,\texttt{duty} - 1)$, which is zero
only when `duty` = 0.5 and `offset` = 0.

### `saw`: sawtooth {#sec:prim-saw}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `freq` | frequency | $> 0$ | — |
| `duty` | fraction | $[0, 1]$ | 1 |
| `phase` | phase | $\mathbb{R}$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u) = \texttt{offset} +
\begin{cases}
-\texttt{amp} + 2\,\texttt{amp}\; \dfrac{p(u)}{\texttt{duty}} & p(u) < \texttt{duty},\\[2ex]
\phantom{-}\texttt{amp} - 2\,\texttt{amp}\; \dfrac{p(u) - \texttt{duty}}{1 - \texttt{duty}} & \text{otherwise}.
\end{cases}
$$ {#eq:saw}

The wave rises linearly from $-$`amp` to $+$`amp` during the fraction `duty`
of each period and falls back during the rest. It is the time integral, rescaled, of a
zero-mean square wave with the same duty cycle, and its own mean is zero for
any `duty`. With `duty` = 1 it is a rising sawtooth, with `duty` = 0 a falling
sawtooth, and with `duty` = 0.5 a symmetric triangle. At $u = 0$ with zero
phase, it starts at its minimum.

### `triangle`: symmetric triangle {#sec:prim-triangle}

`triangle(amp, freq, phase, offset)` is a shorthand for
`saw(amp, freq, duty=0.5, phase, offset)`.

### `chirp`: swept-frequency sinusoid {#sec:prim-chirp}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `f0` | frequency | $\ge 0$ | — |
| `f1` | frequency | $\ge 0$ | — |
| `law` | keyword | `linear`, `exp` | `linear` |
| `phase` | phase | $\mathbb{R}$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u, \hat T) = \texttt{amp}\,\sin\!\big(\Phi(u) + \texttt{phase}\big) + \texttt{offset},
$$
where the instantaneous frequency $\Phi'(u)/2\pi$ moves from `f0` at $u = 0$
to `f1` at $u = \hat T$. For the linear law,
$$
\Phi(u) = 2\pi \left( f_0\, u + \frac{f_1 - f_0}{2\hat T}\, u^2 \right),
$$ {#eq:chirp-lin}
and for the exponential law, which requires $f_0, f_1 > 0$ and spends equal
time in every octave,
$$
\Phi(u) = 2\pi f_0 \hat T \,\frac{(f_1/f_0)^{u/\hat T} - 1}{\ln(f_1/f_0)}
\qquad (f_0 \ne f_1),
$$ {#eq:chirp-exp}
which reduces to the linear law when $f_0 = f_1$. The derivations are in
@sec:app-derivations. End value: $g(\hat T, \hat T)$. Duration-aware.

Chirps are the stimuli used to measure the impedance profile of neurons
(ZAP stimuli) [@Puil1986].

## Synaptic-like waveforms

These waveforms take the value `offset` before `delay` and are defined in
terms of the time since the delay, $v = u - \texttt{delay}$.

### `biexp`: difference of exponentials {#sec:prim-biexp}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `tau_rise` | time | $> 0$ | — |
| `tau_decay` | time | $> \texttt{tau\_rise}$ | — |
| `delay` | time | $\ge 0$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u) = \texttt{offset} + \frac{\texttt{amp}}{K}
\Big( e^{-v/\tau_d} - e^{-v/\tau_r} \Big)\,\mathbb{1}[v \ge 0],
$$ {#eq:biexp}
with $\tau_r$ = `tau_rise`, $\tau_d$ = `tau_decay`, and the normalisation
$$
K = \left(\frac{\tau_r}{\tau_d}\right)^{\frac{\tau_r}{\tau_d - \tau_r}}
  - \left(\frac{\tau_r}{\tau_d}\right)^{\frac{\tau_d}{\tau_d - \tau_r}},
$$ {#eq:biexp-K}
which makes the peak value equal to `amp`. The peak occurs at
$v^* = \frac{\tau_r \tau_d}{\tau_d - \tau_r} \ln(\tau_d/\tau_r)$. End value:
$g(\hat T)$.

### `alpha`: alpha function {#sec:prim-alpha}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `tau` | time | $> 0$ | — |
| `delay` | time | $\ge 0$ | 0 |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

$$
g(u) = \texttt{offset} + \texttt{amp}\; \frac{v}{\tau}\, e^{\,1 - v/\tau}\; \mathbb{1}[v \ge 0].
$$ {#eq:alpha}

The peak value `amp` is reached at $v = \tau$. This is the limit of `biexp`
when `tau_rise` and `tau_decay` both tend to $\tau$. End value: $g(\hat T)$.

## Arbitrary samples

### `file`: samples from a file {#sec:prim-file}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `path` | string | readable file | — |
| `rate` | frequency | $> 0$ | — |
| `column` | count | $\ge 1$ | 1 |
| `interp` | keyword | `hold`, `linear` | `hold` |
| `extend` | keyword | `error`, `hold`, `zero`, `cycle` | `error` |

The file holds $M$ numbers $y_0, \dots, y_{M-1}$, sampled at `rate`. It is a
text file with one row per sample; `column` selects a column when there are
several. Relative paths are resolved from the directory of the description.
With $r$ = `rate` and $j = \lfloor u\,r \rfloor$,
$$
g(u) =
\begin{cases}
y_j & \texttt{interp=hold},\\
y_j + (u r - j)\,(y_{j+1} - y_j) & \texttt{interp=linear}.
\end{cases}
$$
When $j$ falls beyond the last sample, `extend` decides: an error (default),
the last value, zero, or a periodic repetition of the file. End value: $g$
evaluated at $u = \hat T$ by the same rules, except that
with `extend=error` the last sample is held, so that a file exactly as long
as its segment is accepted.

This primitive is the escape hatch for waveforms that have no compact
description, such as a recorded synaptic current played back in dynamic
clamp. It gives up the main advantage of the language, which is that the
description is short. The provenance record MUST include a cryptographic
hash of the file (@sec:output).

## Pulse trains {#sec:prim-pulses}

### `pulses`: train of pulses

A pulse train is the superposition of copies of a *kernel* $h$, one at each
onset time $s_1 < s_2 < \dots$ inside the segment:
$$
g(u) = \texttt{offset} + \texttt{amp} \sum_{n} h(u - s_n).
$$ {#eq:pulses}
The onset times are regular, random (Poisson), or listed explicitly.

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `amp` | amplitude | $\mathbb{R}$ | — |
| `rate` | frequency | $> 0$ | — (unless `times` is given) |
| `shape` | keyword | `square`, `biphasic`, `exp`, `alpha`, `biexp` | `square` |
| `width` | time | $> 0$ | — (for `square`, `biphasic`) |
| `tau` | time | $> 0$ | — (for `exp`, `alpha`) |
| `tau_rise`, `tau_decay` | time | $0 < \tau_r < \tau_d$ | — (for `biexp`) |
| `timing` | keyword | `regular`, `poisson` | `regular` |
| `times` | list of times | increasing, $\ge 0$ | none |
| `delay` | time | $\ge 0$ | 0 |
| `dead` | time | $[0, 1/\texttt{rate})$ | 0 |
| `align` | keyword | `grid`, `exact` | `grid` |
| `offset` | amplitude | $\mathbb{R}$ | 0 |

**Kernels.** With $w$ = `width` and the time $v$ since onset:

| `shape` | $h(v)$ for $v \ge 0$ (and $h = 0$ for $v < 0$) |
|:--|:--|
| `square` | $\mathbb{1}[v < w]$ |
| `biphasic` | $\mathbb{1}[v < w] - \mathbb{1}[w \le v < 2w]$ (each phase lasts $w$) |
| `exp` | $e^{-v/\tau}$ |
| `alpha` | $(v/\tau)\, e^{1 - v/\tau}$ |
| `biexp` | $\big(e^{-v/\tau_d} - e^{-v/\tau_r}\big)/K$, with $K$ from @eq:biexp-K |

: Pulse kernels; every kernel has peak value 1. {#tbl:kernels}

![The five kernels, each as a single pulse at 2 ms, rendered with `pulses(1, times=[2ms], shape=...)`.](figures/fig-kernels.pdf){#fig:kernels}

**Onset times.**

- `timing=regular`: $s_n = \texttt{delay} + n/\texttt{rate}$, for
  $n = 0, 1, 2, \dots$ The first pulse starts at `delay`.
- `timing=poisson`: $s_1 = \texttt{delay} + I_1$ and
  $s_{n+1} = s_n + I_{n+1}$, where the intervals $I_n$ are independent, each
  equal to `dead` plus an exponential variate with mean
  $1/\texttt{rate} - \texttt{dead}$. The mean rate is therefore exactly
  `rate`. With `dead` = 0, the onsets form a homogeneous Poisson process; a
  positive `dead` time is a refractory period that prevents pulses from
  overlapping. The pulse train is then a stochastic generator.
- `times=[...]`: the onsets are the listed local times, plus `delay`.

Only onsets with $s_n < \hat T$ are used. A pulse that starts inside the
segment and would extend beyond its end is truncated; it does not spill into
the next segment.

**Overlap.** Pulses are summed, as @eq:pulses states. Two overlapping square
pulses give a value of 2·`amp`. Use `dead` $\ge$ `width` to exclude
overlaps.

**Edges.** With `align=grid` (default), each onset is moved to the nearest
sample, $s_n \to \operatorname{R}(s_n/\Delta t)\,\Delta t$, and the width of
square and biphasic pulses is rounded to a whole number of samples,
$\max\!\big(1, \operatorname{R}(w/\Delta t)\big)$. Every pulse then has
exactly the same number of samples, which is what an experimenter expects
from a train of identical stimuli. With `align=exact`, @eq:pulses is
evaluated at the sample times without any rounding, so pulse widths can
differ by one sample. An implementation SHOULD warn when $w < \Delta t$.

End value: `offset` plus the summed contribution of the pulses at
$u = \hat T$. This is generally `offset` for square pulses and nonzero for
exponential tails.

## Noise processes

The noise generators below are stochastic. Their random variates come from
the stream attached to the generator (@sec:stochastic). In the formulas,
$\xi_k$ denotes independent standard Gaussian variates and $U_k$ independent
uniform variates on $[0, 1)$, both indexed by the sample within the segment.

### `ou`: Ornstein–Uhlenbeck process {#sec:prim-ou}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `mean` | amplitude | $\mathbb{R}$ | — |
| `sd` | amplitude | $\ge 0$ | — |
| `tau` | time | $> 0$ | — |
| `init` | keyword or amplitude | `stationary`, `mean`, `prev`, or a value | `stationary` |

The Ornstein–Uhlenbeck process [@Uhlenbeck1930] is the stationary Gaussian
process with mean $\mu$ = `mean`, standard deviation $\sigma$ = `sd`, and
autocorrelation
$$
\operatorname{Cov}\big(X(u), X(u + s)\big) = \sigma^2 e^{-|s|/\tau}.
$$
Equivalently, it is the solution of the stochastic differential equation
$$
dX = -\frac{X - \mu}{\tau}\, du + \sigma \sqrt{\frac{2}{\tau}}\; dW,
$$ {#eq:ou-sde}
i.e. white noise passed through a first-order low-pass filter with time
constant $\tau$. Its one-sided power spectral density is
$S(f) = 4\sigma^2\tau / \big(1 + (2\pi f \tau)^2\big)$.

**Realisation.** The samples MUST be generated with the exact update
[@Gillespie1996]
$$
x_{k+1} = \mu + (x_k - \mu)\,\rho + \sigma \sqrt{1 - \rho^2}\;\xi_{k+1},
\qquad \rho = e^{-\Delta t/\tau},
$$ {#eq:ou-exact}
which reproduces the statistics of the continuous process at the sample
times for any $\Delta t$, however large compared with $\tau$. The first
sample of the segment is
$x_0 = \mu + \sigma \xi_0$ for `init=stationary`, $\mu$ for `init=mean`, the
end value of the previous segment for `init=prev`, or the given value.

With `init=stationary`, the realisation is stationary from its first sample.
With any other choice, the mean and variance relax towards their stationary
values with time constants $\tau$ and $\tau/2$, respectively. End value: the
last sample.

### `wnoise`: Gaussian white noise {#sec:prim-wnoise}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `mean` | amplitude | $\mathbb{R}$ | — |
| `sd` | amplitude | $\ge 0$ | — |

$$
x_k = \mu + \sigma\,\xi_k .
$$

The samples are independent. The variance per sample, $\sigma^2$, is fixed,
so the one-sided power spectral density, $2\sigma^2 \Delta t$ on
$[0, f_s/2]$, depends on the sampling rate. Doubling $f_s$ at fixed `sd`
halves the noise power per hertz. A user who wants a fixed spectral density
$S_0$ should choose $\sigma = \sqrt{S_0 f_s / 2}$. End value: the last
sample.

`wnoise` is the limit of `ou` for $\tau \ll \Delta t$, where $\rho \to 0$ in
@eq:ou-exact.

### `unoise`: uniform white noise {#sec:prim-unoise}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `mean` | amplitude | $\mathbb{R}$ | — |
| `sd` | amplitude | $\ge 0$ | — |

$$
x_k = \mu + \sigma\sqrt{12}\,\big(U_k - \tfrac12\big).
$$

The samples are independent and uniformly distributed on
$[\mu - \sigma\sqrt3,\ \mu + \sigma\sqrt3]$, with mean $\mu$ and standard
deviation $\sigma$. The spectral remarks for `wnoise` apply. End value: the
last sample.

### `cnoise`: power-law noise {#sec:prim-cnoise}

| Parameter | Dimension | Domain | Default |
|:--|:--|:--|:--|
| `mean` | amplitude | $\mathbb{R}$ | — |
| `sd` | amplitude | $\ge 0$ | — |
| `alpha` | number | $\ge 0$ | 1 |
| `fmin` | frequency | $\ge 0$ | $1/\hat T$ |
| `fmax` | frequency | $\le f_s/2$ | $f_s/2$ |

A Gaussian process with power spectral density proportional to
$f^{-\alpha}$ between `fmin` and `fmax`, and zero elsewhere. The value
$\alpha = 0$ gives band-limited white noise, $\alpha = 1$ pink noise, and
$\alpha = 2$ Brownian ("red") noise.

**Realisation.** The whole segment is synthesised at once in the frequency
domain [@Timmer1995]. For the $N_i$ samples of the segment and the
frequencies $f_j = j / (N_i \Delta t)$, the Fourier coefficient at $f_j$ has
independent Gaussian real and imaginary parts with variance proportional to
$f_j^{-\alpha}$ inside the band, and zero outside. An inverse discrete
Fourier transform gives the time series, which is scaled so that its
*expected* variance is $\sigma^2$ and shifted by $\mu$. The exact procedure,
including the treatment of the zero and Nyquist frequencies, is in
@sec:app-derivations.

This generator is duration-aware and non-causal: every sample depends on the
length of the segment and on random variates drawn for the whole segment.
The realised standard deviation fluctuates around $\sigma$, more so for large
$\alpha$. End value: the last sample.
