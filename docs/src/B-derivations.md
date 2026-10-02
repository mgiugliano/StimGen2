# Mathematical derivations {#sec:app-derivations}

This appendix collects the derivations behind formulas stated in the
reference chapters.

## Error of segment boundaries {#sec:app-boundaries}

Rounding to the nearest integer satisfies
$\lvert \operatorname{R}(x) - x \rvert \le \tfrac12$. With
$n_i = \operatorname{R}(\tau_i f_s)$ (@eq:boundary), the realised start time
of every segment satisfies
$$
\lvert n_i \Delta t - \tau_i \rvert \le \tfrac12 \Delta t ,
$$
independently of $i$, so the error does not grow along the sequence. The
number of samples of a segment, $N_i = n_{i+1} - n_i$, satisfies
$\lvert N_i - T_i f_s \rvert \le 1$. Equality can occur only through the
tie-breaking rule; for example, at $f_s = 1\,\text{Hz}$, a segment of 1 s
starting at 0.5 s spans the boundaries $\operatorname{R}(0.5) = 0$ and
$\operatorname{R}(1.5) = 2$, i.e. 2 samples. Rounding each
segment separately would instead give a cumulative error of up to
$m\,\Delta t/2$ after $m$ segments.

## Exact update of the Ornstein–Uhlenbeck process {#sec:app-ou}

The solution of @eq:ou-sde over an interval of length $\Delta t$ is
$$
X(u + \Delta t) = \mu + \big(X(u) - \mu\big)\, e^{-\Delta t/\tau}
 + \sigma\sqrt{\frac{2}{\tau}} \int_0^{\Delta t} e^{-(\Delta t - s)/\tau}\, dW(u + s).
$$
The stochastic integral is Gaussian with zero mean and, by the Itô isometry,
variance
$$
\frac{2\sigma^2}{\tau} \int_0^{\Delta t} e^{-2(\Delta t - s)/\tau}\, ds
 = \sigma^2 \big(1 - e^{-2\Delta t/\tau}\big) = \sigma^2 (1 - \rho^2),
$$
with $\rho = e^{-\Delta t/\tau}$. It is independent of $X(u)$ and of the
integrals over other intervals. This gives @eq:ou-exact, which is exact for
any $\Delta t$ [@Gillespie1996]. If $x_0$ is drawn from the stationary law
$\mathcal{N}(\mu, \sigma^2)$, then every $x_k$ has the same law, and
$\operatorname{Cov}(x_k, x_{k+l}) = \sigma^2 \rho^{l} = \sigma^2 e^{-l\Delta t/\tau}$,
which is the autocorrelation of the continuous process at lag $l\,\Delta t$.

For comparison, the Euler–Maruyama scheme
$x_{k+1} = x_k - (x_k - \mu)\Delta t/\tau + \sigma\sqrt{2\Delta t/\tau}\,\xi_{k+1}$
has stationary variance $\sigma^2 / (1 - \Delta t/(2\tau))$, which is
wrong by about $\Delta t/(2\tau)$ in relative terms and diverges as
$\Delta t \to 2\tau$.

The power spectral density follows from the Wiener–Khinchin theorem
[@Papoulis2002]. The two-sided spectrum of $\sigma^2 e^{-|s|/\tau}$ is
$2\sigma^2\tau / \big(1 + (2\pi f\tau)^2\big)$, and the one-sided spectrum
is twice that.

## Peak and normalisation of the biexponential {#sec:app-biexp}

For $v \ge 0$ let $h(v) = e^{-v/\tau_d} - e^{-v/\tau_r}$ with
$\tau_r < \tau_d$. Setting $h'(v) = 0$ gives
$e^{-v/\tau_d}/\tau_d = e^{-v/\tau_r}/\tau_r$, hence
$$
v^* = \frac{\tau_r \tau_d}{\tau_d - \tau_r}\, \ln\frac{\tau_d}{\tau_r}.
$$
Substituting,
$e^{-v^*/\tau_d} = (\tau_r/\tau_d)^{\tau_r/(\tau_d - \tau_r)}$ and
$e^{-v^*/\tau_r} = (\tau_r/\tau_d)^{\tau_d/(\tau_d - \tau_r)}$, so the peak
value $K = h(v^*)$ is @eq:biexp-K. Dividing by $K$ makes the peak equal to
1, and multiplying by `amp` makes it equal to `amp`.

**Limit $\tau_r \to \tau_d$.** Put $\tau_r = \tau$ and
$\tau_d = \tau + \varepsilon$. To first order in $\varepsilon$,
$h(v) \approx \varepsilon\, (v/\tau^2)\, e^{-v/\tau}$, whose peak is at
$v = \tau$ with value $\varepsilon/(e\tau)$. The normalised waveform tends to
$(v/\tau)\, e^{1 - v/\tau}$, the alpha function @eq:alpha.

## Phase of the chirp {#sec:app-chirp}

The phase is the integral of the instantaneous angular frequency,
$\Phi(u) = 2\pi \int_0^u f(s)\, ds$.

- Linear law, $f(s) = f_0 + (f_1 - f_0)\, s/\hat T$:
  $\Phi(u) = 2\pi\big(f_0 u + (f_1 - f_0) u^2 / (2\hat T)\big)$, which is
  @eq:chirp-lin.
- Exponential law, $f(s) = f_0\, k^{s/\hat T}$ with $k = f_1/f_0$:
  $\Phi(u) = 2\pi f_0 \hat T\, (k^{u/\hat T} - 1)/\ln k$, which is
  @eq:chirp-exp. As $k \to 1$, $(k^{x} - 1)/\ln k \to x$, and the phase tends
  to $2\pi f_0 u$.

The time the exponential chirp spends between frequencies $f$ and $2f$ is
$\hat T \ln 2 / \ln k$, the same for every octave.

## Spectrum of discrete white noise {#sec:app-white}

Independent samples with variance $\sigma^2$ taken every $\Delta t$ have a
flat two-sided spectral density $\sigma^2 \Delta t$ on
$[-f_s/2, f_s/2]$, since its integral over that band must equal
$\sigma^2$. The one-sided density on $[0, f_s/2]$ is $2\sigma^2\Delta t$.
Hence a fixed spectral density $S_0$ (one-sided) requires
$\sigma = \sqrt{S_0 f_s/2}$, as stated in @sec:prim-wnoise.

## Synthesis of power-law noise {#sec:app-cnoise}

The procedure for `cnoise` follows @Timmer1995. Let $M = N_i$ be the number
of samples of the segment, $\Delta t$ the sampling interval, and
$f_j = j/(M\Delta t)$ for $j = 0, \dots, \lfloor M/2 \rfloor$.

1. **Target spectrum.** $S_j = f_j^{-\alpha}$ if
   $\texttt{fmin} \le f_j \le \texttt{fmax}$ and $j \ge 1$; otherwise
   $S_j = 0$. In particular $S_0 = 0$, so the mean is set only by `mean`.
2. **Random coefficients.** For $j = 1, \dots, \lfloor M/2 \rfloor$, draw
   $a_j$ and then $b_j$, standard Gaussian (@tbl:consumption), and set
   $Z_j = \sqrt{S_j/2}\,(a_j + i\, b_j)$. If $M$ is even, the coefficient at
   the Nyquist frequency is real, $Z_{M/2} = \sqrt{S_{M/2}}\; a_{M/2}$, and
   $b_{M/2}$ is drawn but not used. Set $Z_0 = 0$.
3. **Hermitian symmetry.** $Z_{M-j} = \overline{Z_j}$ for
   $1 \le j < M/2$, so that the result is real.
4. **Inverse transform.**
   $y_k = \sum_{j=0}^{M-1} Z_j\, e^{2\pi i jk/M}$, for
   $k = 0, \dots, M-1$.
5. **Scaling.** The expected variance of $y_k$ is
   $$
   V = \sum_{1 \le j < M/2} 2 S_j \;+\; \mathbb{1}[M \text{ even}]\; S_{M/2},
   $$
   since each conjugate pair contributes $2\operatorname{Re}(Z_j e^{i\phi})$
   with variance $2 S_j$, and the Nyquist term contributes $S_{M/2}$. The
   samples are $x_k = \mu + \sigma\, y_k / \sqrt{V}$. If $V = 0$ (no
   frequency inside the band), realisation stops with an error.

Because the realisation is built from the whole segment at once, it is
periodic with period $M\Delta t$: the last sample connects smoothly to the
first. This is a known property of spectral synthesis, and it is the price
of an exact spectrum.
