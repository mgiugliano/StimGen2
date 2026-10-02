# Output and provenance {#sec:output}

A description is short, but the samples it produces are what reach the
preparation. This chapter specifies how realised samples are stored, and
which information MUST accompany them so that every stimulus that was ever
played can be identified and regenerated exactly.

## Realised sample files {#sec:sgb}

An implementation that writes realised samples to a file for exchange MUST
support the format described here, with the extension `.sgb` (StimGen
binary). It is deliberately simple and portable.

| Offset | Size (bytes) | Content |
|:--|:--|:--|
| 0 | 8 | magic bytes `SGB` followed by five zero bytes |
| 8 | 8 | length $L$ of the header, unsigned 64-bit integer, little-endian |
| 16 | $L$ | header: a JSON object, UTF-8, padded with spaces so that $L$ is a multiple of 8 |
| $16 + L$ | $8 \times J \times N$ | samples: IEEE 754 binary64, little-endian, channel after channel |

: Layout of a `.sgb` file. {#tbl:sgb}

The JSON header contains at least:

- `"version"`: the version of this specification (`"2"`);
- `"rate"`: the sampling rate in Hz;
- `"samples"`: the number of samples per channel, $N$;
- `"channels"`: a list of objects with `"name"`, `"unit"`, `"rest"`, and
  `"digital"` (true or false), in the order of the sample blocks;
- `"markers"`: a list of objects with `"name"` and `"sample"` (the sample
  index);
- `"provenance"`: the provenance record (@sec:provenance).

Because $L$ is a multiple of 8, the samples start on an 8-byte boundary, so
that a reader can map them directly into memory as an array of doubles
(several languages require this alignment). Trailing spaces are allowed
by JSON, so a reader that parses the whole header is not affected.

All channels have the same number of samples. The samples of channel 1
come first ($N$ values), then those of channel 2, and so on. All multi-byte
values are little-endian, and all sizes are 64-bit, so a file written on one
machine is read identically on any other.

::: note
Recording software usually stores data in its own container, often HDF5.
There, the realised stimulus SHOULD be stored as one dataset per channel,
with the fields of the JSON header (and in particular the provenance record)
as attributes. The `.sgb` format is meant for exchange and for testing
implementations against each other.
:::

## The provenance record {#sec:provenance}

Every realisation, whether written to a file, played on hardware, or both,
MUST produce a provenance record with the following fields.

| Field | Content |
|:--|:--|
| `spec_version` | version of this specification |
| `implementation` | name and version of the implementation |
| `repro_level` | `A` or `B` (@sec:repro-levels) |
| `description` | the full text of the description as given, with every file it uses (`use`, `stimulus "..."`) included verbatim |
| `canonical_sha256` | SHA-256 digest of the canonical form (@sec:syntax) |
| `files` | for each `file(...)` primitive: path and SHA-256 digest of the data file |
| `rate` | sampling rate actually used (Hz) |
| `master_seed` | master seed actually used, whether given or drawn |
| `trial` | for protocols: trial, condition, and repetition indices, the substituted variable values, the protocol seed, order, noise policy, and timing |
| `protocol` | for protocols: the full text of the protocol (or of the index file) |
| `samples_sha256` | SHA-256 digest of the sample blocks, as laid out in @tbl:sgb |
| `warnings` | warnings issued, e.g. empty segments or frequencies above Nyquist |
| `clipping` | number and positions of clipped samples (@sec:quantisation), if known |
| `timestamp` | date and time of realisation (ISO 8601, UTC) |

: Fields of the provenance record. {#tbl:provenance}

The record is a JSON object. A record for a protocol trial is a record for
the stimulus of that trial, whose `description` is the fully substituted
stimulus text, with the fields `trial` and `protocol` added. A single trial
can therefore be regenerated from its own record, without the protocol.

**Regeneration.** Given the fields `description`, `rate`, and `master_seed`,
any conforming implementation regenerates the realisation. The digest
`samples_sha256` lets one verify the result: equality is guaranteed at
Level A, and at Level B the samples agree within the tolerance of
@sec:repro-levels.

::: rationale
Recording the description and the seed costs a few hundred bytes per trial
and makes the stimulus of every recorded sweep recoverable years later, even
if the original files are lost. The digest of the samples protects against
the opposite problem, an implementation that changes its output between
versions without anyone noticing.
:::

## Errors and warnings

Errors stop realisation, and no samples are played or written. They are:
syntax errors, failed static checks (@sec:static-checks), non-finite samples
(Rule 4), invalid values on digital channels, unreadable files, and a
required sampling rate that the hardware cannot provide.

Warnings do not stop realisation, and they are recorded in the provenance.
They include: a segment of positive duration with no sample; a frequency
above $f_s/2$; a pulse narrower than $\Delta t$; channels of different
durations without an explicit `duration`; a trial that could not start at
its `period`; and clipped samples.
