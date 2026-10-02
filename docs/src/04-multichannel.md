# Multichannel stimuli and markers {#sec:tut-multichannel}

Many experiments drive more than one output at a time: two cells recorded
in a pair, a current command together with a light pulse, a stimulus
together with a trigger for a camera. A *stimulus* file describes all
outputs of one trial on a common time base.

## Two outputs

This stimulus tests for a synaptic connection between two neurons. A train
of brief pulses is injected into the first neuron, while the second is held
at a constant current:

```
sg 2 stimulus

channel pre unit=pA {
    1s     dc(0)
    @train
    500ms  pulses(amp=2000, rate=20Hz, width=1ms)
    1s     dc(0)
}

channel post unit=pA {
    2.5s   dc(-50)
}
```

The first line, the *header*, states that the file holds a stimulus. Each
`channel` has a name, a unit, and a waveform in braces. Both waveforms start
at time zero and are sampled on the same grid. Here both last 2.5 s; if they
did not, the shorter one would hold its *rest value* (0 unless stated) until
the end, and StimGen 2 would warn.

Which physical output corresponds to `pre` and `post`, and with which
amplifier gain, is not written in the file. It belongs to the configuration
of the set-up, so the same file works on any rig (@sec:channels).

![The two channels of the pair stimulus, on a common time base. The dashed line is the marker `pre.train`, produced by the label `@train`.](figures/fig-pair.pdf){#fig:pair}

## Markers

The line `@train` *labels* the next segment. When the stimulus is played,
StimGen 2 stores a marker named `pre.train` with the data, at the sample
where the pulse train begins. Analysis scripts can then align responses to
this marker instead of recomputing where the train started.

Markers can also be placed at explicit times:

```
marker flash at 1s, 1.5s
```

## Digital outputs

A digital output drives a trigger line, for instance to open a shutter or to
start a camera. It is declared with `digital`, and its waveform must take
only the values 0 and 1:

```
sg 2 stimulus
rate 20kHz

channel Iinj unit=pA {
    1s  dc(0)
    2s  ou(mean=150, sd=60, tau=5ms)
    1s  dc(0)
}

digital camera {
    1s  dc(0)
    2s  pulses(amp=1, rate=100Hz, width=1ms)    # one frame trigger every 10 ms
    1s  dc(0)
}
```

This file also fixes the sampling rate with `rate 20kHz`. Without that line,
the rate of the set-up is used.

## Same file, same noise?

Two channels may use the same waveform file:

```
channel A unit=pA use "noise.sg"
channel B unit=pA use "noise.sg"
```

Each channel then gets its *own* noise realisation, with the same
statistics, because the random streams depend on the channel. To give two
channels exactly the same samples, noise included, write

```
channel A unit=pA use "noise.sg"
channel B unit=pA copy A
```

Both behaviours are useful, and the file states which one is meant.
