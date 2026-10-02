\appendix

# Glossary {#sec:app-glossary}

Address
:   The position of a stochastic generator instance in a stimulus, written as
    a path such as `ch=Iinj/s1/o1`. It determines the random stream of the
    instance unless a seed is given (@sec:keys).

Block
:   A sequence of segments in braces, `{ ... }`. A block is a timed waveform
    and can be used as an operand (@sec:expressions).

Canonical form
:   A normalised text of a description, with all arguments named and all
    quantities in canonical units. Two descriptions with the same canonical
    form have the same meaning (@sec:syntax).

Channel
:   A named logical output of a stimulus, with a unit, a rest value, and a
    waveform (@sec:channels).

Concatenation
:   Joining timed waveforms one after the other in time, written with `;`
    or a line break (@sec:concatenation).

Condition
:   One combination of the values of the sweep variables of a protocol
    (@sec:protocol).

Digital channel
:   A channel whose samples must be 0 or 1, used for trigger and TTL outputs
    (@sec:channels).

Directory form
:   A protocol given as a directory of fully substituted `.sg` files and an
    index file. It is equivalent to the protocol it was expanded from
    (@sec:directory-form).

Duration-aware
:   A generator whose values depend on the duration of its segment, such as
    `ramp`, `chirp`, or `cnoise` (@sec:generators).

End value
:   The value a segment would take at the first sample after it, for a
    deterministic segment, or its last sample, for a stochastic one. The
    keyword `prev` refers to it (@sec:end-value, @sec:algebra-prev).

Expansion
:   The deterministic conversion of a protocol into an ordered list of trials
    (@sec:expansion).

Frozen noise
:   A noise realisation that is repeated identically, obtained with an
    explicit `seed` or with the protocol policy `noise per-condition`
    (@sec:keys, @sec:protocol-seeds).

Generator
:   A rule that gives a value for every instant of a segment once its
    duration is known. A generator has no duration of its own
    (@sec:generators).

Grid-aligned local time
:   The local time at which a sample is evaluated,
    $u_k = (k - n_i)\Delta t$, so that every segment starts exactly on a
    sample (@sec:local-time).

Header
:   The first line of a `.sg` file, e.g. `sg 2 stimulus`, stating the
    version and the kind of description (@sec:syntax).

Index file
:   The file `protocol.sgi` of a directory form, listing the trials in
    playing order with their seeds and timing (@sec:directory-form).

Label
:   A name written `@name` before a segment. It produces a marker at the
    start of that segment (@sec:markers).

Local time
:   Time measured from the start of the current segment, $u$.

Marker
:   A named instant in a stimulus, stored with the data to help analysis
    (@sec:markers).

Master seed
:   The integer from which the random streams of a stimulus are derived
    (@sec:keys).

Primitive
:   One of the built-in generators of @sec:primitives, such as `sine` or
    `ou`.

Protocol
:   A description of a set of trials: a stimulus template, sweeps,
    repetitions, ordering, and timing (@sec:protocol).

Provenance record
:   The information stored with every realisation that allows it to be
    identified and regenerated (@sec:provenance).

Quantity
:   A number with a unit suffix, such as `500ms` or `-300pA`
    (@sec:lexical).

Realisation
:   The definite sequence of samples obtained from a description for a given
    sampling rate and master seed (@sec:semantics).

Realised duration
:   The duration of a segment after sampling, $\hat T_i = N_i \Delta t$
    (@sec:local-time).

Rest value
:   The value a channel holds before and after its waveform and between
    trials (@sec:channels).

Segment
:   A timed waveform that is one element of a sequence; usually written as a
    duration followed by an expression (@sec:timed).

Sequence
:   Segments joined by concatenation (@sec:concatenation).

Stimulus
:   Waveforms on named channels with a common time base, plus markers: what
    one trial plays (@sec:stimulus).

Stream
:   The sequence of random 64-bit words used by one stochastic generator
    instance (@sec:stochastic).

Sweep
:   A protocol variable together with the list of values it takes
    (@sec:protocol).

Timed waveform
:   A function of time on an interval of known duration (@sec:timed).

Trial
:   One playing of a stimulus within a protocol (@sec:expansion).

Unary map
:   A function applied to every sample of a waveform, such as `abs` or
    `clip` (@sec:maps).

Waveform
:   The description of the values of one channel during one trial: a
    sequence of segments (@sec:layers).
