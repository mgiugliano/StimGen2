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
sg_run.py -- find the sg program and run it from Python or from a shell.

The sg binary is looked up in this order: $SG_BIN, `sg` on the PATH, then
src/sg of a StimGen2 checkout found by walking up from this file or from
the current directory. If it does not exist but its source does, it is
built with `make -C src` (any C99 compiler; no other dependency).

Shell use (prints JSON, convenient for agents):
    python3 sg_run.py which
    python3 sg_run.py check  -e '500ms dc(0); 1s dc(300); 500ms dc(0)'
    python3 sg_run.py check  protocol.sg
    python3 sg_run.py render -r 20kHz -s 1 protocol.sg [-o DIR]
    python3 sg_run.py render -r 20kHz -e '1s sine(50, 8Hz)' -u pA -o sine.sgb

Module use:
    from sg_run import check, render, load
    ok, msg = check(text="1s dc(300)")
    paths = render(text=PROTOCOL_TEXT, rate="20kHz", seed=1, out="fi")   # -> list of .sgb
    h, x = load(paths[0])                     # header dict, samples [channel, k]
"""
import json, os, re, shutil, subprocess, sys, tempfile


def find_sg():
    """Return the path of an sg binary, building it from source if needed."""
    env = os.environ.get("SG_BIN")
    if env and os.access(env, os.X_OK):
        return env
    onpath = shutil.which("sg")
    if onpath and _is_stimgen(onpath):
        return onpath
    for start in (os.path.dirname(os.path.abspath(__file__)), os.getcwd()):
        d = start
        while True:
            cand = os.path.join(d, "src", "sg")
            if os.access(cand, os.X_OK):
                return cand
            if os.path.exists(os.path.join(d, "src", "sg.h")):
                subprocess.run(["make", "-C", os.path.join(d, "src")], check=True,
                               stdout=subprocess.DEVNULL)
                return cand
            parent = os.path.dirname(d)
            if parent == d:
                break
            d = parent
    raise FileNotFoundError("sg not found: set SG_BIN, put sg on the PATH, or run inside a "
                            "StimGen2 checkout (https://github.com/mgiugliano/StimGen2)")


def _is_stimgen(path):
    try:
        return "StimGen 2" in subprocess.run([path, "version"], capture_output=True, text=True,
                                             timeout=10).stdout
    except (OSError, subprocess.SubprocessError):
        return False


def _run(args):
    p = subprocess.run([find_sg(), *args], capture_output=True, text=True)
    return p.returncode, p.stdout, p.stderr


def _source(text, file):
    if (text is None) == (file is None):
        raise ValueError("give exactly one of text= or file=")
    if file is not None:
        return file, None
    # Multi-line texts (stimuli, protocols) go through a temporary file; -e takes one line.
    if "\n" in text.strip():
        fd, path = tempfile.mkstemp(suffix=".sg")
        with os.fdopen(fd, "w") as f:
            f.write(text)
        return path, path
    return None, None


def _is_protocol(text):
    for line in text.splitlines():
        line = line.split("#")[0].strip()
        if line:
            return line.split()[:3] == ["sg", "2", "protocol"]
    return False


def check(text=None, file=None, rate=None, seed=None):
    """Parse and validate. Returns (ok, message); message is sg's output or error."""
    path, tmp = _source(text, file)
    args = ["check"] + (["-r", str(rate)] if rate else []) + (["-s", str(seed)] if seed is not None else [])
    args += [path] if path else ["-e", text]
    try:
        code, out, err = _run(args)
    finally:
        if tmp:
            os.remove(tmp)
    return code == 0, (out + err).strip()


def render(text=None, file=None, rate=None, seed=None, out=None, unit=None, quiet=True):
    """Render a waveform, stimulus or protocol; return the list of .sgb files written."""
    path, tmp = _source(text, file)
    if out is None and (path is None or tmp):
        # sg would name the output after the (temporary) input; choose a stable name instead.
        out = "out_sgb" if _is_protocol(text) else "out.sgb"
    args = ["render"] + (["-q"] if quiet else [])
    args += ["-r", str(rate)] if rate else []
    args += ["-s", str(seed)] if seed is not None else []
    args += ["-u", unit] if unit else []
    args += ["-o", out] if out else []
    args += [path] if path else ["-e", text]
    try:
        code, so, se = _run(args)
    finally:
        if tmp:
            os.remove(tmp)
    if code != 0:
        raise RuntimeError((so + se).strip())
    # sg reports every file it wrote as "sg: wrote NAME (...)" on stderr; -q keeps these.
    written = re.findall(r"wrote (\S+\.sgb)", so + se)
    if not written:   # fall back to the output location
        o = out or os.path.splitext(path)[0] + ".sgb"
        if not os.path.exists(o) and path and os.path.isdir(os.path.splitext(path)[0] + "_sgb"):
            o = os.path.splitext(path)[0] + "_sgb"           # default directory of a protocol
        if os.path.isdir(o):
            written = sorted(os.path.join(o, f) for f in os.listdir(o) if f.endswith(".sgb"))
        else:
            written = [o]
    return written


def load(path):
    """Read a .sgb file: (header dict, numpy array [channel, sample])."""
    import numpy as np
    b = open(path, "rb").read()
    if b[:8] != b"SGB\0\0\0\0\0":
        raise ValueError(path + ": not a .sgb file")
    L = int.from_bytes(b[8:16], "little")
    h = json.loads(b[16:16 + L])
    return h, np.frombuffer(b[16 + L:], "<f8").reshape(len(h["channels"]), h["samples"])


def main():
    import argparse
    ap = argparse.ArgumentParser(description="Run sg (StimGen 2) and report the result as JSON.")
    ap.add_argument("command", choices=["which", "check", "render"])
    ap.add_argument("file", nargs="?")
    ap.add_argument("-e", "--expr", help="description text instead of a file")
    ap.add_argument("-r", "--rate")
    ap.add_argument("-s", "--seed")
    ap.add_argument("-o", "--output")
    ap.add_argument("-u", "--unit")
    a = ap.parse_args()
    if a.command == "which":
        print(json.dumps({"sg": find_sg()}))
        return
    if (a.file is None) == (a.expr is None):
        ap.error("give a FILE or -e TEXT")
    if a.command == "check":
        ok, msg = check(a.expr, a.file, a.rate, a.seed)
        print(json.dumps({"ok": ok, "message": msg}, indent=1))
        sys.exit(0 if ok else 1)
    try:
        paths = render(a.expr, a.file, a.rate, a.seed, a.output, a.unit)
    except RuntimeError as e:
        print(json.dumps({"ok": False, "message": str(e)}, indent=1))
        sys.exit(1)
    print(json.dumps({"ok": True, "files": paths}, indent=1))


if __name__ == "__main__":
    main()
