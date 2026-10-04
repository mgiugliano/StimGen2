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
skill/stimgen2: the agent skill stays true to sg.

    python3 test_skill.py ./sg

Checks that the derived files are up to date (skill/build.py --check), that
every example and every StimGen code block in SKILL.md and references/ is
accepted by sg, and that the scripts (sg_run, sgb_describe, sgb_plot,
sgconvert) work on a stimulus and on a protocol. Plots are skipped without
matplotlib.
"""
import json, os, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SKILL = os.path.join(ROOT, "skill", "stimgen2")
SCRIPTS = os.path.join(SKILL, "scripts")
SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
D = tempfile.mkdtemp(prefix="sgskill")
ENV = dict(os.environ, SG_BIN=SG)
fails = 0


def check(name, ok, detail=""):
    global fails
    print(("ok   " if ok else "FAIL ") + name + ("" if ok else ": " + detail))
    fails += not ok


def run(*args, **kw):
    return subprocess.run(list(args), capture_output=True, text=True, cwd=D, env=ENV, **kw)


def code_blocks(path):
    """Fenced blocks without a language tag that look like StimGen text."""
    out = []
    for b in re.findall(r"```\n(.*?)```", open(path).read(), re.S):
        first = b.strip().splitlines()[0] if b.strip() else ""
        if first.startswith(("sg ", "python", "import", "{\n")) and not first.startswith("sg 2"):
            continue       # shell commands and Python
        if first.startswith(("file ", "sg render", "sg check", "|")) or ":=" in b or '"version"' in b:
            continue       # grammar, JSON, command lines
        out.append(b)
    return out


# 1. derived files
r = subprocess.run([sys.executable, os.path.join(ROOT, "skill", "build.py"), "--check"],
                   capture_output=True, text=True)
check("derived files up to date (skill/build.py --check)", r.returncode == 0, r.stdout + r.stderr)

# 2. examples and code blocks
with open(os.path.join(D, "trace.txt"), "w") as f:
    f.write("\n".join(str(k % 7) for k in range(200000)))
for name in sorted(os.listdir(os.path.join(SKILL, "examples"))):
    r = run(SG, "check", "-r", "20kHz", os.path.join(SKILL, "examples", name))
    check("example " + name, r.returncode == 0, r.stderr)
for doc in ["SKILL.md", "references/language.md"]:
    for i, b in enumerate(code_blocks(os.path.join(SKILL, doc))):
        p = os.path.join(D, "block.sg")
        open(p, "w").write(b)
        r = run(SG, "check", "-r", "20kHz", p)
        check(f"{doc} block {i}", r.returncode == 0, r.stderr.strip() + "\n" + b)
inline = re.findall(r"\| `([^`|]*(?:dc|ramp|chirp|pulses|ou)\([^`|]*)` ",
                    open(os.path.join(SKILL, "references", "language.md")).read())
for e in inline:
    if "$" in e or "..." in e:
        continue
    r = run(SG, "check", "-r", "20kHz", "-e", e if re.match(r"^\d", e) else "1s " + e)
    check("language.md table: " + e, r.returncode == 0, r.stderr)

# 3. scripts
py = sys.executable
r = run(py, os.path.join(SCRIPTS, "sg_run.py"), "which")
check("sg_run which", r.returncode == 0 and json.loads(r.stdout)["sg"] == SG, r.stdout + r.stderr)
r = run(py, os.path.join(SCRIPTS, "sg_run.py"), "check", "-e", "1s sine(1)")
check("sg_run check reports an error", r.returncode == 1 and "freq" in json.loads(r.stdout)["message"])
r = run(py, os.path.join(SCRIPTS, "sg_run.py"), "render", "-r", "10kHz", "-s", "3",
        os.path.join(SKILL, "examples", "fi_protocol.sg"), "-o", "fi")
files = json.loads(r.stdout)["files"] if r.returncode == 0 else []
check("sg_run render protocol -> 33 trials", len(files) == 33, r.stdout + r.stderr)
r = run(py, os.path.join(SCRIPTS, "sg_run.py"), "render", "-s", "1",
        os.path.join(SKILL, "examples", "pair.sg"), "-o", "pair.sgb")
check("sg_run render stimulus", r.returncode == 0 and json.loads(r.stdout)["files"] == ["pair.sgb"], r.stdout)

sys.path.insert(0, SCRIPTS)
os.environ["SG_BIN"] = SG
from sg_run import render, load, check as sgcheck   # noqa: E402
cwd = os.getcwd()
os.chdir(D)
try:
    ok, msg = sgcheck(text=open(os.path.join(SKILL, "examples", "two_sweeps.sg")).read(), rate="1kHz")
    check("sg_run.check() on a multi-line protocol", ok and "9 trial" in msg, msg)
    p = render(text=open(os.path.join(SKILL, "examples", "frozen_vs_fresh.sg")).read(), rate="1kHz", seed=5)
    h0, x0 = load(p[0]); h1, x1 = load(p[1])
    check("render() of text: frozen noise identical across trials", len(p) == 10 and (x0 == x1).all())
    q = render(text="500ms dc(0); 1s dc(300); 500ms dc(0)", rate="1kHz", unit="pA", out="step.sgb")
    h, x = load(q[0])
    check("render() of -e text with unit", h["channels"][0]["unit"] == "pA" and x.max() == 300)
finally:
    os.chdir(cwd)

r = run(py, os.path.join(SCRIPTS, "sgb_describe.py"), "step.sgb")
check("sgb_describe text", r.returncode == 0 and "0.5 s to        1.5 s   300 pA" in r.stdout, r.stdout + r.stderr)
r = run(py, os.path.join(SCRIPTS, "sgb_describe.py"), "fi")
check("sgb_describe directory", r.returncode == 0 and "33 trial file(s)" in r.stdout and "amp=" in r.stdout, r.stdout)
r = run(py, os.path.join(SCRIPTS, "sgb_describe.py"), "pair.sgb", "--json")
s = json.loads(r.stdout) if r.returncode == 0 else {}
check("sgb_describe --json", [c["name"] for c in s.get("channels", [])] == ["pre", "post", "gsyn", "cam"]
      and s["master_seed"] == "1" and any(m["name"] == "pre.train" for m in s["markers"]), r.stdout[:300])
try:
    import matplotlib  # noqa: F401
    for args, out in [(["pair.sgb"], "pair.png"), (["fi", "--stack", "-n", "3", "-o", "fi.png"], "fi.png")]:
        r = run(py, os.path.join(SCRIPTS, "sgb_plot.py"), *args)
        check("sgb_plot " + " ".join(args), r.returncode == 0 and os.path.getsize(os.path.join(D, out)) > 5000,
              r.stderr)
except ImportError:
    print("skip sgb_plot (no matplotlib)")
r = run(py, os.path.join(SCRIPTS, "sgconvert.py"), "neuron", "step.sgb", "-o", "step")
lines = open(os.path.join(D, "step_out.dat")).read().split("\n") if r.returncode == 0 else []
check("sgconvert neuron (pA -> nA)", r.returncode == 0 and any(l.endswith(" 0.3") or l.endswith("\t0.3")
                                                               for l in lines), r.stdout + r.stderr)

shutil.rmtree(D, ignore_errors=True)
print(f"\n{'all skill checks passed' if not fails else str(fails) + ' check(s) FAILED'}")
sys.exit(1 if fails else 0)
