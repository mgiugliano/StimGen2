#!/usr/bin/env python3
# StimGen 2 -- SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 Michele Giugliano
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

"""
sgb_describe.py -- describe .sgb files (StimGen 2 samples) in words or as JSON.

    python3 sgb_describe.py stim.sgb             # a paragraph per channel, then the description
    python3 sgb_describe.py fi_sgb/              # every trial of a rendered protocol, briefly
    python3 sgb_describe.py stim.sgb --json      # machine-readable summary

No plotting library is needed: only numpy. The text form is the one of
`sgconvert.py info`; the JSON form holds, per file: rate, samples, duration,
master seed, protocol trial (index, condition, repetition, variables),
markers, warnings, and per channel the unit, min/max/mean/SD, and either the
constant levels (start, end, value) or a dominant frequency.
"""
import json, os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sgconvert import load, describe, runs, dominant_frequency   # noqa: E402


def summary(path):
    h, x = load(path)
    fs, p = h["rate"], h["provenance"]
    s = {"file": path, "rate_hz": fs, "samples": h["samples"], "duration_s": h["samples"] / fs,
         "master_seed": p["master_seed"], "implementation": p.get("implementation"),
         "trial": p.get("trial"), "warnings": p.get("warnings", []),
         "markers": [{"name": m["name"], "time_s": m["sample"] / fs} for m in h["markers"]],
         "channels": []}
    for c, row in zip(h["channels"], x):
        ch = {"name": c["name"], "unit": c["unit"], "digital": c["digital"], "rest": c["rest"],
              "min": float(row.min()), "max": float(row.max()),
              "mean": float(row.mean()), "sd": float(row.std())}
        r = runs(row, 1e-9 * max(1e-12, float(row.max() - row.min())))
        if r is not None:
            ch["levels"] = [{"start_s": k0 / fs, "end_s": k1 / fs, "value": float(v)} for k0, k1, v in r]
        else:
            d = dominant_frequency(row, fs)
            ch["dominant_frequency_hz"], ch["dominant_power_fraction"] = (d if d else (None, None))
        s["channels"].append(ch)
    s["description"] = p["description"]
    return s


def brief(path):
    """One line per trial file, for directories."""
    s = summary(path)
    t = s["trial"] or {}
    var = ", ".join(f"{k}={v}" for k, v in t.get("variables", {}).items())
    chans = "; ".join(f"{c['name']} {c['min']:.4g}..{c['max']:.4g} {c['unit']}" for c in s["channels"])
    head = f"trial {t['index']} (cond {t.get('condition')}, rep {t.get('repetition')})" if t else "file"
    return f"{os.path.basename(path)}: {head}{': ' + var if var else ''}; {s['duration_s']:g} s; {chans}"


def files_of(target):
    if os.path.isdir(target):
        return sorted(os.path.join(target, f) for f in os.listdir(target) if f.endswith(".sgb"))
    return [target]


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Describe .sgb files in words or as JSON.")
    ap.add_argument("target", help=".sgb file or a directory of trials")
    ap.add_argument("--json", action="store_true", help="print a JSON summary")
    a = ap.parse_args()
    files = files_of(a.target)
    if not files:
        sys.exit(a.target + ": no .sgb files")
    if a.json:
        out = [summary(f) for f in files]
        print(json.dumps(out if os.path.isdir(a.target) else out[0], indent=1))
    elif len(files) == 1 and not os.path.isdir(a.target):
        h, x = load(files[0])
        print(describe(h, x))
    else:
        print(f"{len(files)} trial file(s) in {a.target}:")
        for f in files:
            print("  " + brief(f))


if __name__ == "__main__":
    main()
