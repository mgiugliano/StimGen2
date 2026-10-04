# The `.sgb` file

| Bytes | Content |
|:--|:--|
| 0–7 | `S` `G` `B` and five zero bytes |
| 8–15 | header length L, unsigned 64-bit little-endian |
| 16 … 16+L | JSON header, UTF-8, padded with spaces so that 16+L is a multiple of 8 |
| rest | samples: IEEE 754 float64, little-endian; all samples of channel 0, then channel 1, ... |

```python
import json, numpy as np
b = open("x.sgb", "rb").read()
L = int.from_bytes(b[8:16], "little")
h = json.loads(b[16:16 + L])
x = np.frombuffer(b[16 + L:], "<f8").reshape(len(h["channels"]), h["samples"])
t = np.arange(h["samples"]) / h["rate"]
```

## Header

```json
{
 "version": "2", "rate": 20000, "samples": 60000,
 "channels": [{"name": "Iinj", "unit": "pA", "rest": 0, "digital": false}],
 "markers":  [{"name": "Iinj.step", "sample": 10000}],
 "provenance": {
  "spec_version": "2", "implementation": "sg 0.2 (StimGen 2 reference renderer)",
  "repro_level": "B",
  "description": "### file: fi.sg\n...the stimulus text of this trial, $variables substituted...",
  "canonical_sha256": "...", "files": [], "rate": 20000,
  "master_seed": "4298691129025896212",
  "trial": {"index": 3, "condition": 3, "repetition": 0, "variables": {"amp": "150pA"},
            "protocol_seed": "7", "order": "sequential", "noise": "per-trial",
            "timing": "period 4s", "start": "immediately"},
  "protocol": "sg 2 protocol\n...the full protocol text...",
  "samples_sha256": "...", "warnings": [], "clipping": null,
  "timestamp": "2026-10-04T11:49:57Z"
 }
}
```

- `trial` and `protocol` are present only for trials of a protocol.
- `master_seed` and `protocol_seed` are **strings** (64-bit integers do not fit
  a JSON double).
- Marker names are `channel.label` for `@label` inside a channel, or the bare
  name for a stimulus-level `marker`. `sample` is the sample index.
- `files` lists data files read by `file(...)` with their SHA-256.
- Digital channels hold only 0 and 1.

## Regenerating

The description, the rate and the master seed determine the samples:

```
sg render -r <rate> -s <master_seed> stim.sg
```

This reproduces the file (equal `samples_sha256` on the same platform;
across platforms equal to within a few units in the last place of `sin`,
`exp` and `log`, which is "Level B" of the specification). For a protocol,
`sg render -s <protocol_seed>` reproduces the order and all the trial seeds.

The noise streams are Philox4x64-10, keyed by the first 16 bytes of
SHA-256(`"<master seed>:<address>"`), or of `"fixed:<n>"` for a generator with
`seed=n`. The address is the position in the text, e.g. `ch=Iinj/s1/o1`.
