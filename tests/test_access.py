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
Run every code block of docs/access/sgb-access.md against a file written
by sg, and check that C, Python and Julia read the same values.

    python3 test_access.py ./sg

Julia is optional (skipped if not installed); the JSON.jl example runs only
if the JSON package can be loaded (set JULIA_DEPOT_PATH to use another depot).
"""
import os, re, shutil, subprocess, sys, tempfile
import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
DOC = os.path.join(HERE, "..", "docs", "access", "sgb-access.md")
SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
TMP = tempfile.mkdtemp(prefix="sgaccess")
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("ok    " if ok else "FAIL  ") + name + ("" if ok else "\n" + str(detail)))
    fails += 0 if ok else 1


def run(cmd, env=None):
    p = subprocess.run(cmd, capture_output=True, text=True, cwd=TMP,
                       env=dict(os.environ, **(env or {})))
    return p.returncode, p.stdout + p.stderr


# 1. Extract the code blocks that carry file="..."
text = open(DOC).read()
blocks = re.findall(r'```\{\.(\w+) file="([^"]+)"\}\n(.*?)```', text, re.S)
for lang, name, code in blocks:
    open(os.path.join(TMP, name), "w").write(code)
names = [n for _, n, _ in blocks]
check("code blocks found: " + ", ".join(names), len(blocks) == 7, names)

# 2. The example file, and the truth computed here
code, out = run([SG, "render", "-q", "pair.sg"])
check("sg renders pair.sg", code == 0, out)
b = open(os.path.join(TMP, "pair.sgb"), "rb").read()
L = int.from_bytes(b[8:16], "little")
x = np.frombuffer(b, "<f8", offset=16 + L).reshape(3, -1)
truth = ["pair.sgb: 3 channel(s) x 3000 samples at 10000 Hz"]
for nm, row in zip(("pre", "post", "cam"), x):
    truth.append("%s [%s]: min %.6g, max %.6g, mean %.6g"
                 % (nm, "1" if nm == "cam" else "pA", row.min(), row.max(), row.mean()))
truth += ["marker pre.train at 0.1 s", "master seed 42"]

# 3. Each summary program prints exactly the same lines
results = {}
code, out = run([sys.executable, "read_sgb.py", "pair.sgb"]); results["python/numpy"] = (code, out)
code, out = run([sys.executable, "read_sgb_stdlib.py", "pair.sgb"]); results["python/stdlib"] = (code, out)
cc = shutil.which("cc") or shutil.which("gcc")
code, out = run([cc, "-std=c99", "-pedantic", "-Wall", "-Wextra", "-O2", "-o", "read_sgb", "read_sgb.c"])
check("C example compiles without warnings", code == 0 and not out.strip(), out)
results["C"] = run(["./read_sgb", "pair.sgb"])
julia = shutil.which("julia")
if julia:
    results["julia/Base"] = run([julia, "--startup-file=no", "read_sgb.jl", "pair.sgb"])
else:
    print("skip  Julia not installed")
for lang, (code, out) in results.items():
    lines = out.strip().splitlines()
    check("%-14s prints the expected summary" % lang, code == 0 and lines == truth,
          "\n".join(lines) + "\n--- expected ---\n" + "\n".join(truth))

# 4. The tools: memory map, digest, regeneration
code, out = run([sys.executable, "sgb_tools.py", "pair.sgb"], env={"SG": SG})
check("python tools: memory map, digest and regeneration",
      code == 0 and "post mean %.6g" % x[1].mean() in out and "digest ok: True" in out
      and "regenerated identically: True" in out, out)
if julia:
    code, out = run([julia, "--startup-file=no", "-e", "import JSON"])
    if code == 0:
        code, out = run([julia, "--startup-file=no", "sgb_tools.jl", "pair.sgb"])
        check("julia tools: JSON.jl header, memory map, digest",
              code == 0 and "channels: pre, post, cam; rate 10000 Hz" in out
              and "post mean %s" % float("%.6g" % x[1].mean()) in out.replace("e+0", "e")
              and "digest ok: true" in out, out)
    else:
        print("skip  sgb_tools.jl: the Julia package JSON is not installed")

shutil.rmtree(TMP, ignore_errors=True)
print("\n%s" % ("all access tests passed" if fails == 0 else "%d access test(s) FAILED" % fails))
sys.exit(1 if fails else 0)
