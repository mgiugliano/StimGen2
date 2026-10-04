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
tools/sgconvert.py: every format read back and compared with the .sgb samples.

    python3 test_convert.py ./sg
HDF5 is tested only if h5py is installed.
"""
import json, os, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
CONV = os.path.join(HERE, "..", "tools", "sgconvert.py")
D = tempfile.mkdtemp(prefix="sgconv")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("ok    " if ok else "FAIL  ") + name + ("" if ok else "  " + str(detail)))
    fails += 0 if ok else 1


def conv(fmt, *extra):
    p = subprocess.run([sys.executable, CONV, fmt, "s.sgb", *extra], cwd=D, capture_output=True, text=True)
    if p.returncode:
        raise RuntimeError(p.stderr)
    return p.stdout


open(os.path.join(D, "s.sg"), "w").write(
    "sg 2 stimulus\nrate 10kHz\nseed 5\nchannel I unit=pA {\n  100ms dc(0)\n  @on\n  200ms dc(300)\n  100ms dc(0)\n}\n"
    "channel V unit=mV { 400ms sine(10, 25Hz, offset=-70) }\nchannel G unit=nS { 400ms pos(ou(5, 2, 3ms)) }\n")
subprocess.run([SG, "render", "-q", "s.sg"], cwd=D, check=True)
b = open(os.path.join(D, "s.sgb"), "rb").read()
L = int.from_bytes(b[8:16], "little")
h = json.loads(b[16:16 + L])
x = np.frombuffer(b, "<f8", offset=16 + L).reshape(3, -1)
t = np.arange(x.shape[1]) / 1e4

info = conv("info")
check("info: levels, oscillation, noise, markers, description",
      "0.1 s to        0.3 s   300 pA" in info and "dominant frequency 25 Hz" in info and "noise-like" in info
      and "I.on at 0.1 s" in info and "channel V unit=mV" in info, info)
conv("csv")
c = np.loadtxt(os.path.join(D, "s.csv"), delimiter=",", skiprows=1)
hdr = open(os.path.join(D, "s.csv")).readline().strip()
check("csv: time and channels, exact to 10 digits", hdr == "t (s),I (pA),V (mV),G (nS)" and np.allclose(c[:, 1:].T, x, rtol=1e-9, atol=1e-12)
      and np.allclose(c[:, 0], t))
conv("npz")
z = np.load(os.path.join(D, "s.npz"))
check("npz: arrays identical, metadata kept", np.array_equal(z["I"], x[0]) and np.array_equal(z["G"], x[2])
      and float(z["rate"]) == 1e4 and str(z["master_seed"]) == "5" and list(z["marker_names"]) == ["I.on"])
conv("mat")
from scipy.io import loadmat
m = loadmat(os.path.join(D, "s.mat"))
check("mat: x[sample, channel] identical, per-channel arrays", np.array_equal(m["x"].T, x) and np.array_equal(m["ch_V"].ravel(), x[1]))
conv("atf")
lines = open(os.path.join(D, "s.atf")).read().splitlines()
n_opt, n_col = map(int, lines[1].split("\t"))
a = np.loadtxt(lines[3 + n_opt:])
check("atf: header 'ATF 1.0', 4 columns, values", lines[0] == "ATF\t1.0" and n_col == 4 and a.shape == (4000, 4)
      and np.allclose(a[:, 1:].T, x, rtol=1e-9, atol=1e-12) and lines[2 + n_opt].startswith('"Time (s)"'))
out = conv("neuron")
iI = np.loadtxt(os.path.join(D, "s_I.dat")); iV = np.loadtxt(os.path.join(D, "s_V.dat")); iG = np.loadtxt(os.path.join(D, "s_G.dat"))
check("neuron: time in ms; pA -> nA, mV -> mV, nS -> uS",
      np.allclose(iI[:, 0], t * 1e3) and np.allclose(iI[:, 1], x[0] * 1e-3) and np.allclose(iV[:, 1], x[1])
      and np.allclose(iG[:, 1], x[2] * 1e-3) and "(nA)" in open(os.path.join(D, "s_I.dat")).readline())
conv("brian")
br = np.load(os.path.join(D, "s.brian.npz"))
check("brian: dt and values for TimedArray", float(br["dt"]) == 1e-4 and np.array_equal(br["values"], x) and list(br["units"]) == ["pA", "mV", "nS"])
conv("json")
check("json: the header", json.load(open(os.path.join(D, "s.json"))) == h)
try:
    import h5py
    conv("h5")
    with h5py.File(os.path.join(D, "s.h5")) as f:
        ok = np.array_equal(f["channels/I"][:], x[0]) and f["channels/V"].attrs["unit"] == "mV" and f["markers"].attrs["I.on"] == 1000
    check("h5: datasets, units, markers", ok)
except ImportError:
    print("skip  h5: h5py is not installed")
print("\n" + ("all convert tests passed" if not fails else f"{fails} convert test(s) FAILED"))
sys.exit(1 if fails else 0)
