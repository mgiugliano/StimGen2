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
Run the output of 'sg examples' as a shell script, exactly as printed:
indented lines are commands, here-document lines are kept verbatim, the
other lines become comments.  Every example must succeed.

    python3 test_examples.py ./sg
"""
import os, shutil, subprocess, sys, tempfile

SG = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else "./sg")
TMP = tempfile.mkdtemp(prefix="sgexamples")
text = subprocess.run([SG, "examples"], capture_output=True, text=True, check=True).stdout
script, inside, ncmd = ["set -e"], False, 0
for line in text.splitlines():
    if inside:
        script.append(line)
        inside = line != "END"
    elif line.startswith("  "):
        script.append(line[2:])
        ncmd += 1
        inside = "<<'END'" in line
    else:
        script.append("# " + line)
open(os.path.join(TMP, "examples.sh"), "w").write("\n".join(script) + "\n")
env = dict(os.environ, PATH=os.path.dirname(SG) + os.pathsep + os.environ["PATH"])
p = subprocess.run(["bash", "examples.sh"], cwd=TMP, env=env, capture_output=True, text=True)
made = sorted(f for f in os.listdir(TMP) if f.endswith(".sgb"))
ok = p.returncode == 0 and len(made) >= 25 and os.path.exists(os.path.join(TMP, "steps_again", "0038.sgb"))
print(("ok    " if ok else "FAIL  ") + "%d example commands of 'sg examples' run as printed (%d .sgb files)"
      % (ncmd, len(made)))
if not ok:
    print(p.stdout[-2000:], p.stderr[-2000:])
shutil.rmtree(TMP, ignore_errors=True)
sys.exit(0 if ok else 1)
