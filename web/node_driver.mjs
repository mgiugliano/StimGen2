// StimGen 2 -- SPDX-License-Identifier: MIT
//
// Copyright (c) 2026 Michele Giugliano
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// node_driver.mjs -- the WebAssembly sg with the arguments of tests/api_main.c,
// for tests/test_api.py --level-b.  Files of the current directory are copied
// into the module's file system, so that 'use' and file() find them.
//
//   node node_driver.mjs render FILE RATE SEED UNIT   (.sgb bytes on stdout)
//   node node_driver.mjs check|canon|kind FILE [UNIT] | expand FILE SEED | help TOPIC | prims

import fs from "node:fs";
import { loadSG } from "./sglib.mjs";

const [cmd, a2 = "", a3 = "", a4 = "", a5 = ""] = process.argv.slice(2);
const sg = await loadSG();
for (const f of fs.readdirSync(".")) {
  if (fs.statSync(f).isFile()) sg.writeFile("/" + f, fs.readFileSync(f));
}
const src = ["help", "prims"].includes(cmd) ? "" : fs.readFileSync(a2, "utf8");
const fail = (msg) => { process.stderr.write("sg: error: " + msg + "\n"); process.exit(1); };
let r;
switch (cmd) {
  case "render":
    r = sg.render(src, { rate: a3, seed: a4, unit: a5 });
    if (r.error) fail(r.error);
    process.stdout.write(Buffer.from(r.bytes));
    process.stderr.write("warnings: " + JSON.stringify(r.warnings) + "\n");
    break;
  case "check": {
    r = sg.check(src, a3);
    if (r.error) fail(r.error);
    const { warnings, ...summary } = r;
    process.stdout.write(JSON.stringify(summary));
    break;
  }
  case "canon": r = sg.canon(src, a3); if (r.error) fail(r.error); process.stdout.write(r.text); break;
  case "kind": r = sg.kind(src); if (r.error) fail(r.error); process.stdout.write(r.kind); break;
  case "expand": r = sg.expand(src, a3); if (r.error) fail(r.error); process.stdout.write(JSON.stringify(r)); break;
  case "help": r = sg.help(a2); if (r.error) fail(r.error); process.stdout.write(r.text); break;
  case "prims": process.stdout.write(JSON.stringify(sg.prims())); break;
  default: fail("usage: node_driver.mjs render|check|canon|kind|expand|help|prims ...");
}
