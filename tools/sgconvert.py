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
sgconvert.py -- describe a .sgb file, or convert it for other software.

    python3 sgconvert.py info  stim.sgb              # a plain-text description
    python3 sgconvert.py csv   stim.sgb [-o out.csv] # time + one column per channel
    python3 sgconvert.py npz   stim.sgb              # NumPy: t, one array per channel, header
    python3 sgconvert.py mat   stim.sgb              # MATLAB (needs scipy)
    python3 sgconvert.py h5    stim.sgb              # HDF5 (needs h5py)
    python3 sgconvert.py atf   stim.sgb              # Axon Text File (pClamp)
    python3 sgconvert.py neuron stim.sgb             # NEURON Vector.play files (ms, nA/mV/uS)
    python3 sgconvert.py brian stim.sgb              # Brian2 TimedArray data (.npz)
    python3 sgconvert.py json  stim.sgb              # the header (metadata, provenance)

Every output keeps the essential metadata (rate, units, markers, master
seed); the description text of the stimulus travels in the header formats.
Needs numpy; scipy and h5py only for mat and h5.
"""
import argparse, json, os, sys
import numpy as np


def load(path):
    """(header, samples[channel, k]) of a .sgb file (spec Sec. 15.1)."""
    with open(path, "rb") as f:
        b = f.read()
    if b[:8] != b"SGB\0\0\0\0\0":
        sys.exit(f"{path}: not a .sgb file")
    L = int.from_bytes(b[8:16], "little")
    h = json.loads(b[16:16 + L])
    x = np.frombuffer(b, "<f8", offset=16 + L).reshape(len(h["channels"]), h["samples"])
    return h, x


def label(c):
    return c["name"] + (f" ({c['unit']})" if c["unit"] != "1" else "")


# ------------------------------------------------------------------ info
def runs(x, tol):
    """Piecewise-constant runs [(k0, k1, value)] if the signal is made of levels."""
    change = np.flatnonzero(np.abs(np.diff(x)) > tol) + 1
    if len(change) > max(20, 0.02 * len(x)):
        return None
    edges = np.r_[0, change, len(x)]
    return [(int(a), int(b), float(x[a])) for a, b in zip(edges[:-1], edges[1:])]


def dominant_frequency(x, fs):
    if len(x) < 16:
        return None
    y = x - x.mean()
    if not np.any(y):
        return None
    P = np.abs(np.fft.rfft(y * np.hanning(len(y)))) ** 2
    f = np.fft.rfftfreq(len(y), 1 / fs)
    k = int(np.argmax(P[1:])) + 1
    return float(f[k]), float(P[k] / P[1:].sum())


def describe(h, x, max_runs=12):
    fs, N = h["rate"], h["samples"]
    p = h["provenance"]
    out = [f"StimGen 2 sample file: {len(h['channels'])} channel(s), {N} samples at {fs:g} Hz "
           f"({N / fs:.6g} s); master seed {p['master_seed']}; written by {p['implementation']}."]
    if p.get("trial"):
        t = p["trial"]
        v = ", ".join(f"{k}={val}" for k, val in t.get("variables", {}).items())
        out.append(f"Protocol trial {t.get('index')} (condition {t.get('condition')}, repetition {t.get('repetition')})"
                   + (f": {v}" if v else "") + ".")
    for c, row in zip(h["channels"], x):
        unit = "" if c["unit"] == "1" else " " + c["unit"]
        out.append(f"\nChannel {c['name']}{' (digital)' if c['digital'] else ''}: min {row.min():.6g}{unit}, "
                   f"max {row.max():.6g}{unit}, mean {row.mean():.6g}{unit}, SD {row.std():.6g}{unit}; rest {c['rest']:g}{unit}.")
        span = max(1e-12, float(row.max() - row.min()))
        r = runs(row, 1e-9 * span)
        if r is not None:
            out.append(f"  Made of {len(r)} constant level(s):")
            for k0, k1, val in r[:max_runs]:
                out.append(f"    {k0 / fs:10.6g} s to {k1 / fs:10.6g} s   {val:.6g}{unit}")
            if len(r) > max_runs:
                out.append(f"    ... and {len(r) - max_runs} more")
        else:
            d = dominant_frequency(row, fs)
            kind = "fluctuating"
            if d and d[1] > 0.3:
                kind = f"oscillating, dominant frequency {d[0]:.4g} Hz ({100 * d[1]:.0f}% of the power)"
            elif d:
                kind = f"noise-like (no dominant frequency; largest peak {d[0]:.4g} Hz)"
            out.append(f"  Not piecewise constant: {kind}.")
    if h["markers"]:
        out.append("\nMarkers: " + ", ".join(f"{m['name']} at {m['sample'] / fs:.6g} s" for m in h["markers"]) + ".")
    if p.get("warnings"):
        out.append("Warnings: " + "; ".join(p["warnings"]) + ".")
    desc = "\n".join(l for l in p["description"].splitlines() if not l.startswith("### file:")).strip()
    out.append("\nDescription that produced it:\n" + "\n".join("    " + l for l in desc.splitlines()))
    return "\n".join(out)


# ------------------------------------------------------------------ writers
def out_name(args, ext):
    return args.output or os.path.splitext(args.file)[0] + ext


def to_csv(h, x, args):
    fs, path = h["rate"], out_name(args, ".csv")
    t = np.arange(h["samples"]) / fs
    head = "t (s)," + ",".join(label(c) for c in h["channels"])
    np.savetxt(path, np.column_stack([t, x.T]), delimiter=",", header=head, comments="", fmt="%.10g")
    return [path]


def flat_meta(h):
    p = h["provenance"]
    return {"rate": h["rate"], "names": [c["name"] for c in h["channels"]], "units": [c["unit"] for c in h["channels"]],
            "marker_names": [m["name"] for m in h["markers"]], "marker_samples": [m["sample"] for m in h["markers"]],
            "master_seed": p["master_seed"], "description": p["description"], "header": json.dumps(h)}


def to_npz(h, x, args):
    path = out_name(args, ".npz")
    arrays = {c["name"]: x[i] for i, c in enumerate(h["channels"])}
    np.savez(path, t=np.arange(h["samples"]) / h["rate"], **arrays, **{k: np.array(v) for k, v in flat_meta(h).items()})
    return [path]


def to_mat(h, x, args):
    try:
        from scipy.io import savemat
    except ImportError:
        sys.exit("mat needs scipy (pip install scipy)")
    path = out_name(args, ".mat")
    d = {"t": np.arange(h["samples"]) / h["rate"], "x": x.T}
    d.update({k: v for k, v in flat_meta(h).items()})
    d.update({"ch_" + c["name"]: x[i] for i, c in enumerate(h["channels"])})
    savemat(path, d)
    return [path]


def to_h5(h, x, args):
    try:
        import h5py
    except ImportError:
        sys.exit("h5 needs h5py (pip install h5py)")
    path = out_name(args, ".h5")
    with h5py.File(path, "w") as f:
        f.attrs["rate"] = h["rate"]
        f.attrs["header"] = json.dumps(h)
        f.attrs["master_seed"] = h["provenance"]["master_seed"]
        f.attrs["description"] = h["provenance"]["description"]
        for i, c in enumerate(h["channels"]):
            d = f.create_dataset("channels/" + c["name"], data=x[i])
            d.attrs["unit"] = c["unit"]; d.attrs["rest"] = c["rest"]; d.attrs["digital"] = c["digital"]
        g = f.create_group("markers")
        for m in h["markers"]:
            g.attrs[m["name"]] = m["sample"]
    return [path]


def to_atf(h, x, args):
    """Axon Text File 1.0, a format read by pClamp (Clampex stimulus files, Clampfit)."""
    path, fs = out_name(args, ".atf"), h["rate"]
    opt = [f'"AcquisitionMode=Episodic Stimulation"', f'"Comment=StimGen 2, master seed {h["provenance"]["master_seed"]}"',
           '"SignalsExported=' + ",".join(c["name"] for c in h["channels"]) + '"']
    cols = ['"Time (s)"'] + [f'"{c["name"]} ({c["unit"] if c["unit"] != "1" else ""})"' for c in h["channels"]]
    with open(path, "w") as f:
        f.write("ATF\t1.0\n")
        f.write(f"{len(opt)}\t{len(cols)}\n")
        for o in opt:
            f.write(o + "\n")
        f.write("\t".join(cols) + "\n")
        t = np.arange(h["samples"]) / fs
        for k in range(h["samples"]):
            f.write(f"{t[k]:.9g}\t" + "\t".join(f"{v:.10g}" for v in x[:, k]) + "\n")
    return [path]


# NEURON's units: nA for currents (IClamp), mV (SEClamp), uS (conductances)
NEURON_SCALE = {"fA": 1e-6, "pA": 1e-3, "nA": 1, "uA": 1e3, "mA": 1e6, "A": 1e9,
                "uV": 1e-3, "mV": 1, "V": 1e3, "pS": 1e-6, "nS": 1e-3, "uS": 1, "mS": 1e3, "S": 1e6}
NEURON_UNIT = {"A": "nA", "V": "mV", "S": "uS"}


def to_neuron(h, x, args):
    """One two-column text file per channel: time (ms) and value in NEURON's unit."""
    base, fs, files = os.path.splitext(args.output or args.file)[0], h["rate"], []
    t_ms = np.arange(h["samples"]) / fs * 1e3
    for i, c in enumerate(h["channels"]):
        k = NEURON_SCALE.get(c["unit"], 1.0)
        unit = NEURON_UNIT.get(c["unit"][-1:], c["unit"]) if c["unit"] in NEURON_SCALE else c["unit"]
        path = f"{base}_{c['name']}.dat"
        np.savetxt(path, np.column_stack([t_ms, x[i] * k]), fmt="%.10g",
                   header=f"t (ms)  {c['name']} ({unit})  -- StimGen 2, master seed {h['provenance']['master_seed']}\n"
                          f"python: t, v = numpy.loadtxt('{os.path.basename(path)}', unpack=True); "
                          f"tv = h.Vector(t); vv = h.Vector(v); vv.play(stim._ref_amp, tv, True)")
        files.append(path)
    return files


def to_brian(h, x, args):
    """Arrays for brian2.TimedArray: values in the channel unit, dt in seconds."""
    path = out_name(args, ".brian.npz")
    np.savez(path, dt=1 / h["rate"], names=np.array([c["name"] for c in h["channels"]]),
             units=np.array([c["unit"] for c in h["channels"]]), values=x, header=json.dumps(h))
    return [path]


def to_json(h, x, args):
    path = out_name(args, ".json")
    with open(path, "w") as f:
        json.dump(h, f, indent=1)
    return [path]


WRITERS = {"csv": to_csv, "npz": to_npz, "mat": to_mat, "h5": to_h5, "atf": to_atf,
           "neuron": to_neuron, "brian": to_brian, "json": to_json}


def main():
    ap = argparse.ArgumentParser(description="Describe or convert a StimGen 2 .sgb file.",
                                 epilog="Brian2: d = numpy.load('x.brian.npz'); "
                                        "I = TimedArray(d['values'][0] * pA, dt=float(d['dt']) * second)")
    ap.add_argument("format", choices=["info"] + list(WRITERS))
    ap.add_argument("file")
    ap.add_argument("-o", "--output", help="output file (default: input name with a new extension)")
    a = ap.parse_args()
    h, x = load(a.file)
    if a.format == "info":
        print(describe(h, x))
        return
    for p in WRITERS[a.format](h, x, a):
        print("wrote", p)


if __name__ == "__main__":
    main()
