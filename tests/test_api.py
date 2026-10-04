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
The library interface (src/sg_api.c) must give exactly what the command line
gives.  Runs the corpus of compare_cli.py through sg and through a driver of
the library, and compares .sgb files, error messages, canonical forms,
summaries, protocol expansions and help texts.

    python3 test_api.py SG DRIVER...            # e.g. ./sg ./api_main
    python3 test_api.py --level-b SG DRIVER...  # e.g. ./sg node ../web/node_driver.mjs

DRIVER takes the arguments of tests/api_main.c.  With --level-b (used for
the WebAssembly build, whose math library differs from the native one) the
samples may differ in the last bits of sin/exp/log (spec Sec. 11.5): they
are compared with a tolerance, and everything else must still be equal.
"""
import json, os, re, shutil, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
args = sys.argv[1:]
LEVEL_B = args and args[0] == "--level-b"
if LEVEL_B:
    args = args[1:]
SG, DRIVER = os.path.abspath(args[0]), args[1:]
DRIVER = [os.path.abspath(a) if os.path.exists(a) else a for a in DRIVER]
import compare_cli as corpus                                  # noqa: E402

fails, n_cases, n_bitexact, n_samples_cmp, max_rel = 0, 0, 0, 0, 0.0


def check(name, ok, detail=""):
    global fails
    if not ok:
        fails += 1
        print("FAIL  %s\n      %s" % (name, str(detail)[:600]))


def fresh(text):
    d = tempfile.mkdtemp(prefix="sgapi")
    for f, c in corpus.FIXTURES.items():
        open(os.path.join(d, f), "w").write(c)
    open(os.path.join(d, "editor"), "w").write(text)
    return d


def run(cmd, d):
    p = subprocess.run(cmd, cwd=d, capture_output=True)
    return p.returncode, p.stdout, p.stderr.decode()


def split(b):
    L = int.from_bytes(b[8:16], "little")
    return json.loads(b[16:16 + L]), b[16:16 + L], b[16 + L:]


def errline(stderr):
    return [l for l in stderr.splitlines() if l.startswith("sg: error:")]


def compare_render(name, text, rate, seed, unit):
    global n_cases, n_bitexact, n_samples_cmp, max_rel
    d = fresh(text)
    try:
        cli = [SG, "render", "-q", "-o", "cli.sgb"] + (["-r", rate] if rate else []) + \
              (["-s", seed] if seed else []) + (["-u", unit] if unit else []) + ["editor"]
        c1, _, e1 = run(cli, d)
        c2, out, e2 = run(DRIVER + ["render", "editor", rate, seed, unit], d)
        n_cases += 1
        if c1 or c2:
            check(name + ": same error", (c1 != 0) == (c2 != 0) and errline(e1) == errline(e2),
                  "cli %d %s | lib %d %s" % (c1, errline(e1), c2, errline(e2)))
            return
        a = open(os.path.join(d, "cli.sgb"), "rb").read()
        ha, ra, sa = split(a)
        hb, rb, sb = split(out)
        ts = re.compile(rb'"timestamp": "[^"]*"')
        if not seed:                                  # each side drew its own seed
            ts = re.compile(rb'"timestamp": "[^"]*"|"master_seed": "\d+"')
        if ts.sub(b"", ra).rstrip(b" ") == ts.sub(b"", rb).rstrip(b" ") and sa == sb:   # (padding)
            n_bitexact += 1
            return
        if not LEVEL_B:
            check(name + ": identical .sgb", False, "the files differ")
            return
        for h in (ha, hb):
            h["provenance"]["timestamp"] = h["provenance"]["samples_sha256"] = None
            if not seed:
                h["provenance"]["master_seed"] = None          # drawn on each side
        xa, xb = np.frombuffer(sa, "<f8"), np.frombuffer(sb, "<f8")
        scale = max(1.0, float(np.max(np.abs(xa), initial=0)))
        rel = float(np.max(np.abs(xa - xb), initial=0)) / scale if xa.shape == xb.shape else np.inf
        max_rel = max(max_rel, rel)
        n_samples_cmp += 1
        check(name + ": same header, samples within Level B", ha == hb and rel < 1e-12,
              "header equal: %s, max relative difference %g" % (ha == hb, rel))
    finally:
        shutil.rmtree(d, ignore_errors=True)


def compare_text(name, text, cli_args, drv_args, transform=None):
    d = fresh(text)
    try:
        c1, o1, e1 = run([SG] + cli_args, d)
        c2, o2, e2 = run(DRIVER + drv_args, d)
        if c1 or c2:
            check(name, (c1 != 0) == (c2 != 0) and errline(e1) == errline(e2), (errline(e1), errline(e2)))
        else:
            o1, o2 = o1.decode(), o2.decode()
            if transform:
                o1, o2 = transform(o1, o2, d)
            check(name, o1 == o2, "cli: %r\n      lib: %r" % (o1[:300], o2[:300]))
    finally:
        shutil.rmtree(d, ignore_errors=True)


def kind_of(text):
    m = re.match(r"\s*(?:#[^\n]*\n\s*)*sg\s+2\s+(\w+)", text)
    return m.group(1) if m else "waveform"


# --- render, canon, kind, check on every single-stimulus input
inputs = []
for f, c in sorted(corpus.FIXTURES.items()):
    if f.endswith(".sg"):
        inputs.append((f, c))
for name, text in inputs:
    k = kind_of(text)
    if k in ("protocol", "index"):             # several trials: see below
        continue
    rate = "20kHz" if not name.startswith("rnd") else "10kHz"
    for seed, unit in (("7", "pA"), ("7", ""), ("", "pA")):
        if seed == "" and "ou(" not in text and "noise" not in text and "poisson" not in text:
            compare_render(name + " drawn-seed", text, rate, seed, unit)     # deterministic text
        elif seed:
            compare_render("%s render -s %s -u %s" % (name, seed, unit), text, rate, seed, unit)
    compare_text(name + " canon", text, ["canon", "-u", "pA", "editor"], ["canon", "editor", "pA"])
    d = fresh(text)
    c2, o2, e2 = run(DRIVER + ["kind", "editor"], d)
    if c2:                                     # a bad header: the same error as sg check
        c1, _, e1 = run([SG, "check", "editor"], d)
        check(name + " kind", c1 != 0 and errline(e1) == errline(e2), (errline(e1), errline(e2)))
    else:
        check(name + " kind", o2.decode() == k, (o2, k))
    shutil.rmtree(d, ignore_errors=True)
    def summary(o1, o2, d):
        m = re.search(r"ok, (\d+) channel\(s\), (\d+) samples at ([0-9.e+]+) Hz", o1)
        j = json.loads(o2)
        return ((int(m.group(1)), int(m.group(2)), float(m.group(3))) if m else o1,
                (j["channels"], j["samples"], j["rate"]))
    compare_text(name + " check", text, ["check", "-u", "pA", "editor"], ["check", "editor", "pA"], summary)

# --- protocols: expansion
for name, text in inputs:
    if kind_of(text) != "protocol":
        continue
    def trials(o1, o2, d):
        sub = os.path.join(d, "E")
        idx = [l.split() for l in open(os.path.join(sub, "protocol.sgi")) if "seed=" in l]
        cli = [(r[0][:-3], r[1][5:], open(os.path.join(sub, r[0])).read()) for r in idx]
        j = json.loads(o2)
        return cli, [(t["name"], t["seed"], t["text"]) for t in j["trials"]]
    compare_text(name + " expand", text, ["expand", "-q", "-s", "5", "editor", "E"],
                 ["expand", "editor", "5"], trials)

# --- help texts and the generator table
topics = ["", "syntax", "maps", "units", "stimulus", "noise", "output", "protocol", "license",
          "examples", "generators", "sine", "pulses", "clip", "nosuchtopic"]
for t in topics:
    compare_text("help " + (t or "(overview)"), "", ["help"] + ([t] if t else []), ["help", t])
d = fresh("")
c, out, _ = run(DRIVER + ["prims"], d)
shutil.rmtree(d, ignore_errors=True)
pr = json.loads(out) if c == 0 else []
check("generator table: 16 primitives and 10 functions",
      len(pr) == 26 and sum(1 for p in pr if not p["map"]) == 16 and pr[11]["name"] == "pulses")

print("%d renders: %d byte-identical to sg%s" % (n_cases, n_bitexact,
      ("; %d within Level B (largest relative difference %.2g)" % (n_samples_cmp, max_rel)) if LEVEL_B else ""))
print("all api tests passed" if fails == 0 else "%d api test(s) FAILED" % fails)
sys.exit(1 if fails else 0)
