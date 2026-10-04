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
sgb_plot.py -- plot .sgb files (StimGen 2 samples) to an image file.

    python3 sgb_plot.py stim.sgb                       # writes stim.png
    python3 sgb_plot.py stim.sgb -o stim.pdf -t 0.4:1.2 -c Iinj
    python3 sgb_plot.py fi_sgb/ -o fi.png              # trials of a protocol, overlaid
    python3 sgb_plot.py fi_sgb/ --stack -n 6           # the first 6 trials, one under the other

Each channel gets its own axis (digital channels are drawn as steps);
markers are dashed red lines. Works headless (matplotlib Agg backend), so
it is safe to call from scripts and agents; open the image to look at it.
"""
import os, sys
import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sgconvert import load   # noqa: E402


def label(c):
    return c["name"] if c["unit"] in ("1", "") else f"{c['name']} ({c['unit']})"


def plot_files(files, out, channels=None, tlim=None, stack=False, maxn=8, title=None):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    data = [load(f) for f in files[:maxn]]
    h0 = data[0][0]
    names = [c["name"] for c in h0["channels"]]
    sel = [names.index(c) for c in channels] if channels else list(range(len(names)))
    rows = len(sel) * (len(data) if stack else 1)
    fig, axes = plt.subplots(rows, 1, sharex=True, squeeze=False, figsize=(8, 1.2 + 1.3 * rows))
    axes = axes[:, 0]
    cmap = plt.get_cmap("viridis")
    for j, (h, x) in enumerate(data):
        t = np.arange(h["samples"]) / h["rate"]
        color = "k" if len(data) == 1 else cmap(j / max(1, len(data) - 1))
        tr = (h["provenance"].get("trial") or {})
        tag = ", ".join(f"{k}={v}" for k, v in tr.get("variables", {}).items()) or os.path.basename(files[j])
        for k, i in enumerate(sel):
            ax = axes[j * len(sel) + k] if stack else axes[k]
            c = h["channels"][i]
            ax.plot(t, x[i], lw=0.8, color=color, label=tag,
                    drawstyle="steps-post" if c["digital"] else "default")
            ax.set_ylabel(label(c))
            if stack:
                ax.set_title(tag, fontsize=8, loc="left")
            if j == 0 or stack:
                for m in h["markers"]:
                    if m["name"].split(".")[0] in (c["name"], m["name"]):
                        ax.axvline(m["sample"] / h["rate"], ls="--", lw=0.6, color="tab:red")
    for ax in axes:
        ax.spines[["top", "right"]].set_visible(False)
    if len(data) > 1 and not stack:
        axes[0].legend(fontsize=7, frameon=False, loc="upper left", bbox_to_anchor=(1.01, 1))
    axes[-1].set_xlabel("time (s)")
    if tlim:
        axes[-1].set_xlim(*tlim)
    if title:
        fig.suptitle(title, fontsize=10)
    fig.tight_layout()
    fig.savefig(out, dpi=130)
    plt.close(fig)
    return out


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Plot .sgb files to an image (png, pdf, svg).")
    ap.add_argument("target", help=".sgb file or directory of trials")
    ap.add_argument("-o", "--output", help="image file (default: <target>.png)")
    ap.add_argument("-c", "--channels", help="comma-separated channel names")
    ap.add_argument("-t", "--time", help="time window in s, e.g. 0.5:1.5")
    ap.add_argument("-n", "--max", type=int, default=8, help="at most this many trials (default 8)")
    ap.add_argument("--stack", action="store_true", help="one axis per trial instead of overlaying")
    ap.add_argument("--title")
    a = ap.parse_args()
    if os.path.isdir(a.target):
        files = sorted(os.path.join(a.target, f) for f in os.listdir(a.target) if f.endswith(".sgb"))
    else:
        files = [a.target]
    if not files:
        sys.exit(a.target + ": no .sgb files")
    out = a.output or os.path.splitext(a.target.rstrip("/"))[0] + ".png"
    tl = tuple(float(v) for v in a.time.split(":")) if a.time else None
    plot_files(files, out, a.channels.split(",") if a.channels else None, tl, a.stack, a.max, a.title)
    print("wrote", out)


if __name__ == "__main__":
    main()
