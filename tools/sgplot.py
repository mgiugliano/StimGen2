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
sgplot.py -- read and plot .sgb files written by sg (StimGen 2).

    python3 sgplot.py stim.sgb                 # show a plot
    python3 sgplot.py stim.sgb -o stim.pdf     # save it (pdf, png, svg)
    python3 sgplot.py stim.sgb -c I,N -t 0.5:1.5

As a module:
    from sgplot import load
    h, t, x = load("stim.sgb")   # header (dict), time (s), samples [channel, k]
"""
import argparse, json
import numpy as np


def load(path):
    """Return (header, time in s, samples as array [channel, sample])."""
    b = open(path, "rb").read()
    if b[:8] != b"SGB\0\0\0\0\0":
        raise ValueError(path + ": not a .sgb file")
    L = int.from_bytes(b[8:16], "little")              # header length
    h = json.loads(b[16:16 + L])                        # JSON header
    x = np.frombuffer(b[16 + L:], "<f8").reshape(len(h["channels"]), h["samples"])
    return h, np.arange(h["samples"]) / h["rate"], x


def plot(h, t, x, channels=None, tlim=None, ax=None):
    """Plot channels one under the other; markers as dashed lines."""
    import matplotlib.pyplot as plt
    names = [c["name"] for c in h["channels"]]
    sel = [names.index(c) for c in channels] if channels else range(len(names))
    if ax is None:
        _, ax = plt.subplots(len(sel), 1, sharex=True, squeeze=False,
                             figsize=(7, 1.6 + 1.2 * len(sel)))
        ax = ax[:, 0]
    for a, i in zip(ax, sel):
        c = h["channels"][i]
        a.plot(t, x[i], lw=0.8, color="k", drawstyle="steps-post" if c["digital"] else "default")
        a.set_ylabel("%s (%s)" % (c["name"], c["unit"]) if c["unit"] != "1" else c["name"])
        for m in h["markers"]:
            if m["name"].split(".")[0] in (c["name"], m["name"]):   # own or global marker
                a.axvline(m["sample"] / h["rate"], ls="--", lw=0.6, color="tab:red")
        a.spines[["top", "right"]].set_visible(False)
    ax[-1].set_xlabel("time (s)")
    if tlim:
        ax[-1].set_xlim(*tlim)
    return ax


def main():
    p = argparse.ArgumentParser(description="Plot a .sgb file written by sg.")
    p.add_argument("file")
    p.add_argument("-o", "--output", help="save to this file instead of showing")
    p.add_argument("-c", "--channels", help="comma-separated channel names")
    p.add_argument("-t", "--time", help="time window, e.g. 0.5:1.5 (s)")
    a = p.parse_args()
    import matplotlib
    if a.output:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    h, t, x = load(a.file)
    tl = tuple(float(v) for v in a.time.split(":")) if a.time else None
    plot(h, t, x, a.channels.split(",") if a.channels else None, tl)
    plt.tight_layout()
    plt.savefig(a.output) if a.output else plt.show()


if __name__ == "__main__":
    main()
