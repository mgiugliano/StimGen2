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

// sglib.mjs -- JavaScript interface to sg compiled to WebAssembly (sg.js, sg.wasm).
//
//   import { loadSG } from "./sglib.mjs";
//   const sg = await loadSG();
//   const r = sg.render("500ms dc(0); 1s dc(300); 500ms dc(0)", { rate: "20kHz", unit: "pA" });
//   if (r.error) console.log(r.error); else console.log(r.header.rate, r.channels[0]);
//
// The same C code as the command-line sg runs inside: render() returns the
// .sgb file that "sg render" would write.  Files used by 'use' and file()
// are given with writeFile(name, data) before rendering.

import createSG from "./sg.js";

// Parse a .sgb file (spec Sec. 15.1): header, and one Float64Array per channel.
export function parseSgb(bytes) {
  const u8 = bytes instanceof Uint8Array ? bytes : new Uint8Array(bytes);
  const magic = [0x53, 0x47, 0x42, 0, 0, 0, 0, 0];
  if (u8.length < 16 || magic.some((b, i) => u8[i] !== b)) throw new Error("not a .sgb file");
  const L = Number(new DataView(u8.buffer, u8.byteOffset + 8, 8).getBigUint64(0, true));
  const header = JSON.parse(new TextDecoder().decode(u8.subarray(16, 16 + L)));
  const N = header.samples, J = header.channels.length, off = u8.byteOffset + 16 + L;
  // 16 + L is a multiple of 8, so the samples can be viewed in place when the
  // buffer itself is aligned; otherwise copy them (little-endian machines).
  const all = off % 8 === 0 ? new Float64Array(u8.buffer, off, N * J)
                            : new Float64Array(u8.slice(16 + L).buffer);
  const channels = [];
  for (let j = 0; j < J; j++) channels.push(all.subarray(j * N, (j + 1) * N));
  return { header, channels };
}

export async function loadSG(moduleOptions = {}) {
  const M = await createSG(moduleOptions);
  const S = (p) => M.UTF8ToString(p);
  // Call fn(...strings): the strings are copied to the heap, not the stack.
  function call(fn, ...strs) {
    const ptrs = strs.map((s) => M.stringToNewUTF8(s ?? ""));
    try { return fn(...ptrs); } finally { ptrs.forEach((p) => M._free(p)); }
  }
  const error = () => S(M._sg_api_error());
  const warnings = () => JSON.parse(S(M._sg_api_warnings()));
  const text = () => S(M._sg_api_text());
  const result = (r, f) => (r ? { error: error() } : f());

  return {
    version: S(M._sg_api_version()),
    FS: M.FS,
    writeFile(name, data) { M.FS.writeFile(name, data); },

    // A waveform or stimulus -> { bytes, header, channels, warnings } or { error, warnings }.
    // opts: rate ("20kHz"), seed ("42"), unit ("pA"), and for protocol trials
    // trial (JSON text of the trial record) and protocol (protocol text).
    render(src, opts = {}) {
      const r = call(M._sg_api_render_trial, src, opts.rate, opts.seed, opts.unit, opts.trial, opts.protocol);
      if (r) return { error: error(), warnings: warnings() };
      const p = M._sg_api_result(), n = M._sg_api_result_size();
      const bytes = M.HEAPU8.slice(p, p + n);                  // a copy, 8-byte aligned
      return Object.assign({ bytes, warnings: warnings() }, parseSgb(bytes));
    },
    check(src, unit = "") {
      const r = call(M._sg_api_check, src, unit);
      return r ? { error: error(), warnings: warnings() } : Object.assign(JSON.parse(text()), { warnings: warnings() });
    },
    canon(src, unit = "") { return result(call(M._sg_api_canon, src, unit), () => ({ text: text() })); },
    kind(src) { return result(call(M._sg_api_kind, src), () => ({ kind: text() })); },
    expand(src, seed = "") { return result(call(M._sg_api_expand, src, seed), () => JSON.parse(text())); },
    help(topic = "") { return result(call(M._sg_api_help, topic), () => ({ text: text() })); },
    prims() { M._sg_api_prims(); return JSON.parse(text()); },
  };
}
