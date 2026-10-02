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
Thorough conformance tests of sg against the StimGen 2 specification.

Complements run_tests.py with randomised and property-based checks:
  A  exact segment boundaries, against Python's exact Fractions
  B  canonical form: idempotent, and it renders to the very same samples
  C  every remaining primitive and option against its closed form
  D  'prev' and end values, nested blocks, repeat
  E  units and dimensions
  F  stimulus rules: duration, rest, copy, digital, rate, seed
  G  regeneration from the provenance record alone
  H  statistics of the noise generators (incl. OU exactness for dt >> tau)
  I  independence of compiler optimisation (-O0 build gives the same bytes)
  J  errors that must be reported
  K  protocols: order, seeds, substitution, sweeps, directory form
Usage:  python3 test_thorough.py ./sg
"""
import json, math, os, random, shutil, subprocess, sys, tempfile
from fractions import Fraction
import numpy as np
np.seterr(over='ignore', divide='ignore', invalid='ignore')  # masked by np.where

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
TMP = tempfile.mkdtemp(prefix="sgthorough")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("ok    " if ok else "FAIL  ") + name + ("" if ok else "  " + str(detail)))
    fails += 0 if ok else 1


def sg(*args, ok=True, exe=None):
    p = subprocess.run([exe or SG, *args], capture_output=True, text=True, cwd=TMP)
    if ok and p.returncode:
        raise RuntimeError(" ".join(args) + "\n" + p.stderr)
    return p


def load(path):
    b = open(path, "rb").read()
    L = int.from_bytes(b[8:16], "little")
    h = json.loads(b[16:16 + L])
    return h, np.frombuffer(b[16 + L:], "<f8").reshape(len(h["channels"]), -1), b[16 + L:]


def render(text, rate="10kHz", seed="5", unit=None, exe=None, extra=()):
    src, out = os.path.join(TMP, "t.sg"), os.path.join(TMP, "t.sgb")
    open(src, "w").write(text)
    args = ["render", "-q", "-r", rate, "-o", out, *extra]
    if seed is not None: args += ["-s", seed]
    if unit: args += ["-u", unit]
    sg(*args, src, exe=exe)
    return load(out)


def close(a, b, tol=1e-12):
    a, b = np.asarray(a, float), np.asarray(b, float)
    return a.shape == b.shape and np.max(np.abs(a - b), initial=0) <= tol * max(1, np.max(np.abs(b), initial=1))


rng = random.Random(2026)

# ---------------------------------------------------------------- A
print("A. exact segment boundaries")
for trial in range(25):
    rate = rng.choice(["1kHz", "8kHz", "10kHz", "20kHz", "25kHz", "44.1kHz", "12345Hz", "3.3kHz"])
    fs = Fraction(rate[:-3]) * 1000 if rate.endswith("kHz") else Fraction(rate[:-2])
    durs = []
    for i in range(rng.randint(5, 40)):
        u = rng.choice(["s", "ms", "us"])
        v = Fraction(rng.randint(0, 3000), 10 ** rng.randint(0, 3))
        durs.append((v, u))
    text = "\n".join("%s%s dc(%d)" % (str(float(v)) if v.denominator != 1 else str(v), u, i)
                     for i, (v, u) in enumerate(durs))
    h, x, _ = render(text, rate=rate)
    scale = {"s": 1, "ms": Fraction(1, 1000), "us": Fraction(1, 10 ** 6)}
    tau, ok = Fraction(0), True
    n = [round(Fraction(0))]
    for v, u in durs:
        tau += Fraction(str(float(v)) if v.denominator != 1 else str(v)) * scale[u]
        n.append(round(tau * fs))                 # Fraction rounds half to even
    for i in range(len(durs)):
        if not np.all(x[0][n[i]:n[i + 1]] == i): ok = False
    ok = ok and x.shape[1] == n[-1]
    check("random boundaries, %d segments at %s" % (len(durs), rate), ok)

# ---------------------------------------------------------------- B
print("B. canonical form")
GENS = ["dc({a})", "ramp({a}, {b})", "relax({a}, {b}, {t}ms)", "sine({a}, {f}Hz)",
        "sine({a}, {f}Hz, phase={p}deg, clock=global)", "square({a}, {f}Hz, duty={d}%)",
        "saw({a}, {f}Hz, duty=0.{d})", "triangle({a}, {f}Hz)", "chirp({a}, {f}Hz, {g}Hz, law=exp)",
        "biexp({a}, 2ms, {t}ms, delay=10ms)", "alpha({a}, {t}ms)",
        "pulses({a}, {f}Hz, width=2ms)", "pulses({a}, {f}Hz, shape=exp, tau={t}ms, timing=poisson)",
        "ou({a}, 1, {t}ms)", "ou({a}, 1, {t}ms, init=mean, seed={s})", "wnoise(0, {a})",
        "unoise({a}, 1)", "cnoise(0, 1, alpha=1.5)", "prev"]
MAPS = ["abs({e})", "pos({e})", "clip({e}, -1, 1)", "spow({e}, 0.5)", "min({e}, {e2})", "-({e})"]


def rnd_expr(depth=0):
    r = rng.random()
    if depth < 2 and r < 0.25:
        return "(%s %s %s)" % (rnd_expr(depth + 1), rng.choice("+-*"), rnd_expr(depth + 1))
    if depth < 2 and r < 0.4:
        return rng.choice(MAPS).format(e=rnd_expr(depth + 1), e2=rnd_expr(depth + 1))
    if depth < 1 and r < 0.5:
        return "{ %s }" % " ; ".join("%dms %s" % (rng.randint(10, 80), rnd_expr(2)) for _ in range(2))
    return rng.choice(GENS).format(a=rng.choice([-2, 0.5, 1, 3]), b=rng.choice([0, 1, 4]),
                                   t=rng.choice([3, 5, 20]), f=rng.choice([2, 5, 13]),
                                   g=rng.choice([20, 40]), p=rng.choice([0, 45, 90]),
                                   d=rng.choice([25, 50, 75]), s=rng.randint(0, 99))


def rnd_wave():
    segs = []
    for _ in range(rng.randint(1, 5)):
        e = rnd_expr()
        segs.append(e if e.startswith("{") else "%dms %s" % (rng.randint(20, 200), e))
    if rng.random() < 0.3:
        segs.append("repeat %d { 30ms %s }" % (rng.randint(0, 3), rnd_expr(2)))
    return "\n".join(segs) + "\n"


nb, nsame = 0, 0
for i in range(150):
    w = rnd_wave()
    src = os.path.join(TMP, "w.sg")
    open(src, "w").write(w)
    p = sg("canon", src, ok=False)
    if p.returncode:                 # e.g. mismatched block durations: fine, skip
        continue
    c1 = p.stdout
    open(src, "w").write(c1)
    c2 = sg("canon", src).stdout
    nb += 1
    _, x1, r1 = render(w)
    _, x2, r2 = render(c1)
    nsame += (c1 == c2 and r1 == r2)
    if c1 != c2 or r1 != r2:
        check("canonical round trip", False, w)
        break
check("canonical form idempotent and renders identically (%d random waveforms)" % nb, nsame == nb)

# ---------------------------------------------------------------- C
print("C. primitives and options against closed forms")
fs = 10000.0
u = np.arange(10000) / fs
h, x, _ = render("1s relax(from=2, to=-1, tau=50ms)")
check("relax", close(x[0], -1 + 3 * np.exp(-u / 0.05)))
for duty in (0, 0.3, 1):
    h, x, _ = render("1s saw(2, 3Hz, duty=%g, offset=1)" % duty)
    p = (3 * u) % 1
    ref = np.where(p < duty, -2 + 4 * p / duty if duty else 0, 2 - 4 * (p - duty) / (1 - duty) if duty < 1 else 0) + 1
    check("saw duty=%g" % duty, close(x[0], ref))
h, x, _ = render("1s triangle(1, 4Hz, phase=90deg)")
p = (4 * u + 0.25) % 1
check("triangle with phase", close(x[0], np.where(p < 0.5, -1 + 4 * p, 1 - 4 * (p - 0.5))))
h, x, _ = render("1s alpha(2, 20ms, delay=100ms, offset=-1)")
v = u - 0.1
check("alpha", close(x[0], -1 + np.where(v >= 0, 2 * v / 0.02 * np.exp(1 - v / 0.02), 0)))
h, x, _ = render("300ms sine(1, 7Hz); 200ms dc(0); 300ms sine(1, 7Hz, clock=global)")
t = np.arange(8000) / fs
check("clock=local restarts, clock=global follows t",
      close(x[0][:3000], np.sin(2 * np.pi * 7 * t[:3000])) and close(x[0][5000:], np.sin(2 * np.pi * 7 * t[5000:])))
data = np.array([0.0, 1, 4, 9, 16])
open(os.path.join(TMP, "d.txt"), "w").write("# comment\n" + "\n".join("%g, %g" % (k, y) for k, y in enumerate(data)))
h, x, _ = render('1s file("d.txt", rate=4Hz, column=2, interp=linear, extend=hold)', rate="8Hz")
check("file: linear interpolation, column 2, hold", close(x[0], [0, 0.5, 1, 2.5, 4, 6.5, 9, 12.5]))
h, x, _ = render('2s file("d.txt", rate=4Hz, column=2, extend=cycle)', rate="4Hz")
check("file: cycle", close(x[0], [0, 1, 4, 9, 16, 0, 1, 4]))
h, x, _ = render('2s file("d.txt", rate=4Hz, column=2, extend=zero)', rate="4Hz")
check("file: zero", close(x[0], [0, 1, 4, 9, 16, 0, 0, 0]))
check("file: digest recorded", len(h["provenance"]["files"]) == 1)
h, x, _ = render("100ms pulses(1, times=[10ms, 12ms, 50.05ms], shape=exp, tau=5ms)")
t = np.arange(1000) / fs
ref = sum(np.where(t >= s, np.exp(-(t - s) / 0.005), 0) for s in (0.010, 0.012, 0.0501))  # 50.05 ms -> grid 50.1? see below
on = [round(s * fs) for s in (0.010, 0.012, 0.05005)]
ref = sum(np.where(np.arange(1000) >= o, np.exp(-(np.arange(1000) - o) / fs / 0.005), 0) for o in on)
check("pulses: explicit times, exp kernels superpose, grid onsets", close(x[0], ref, 1e-12))
h, x, _ = render("100ms pulses(1, times=[10.03ms], width=1.04ms, align=exact)")
k = np.arange(1000)
check("pulses: align=exact evaluates at sample times",
      close(x[0], ((k / fs >= 0.01003) & (k / fs - 0.01003 < 0.00104)).astype(float)))
h, x, _ = render("1s pulses(2, 10Hz, shape=biexp, tau_rise=1ms, tau_decay=10ms, delay=25ms)")
tr, td = 0.001, 0.010
K = (tr / td) ** (tr / (td - tr)) - (tr / td) ** (td / (td - tr))
ref = np.zeros(10000)
for i in range(10):
    o = round((0.025 + i / 10) * fs)
    vv = (np.arange(10000) - o) / fs
    ref += np.where(vv >= 0, (np.exp(-vv / td) - np.exp(-vv / tr)) / K, 0)
check("pulses: biexp kernel, delay, peaks", close(x[0], 2 * ref, 1e-12))
h, x, _ = render("1s dc(3); 100ms ou(0, 1, 5ms, init=prev); 100ms ou(0, 1, 5ms, init=mean); 100ms ou(0, 1, 5ms, init=-4)")
check("ou init=prev / mean / value", x[0][10000] == 3 and x[0][11000] == 0 and x[0][12000] == -4)
h, x, _ = render("1s exp(sine(1, 1Hz)) + log(2 + sine(1, 1Hz)) - sqrt(1 + sine(1, 1Hz)) + max(sine(1,1Hz), 0) + pow(sine(1,1Hz), 2)")
s1 = np.sin(2 * np.pi * u)
check("maps exp, log, sqrt, max, pow", close(x[0], np.exp(s1) + np.log(2 + s1) - np.sqrt(1 + s1) + np.maximum(s1, 0) + s1 ** 2))

# ---------------------------------------------------------------- D
print("D. prev, end values, nesting")
h, x, _ = render("10ms ramp(0, 1); 10ms ramp(to=0)")
check("end value of a ramp is 'to' exactly (prev = 1, not the last sample)", x[0][100] == 1.0)
h, x, _ = render("10ms wnoise(0, 1); 10ms dc(prev)")
check("prev after noise = last sample", np.all(x[0][100:] == x[0][99]))
h, x, _ = render("10ms dc(5); { 10ms dc(prev) ; 10ms prev + 1 } ; 10ms dc(prev)")
check("prev inherited by first segment of a block, chained", list(x[0][::100]) == [5, 5, 6, 6])
h, x, _ = render("10ms dc(2); repeat 3 { 10ms prev * 2 }; repeat 0 { 10ms dc(9) }; 10ms dc(prev)")
check("prev chains through repeat copies; repeat 0 passes prev", list(x[0][::100]) == [2, 4, 8, 16, 16])
h, x, _ = render("10ms dc(-3); 10ms abs(prev) + 1")
check("prev inside an expression", x[0][100] == 4)
h, x, _ = render("10ms sine(1, 25Hz); 10ms dc(prev)")
check("end value of a sine is its right continuation", abs(x[0][150] - math.sin(2 * math.pi * 25 * 0.01)) < 1e-12)
h, x, _ = render("0.15ms dc(0); { 0.15ms dc(1) ; 0.15ms dc(2) } + 0", rate="10kHz")
check("nested block boundaries use absolute times", list(x[0]) == [0, 0, 1, 2], list(x[0]))

# ---------------------------------------------------------------- E
print("E. units")
h, x, _ = render("1s dc(0.5V) + dc(20mV)", unit="mV")
check("V and mV converted to mV", x[0][0] == 520)
h, x, _ = render("1s dc(2nS)", unit="pS")
check("nS converted to pS", abs(x[0][0] - 2000) < 1e-9)
h, x, _ = render("2s sine(1, 0.5Hz, phase=0.25rad); 1min dc(0)")
check("phase in rad, duration in min", close(x[0][:3], np.sin(2 * np.pi * 0.5 * np.arange(3) / fs + 0.25)) and x.shape[1] == 620000)
h, x, _ = render("1s sine(1, 0.001kHz)")
check("frequency in kHz", close(x[0], np.sin(2 * np.pi * u)))

# ---------------------------------------------------------------- F
print("F. stimulus rules")
open(os.path.join(TMP, "s.sg"), "w").write("""sg 2 stimulus
seed 99
duration 3s
channel A unit=mV rest=-70 { 1s dc(-60) ; @x 500ms dc(-50) }
channel B unit=pA { repeat 2 { @p 1s dc(1) } }
digital D { 2s pulses(1, 2Hz, width=100ms) }
marker m at 0.00005s
""")
h, x, _ = render(open(os.path.join(TMP, "s.sg")).read(), seed=None)
check("explicit duration and rest padding", x.shape[1] == 30000 and np.all(x[0][15000:] == -70) and x[0][0] == -60)
check("stimulus seed used without -s", h["provenance"]["master_seed"] == "99")
mk = sorted((m["name"], m["sample"]) for m in h["markers"])
check("labels in repeat give one marker per copy; explicit marker rounded half to even",
      mk == [("A.x", 10000), ("B.p", 0), ("B.p", 10000), ("m", 0)], mk)
h, x, _ = render(open(os.path.join(TMP, "s.sg")).read(), seed="7")
check("-s overrides the stimulus seed", h["provenance"]["master_seed"] == "7")

# ---------------------------------------------------------------- G
print("G. regeneration from the provenance record")
h, x, raw = render("1s ou(0, 1, 5ms) * sine(1, 3Hz)\n500ms cnoise(0, 2)\n", seed=None)
_b = open(os.path.join(TMP, "t.sgb"), "rb").read()
check("sgb: samples start on an 8-byte boundary (header padded)",
      (16 + int.from_bytes(_b[8:16], "little")) % 8 == 0 and len(_b) % 8 == 0)
prov = h["provenance"]
desc = prov["description"].split("\n", 1)[1]           # drop the "### file:" line
h2, x2, raw2 = render(desc, rate="%gHz" % prov["rate"], seed=prov["master_seed"])
import hashlib
check("drawn seed recorded; description + rate + seed regenerate the samples",
      hashlib.sha256(raw2).hexdigest() == prov["samples_sha256"])

# ---------------------------------------------------------------- H
print("H. statistics")
h, x, _ = render("200s ou(0, 2, 0.05ms)", rate="1kHz")     # dt = 20 tau
y = x[0]
check("ou exact for dt >> tau: sd %.3f (2)" % y.std(), abs(y.std() - 2) < 0.03)
check("ou exact for dt >> tau: uncorrelated samples", abs(np.corrcoef(y[:-1], y[1:])[0, 1]) < 0.01)
h, x, _ = render("100s wnoise(0, 1)", rate="10kHz")
P = np.abs(np.fft.rfft(x[0])) ** 2 / (len(x[0]) * 1e4) * 2      # one-sided PSD
check("wnoise one-sided PSD = 2 sd^2 dt = %.2e (got %.2e)" % (2e-4, P[1:].mean()), abs(P[1:].mean() / 2e-4 - 1) < 0.02)
h, x, _ = render("100s unoise(1, 2)", rate="10kHz")
y = x[0]
check("unoise mean, sd and range", abs(y.mean() - 1) < 0.02 and abs(y.std() - 2) < 0.02
      and y.min() >= 1 - 2 * 3 ** 0.5 and y.max() < 1 + 2 * 3 ** 0.5)
counts = []
for s in range(40):
    h, x, _ = render("10s pulses(1, 20Hz, width=0.1ms, timing=poisson)", rate="10kHz", seed=str(s))
    counts.append(np.sum(np.diff(np.r_[0, x[0]]) > 0))
check("poisson pulses: mean count %.1f (200), Fano %.2f (1)" % (np.mean(counts), np.var(counts) / np.mean(counts)),
      abs(np.mean(counts) - 200) < 8 and 0.5 < np.var(counts) / np.mean(counts) < 1.6)

# ---------------------------------------------------------------- I
print("I. compiler independence")
src_dir = os.path.join(os.path.dirname(SG))
exe0 = os.path.join(TMP, "sg_O0")
subprocess.run(["cc", "-std=c99", "-O0", "-ffp-contract=off", "-o", exe0] +
               sorted(__import__("glob").glob(os.path.join(src_dir, "*.c"))) + ["-lm"],
               check=True)
same = True
for text in ("1s ou(0, 1, 5ms) + sine(1, 3Hz)\n", "500ms cnoise(0, 1, alpha=1)\n",
             "1s pulses(1, 50Hz, shape=alpha, tau=2ms, timing=poisson)\n"):
    _, _, r1 = render(text)
    _, _, r0 = render(text, exe=exe0)
    same = same and r1 == r0
check("-O0 and -O2 builds write identical samples", same)

# ---------------------------------------------------------------- J
print("J. errors")
bad = {
    "label at end": "1s dc(0)\n@end\n",
    "repeat count not integer": "repeat 2.5 { 1s dc(0) }",
    "duration not a whole ps": "1.0000000000001s dc(0)",
    "prev for a time": "1s relax(0, 1, tau=prev)",
    "keyword not allowed": "1s chirp(1, 1Hz, 2Hz, law=log)",
    "parameter twice": "1s sine(1, freq=2Hz, freq=3Hz)",
    "positional after named": "1s sine(amp=1, 2Hz)",
    "too many arguments": "1s dc(1, 2)",
    "pulses without rate": "1s pulses(1, width=1ms)",
    "dead >= 1/rate": "1s pulses(1, 10Hz, width=1ms, timing=poisson, dead=200ms)",
    "exp chirp from 0 Hz": "1s chirp(1, 0Hz, 5Hz, law=exp)",
    "log of negative": "1s log(sine(1, 1Hz))",
    "pow of negative, fractional": "1s pow(sine(1, 1Hz), 0.5)",
    "file too short": '1s file("d.txt", rate=100Hz)',
    "cnoise empty band": "1s cnoise(0, 1, fmin=1.5Hz, fmax=1.9Hz)",
    "substitution outside protocol": "1s dc($a)",
    "unbalanced bracket": "1s dc(0",
    "unknown unit": "1s dc(0); 1s sine(1, 5Hertz)",
}
for what, text in bad.items():
    open(os.path.join(TMP, "e.sg"), "w").write(text)
    p = sg("check", "e.sg", ok=False)
    check("error: " + what, p.returncode == 1 and "error" in p.stderr, p.stderr.strip()[:120])
stim_bad = {
    "duration shorter than a channel": "sg 2 stimulus\nduration 1s\nchannel A unit=pA { 2s dc(0) }\n",
    "digital value not 0/1": "sg 2 stimulus\ndigital D { 1s dc(0.5) }\n",
    "copy of a copy": "sg 2 stimulus\nchannel A unit=pA { 1s dc(0) }\nchannel B unit=pA copy A\nchannel C unit=pA copy B\n",
    "amplitude unit vs channel unit": "sg 2 stimulus\nchannel A unit=pA { 1s dc(3mV) }\n",
    "channel defined twice": "sg 2 stimulus\nchannel A unit=pA { 1s dc(0) }\nchannel A unit=pA { 1s dc(0) }\n",
}
for what, text in stim_bad.items():
    open(os.path.join(TMP, "e.sg"), "w").write(text)
    p = sg("check", "e.sg", ok=False)
    check("error: " + what, p.returncode == 1 and "error" in p.stderr, p.stderr.strip()[:120])
open(os.path.join(TMP, "e.sg"), "w").write("sg 2 stimulus\nrate 10kHz\nchannel A unit=pA { 1s dc(0) }\n")
p = sg("render", "-r", "20kHz", "e.sg", ok=False)
check("error: --rate differs from the stimulus rate", p.returncode == 1)


# ---------------------------------------------------------------- K
print("K. protocols and the directory form")
from refstream import Stream as RefStream   # independent Philox/SHA-256 streams
import hashlib as _h, glob as _g


def seed64(text):
    return int.from_bytes(_h.sha256(text.encode()).digest()[:8], "big")


def ref_order(C, R, order, s):
    n = C * R
    tc = [j // R if order == "grouped" else j % C for j in range(n)]
    tr = [j % R if order == "grouped" else j // C for j in range(n)]
    if order in ("shuffled", "shuffled-blocks"):
        st = RefStream("order:%d" % s)
        blk = n if order == "shuffled" else C
        for b0 in range(0, n, blk):
            for j in range(blk - 1, 0, -1):
                q = int(math.floor(st.uniform() * (j + 1)))
                tc[b0 + j], tc[b0 + q] = tc[b0 + q], tc[b0 + j]
                tr[b0 + j], tr[b0 + q] = tr[b0 + q], tr[b0 + j]
    return list(zip(tc, tr))


def read_index(d):
    rows = []
    for line in open(os.path.join(d, "protocol.sgi")):
        if "condition" in line:
            name, sd = line.split()[:2]
            c = int(line.split("condition")[1].split(",")[0]); r = int(line.split("repetition")[1])
            rows.append((name[:-3], int(sd[5:]), c, r))
    return rows


PROTO = """sg 2 protocol
seed     %(seed)d
sweep    amp = from -300pA to 50pA step 50pA
repeat   %(R)d
order    %(order)s
noise    %(noise)s
period   5s

stimulus {
    channel Iinj unit=pA {
        500ms dc(0)
        @step
        100ms dc($amp)
        100ms ou(0, 5, 2ms)
    }
}
"""
for order in ("sequential", "grouped", "shuffled", "shuffled-blocks"):
    d = os.path.join(TMP, "x_" + order)
    open(os.path.join(TMP, "p.sg"), "w").write(PROTO % dict(seed=2026, R=3, order=order, noise="per-trial"))
    sg("expand", "-q", "p.sg", d)
    rows = read_index(d)
    want = ref_order(8, 3, order, 2026)
    check("order %s (8 conditions x 3)" % order, [(c, r) for _, _, c, r in rows] == want)
if True:
    rows = read_index(os.path.join(TMP, "x_shuffled-blocks"))
    check("shuffled-blocks: every block holds every condition once",
          all(sorted(c for _, _, c, _ in rows[b:b + 8]) == list(range(8)) for b in range(0, 24, 8)))
    rows = read_index(os.path.join(TMP, "x_shuffled"))
    check("trial seeds = first 8 bytes of SHA-256('trial:s:j')",
          all(sd == seed64("trial:2026:%d" % j) for j, (_, sd, _, _) in enumerate(rows)))
    texts = [open(os.path.join(TMP, "x_shuffled", n + ".sg")).read() for n, _, _, _ in rows]
    check("substitution: $amp -> the written value of the condition",
          all("dc(%dpA)" % (-300 + 50 * c) in t for t, (_, _, c, _) in zip(texts, rows)))

# render the protocol and the directory: same samples; noise from the trial seed
open(os.path.join(TMP, "p.sg"), "w").write(PROTO % dict(seed=7, R=2, order="shuffled", noise="per-trial"))
sg("render", "-q", "-r", "10kHz", "-o", "out_p", "p.sg")
sg("expand", "-q", "p.sg", "dir_p")
sg("render", "-q", "-r", "10kHz", "-o", "out_d", "dir_p")
rows = read_index(os.path.join(TMP, "dir_p"))
same, noise_ok, step_ok, prov_ok = True, True, True, True
for name, sd, c, r in rows:
    h1, x1, b1 = load(os.path.join(TMP, "out_p", name + ".sgb"))
    h2, x2, b2 = load(os.path.join(TMP, "out_d", name + ".sgb"))
    same = same and b1 == b2
    st = RefStream("%d:ch=Iinj/s2" % sd)
    rho = math.exp(-1e-4 / 2e-3)
    ref = [5 * st.gauss()]
    for k in range(1, 1000):
        ref.append(ref[-1] * rho + 5 * math.sqrt(1 - rho * rho) * st.gauss())
    noise_ok = noise_ok and close(x1[0][6000:7000], ref)
    step_ok = step_ok and np.all(x1[0][5000:6000] == -300 + 50 * c)
    tr = h1["provenance"]["trial"]
    prov_ok = prov_ok and tr["condition"] == c and tr["repetition"] == r and \
        tr["variables"]["amp"] == "%dpA" % (-300 + 50 * c) and h1["provenance"]["master_seed"] == str(sd) \
        and "sg 2 protocol" in h1["provenance"]["protocol"]
check("protocol and its directory form give identical samples (16 trials)", same)
check("each trial's noise comes from its own master seed", noise_ok)
check("each trial plays its condition", step_ok)
check("provenance: trial record, variables, protocol text", prov_ok)
h, x, raw = load(os.path.join(TMP, "out_p", rows[3][0] + ".sgb"))
desc = h["provenance"]["description"].split("\n", 1)[1]
_, x3, raw3 = render(desc, rate="10kHz", seed=h["provenance"]["master_seed"])
check("a trial is regenerated from its description and master seed", raw3 == raw)

# noise policies
open(os.path.join(TMP, "p.sg"), "w").write(PROTO % dict(seed=3, R=3, order="grouped", noise="per-condition"))
sg("render", "-q", "-r", "10kHz", "-o", "out_c", "p.sg")
X = [load(os.path.join(TMP, "out_c", "%04d.sgb" % j))[1][0] for j in range(24)]
check("noise per-condition: repetitions identical, conditions differ",
      all(np.array_equal(X[3 * c][6000:], X[3 * c + r][6000:]) for c in range(8) for r in range(3))
      and not np.array_equal(X[0][6000:], X[3][6000:]))
open(os.path.join(TMP, "p.sg"), "w").write(PROTO % dict(seed=3, R=2, order="sequential", noise="fixed"))
sg("render", "-q", "-r", "10kHz", "-o", "out_f", "p.sg")
X = [load(os.path.join(TMP, "out_f", "%04d.sgb" % j))[1][0] for j in range(16)]
check("noise fixed: every trial has the same noise", all(np.array_equal(X[0][6000:], y[6000:]) for y in X))
open(os.path.join(TMP, "p.sg"), "w").write(PROTO % dict(seed=3, R=2, order="sequential", noise="per-trial"))
sg("render", "-q", "-r", "10kHz", "-o", "out_t", "p.sg")
X = [load(os.path.join(TMP, "out_t", "%04d.sgb" % j))[1][0] for j in range(16)]
check("noise per-trial: repetitions differ", not np.array_equal(X[0][6000:], X[8][6000:]))
sg("render", "-q", "-r", "10kHz", "-s", "4", "-o", "out_s", "p.sg")
h, _, _ = load(os.path.join(TMP, "out_s", "0000.sgb"))
check("-s sets the protocol seed", h["provenance"]["trial"]["protocol_seed"] == "4"
      and h["provenance"]["master_seed"] == str(seed64("trial:4:0")))

# sweeps: product order, tuples, let, $( ), linspace, logspace, file template, use
open(os.path.join(TMP, "w.sg"), "w").write("10ms dc($a)\n10ms dc($(b * 2))\n10ms dc($c)\n")
open(os.path.join(TMP, "p.sg"), "w").write("""sg 2 protocol
sweep a = [1, 2]
sweep (b, d) = [(10, 1ms), (20, 2ms), (30, 3ms)]
let   c = a * 100 + b
stimulus "w.sg"
""")
sg("expand", "-q", "p.sg", "dir_s")
vals = []
for j in range(6):
    t = open(os.path.join(TMP, "dir_s", "%04d.sg" % j)).read()
    vals.append(t)
want = [(a, b) for a in (1, 2) for b in (10, 20, 30)]
check("Cartesian product, first sweep slowest; tuples; let; $( )",
      all("dc(%d)" % a in t and "dc(%d)" % (2 * b) in t and "dc(%d)" % (100 * a + b) in t
          for t, (a, b) in zip(vals, want)))
open(os.path.join(TMP, "p.sg"), "w").write("""sg 2 protocol
sweep f = logspace(1Hz, 1kHz, 4)
sweep w = linspace(0.5ms, 2ms, 4)
let   q = 200pA * 1ms / w
stimulus { 1s pulses($q, $f, width=$w) }
""")
sg("expand", "-q", "p.sg", "dir_l")
t0 = open(os.path.join(TMP, "dir_l", "0000.sg")).read()
t15 = open(os.path.join(TMP, "dir_l", "0015.sg")).read()
check("logspace and linspace in the unit of their first value; let in engineering notation",
      "pulses(400pA, 1Hz, width=0.5ms)" in t0 and "pulses(100pA, 1000Hz, width=2ms)" in t15, t0 + t15)
open(os.path.join(TMP, "n.sg"), "w").write("100ms ou(0, 1, 5ms)\n")
open(os.path.join(TMP, "p.sg"), "w").write("""sg 2 protocol
repeat 2
stimulus {
    channel A unit=pA use "n.sg"
}
""")
os.makedirs(os.path.join(TMP, "sub"), exist_ok=True)
sg("expand", "-q", "-s", "1", "p.sg", "sub/dir_u")
check("expand copies files used by the trials", os.path.exists(os.path.join(TMP, "sub", "dir_u", "n.sg")))
sg("render", "-q", "-r", "1kHz", "-o", "out_u", "sub/dir_u")
check("the copied directory renders on its own", os.path.exists(os.path.join(TMP, "out_u", "0001.sgb")))

# a directory without index: lexical order, seed from -s
d = os.path.join(TMP, "plain"); os.makedirs(d, exist_ok=True)
for name, lev in (("b.sg", 2), ("a.sg", 1), ("c.sg", 3)):
    open(os.path.join(d, name), "w").write("10ms dc(%d) + wnoise(0, 1)\n" % lev)
sg("render", "-q", "-r", "1kHz", "-s", "9", "-o", "out_plain", "plain")
xs = [load(os.path.join(TMP, "out_plain", n + ".sgb")) for n in "abc"]
check("directory without index: every .sg file, seed from -s",
      [round(np.mean(x[1][0]) - np.mean(x[1][0] - [1, 2, 3][i])) for i, x in enumerate(xs)] == [1, 2, 3]
      and all(x[0]["provenance"]["master_seed"] == "9" for x in xs))

# protocol errors
pbad = {
    "unknown variable": "sg 2 protocol\nstimulus { 1s dc($zz) }\n",
    "no stimulus": "sg 2 protocol\nrepeat 2\n",
    "bad order": "sg 2 protocol\norder random\nstimulus { 1s dc(0) }\n",
    "bad noise policy": "sg 2 protocol\nnoise sometimes\nstimulus { 1s dc(0) }\n",
    "derived dimension substituted": "sg 2 protocol\nsweep a=[1pA]\nlet q = a * 1s\nstimulus { 1s dc($q) }\n",
    "range that never arrives": "sg 2 protocol\nsweep a = from 0 to 10 step -1\nstimulus { 1s dc($a) }\n",
    "mixed dimensions in a range": "sg 2 protocol\nsweep a = from 0pA to 1s step 1pA\nstimulus { 1s dc($a) }\n",
    "repeat zero": "sg 2 protocol\nrepeat 0\nstimulus { 1s dc(0) }\n",
    "period without unit": "sg 2 protocol\nperiod 5\nstimulus { 1s dc(0) }\n",
    "error inside a trial": "sg 2 protocol\nsweep a = [1, -1]\nstimulus { 1s sqrt(dc($a)) }\n",
}
for what, text in pbad.items():
    open(os.path.join(TMP, "e.sg"), "w").write(text)
    p = sg("check", "e.sg", ok=False)
    check("protocol error: " + what, p.returncode == 1 and "error" in p.stderr, p.stderr.strip()[:120])

shutil.rmtree(TMP, ignore_errors=True)
print("\n%s" % ("all thorough tests passed" if fails == 0 else "%d thorough test(s) FAILED" % fails))
sys.exit(1 if fails else 0)
