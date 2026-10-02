# The protocol layer {#sec:protocol}

An experiment rarely plays a single stimulus once. It repeats it, varies a
parameter across trials (a family of current steps, a set of frequencies),
shuffles the order to avoid slow drifts, and leaves time between trials for
the cell to recover. The protocol layer describes these things. It is the
only layer with variables, and it is defined entirely by how it *expands*
into a list of ordinary stimuli.

## A first protocol

```
sg 2 protocol
seed     2026
sweep    amp = from -300pA to 50pA step 50pA
repeat   2
order    shuffled
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

This protocol plays eight current steps, from $-300$ to $50\,\text{pA}$,
twice each, in a random but reproducible order, with one trial every 5 s.
Sixteen trials in total.

## Elements of a protocol

| Statement | Meaning | Default |
|:--|:--|:--|
| `stimulus { ... }` or `stimulus "file.sg"` | the stimulus template (required); inline, it may also be a plain waveform | — |
| `sweep name = values` | a variable and the values it takes | none |
| `let name = expression` | a derived variable | none |
| `repeat n` | number of repetitions of every condition | 1 |
| `order ...` | `sequential`, `grouped`, `shuffled`, or `shuffled-blocks` | `sequential` |
| `period t` or `gap t` | onset-to-onset time, or pause between trials | `gap 0s` |
| `seed n` | protocol seed | drawn and recorded |
| `noise ...` | `per-trial`, `per-condition`, or `fixed` | `per-trial` |
| `start ...` | `immediately` or `on trigger` | `immediately` |

: Statements of a protocol. {#tbl:protocol}

### Variables and substitution

A variable is defined by `sweep` (it takes a list of values) or by `let` (it
is computed from other variables). Inside the stimulus template, `$name`
stands for the value of a variable, and `$( expression )` for an arithmetic
expression of variables, e.g. `$(amp / 2)`. A substitution may appear
wherever a numeric literal may appear: an amplitude, a duration, a
frequency, or a `repeat` count. Values carry units. Within `let` and `$( ... )`, quantities may be
multiplied and divided, giving derived dimensions such as charge
(current × time), but every substituted value must have the dimension of
the field where it appears (@sec:units).

Substitution is textual. Each `$name` or `$( ... )` is replaced by the text
of its value before the template is parsed: a value listed in a sweep keeps
the form in which it was written (e.g. `-300pA`); a value of a range
(`from`, `linspace`, `logspace`) is written in the unit of its first value
(e.g. `-300pA`, `-250pA`, …, `0pA`); a computed value is written in
engineering notation with a unit from @tbl:unit-tokens (e.g. `400pA`). A
computed value whose dimension has no unit there, such as a charge, can be
used in other `let` expressions but cannot be substituted.

Variables exist only in protocols. A waveform or stimulus file never
contains `$`.

### Sweep values

The values of a sweep are written in one of these forms:

- a list: `[10Hz, 20Hz, 40Hz]`;
- an arithmetic range: `from A to B step S`, which includes $B$ if it is
  reached exactly. The values $A + iS$ are computed in exact decimal
  arithmetic, so that `from -0.3nA to 0.1nA step 0.1nA` gives exactly
  `-0.3nA`, `-0.2nA`, `-0.1nA`, `0nA`, `0.1nA`;
- `linspace(A, B, n)`: $n$ equally spaced values from $A$ to $B$ inclusive;
- `logspace(A, B, n)`: $n$ values equally spaced on a logarithmic scale;
- a list of tuples for variables that change together:
  `sweep (amp, dur) = [(100pA, 1s), (200pA, 500ms), (400pA, 250ms)]`.

### Conditions

With several `sweep` statements, the *conditions* are all combinations of
their values (the Cartesian product), enumerated with the first sweep
varying slowest. With no sweep there is one condition. The `let` variables
are then evaluated for each condition. For example,

```
sweep amp = [100pA, 200pA]
sweep dur = [250ms, 500ms]
let   charge = amp * dur
```

defines four conditions: (100, 250), (100, 500), (200, 250), (200, 500).

## Expansion semantics {#sec:expansion}

**Definition 7 (expansion).** The expansion of a protocol is an ordered list
of trials. Each trial consists of:

1. a *trial index* $j = 0, 1, \dots$ in playing order;
2. a *condition index* and a *repetition index*;
3. a fully substituted stimulus, with no variables left;
4. a master seed (@sec:protocol-seeds);
5. a start rule (`period`, `gap`, or trigger) relative to the previous
   trial.

Playing a protocol means playing its trials in order, each as an
independent stimulus. Expansion is deterministic. The same protocol file
and the same protocol seed always give the same list of trials.

### Ordering

Let $C$ be the number of conditions and $R$ the number of repetitions.

- `sequential`: for each repetition, all conditions in order. The list is
  $c_1, c_2, \dots, c_C, c_1, c_2, \dots$, i.e. the whole sweep is run, then
  run again.
- `grouped`: for each condition, all its repetitions in a row:
  $c_1, c_1, \dots, c_2, c_2, \dots$
- `shuffled`: a random permutation of all $C \times R$ trials.
- `shuffled-blocks`: $R$ blocks, each one a separate random permutation of
  the $C$ conditions. Every condition is played once before any is played
  twice, which balances slow drifts across conditions.

Random permutations are drawn with the Fisher–Yates algorithm, using the
stream whose key is derived as in @sec:keys from the string
`order:s`, where $s$ is the protocol seed. For a list of length $m$, for
$i = m-1$ down to 1, the element at position $i$ is exchanged with the one
at position $\lfloor U (i+1) \rfloor$, where $U$ is the next uniform variate
of the stream (@sec:variates). For `shuffled-blocks` the $R$ blocks are
permuted one after the other, from the same stream. Before shuffling, the
list is in `sequential` order.

### Timing between trials

- `period t`: each trial starts $t$ after the start of the previous one.
  If a trial is longer than $t$, the next one starts as soon as it ends, and
  the implementation MUST record the delay.
- `gap t`: each trial starts $t$ after the end of the previous one.
- `start on trigger`: each trial waits for an external trigger, after the
  `period` or `gap` has elapsed.

Between trials, every channel holds its rest value.

### Seeds of the trials {#sec:protocol-seeds}

The master seed of trial $j$, in condition $c$ and repetition $r$, is
derived from the protocol seed $s$ according to the `noise` statement:

| `noise` | Master seed of the trial is derived from | Effect |
|:--|:--|:--|
| `per-trial` | `s`, $j$ | every trial has a new realisation |
| `per-condition` | `s`, $c$ | repetitions of a condition share one realisation |
| `fixed` | `s` | all trials share one master seed |

: Noise policies of a protocol. {#tbl:noise-policy}

Trials, conditions and repetitions are numbered from 0. The protocol seed
$s$ is the one given by `seed`, unless the program that expands the protocol
is given another one (e.g. on the command line); without either, it is
drawn and recorded. The derived seed is the first 64 bits of the SHA-256 digest of the string
`trial:s:j`, `condition:s:c`, or `fixed:s`, i.e. its first 8 bytes read as a
big-endian unsigned integer.
Generators with an explicit `seed` are not affected (@sec:keys).

`noise per-condition` is the usual design for measuring the reliability of
spike timing: the same noise realisation is repeated, and the response
variability is attributed to the neuron.

## The directory form {#sec:directory-form}

A protocol can also be given as a directory:

```
steps/
    protocol.sgi
    0000.sg
    0001.sg
    ...
```

Each `.sg` file is one fully substituted stimulus, and the *index* file
`protocol.sgi` lists the trials in playing order, one per line, with their
start rule and master seed:

```
sg 2 index
period 5s
0000.sg  seed=13958223740118447071
0001.sg  seed=772146009915234880
...
```

If the index file is missing, the trials are the `.sg` files of the
directory in lexical order of their names, each played once, and the timing
comes from the command line.

When it writes a directory form, an implementation SHOULD copy into it the
files that the trials refer to by relative names (`use`, `file(...)`), so
that the directory is self-contained.

**Equivalence.** Every protocol expands into exactly one directory form, and
an implementation MUST be able to write it (for example with a command such
as `sg expand protocol.sg steps/`). Playing the directory gives the same
trials, samples, and seeds as playing the protocol. Conversely, any
directory of `.sg` files is a valid protocol. The two forms are two views of
the same object.

::: rationale
**Why a protocol file, and why also a directory.** A directory of stimulus
files traversed by a sequencer is a simple and robust way to run a family of
stimuli: what is played is exactly what is on disk, and each file can be
opened and checked. It has three weaknesses. The family is usually generated
by a shell loop, which lives outside the data and is easily lost. Shuffling
and its seed are left to the sequencer and are seldom recorded. And
relations between parameters (a duration that depends on an amplitude) are
hidden in the loop.

A protocol file keeps all of this in one short text that is stored with the
data. Defining it by its expansion means that nothing is lost: the directory
remains available, as an output of `expand` rather than of a hand-written
loop, and a user who prefers to prepare stimuli by other means can still
supply a directory. Variables are confined to the protocol layer, so that
waveforms stay free of control flow and remain easy to read.
:::

For comparison, the protocol at the beginning of this chapter replaces a
loop such as

```
for amp in $(seq -300 50 50); do
    echo "500ms dc(0); 1s dc($amp); 500ms dc(0)" > steps/step_$amp.sg
done
```

followed by a sequencer invocation that must be told separately how many
times to repeat, in which order, at which interval, and with which seed.
