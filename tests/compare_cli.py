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
Compare two builds of sg on a large corpus of command lines.

    python3 compare_cli.py REF_SG NEW_SG

Every case runs in a fresh directory with the same fixture files, once with
each binary.  Exit codes, stdout, stderr and every file written must be
identical.  Only these are masked: the time stamp of .sgb headers, temporary
directory names, and -- where a seed was drawn from the operating system, so
that no two runs can agree -- the seed-dependent values.
"""
import glob, json, os, random, re, shutil, subprocess, sys, tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
DOCS = os.path.join(HERE, "..", "docs")

FIXTURES = {
    "step.sg": "500ms dc(0)\n1s dc(300)\n500ms dc(0)\n",
    "noise.sg": "1s ou(0, 10, 5ms)\n",
    "d.txt": "# data\n0\n1\n4\n9\n16\n",
    "pair.sg": "sg 2 stimulus\nrate 10kHz\nseed 3\nchannel pre unit=pA { 100ms dc(0); @t 200ms pulses(2000, 20Hz, width=1ms) }\n"
               "channel post unit=pA use \"noise.sg\"\nchannel cp unit=pA copy post\ndigital cam { 300ms pulses(1, 100Hz, width=1ms) }\nmarker m at 0.15s\n",
    "steps.sg": "sg 2 protocol\nseed 11\nsweep amp = from -300pA to 300pA step 150pA\nsweep (d, k) = [(100ms, 1), (200ms, 2)]\n"
                "let q = amp * 2\nrepeat 2\norder shuffled-blocks\nnoise per-condition\nperiod 5s\n"
                "stimulus {\n channel I unit=pA { 50ms dc(0); @s $d dc($amp); 50ms ou(0, 5, 2ms) }\n}\n",
    "wprot.sg": "sg 2 protocol\nseed 4\nsweep f = logspace(1Hz, 100Hz, 3)\nstimulus { 200ms sine(1, $f) + wnoise(0, 0.1) }\n",
}

cases = []                                   # (name, argv list or shell string)


def add(name, *argv):
    cases.append((name, list(argv)))


# --- help, info, usage errors
for a in ([], ["-h"], ["--help"], ["help"], ["version"], ["--version"], ["license"], ["examples"],
          ["selftest"], ["frobnicate"], ["render"], ["render", "-r"], ["render", "-x", "a"],
          ["render", "-r", "10kHz", "a", "b"], ["render", "-e", "1s dc(0)"], ["render", "-r", "abc", "-e", "1s dc(0)"],
          ["render", "-r", "10kHz", "-s", "x1", "-e", "1s dc(0)"], ["render", "-r", "10kHz", "step.sg", "-e", "1s dc(0)"],
          ["render", "-r", "10kHz", "missing.sg"], ["expand", "step.sg", "out"], ["expand", "steps.sg"],
          ["canon", "steps.sg"], ["help", "nosuchtopic"]):
    add("cli " + " ".join(a), *a)
for t in ("syntax", "maps", "functions", "units", "stimulus", "noise", "seeds", "output", "protocol",
          "protocols", "license", "examples", "generators", "primitives"):
    add("help " + t, "help", t)
for p in ("dc", "ramp", "relax", "sine", "square", "saw", "triangle", "chirp", "biexp", "alpha", "file",
          "pulses", "ou", "wnoise", "unoise", "cnoise", "abs", "pos", "sqrt", "exp", "log", "pow", "spow",
          "clip", "min", "max"):
    add("help " + p, "help", p)

# --- the fixture files, every command
for f in ("step.sg", "pair.sg"):
    for extra in ([], ["-u", "pA"]):
        add("check %s %s" % (f, extra), "check", *extra, f)
        add("canon %s %s" % (f, extra), "canon", *extra, f)
        add("render %s %s" % (f, extra), "render", "-r", "10kHz", "-s", "5", *extra, f)
        add("render -t %s %s" % (f, extra), "render", "-r", "10kHz", "-s", "5", "-t", *extra, f)
        add("render -t stdout %s" % f, "render", "-r", "1kHz", "-s", "5", "-t", "-o", "-", f)
add("render pair drawn seed", "render", "-r", "10kHz", "pair.sg")         # stimulus seed 3
add("render step quiet", "render", "-q", "-r", "20kHz", "-s", "1", "step.sg", "-o", "q.sgb")
for f in ("steps.sg", "wprot.sg"):
    add("check " + f, "check", f)
    add("check -s " + f, "check", "-s", "9", f)
    add("render " + f, "render", "-r", "10kHz", f)
    add("render -s " + f, "render", "-r", "10kHz", "-s", "9", f, "-o", "outdir")
    add("render -t " + f, "render", "-r", "1kHz", "-t", f)
    add("expand " + f, "expand", f, "exp")
    add("expand -s " + f, "expand", "-s", "2", f, "exp2")
cases.append(("render expanded directory", "%(sg)s expand -q steps.sg D && %(sg)s render -r 10kHz -o R D && %(sg)s check D"))
cases.append(("directory without index", "mkdir P && cp step.sg P/a.sg && printf '1s wnoise(0,1)\\n' > P/b.sg && "
              "%(sg)s render -r 1kHz -s 3 -o R P && %(sg)s render -r 1kHz -o R2 P/"))
cases.append(("index file given", "%(sg)s expand -q -s 1 steps.sg D && %(sg)s render -r 10kHz -o R D/protocol.sgi"))

# --- every example of the documents
blocks = []
for md in sorted(glob.glob(os.path.join(DOCS, "src", "*.md"))) + [os.path.join(DOCS, "access", "sgb-access.md")]:
    for m in re.finditer(r"```(?:\{[^}]*\})?\n(.*?)```", open(md).read(), re.S):
        code = m.group(1)
        if re.match(r"\s*(sg 2|[0-9.]+(s|ms|us|min)\s|\{|repeat\s)", code) and "$ " not in code[:3]:
            blocks.append(code)
for i, code in enumerate(blocks):
    FIXTURES["doc%03d.sg" % i] = code
    for argv in (["check", "-u", "pA"], ["canon", "-u", "pA"], ["render", "-r", "20kHz", "-s", "7", "-u", "pA"]):
        add("doc%03d %s" % (i, argv[0]), *argv, "doc%03d.sg" % i)

# --- the examples of 'sg examples', as a script
cases.append(("sg examples script", "EXAMPLES"))

# --- error corpus (from the test suites)
errors = ["dc(0)", "1 dc(0)", "1s sine(1, 10ms)", "1s 1 / sine(1, 1Hz)", "1s sqrt(sine(1, 1Hz))",
          "{1s dc(0)} + {2s dc(0)}", "1s foo(1)", "1s ramp(4)", "1s biexp(1, 5ms, 2ms)", "1s dc(3pA)",
          "1s dc(0)\n@end\n", "repeat 2.5 { 1s dc(0) }", "1.0000000000001s dc(0)", "1s relax(0, 1, tau=prev)",
          "1s chirp(1, 1Hz, 2Hz, law=log)", "1s sine(1, freq=2Hz, freq=3Hz)", "1s sine(amp=1, 2Hz)",
          "1s dc(1, 2)", "1s pulses(1, width=1ms)", "1s pulses(1, 10Hz, width=1ms, timing=poisson, dead=200ms)",
          "1s chirp(1, 0Hz, 5Hz, law=exp)", "1s log(sine(1, 1Hz))", "1s pow(sine(1, 1Hz), 0.5)",
          '1s file("d.txt", rate=100Hz)', "1s cnoise(0, 1, fmin=1.5Hz, fmax=1.9Hz)", "1s dc($a)", "1s dc(0",
          "1s dc(0); 1s sine(1, 5Hertz)", "sg 3 waveform\n1s dc(0)\n", "sg 2 recipe\n", "1s dc(0) dc(1)",
          "sg 2 stimulus\nduration 1s\nchannel A unit=pA { 2s dc(0) }\n", "sg 2 stimulus\ndigital D { 1s dc(0.5) }\n",
          "sg 2 stimulus\nchannel A unit=pA { 1s dc(0) }\nchannel B unit=pA copy A\nchannel C unit=pA copy B\n",
          "sg 2 stimulus\nchannel A unit=pA { 1s dc(3mV) }\n", "sg 2 stimulus\nchannel A unit=pA { 1s dc(0) }\nchannel A unit=pA { 1s dc(0) }\n",
          "sg 2 stimulus\nchannel A unit=kg { 1s dc(0) }\n", "sg 2 stimulus\nrate 10kHz\nchannel A unit=pA { 1s dc(0) }\n",
          "sg 2 protocol\nstimulus { 1s dc($zz) }\n", "sg 2 protocol\nrepeat 2\n", "sg 2 protocol\norder random\nstimulus { 1s dc(0) }\n",
          "sg 2 protocol\nnoise sometimes\nstimulus { 1s dc(0) }\n", "sg 2 protocol\nsweep a=[1pA]\nlet q = a * 1s\nstimulus { 1s dc($q) }\n",
          "sg 2 protocol\nsweep a = from 0 to 10 step -1\nstimulus { 1s dc($a) }\n", "sg 2 protocol\nrepeat 0\nstimulus { 1s dc(0) }\n",
          "sg 2 protocol\nperiod 5\nstimulus { 1s dc(0) }\n", "sg 2 protocol\nsweep a = [1, -1]\nstimulus { 1s sqrt(dc($a)) }\n",
          "1s sine(1, 9kHz)", "1s pulses(1, 10Hz, width=10us)", "sg 2 stimulus\nchannel A unit=pA { 1s dc(0) }\nchannel B unit=pA { 2s dc(0) }\n",
          "1s pulses(1, 10Hz, width=1ms, seed=4)", "100us dc(0); 10us dc(1); 100us dc(0)"]
for i, e in enumerate(errors):
    FIXTURES["err%03d.sg" % i] = e
    add("err%03d check" % i, "check", "err%03d.sg" % i)
    add("err%03d render" % i, "render", "-r", "20kHz", "-s", "1", "err%03d.sg" % i)

# --- random waveforms (the generator of test_thorough, fixed seed)
rng = random.Random(77)
GENS = ["dc({a})", "ramp({a}, {b})", "relax({a}, {b}, {t}ms)", "sine({a}, {f}Hz, phase={p}deg, clock=global)",
        "square({a}, {f}Hz, duty={d}%)", "saw({a}, {f}Hz)", "chirp({a}, {f}Hz, 40Hz, law=exp)", "biexp({a}, 2ms, {t}ms)",
        "alpha({a}, {t}ms)", "pulses({a}, {f}Hz, width=2ms)", "pulses({a}, {f}Hz, shape=exp, tau={t}ms, timing=poisson)",
        "ou({a}, 1, {t}ms)", "ou({a}, 1, {t}ms, init=mean, seed=3)", "wnoise(0, {a})", "unoise({a}, 1)",
        "cnoise(0, 1, alpha=1.5)", "prev", 'file("d.txt", rate=50Hz, extend=cycle, interp=linear)']
for i in range(80):
    segs = []
    for _ in range(rng.randint(1, 4)):
        g = rng.choice(GENS).format(a=rng.choice([-2, 0.5, 3]), b=rng.choice([0, 4]), t=rng.choice([3, 20]),
                                    f=rng.choice([2, 13]), p=rng.choice([0, 90]), d=rng.choice([25, 75]))
        if rng.random() < 0.3:
            g = "abs(%s) %s %s" % (g, rng.choice("+*-"), rng.choice(GENS[:4]).format(a=1, b=2, t=5, f=3, p=0, d=50))
        segs.append("%dms %s" % (rng.randint(30, 300), g))
    FIXTURES["rnd%03d.sg" % i] = "\n".join(segs) + "\n"
    add("rnd%03d" % i, "render", "-r", rng.choice(["7kHz", "10kHz", "44.1kHz"]), "-s", str(i), "rnd%03d.sg" % i)


# ------------------------------------------------------------------ runner
TS = re.compile(rb'"timestamp": "[^"]*"')


def normalize_text(t, tmp):
    t = t.replace(tmp, "<TMP>")
    t = re.sub(r"(master seed|protocol seed) \d+", r"\1 <SEED>", t)
    return t


def run_case(sg, name, argv, tmp):
    for f, c in FIXTURES.items():
        open(os.path.join(tmp, f), "w").write(c)
    if argv == "EXAMPLES":
        text = subprocess.run([sg, "examples"], capture_output=True, text=True).stdout
        out, inside = ["set -e"], False
        for line in text.splitlines():
            if inside:
                out.append(line); inside = line != "END"
            elif line.startswith("  "):
                out.append(line[2:]); inside = "<<'END'" in line
            else:
                out.append("# " + line)
        script = "\n".join(out) + "\n"
        bindir = tempfile.mkdtemp()                       # the binary under the name 'sg'
        os.symlink(sg, os.path.join(bindir, "sg"))
        env = dict(os.environ, PATH=bindir + os.pathsep + os.environ["PATH"])
        p = subprocess.run(["bash", "-c", script], cwd=tmp, capture_output=True, text=True, env=env)
        shutil.rmtree(bindir, ignore_errors=True)
        if p.returncode != 0:
            raise SystemExit("the examples script fails with %s:\n%s" % (sg, p.stderr[-500:]))
    elif isinstance(argv, str):
        p = subprocess.run(["bash", "-c", "set -e; " + argv % {"sg": sg}], cwd=tmp, capture_output=True, text=True)
    else:
        p = subprocess.run([sg, *argv], cwd=tmp, capture_output=True, text=True)
    files = {}
    for root, _, names in os.walk(tmp):
        for n in names:
            path = os.path.join(root, n)
            rel = os.path.relpath(path, tmp)
            if rel in FIXTURES:
                continue
            files[rel] = open(path, "rb").read()
    return p.returncode, normalize_text(p.stdout, tmp), normalize_text(p.stderr, tmp), files


def split_sgb(b):
    L = int.from_bytes(b[8:16], "little")
    return b[:16], json.loads(b[16:16 + L]), b[16:16 + L], b[16 + L:]


def same_file(rel, a, b):
    if not rel.endswith(".sgb"):
        return a == b, "content differs"
    ha, ja, rawa, sa = split_sgb(a)
    hb, jb, rawb, sb = split_sgb(b)
    if TS.sub(b"", rawa) == TS.sub(b"", rawb) and ha == hb and sa == sb:
        return True, ""
    # a seed drawn from the OS can never agree: compare everything else
    pa, pb = ja["provenance"], jb["provenance"]
    drawn = pa["master_seed"] != pb["master_seed"]
    if drawn:
        for j in (ja, jb):
            for k in ("master_seed", "samples_sha256", "timestamp"):
                j["provenance"][k] = None
            if j["provenance"].get("trial"):
                j["provenance"]["trial"]["protocol_seed"] = None
        return ja == jb and len(sa) == len(sb), "drawn seed, and the headers differ"
    return False, "header or samples differ"


def compare(name, argv):
    ta, tb = tempfile.mkdtemp(), tempfile.mkdtemp()
    try:
        ca, oa, ea, fa = run_case(REF, name, argv, ta)
        cb, ob, eb, fb = run_case(NEW, name, argv, tb)
        problems = []
        if ca != cb: problems.append("exit code %d vs %d" % (ca, cb))
        if oa != ob: problems.append("stdout differs")
        if ea != eb: problems.append("stderr differs:\n  ref: %s\n  new: %s" % (ea[-300:], eb[-300:]))
        if sorted(fa) != sorted(fb): problems.append("different files: %s vs %s" % (sorted(fa)[:5], sorted(fb)[:5]))
        nfiles = 0
        for rel in sorted(set(fa) & set(fb)):
            ok, why = same_file(rel, fa[rel], fb[rel])
            nfiles += 1
            if not ok:
                problems.append("%s: %s" % (rel, why))
        return problems, nfiles
    finally:
        shutil.rmtree(ta, ignore_errors=True)
        shutil.rmtree(tb, ignore_errors=True)


if __name__ == "__main__":          # (imported by test_api.py for the corpus)
  REF, NEW = (os.path.abspath(p) for p in sys.argv[1:3])
  bad, total_files = 0, 0
  for name, argv in cases:
      problems, n = compare(name, argv)
      total_files += n
      if problems:
          bad += 1
          print("DIFF  %s\n      %s" % (name, "\n      ".join(problems)))
  print("\n%d cases, %d output files compared: %s" % (len(cases), total_files,
        "all identical" if bad == 0 else "%d case(s) differ" % bad))
  sys.exit(1 if bad else 0)
