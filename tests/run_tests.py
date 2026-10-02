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
Reference tests for sg, the StimGen 2 renderer.

The expected samples are computed here *independently* of the C code:
SHA-256 from Python's hashlib, Philox4x64-10 from numpy, and every formula
re-implemented from the specification.  Usage:  python3 run_tests.py ./sg
"""
import hashlib, json, math, os, subprocess, sys, tempfile
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import numpy as np

SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
TMP = tempfile.mkdtemp(prefix="sgtest")
failures = 0


def check(name, ok, detail=""):
    global failures
    print(("ok    " if ok else "FAIL  ") + name + ("" if ok else "  " + detail))
    failures += 0 if ok else 1


def run(*args, expect_ok=True):
    p = subprocess.run([SG, *args], capture_output=True, text=True, cwd=TMP)
    if expect_ok and p.returncode != 0:
        raise RuntimeError(p.stderr)
    return p


def render(text, rate="10kHz", seed="42", extra=(), path=None):
    """Render a description; return (header, samples[channel, k])."""
    out = os.path.join(TMP, "out.sgb")
    src = ["-e", text] if path is None else [path]
    run("render", "-q", "-r", rate, "-s", seed, "-o", out, *extra, *src)
    b = open(out, "rb").read()
    assert b[:8] == b"SGB\0\0\0\0\0"
    L = int.from_bytes(b[8:16], "little")
    h = json.loads(b[16:16 + L])
    x = np.frombuffer(b[16 + L:], "<f8").reshape(len(h["channels"]), -1)
    return h, x, b[16 + L:]


# --- the random streams of the specification (Sec. 11) -------------------

from refstream import Stream  # noqa: E402


def close(a, b, tol=1e-12):
    a, b = np.asarray(a, float), np.asarray(b, float)
    scale = max(1.0, float(np.max(np.abs(b))) if b.size else 1.0)
    return a.shape == b.shape and float(np.max(np.abs(a - b), initial=0)) <= tol * scale


# ------------------------------------------------------------------------
# 1. Segment boundaries (spec Sec. 8.2): 0.15 ms segments at 10 kHz
seq = "; ".join("150us dc(%d)" % (i % 2) for i in range(40))
h, x, _ = render(seq)
runs = np.diff(np.flatnonzero(np.diff(np.r_[-1, x[0], -1]) != 0))
check("boundaries: 40 x 0.15 ms = 60 samples", x.shape[1] == 60)
check("boundaries: lengths 2,1,1,2,...", list(runs[:8]) == [2, 1, 1, 2, 2, 1, 1, 2], str(runs[:8]))

# 2. Deterministic generators against closed forms
fs = 10000.0
h, x, _ = render("1s sine(amp=3, freq=7Hz, phase=0.3, offset=1)")
u = np.arange(10000) / fs
check("sine", close(x[0], 3 * np.sin(2 * np.pi * 7 * u + 0.3) + 1))
h, x, _ = render("2s chirp(amp=2, f0=1Hz, f1=20Hz)")
u = np.arange(20000) / fs
check("chirp linear", close(x[0], 2 * np.sin(2 * np.pi * (u + 19 * u * u / 4))))
h, x, _ = render("2s chirp(amp=2, f0=1Hz, f1=16Hz, law=exp)")
check("chirp exp", close(x[0], 2 * np.sin(2 * np.pi * 2 * (16 ** (u / 2) - 1) / math.log(16))))
h, x, _ = render("1s biexp(amp=4, tau_rise=5ms, tau_decay=20ms, delay=100ms)")
u = np.arange(10000) / fs
v = u - 0.1
tr, td = 0.005, 0.020
K = (tr / td) ** (tr / (td - tr)) - (tr / td) ** (td / (td - tr))
ref = np.where(v >= 0, 4 * (np.exp(-v / td) - np.exp(-v / tr)) / K, 0)
check("biexp", close(x[0], ref), str(np.max(np.abs(x[0] - ref))))
check("biexp peak = amp", abs(np.max(x[0]) - 4) < 1e-3)
h, x, _ = render("1s dc(-1); 1s ramp(to=4); 1s square(2, 5Hz, duty=25%)")
u = np.arange(10000) / fs
check("ramp from prev", close(x[0][10000:20000], -1 + 5 * np.arange(10000) / 10000))
check("square duty", close(x[0][20000:], np.where((5 * u) % 1 < 0.25, 2, -2)))

# 3. Ornstein-Uhlenbeck, exact update, stream keyed by "42:ch=out/s0"
h, x, _ = render("1s ou(mean=1, sd=0.5, tau=5ms)")
st, rho = Stream("42:ch=out/s0"), math.exp(-1e-4 / 5e-3)
ref = [1 + 0.5 * st.gauss()]
for k in range(1, 10000):
    ref.append(1 + (ref[-1] - 1) * rho + 0.5 * math.sqrt(1 - rho * rho) * st.gauss())
check("ou: exact update and stream", close(x[0], ref))
check("ou: master seed recorded", h["provenance"]["master_seed"] == "42")

# 4. Addresses inside expressions: noise is operand o1 of segment s1
h, x, _ = render("1s dc(0); 1s sine(1, 1Hz) + wnoise(0, 1); 1s unoise(2, 3)")
st = Stream("42:ch=out/s1/o1")
u = np.arange(10000) / fs
ref = np.sin(2 * np.pi * u) + np.array([st.gauss() for _ in range(10000)])
check("wnoise at address ch=out/s1/o1", close(x[0][10000:20000], ref))
st = Stream("42:ch=out/s2")
ref = [2 + 3 * math.sqrt(12) * (st.uniform() - 0.5) for _ in range(10000)]
check("unoise", close(x[0][20000:], ref))

# 5. Frozen noise and locality
h, x, _ = render("200ms ou(2, 0.5, 100ms, seed=21); 50ms dc(0); "
                 "200ms ou(2, 0.5, 100ms, seed=21); 50ms dc(0); 200ms ou(2, 0.5, 100ms)")
a, b, c = x[0][0:2000], x[0][2500:4500], x[0][5000:7000]
check("frozen noise: same seed, same samples", np.array_equal(a, b))
check("frozen noise: no seed, different samples", not np.array_equal(a, c))
st, rho = Stream("fixed:21"), math.exp(-1e-4 / 0.1)
ref = [2 + 0.5 * st.gauss()]
for k in range(1, 2000):
    ref.append(2 + (ref[-1] - 2) * rho + 0.5 * math.sqrt(1 - rho * rho) * st.gauss())
check("frozen noise: stream fixed:21", close(a, ref))
_, x1, _ = render("1s dc(0); 1s ou(0, 1, 5ms)")
_, x2, _ = render("1s dc(7); 1s ou(0, 1, 5ms)")
check("locality: editing segment 0 leaves noise of segment 1", np.array_equal(x1[0][10000:], x2[0][10000:]))
_, x3, _ = render("1s dc(0); 1s ou(0, 1, 5ms)", seed="43")
check("another master seed gives another realisation", not np.array_equal(x1[0][10000:], x3[0][10000:]))

# 6. Repeat: copies get different streams, prev chains through copies
h, x, _ = render("repeat 3 { 10ms wnoise(0, 1) }")
st = [Stream("42:ch=out/s0/r%d/s0" % r) for r in range(3)]
ref = [s.gauss() for s in st for _ in range(100)]
check("repeat: one stream per copy", close(x[0], ref))

# 7. Poisson pulses with dead time, grid alignment
h, x, _ = render("1s pulses(amp=5, rate=50Hz, width=1ms, timing=poisson, dead=2ms)")
st, s, ref = Stream("42:ch=out/s0"), 0.0, np.zeros(10000)
while True:
    s += 0.002 + (1 / 50 - 0.002) * st.expo()
    if s >= 1:
        break
    o = int(np.rint(s * fs))
    ref[o:o + 10] += 5
check("poisson pulses", close(x[0], ref))
h, x, _ = render("100ms pulses(amp=1, rate=100Hz, width=1ms, shape=biphasic)")
ref = np.zeros(1000)
for o in range(0, 1000, 100):
    ref[o:o + 10] += 1; ref[o + 10:o + 20] -= 1
check("regular biphasic pulses, first at t = 0", close(x[0], ref))

# 8. Power-law noise: the procedure of spec App. B.6, with numpy's FFT
N = 3000
h, x, _ = render("300ms cnoise(mean=1, sd=2, alpha=1)")
st, T = Stream("42:ch=out/s0"), N / fs
Z, V = np.zeros(N, complex), 0.0
for j in range(1, N // 2 + 1):
    f = j / T
    S = f ** -1.0 if (1 / T <= f <= fs / 2) else 0.0
    a, b = st.gauss(), st.gauss()
    if 2 * j == N:
        Z[j] = math.sqrt(S) * a; V += S
    else:
        Z[j] = math.sqrt(S / 2) * (a + 1j * b); Z[N - j] = np.conj(Z[j]); V += 2 * S
y = np.real(np.fft.ifft(Z) * N)
check("cnoise (Bluestein FFT, N = 3000)", close(x[0], 1 + 2 * y / math.sqrt(V), 1e-9))

# 9. Algebra: maps, precedence, blocks as operands
h, x, _ = render("1s 1 + 2 * 3 - abs(sine(1, 1Hz)) / 2")
u = np.arange(10000) / fs
check("precedence and abs", close(x[0], 7 - np.abs(np.sin(2 * np.pi * u)) / 2))
h, x, _ = render("{ 1s ramp(0, 1) ; 3s dc(1) ; 1s ramp(1, 0) } * sine(2, 8Hz)")
u = np.arange(50000) / fs
env = np.r_[np.arange(10000) / 1e4, np.ones(30000), 1 - np.arange(10000) / 1e4]
check("envelope block * generator", close(x[0], env * 2 * np.sin(2 * np.pi * 8 * u)))
h, x, _ = render("1s clip(spow(sine(4, 2Hz), 0.5), -1.5, 1.5)")
s4 = 4 * np.sin(2 * np.pi * 2 * np.arange(10000) / fs)
check("spow and clip", close(x[0], np.clip(np.sign(s4) * np.abs(s4) ** 0.5, -1.5, 1.5)))

# 10. Stimulus files: channels, units, copy, use, digital, markers, padding
open(os.path.join(TMP, "noise.sg"), "w").write("# a waveform file\n1s ou(0, 10, 5ms)\n")
open(os.path.join(TMP, "stim.sg"), "w").write("""sg 2 stimulus
rate 10kHz
seed 7
channel I unit=pA {
    500ms dc(0)
    @step
    1s    dc(0.3nA)
}
channel N unit=pA use "noise.sg"
channel C unit=pA copy N
channel M unit=pA use "noise.sg"
digital cam { 1s pulses(1, 100Hz, width=1ms) }
marker go at 0.25s
""")
h, x, _ = render(None, path="stim.sg", rate="10kHz", seed="7")
names = [c["name"] for c in h["channels"]]
check("stimulus: channels", names == ["I", "N", "C", "M", "cam"], str(names))
check("stimulus: 0.3nA converted to 300 pA", close(x[0][5000:], np.full(10000, 300.0)))
check("stimulus: copy has identical samples", np.array_equal(x[1], x[2]))
check("stimulus: 'use' twice gives independent noise", not np.array_equal(x[1][:10000], x[3][:10000]))
check("stimulus: shorter channels hold rest", np.all(x[1][10000:] == 0) and x.shape[1] == 15000)
mk = {m["name"]: m["sample"] for m in h["markers"]}
check("stimulus: markers", mk == {"I.step": 5000, "go": 2500}, str(mk))
check("stimulus: digital channel", set(np.unique(x[4])) <= {0.0, 1.0} and h["channels"][4]["digital"])

# 11. Output format and provenance
h, x, raw = render("1s ou(0, 1, 5ms)")
check("sgb: samples_sha256", hashlib.sha256(raw).hexdigest() == h["provenance"]["samples_sha256"])
_, _, raw2 = render("1s ou(0, 1, 5ms)")
check("sgb: identical bytes on a second run", raw == raw2)
h, x, _ = render("1s ou(0, 1, 5ms)", rate="20kHz")
check("rate is recorded", h["rate"] == 20000 and x.shape[1] == 20000)

# 12. Errors that must be reported
bad = {
    "missing duration": "dc(0)",
    "duration without unit": "1 dc(0)",
    "wrong unit": "1s sine(1, 10ms)",
    "division by zero": "1s 1 / sine(1, 1Hz)",
    "sqrt of negative": "1s sqrt(sine(1, 1Hz))",
    "mismatched durations": "{1s dc(0)} + {2s dc(0)}",
    "unknown generator": "1s foo(1)",
    "missing parameter": "1s ramp(4)",
    "domain": "1s biexp(1, 5ms, 2ms)",
    "unit on dimensionless channel": "1s dc(3pA)",
}
for what, text in bad.items():
    p = run("check", "-e", text, expect_ok=False)
    check("error reported: " + what, p.returncode == 1 and "error" in p.stderr, p.stderr.strip())

print("\n%s" % ("all tests passed" if failures == 0 else "%d test(s) FAILED" % failures))
sys.exit(1 if failures else 0)
