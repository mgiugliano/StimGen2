# Combining and transforming waveforms {#sec:tut-combine}

Elementary generators become much more useful when they are combined. This
chapter shows how to add and multiply them, how to transform them, and how
to build larger waveforms from smaller ones.

## Arithmetic on generators

Within a segment, generators can be combined with `+`, `-`, `*`, and `/`,
with the usual precedence and parentheses:

```
5s  sine(amp=50, freq=8Hz) + ou(mean=0, sd=20, tau=5ms)   # oscillation in noise
5s  100 + sine(amp=50, freq=8Hz)                           # sine with an offset
5s  (1 + 0.5*sine(1, 2Hz)) * sine(amp=80, freq=40Hz)       # amplitude modulation
5s  ou(mean=0, sd=1, tau=5ms) * (20 + 10*sine(1, 1Hz))     # noise of varying size
```

The operations act sample by sample. A number such as `100` or `0.5` is a
constant generator, so a waveform is scaled by multiplying it by a number.

![The four segments above, first two seconds of each.](figures/fig-combine.pdf){#fig:combine}

## Transformations

Functions such as `abs`, `pos`, `clip`, and `pow` transform a waveform
sample by sample (@tbl:maps):

```
5s  abs(sine(4, 1Hz))                         # full-wave rectified sine
5s  pos(sine(4, 1Hz))                         # half-wave rectified sine
10s pos(ou(mean=10, sd=5, tau=3ms))           # a conductance that is never negative
10s clip(ou(mean=0, sd=100, tau=5ms), -150, 150)
```

The positive part `pos` is handy in dynamic clamp, where a conductance must
not become negative. Operations that are not defined, such as the square
root of a negative value or a division by zero, stop the realisation with
an error that names the segment and the sample. A stimulus is never
silently altered.

## Ramps that continue

A ramp needs a start and an end. Often the start is simply where the
previous segment ended, and the keyword `prev` says so:

```
1s   dc(-50)
2s   ramp(from=prev, to=200)     # from -50 to 200
1s   ramp(from=prev, to=0)       # back to 0
```

Since `from=prev` is the default of `ramp`, the second line can be shortened
to `2s ramp(to=200)`. We recommend writing `from` explicitly whenever the
ramp does not continue from the previous segment.

## Blocks: waveforms inside waveforms

A sequence of segments in braces is a *block*. A block has a duration, the
sum of its segments, and can be used inside an expression. This is how an
envelope is applied to a carrier:

```
{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(amp=80, freq=8Hz)
```

The block lasts 5 s and describes a trapezoidal envelope. The sine has no
duration of its own, so it takes the duration of the block and is
multiplied by it, sample by sample. The result is a 5 s oscillation that
fades in and out.

Two blocks combined in this way must have the same duration. Otherwise
StimGen 2 reports an error before playing anything (@sec:duration-rules).

![A block as an envelope. Top: the block alone; middle: the carrier; bottom: their product, as written in the text.](figures/fig-envelope.pdf){#fig:envelope}

## Repeating a part of a waveform

A part of a waveform can be repeated with `repeat`:

```
1s  dc(0)
repeat 10 { 2ms dc(1000) ; 48ms dc(0) }    # ten 2 ms pulses at 20 Hz
1s  dc(0)
```

If the repeated part contains noise, each copy gets a different realisation,
unless a seed is given. Note that `repeat` repeats a part of *one*
waveform. Repeating a whole trial, with pauses in between, is done by a
protocol (@sec:tut-protocols).

## A larger example

The following waveform measures the response of a neuron to steps of
increasing size, each embedded in background noise, inside a single trial:

```
# staircase in noise
1s   dc(0)
{
    2s dc(100) ; 2s dc(200) ; 2s dc(300)
} + ou(mean=0, sd=30, tau=5ms, seed=3)
1s   dc(0)
```

The noise is added to the whole staircase at once and has a fixed seed, so
every presentation of this file uses the same noise.
