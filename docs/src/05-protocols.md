# Protocols: repetitions and sweeps {#sec:tut-protocols}

An experiment is a series of trials. A *protocol* file describes the
series: which stimulus is played, which of its parameters change from trial
to trial, how often each condition is repeated, in which order, and with
which timing.

## A family of current steps

The classic protocol for measuring the current–voltage relation and the
firing rate of a neuron is a family of current steps:

```
sg 2 protocol
sweep    amp = from -300pA to 300pA step 50pA
repeat   3
order    shuffled-blocks
period   5s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        @step
        1s    dc($amp)
        500ms dc(0)
    }
}
```

The `sweep` line defines a *variable*, `amp`, which takes thirteen values
from $-300$ to $300\,\text{pA}$. Inside the stimulus, `$amp` stands for the
current value. Each value is played three times (`repeat 3`), and the trials
start every 5 s (`period 5s`).

With `order shuffled-blocks`, the thirteen steps are played in a random
order, then again in another random order, and a third time in yet another
order. Each step is thus played once before any is played twice, and slow
drifts in the recording affect all amplitudes alike. The order is random but
reproducible: it is derived from the protocol seed, which is recorded with
the data. The other orders are `sequential`, `grouped`, and `shuffled`
(@sec:expansion). @Fig:steps shows the conditions and the order of the trials.

![The step protocol above, rendered by `sg render` (protocol seed 1). Left: the thirteen conditions. Right: the amplitude of each of the 39 trials in playing order; each block of thirteen trials (dotted lines) contains every amplitude once.](figures/fig-steps.pdf){#fig:steps}

## Several variables

With two `sweep` lines, every combination of values is played:

```
sweep amp  = [100pA, 200pA, 400pA]
sweep freq = [5Hz, 10Hz, 20Hz, 40Hz]
```

gives twelve conditions. A variable can also be computed from others with
`let`, for example to keep the total charge of a pulse constant:

```
sweep width  = [0.5ms, 1ms, 2ms]
let   amp    = 200pA * 1ms / width     # 400, 200, 100 pA
```

Variables can be used for any number in the stimulus, including durations,
as in `$width dc($amp)`, and `$( ... )` holds an expression, as in
`dc($(amp / 2))`. They exist
only in protocols; waveform and stimulus files never contain them, which
keeps those files simple to read.

## Frozen noise and reliability

To measure how reliably a neuron reproduces its spike times, the same noise
realisation is presented many times. With a protocol, this takes one line:

```
sg 2 protocol
repeat 50
gap    2s
noise  per-condition

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        10s   ou(mean=150, sd=80, tau=5ms)
        500ms dc(0)
    }
}
```

`noise per-condition` gives all repetitions of a condition the same
realisation. With the default, `noise per-trial`, every trial would receive
fresh noise. With several noise levels as a sweep, each level would have its
own frozen realisation. Here the trials are separated by a pause of 2 s
after each one ends (`gap 2s`), rather than starting at fixed intervals.

## Protocols and directories

A protocol can be *expanded* into a directory that contains one plain `.sg`
file per trial and an index file that lists them in order, with their seeds
and timing:

```
sg expand steps.sg steps/
```

Playing the directory is exactly the same as playing the protocol. The
directory is useful to inspect every single trial before an experiment, to
modify one by hand, or to play stimuli prepared by other means; any
directory of `.sg` files can be played as a protocol (@sec:directory-form).

Without protocol files, a family of steps would typically be produced by a
shell loop that writes one file per amplitude:

```
for amp in $(seq -300 50 300); do
    echo "500ms dc(0); 1s dc($amp); 500ms dc(0)" > steps/step_$amp.sg
done
```

This works, but the loop, the number of repetitions, the order, the
interval, and the seed then live outside the data and are easily lost. The
protocol file keeps them in one short text, stored with every recording.
