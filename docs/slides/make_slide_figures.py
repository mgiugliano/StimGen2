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
make_slide_figures.py -- figures of the tutorial deck, as PNG with large fonts.

Reuses the figure functions of docs/figures/make_figures.py (every figure is
rendered by sg), with slide-sized text, and writes docs/slides/figs/*.png
plus figs.json (pixel sizes, used by build_deck.js to keep aspect ratios).
"""
import json, os, sys
HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "figures"))
import make_figures as mf                      # noqa: E402
import matplotlib.pyplot as plt               # noqa: E402
from PIL import Image                          # noqa: E402

OUT = os.path.join(HERE, "figs")
os.makedirs(OUT, exist_ok=True)
plt.rcParams.update({"font.size": 13, "axes.titlesize": 12, "axes.labelsize": 13,
                     "xtick.labelsize": 11, "ytick.labelsize": 11, "lines.linewidth": 1.4})
mf.W = 7.5                                     # inches; fonts look large on a slide


def save(fig, name):
    path = os.path.join(OUT, name.replace("fig-", "") + ".png")
    fig.savefig(path, dpi=200, bbox_inches="tight", transparent=False, facecolor="white")
    plt.close(fig)
    print("wrote", path)


mf.save = save
for f in (mf.fig_step, mf.fig_generators, mf.fig_noise, mf.fig_envelope,
          mf.fig_pair, mf.fig_steps, mf.fig_gallery):
    f()
sizes = {n[:-4]: Image.open(os.path.join(OUT, n)).size for n in sorted(os.listdir(OUT)) if n.endswith(".png")}
json.dump(sizes, open(os.path.join(OUT, "figs.json"), "w"), indent=1)
print(sizes)
