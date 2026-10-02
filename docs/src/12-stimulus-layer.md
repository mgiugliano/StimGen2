# The stimulus layer {#sec:stimulus}

A *stimulus* is what one trial of an experiment plays: several waveforms on
named output channels, sampled on a common grid, together with markers that
label instants of interest. This chapter defines the stimulus layer. Its
text syntax is summarised here and given formally in @sec:syntax.

## Definition

**Definition 6 (stimulus).** A stimulus is a tuple
$$
S = \big(\{(c_j, U_j, r_j, w_j)\}_{j=1}^{J},\ M,\ f_s^{\star},\ s^{\star}\big),
$$
where, for each channel $j$, $c_j$ is a unique name, $U_j$ a physical unit,
$r_j$ a rest value, and $w_j$ a waveform; $M$ is a set of markers;
$f_s^{\star}$ is an optional sampling rate; and $s^{\star}$ is an optional
master seed.

A stimulus file looks like this:

```
sg 2 stimulus
rate 20kHz
seed 4711

channel Iinj unit=pA rest=0 {
    1s     dc(0)
    @on
    500ms  dc(300)
    1s     dc(0)
}

channel Vcmd unit=mV rest=-70 {
    2.5s   dc(-70)
}

marker sync at 0s, 1s
```

## Channels {#sec:channels}

A channel is a *logical* output. Its name identifies it in the description,
in the output files, and in the addresses of its random streams
(@sec:keys). A channel has:

- a **unit** $U_j$, such as `pA`, `nA`, `mV`, `nS`, or `V` (required). The
  samples of the channel are numbers in this unit;
- a **rest value** $r_j$ (default 0), the value the output holds before the
  first sample, after the last one, and between trials;
- a **waveform** $w_j$, written inline in braces or taken from a file with
  `use "name.sg"`.

The stimulus does not say which physical output, D/A converter, or
amplifier gain corresponds to a channel. That binding belongs to the
configuration of the set-up, outside StimGen 2, so that the same `.sg` file
can be played on different rigs. The gain $\gamma$ used in
@eq:quantisation comes from that binding.

### Units and dimensional checks

Each unit has a dimension: current (`A` with an SI prefix), voltage (`V`),
conductance (`S`), or dimensionless (`1`). If an amplitude literal in the
waveform of a channel carries a unit (e.g. `dc(0.3nA)` on a channel in
`pA`), it MUST have the dimension of the channel, and it is converted to the
channel unit (here, 300). A literal of another dimension is an error.
Literals without a unit are taken in the channel unit.

### Copies of a channel

The construct `channel B unit=pA copy A` gives channel B *the same samples*
as channel A, noise included. In contrast, two channels that `use` the same
waveform file receive different noise realisations, because their stochastic
instances have different addresses (@sec:keys). Both behaviours are useful,
and the description states which one is meant.

### Digital channels

A channel declared with `digital NAME { ... }` drives a digital output line,
for example a TTL trigger for a camera or a light source. Its waveform is
written like any other, but every realised sample MUST be exactly 0 or 1;
any other value is an error. Digital channels have no unit, and their rest
value is 0 unless stated.

## Duration and alignment {#sec:alignment}

All channels share time zero and the sampling grid $t_k = k\,\Delta t$. Each
channel waveform is sampled with the rules of @sec:sampling, independently
of the others. The duration of the stimulus is the longest channel duration,
$$
T_S = \max_j T_{w_j},
$$
or the value given by an explicit `duration` statement, which MUST NOT be
shorter than any channel. A channel that ends earlier holds its rest value
until $T_S$. An implementation SHOULD warn when channel durations differ
and no explicit `duration` is given, since this is more often an oversight
than an intention.

The sampling grid is common to all channels. Different sampling rates for
different channels are not supported.

## Markers {#sec:markers}

A *marker* is a named instant within the stimulus. Markers do not change any
output. They are written to the output files with the realised data, so that
analysis software can align responses to stimulus events without having to
re-derive them (@sec:output). Markers come from two sources.

1. **Segment labels.** A line `@name` inside a waveform labels the start of
   the next segment. The marker time is the realised start of that segment,
   $n_i\,\Delta t$. Labels must be unique within a channel, and the marker
   is reported as `channel.name` (e.g. `Iinj.on`).
2. **Explicit markers.** The statement `marker NAME at t1, t2, ...` places
   markers at the given times, each rounded to the nearest sample with the
   rounding of @eq:boundary.

A digital channel can be used to send a marker to external equipment; a
marker is only a label in the data.

## Sampling rate and seed

- `rate` states the sampling rate the stimulus requires. If it is present,
  the implementation MUST use it, or refuse to play the stimulus if the
  hardware cannot. If it is absent, the rate comes from the configuration of
  the set-up or from the command line. The rate actually used is always
  recorded.
- `seed` sets the master seed of the stimulus (@sec:keys). A protocol may
  override it for each trial (@sec:protocol-seeds). If no seed is given
  anywhere, one is drawn and recorded.

## A waveform is also a stimulus

A file or command-line text that contains only a waveform is accepted
wherever a stimulus is expected. It is played as a stimulus with one channel,
whose name, unit, and rest value come from the configuration (by default:
name `out`, the unit configured for the default output, rest value 0). This
keeps the simplest case simple.
