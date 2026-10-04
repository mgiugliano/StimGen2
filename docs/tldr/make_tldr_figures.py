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
make_tldr_figures.py -- the figures of the short paper (docs/tldr/).

Every stimulus is rendered by sg from the text shown in the paper; a leaky
integrate-and-fire neuron (a few lines of numpy) stands for "a model" that
receives exactly the same samples as the cell would.  Writes the figures
and numbers.json (values quoted in the text).
"""
import json, os, subprocess, sys, tempfile
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", ".."))
SG = os.path.join(ROOT, "src", "sg")
TMP = tempfile.mkdtemp(prefix="sgtldr")
plt.rcParams.update({"font.size": 8, "axes.spines.top": False, "axes.spines.right": False, "lines.linewidth": 0.9})
INK, TEAL, ORANGE, MUTED = "#1B2A41", "#0B7A75", "#F26419", "#5C6B7A"
numbers = {}


def load(path):
    b = open(path, "rb").read()
    L = int.from_bytes(b[8:16], "little")
    h = json.loads(b[16:16 + L])
    return h, np.frombuffer(b, "<f8", offset=16 + L).reshape(len(h["channels"]), -1)


def render_protocol(text, name, seed="1"):
    src, out = os.path.join(TMP, name + ".sg"), os.path.join(TMP, name)
    open(src, "w").write(text)
    subprocess.run([SG, "render", "-q", "-r", "10kHz", "-s", seed, "-o", out, src], check=True)
    trials = []
    for f in sorted(os.listdir(out)):
        h, x = load(os.path.join(out, f))
        trials.append((h, x[0]))
    return trials


def lif(I_pA, fs, noise_mV=0.0, seed=0):
    """Leaky integrate-and-fire: tau 20 ms, R 100 MOhm, threshold -50 mV, reset -65 mV, 2 ms refractory."""
    dt, tau, R, EL, Vth, Vr, ref = 1 / fs, 0.020, 100e6, -70.0, -50.0, -65.0, 0.002
    rng, V, last, spikes = np.random.default_rng(seed), EL, -1.0, []
    for k, I in enumerate(I_pA):
        t = k * dt
        if t - last < ref:
            continue
        V += dt / tau * (EL - V + R * I * 1e-12 * 1e3) + noise_mV * np.sqrt(dt / tau) * rng.standard_normal()
        if V >= Vth:
            spikes.append(t); V = Vr; last = t
    return np.array(spikes)


def save(fig, name):
    fig.savefig(os.path.join(HERE, name), bbox_inches="tight")
    plt.close(fig)
    print("wrote", name)


# ------------------------------------------------------------------ 1. pipeline
def fig_pipeline():
    fig, ax = plt.subplots(figsize=(6.3, 2.3)); ax.axis("off"); ax.set_xlim(0, 10); ax.set_ylim(0, 3.4)
    def box(x, y, w, h, title, sub, fill):
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0.02,rounding_size=0.12", fc=fill, ec="none"))
        dark = fill in (INK, TEAL)
        ax.text(x + w / 2, y + h * 0.63, title, ha="center", va="center", fontsize=9, fontweight="bold", color="white" if dark else INK)
        ax.text(x + w / 2, y + h * 0.28, sub, ha="center", va="center", fontsize=7, color="#D8ECEA" if dark else MUTED)
    box(0.0, 1.15, 1.9, 1.1, "description", ".sg text", INK)
    box(2.5, 2.05, 2.0, 0.95, "web planner", "click · type · see", "#EEF4F3")
    box(2.5, 0.4, 2.0, 0.95, "sg", "command line", "#EEF4F3")
    box(5.1, 1.15, 1.7, 1.1, "samples", ".sgb + record", TEAL)
    for (y, t, s) in ((2.45, "the rig", "Gecko → NI board → cell"), (1.25, "models", "NEURON · Brian2"), (0.05, "analysis", "Python · MATLAB · pClamp")):
        box(7.4, y, 2.6, 0.85, t, s, "#EEF4F3")
    arr = dict(arrowstyle="-|>", color=ORANGE, lw=1.2, mutation_scale=9)
    for (a, b) in (((1.9, 1.85), (2.5, 2.5)), ((1.9, 1.55), (2.5, 0.9)), ((4.5, 2.5), (5.1, 1.9)), ((4.5, 0.9), (5.1, 1.5))):
        ax.annotate("", b, a, arrowprops=arr)
    for y in (2.85, 1.68, 0.48):
        ax.annotate("", (7.4, y), (6.8, 1.7), arrowprops=arr)
    ax.text(6.95, 3.25, "converters (sgconvert)", fontsize=7, color=MUTED, ha="center")
    save(fig, "fig-pipeline.pdf")


# ------------------------------------------------------------------ 2. f-I
FI = """sg 2 protocol
sweep  amp = from 0pA to 500pA step 50pA
period 4s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        2s    dc($amp)
        500ms dc(0)
    }
}
"""


def fig_fi():
    trials = render_protocol(FI, "fi")
    fs = trials[0][0]["rate"]
    fig, (a1, a2, a3) = plt.subplots(1, 3, figsize=(6.3, 1.9), gridspec_kw={"width_ratios": [1.2, 1.2, 1]})
    amps, rates = [], []
    cmap = plt.get_cmap("viridis")
    for i, (h, x) in enumerate(trials):
        amp = float(h["provenance"]["trial"]["variables"]["amp"][:-2])
        t = np.arange(len(x)) / fs
        a1.plot(t, x, color=cmap(i / (len(trials) - 1)))
        sp = lif(x, fs)
        amps.append(amp); rates.append(np.sum((sp >= 0.5) & (sp < 2.5)) / 2.0)
        if amp in (250.0, 350.0):
            a2.eventplot([sp], lineoffsets=1 if amp == 250 else 2, linelengths=0.7, linewidths=0.8, colors=[cmap(i / (len(trials) - 1))])
    a1.set_xlabel("time (s)"); a1.set_ylabel("current (pA)"); a1.set_title("11 trials rendered by sg", loc="left", fontsize=8)
    a2.set_yticks([1, 2]); a2.set_yticklabels(["250 pA", "350 pA"]); a2.set_xlim(0.4, 1.2); a2.set_xlabel("time (s)")
    a2.set_title("model spikes", loc="left", fontsize=8)
    a3.plot(amps, rates, "o-", color=TEAL, ms=3); a3.set_xlabel("amplitude (pA)"); a3.set_ylabel("rate (Hz)")
    a3.set_title("f–I curve of the model", loc="left", fontsize=8)
    fig.tight_layout()
    save(fig, "fig-fi.pdf")
    numbers["fi_trials"] = len(trials)
    numbers["fi_rheobase_pA"] = next(a for a, r in zip(amps, rates) if r > 0)
    numbers["fi_rate_500"] = rates[-1]


# ------------------------------------------------------------------ 3. frozen vs fresh noise
NOISE = """sg 2 protocol
repeat 10
noise  {policy}
gap    1s

stimulus {{
    channel Iinj unit=pA {{
        2s ou(mean=150, sd=100, tau=5ms)
    }}
}}
"""


def reliability(trains, window=0.002):
    """Fraction of spikes that have a partner within +-2 ms in each other trial (mean over pairs)."""
    vals = []
    for i, a in enumerate(trains):
        for j, b in enumerate(trains):
            if i == j or not len(a) or not len(b):
                continue
            vals.append(np.mean([np.min(np.abs(b - s)) <= window for s in a]))
    return float(np.mean(vals))


def fig_noise():
    fig, axs = plt.subplots(2, 2, figsize=(6.3, 2.6), gridspec_kw={"height_ratios": [1, 2.2]}, sharex=True)
    for col, policy in enumerate(("per-condition", "per-trial")):
        trials = render_protocol(NOISE.format(policy=policy), policy, seed="7")
        fs = trials[0][0]["rate"]
        t = np.arange(len(trials[0][1])) / fs
        for i, (h, x) in enumerate(trials[:3]):
            axs[0, col].plot(t, x, color=[INK, TEAL, ORANGE][i], lw=0.5, alpha=0.8)
        trains = [lif(x, fs, noise_mV=0.5, seed=100 + i) for i, (h, x) in enumerate(trials)]
        axs[1, col].eventplot(trains, colors=INK, linelengths=0.7)
        r = reliability(trains)
        numbers[f"reliability_{policy}"] = round(r, 2)
        title = "noise per-condition (frozen)" if col == 0 else "noise per-trial (fresh)"
        axs[0, col].set_title(f"{title}", loc="left", fontsize=8)
        axs[1, col].set_title(f"model spikes, reliability {r:.2f}", loc="left", fontsize=7.5, color=MUTED)
        axs[1, col].set_xlabel("time (s)"); axs[1, col].set_xlim(0, 2)
    axs[0, 0].set_ylabel("pA"); axs[1, 0].set_ylabel("trial")
    fig.tight_layout()
    save(fig, "fig-noise.pdf")


if __name__ == "__main__":
    if not os.path.exists(SG):
        sys.exit("build sg first: make")
    fig_pipeline(); fig_fi(); fig_noise()
    json.dump(numbers, open(os.path.join(HERE, "numbers.json"), "w"), indent=1)
    print(numbers)
