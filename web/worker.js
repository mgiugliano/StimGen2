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

// worker.js -- runs sg (WebAssembly) off the main thread, so that typing
// never waits for rendering.  Messages: {id, op, ...} -> {id, ...result}.

import { loadSG } from "./sglib.mjs";

const ready = loadSG();

self.onmessage = async (e) => {
  const sg = await ready;
  const { id, op } = e.data;
  let r;
  try {
    if (op === "init") r = { version: sg.version, prims: sg.prims() };
    else if (op === "file") { sg.writeFile("/" + e.data.name, new Uint8Array(e.data.data)); r = { ok: true }; }
    else if (op === "help") r = sg.help(e.data.topic);
    else if (op === "kind") r = sg.kind(e.data.text);
    else if (op === "canon") r = sg.canon(e.data.text, e.data.unit || "");
    else if (op === "expand") r = sg.expand(e.data.text, e.data.seed);
    else if (op === "render") {
      const t0 = performance.now();
      r = sg.render(e.data.text, e.data.opts);
      r.ms = performance.now() - t0;
      if (!r.error) {                      // send the bytes once; channels are views into them
        self.postMessage({ id, ...r, channels: undefined }, [r.bytes.buffer]);
        return;
      }
    }
  } catch (err) {
    r = { error: String(err) };
  }
  self.postMessage({ id, ...r });
};
