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
make_figures.py -- every figure of the StimGen 2 document.

Each figure is made by rendering the StimGen 2 text printed in the
document with sg (src/sg) and reading the .sgb file with tools/sgplot.py.
Run from anywhere:  python3 docs/figures/make_figures.py
"""
import os, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SG = os.path.join(ROOT, "src", "sg")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from sgplot import load, plot  # noqa: E402

TMP = tempfile.mkdtemp(prefix="sgfig")
plt.rcParams.update({"font.size": 8, "axes.titlesize": 8, "axes.labelsize": 8,
                     "lines.linewidth": 0.8, "axes.spines.top": False,
                     "axes.spines.right": False, "figure.dpi": 150})
W = 6.3                                    # text width in inches


def render(text, rate="20kHz", seed="1", unit="pA", files=None):
    """Render StimGen 2 text with sg; return (header, t, x)."""
    for name, content in (files or {}).items():
        open(os.path.join(TMP, name), "w").write(content)
    src = os.path.join(TMP, "fig.sg")
    open(src, "w").write(text)
    out = os.path.join(TMP, "fig.sgb")
    subprocess.run([SG, "render", "-q", "-r", rate, "-s", seed, "-u", unit, "-o", out, src],
                   check=True)
    return load(out)


def save(fig, name):
    fig.savefig(os.path.join(HERE, name + ".pdf"), bbox_inches="tight")
    plt.close(fig)
    print("wrote", name + ".pdf")


def panel(ax, t, y, title=None, ylabel=None, **kw):
    ax.plot(t, y, color=kw.pop("color", "k"), **kw)
    if title: ax.set_title(title, loc="left", family="monospace")
    if ylabel: ax.set_ylabel(ylabel)


# ------------------------------------------------------------------ Part I
def fig_step():
    text = "500ms  dc(0)\n1s     dc(300)\n500ms  dc(0)\n"
    h, t, x = render(text)
    fig, ax = plt.subplots(figsize=(W, 1.6))
    panel(ax, t, x[0], ylabel="current (pA)")
    for b in (0.5, 1.5):
        ax.axvline(b, ls=":", color="0.5")
    for c, lab in ((0.25, "500ms dc(0)"), (1.0, "1s dc(300)"), (1.75, "500ms dc(0)")):
        ax.text(c, 330, lab, ha="center", family="monospace", fontsize=7)
    ax.set_ylim(-30, 380); ax.set_xlabel("time (s)")
    save(fig, "fig-step")


def fig_generators():
    lines = ["2s    ramp(from=0, to=500)",
             "5s    square(amp=100, freq=2Hz, duty=25%)",
             "10s   chirp(amp=50, f0=0.5Hz, f1=20Hz)",
             "200ms biexp(amp=30, tau_rise=1ms, tau_decay=8ms)"]
    fig, axs = plt.subplots(2, 2, figsize=(W, 3.2))
    for ax, line in zip(axs.flat, lines):
        h, t, x = render(line + "\n")
        panel(ax, t, x[0], title=line.split(None, 1)[1].strip())
        ax.set_xlabel("time (s)")
    fig.tight_layout()
    save(fig, "fig-generators")


def fig_noise():
    text = "1s   dc(0)\n10s  ou(mean=100, sd=50, tau=5ms)\n1s   dc(0)\n"
    fig, axs = plt.subplots(2, 1, figsize=(W, 3.4), sharex=True)
    for i, seed in enumerate(("1", "2", "3")):
        h, t, x = render(text, seed=seed)
        axs[0].plot(t, x[0] + 300 * i, color=("k", "tab:blue", "tab:orange")[i],
                    label="master seed %s" % seed)
    axs[0].set_title("three playings without a seed: new realisation each time", loc="left")
    axs[0].set_yticks([]); axs[0].set_ylabel("current (offset)")
    fixed = "1s   dc(0)\n10s  ou(mean=100, sd=50, tau=5ms, seed=17)\n1s   dc(0)\n"
    h, t, a = render(fixed, seed="1")
    h, t, b = render(fixed, seed="999")
    axs[1].plot(t, a[0], color="k", label="master seed 1")
    axs[1].plot(t, b[0], color="tab:red", ls="--", label="master seed 999")
    axs[1].set_title("with seed=17: the same samples, whatever the master seed (max diff %g)"
                     % np.max(np.abs(a - b)), loc="left")
    axs[1].legend(frameon=False, loc="upper right", fontsize=7)
    axs[1].set_ylabel("current (pA)"); axs[1].set_xlabel("time (s)")
    axs[1].set_xlim(0.8, 1.6)
    fig.tight_layout()
    save(fig, "fig-noise")


def fig_combine():
    lines = ["5s  sine(amp=50, freq=8Hz) + ou(mean=0, sd=20, tau=5ms)",
             "5s  100 + sine(amp=50, freq=8Hz)",
             "5s  (1 + 0.5*sine(1, 2Hz)) * sine(amp=80, freq=40Hz)",
             "5s  ou(mean=0, sd=1, tau=5ms) * (20 + 10*sine(1, 1Hz))"]
    fig, axs = plt.subplots(4, 1, figsize=(W, 4.6), sharex=True)
    for ax, line in zip(axs, lines):
        h, t, x = render(line + "\n")
        panel(ax, t, x[0], title=line[4:])
        ax.set_xlim(0, 2)
    axs[-1].set_xlabel("time (s)")
    fig.tight_layout()
    save(fig, "fig-combine")


def fig_envelope():
    env = "{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) }"
    car = "sine(amp=80, freq=8Hz)"
    fig, axs = plt.subplots(3, 1, figsize=(W, 3.4), sharex=True)
    for ax, text in zip(axs, (env, "5s " + car, env + " * " + car)):
        h, t, x = render(text + "\n")
        panel(ax, t, x[0], title=text)
    axs[-1].set_xlabel("time (s)")
    fig.tight_layout()
    save(fig, "fig-envelope")


def fig_pair():
    text = """sg 2 stimulus

channel pre unit=pA {
    1s     dc(0)
    @train
    500ms  pulses(amp=2000, rate=20Hz, width=1ms)
    1s     dc(0)
}

channel post unit=pA {
    2.5s   dc(-50)
}
"""
    h, t, x = render(text)
    fig, axs = plt.subplots(2, 1, figsize=(W, 2.6), sharex=True)
    plot(h, t, x, ax=axs)
    axs[0].text(1.0, 2050, " marker pre.train", color="tab:red", fontsize=7)
    axs[1].set_ylim(-80, 20)
    fig.tight_layout()
    save(fig, "fig-pair")


def fig_steps():
    """The step protocol of Chapter 5, rendered by 'sg render' (one .sgb per
    trial); left: the thirteen conditions, right: the order of the trials."""
    proto = """sg 2 protocol
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
"""
    src, out = os.path.join(TMP, "steps.sg"), os.path.join(TMP, "steps_out")
    open(src, "w").write(proto)
    subprocess.run([SG, "render", "-q", "-r", "20kHz", "-s", "1", "-o", out, src], check=True)
    trials = sorted(f for f in os.listdir(out) if f.endswith(".sgb"))
    fig, (a1, a2) = plt.subplots(1, 2, figsize=(W, 2.3), gridspec_kw={"width_ratios": [1.3, 1]})
    cmap = plt.get_cmap("coolwarm")
    amps, seen = [], set()
    for f in trials:
        h, t, x = load(os.path.join(out, f))
        c = h["provenance"]["trial"]["condition"]
        amps.append(float(h["provenance"]["trial"]["variables"]["amp"][:-2]))
        if c not in seen:
            seen.add(c)
            a1.plot(t, x[0], color=cmap(c / 12))
    a1.set_xlabel("time (s)"); a1.set_ylabel("current (pA)")
    a1.set_title("13 conditions", loc="left")
    a2.scatter(range(len(amps)), amps, c=[cmap((a + 300) / 600) for a in amps], s=9)
    for b in (13, 26):
        a2.axvline(b - 0.5, ls=":", color="0.5")
    a2.set_xlabel("trial (played every 5 s)"); a2.set_ylabel("amp (pA)")
    a2.set_title("order: shuffled-blocks, 3 blocks", loc="left")
    fig.tight_layout()
    save(fig, "fig-steps")


# ------------------------------------------------------------------ Part II
def fig_boundaries():
    seq = "; ".join("150us dc(%d)" % (i % 2) for i in range(8))
    h, t, x = render(seq, rate="10kHz")
    fig, ax = plt.subplots(figsize=(W, 1.8))
    ax.stem(t * 1e3, x[0], linefmt="k-", markerfmt="ko", basefmt=" ")
    for i in range(9):
        ax.axvline(0.15 * i, ls=":", color="tab:red", lw=0.8)
    ax.set_xlabel("time (ms)"); ax.set_ylabel("sample value")
    ax.set_title("eight segments of 0.15 ms at 10 kHz: nominal boundaries (dotted) and samples",
                 loc="left")
    ax.set_ylim(-0.2, 1.3)
    save(fig, "fig-boundaries")


def fig_gallery():
    rec = "\n".join("%.6f" % (np.exp(-k / 60) * np.sin(k / 8)) for k in range(400))
    items = [("400ms", "dc(1)"), ("400ms", "ramp(from=-1, to=1)"),
             ("400ms", "relax(from=1, to=0, tau=60ms)"), ("400ms", "sine(1, 5Hz)"),
             ("400ms", "square(1, 5Hz, duty=30%)"), ("400ms", "saw(1, 5Hz)"),
             ("400ms", "triangle(1, 5Hz)"), ("400ms", "chirp(1, f0=2Hz, f1=30Hz)"),
             ("400ms", "biexp(1, 5ms, 40ms, delay=50ms)"), ("400ms", "alpha(1, 30ms, delay=50ms)"),
             ("400ms", 'file("rec.txt", rate=1kHz)'), ("400ms", "pulses(1, 20Hz, width=5ms)"),
             ("400ms", "ou(0, 1, 10ms)"), ("400ms", "wnoise(0, 1)"),
             ("400ms", "unoise(0, 1)"), ("400ms", "cnoise(0, 1, alpha=2)")]
    fig, axs = plt.subplots(4, 4, figsize=(W, 4.6), sharex=True)
    for ax, (d, g) in zip(axs.flat, items):
        h, t, x = render("%s %s\n" % (d, g), rate="5kHz", seed="3", unit="1",
                         files={"rec.txt": rec})
        ax.plot(t, x[0], color="k", lw=0.6)
        ax.set_title(g, family="monospace", fontsize=6)
        ax.tick_params(labelsize=6)
    for ax in axs[-1]: ax.set_xlabel("time (s)", fontsize=7)
    fig.tight_layout()
    save(fig, "fig-gallery")


def fig_kernels():
    shapes = [("square", "width=2ms"), ("biphasic", "width=2ms"), ("exp", "tau=3ms"),
              ("alpha", "tau=3ms"), ("biexp", "tau_rise=1ms, tau_decay=5ms")]
    fig, ax = plt.subplots(figsize=(W, 1.9))
    for i, (sh, par) in enumerate(shapes):
        h, t, x = render("25ms pulses(1, times=[2ms], shape=%s, %s)\n" % (sh, par),
                         rate="20kHz", unit="1")
        ax.plot(t * 1e3, x[0] - 2.4 * i, color="k")
        ax.text(26, -2.4 * i, "shape=" + sh, va="center", family="monospace", fontsize=7)
    ax.set_yticks([]); ax.set_xlabel("time (ms)"); ax.set_xlim(0, 33)
    save(fig, "fig-kernels")


if __name__ == "__main__":
    if not os.path.exists(SG):
        sys.exit("build sg first: make -C src")
    for f in (fig_step, fig_generators, fig_noise, fig_combine, fig_envelope, fig_pair,
              fig_steps, fig_boundaries, fig_gallery, fig_kernels):
        f()
