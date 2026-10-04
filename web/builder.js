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

// builder.js -- the visual helpers of the planner.  Everything here produces
// or rewrites StimGen 2 text; the editor stays the single source of truth.
//
//   makeForm(prim, values)    a form for one generator; form.line() gives the segment text
//   parseCanonLine(text)      a simple segment in canonical form -> {label, dur, name, args}
//   segmentsOf(text)          top-level segments of a waveform, for the timeline
//   edits                     text transformations used by the menus

// ------------------------------------------------------------------ units
export const UNITS = {
  time: [["ms", 1e-3], ["s", 1], ["us", 1e-6], ["min", 60]],
  frequency: [["Hz", 1], ["kHz", 1e3]],
  phase: [["deg", Math.PI / 180], ["rad", 1]],
  fraction: [["%", 0.01], ["", 1]],
  amplitude: [["", 1], ["pA", 0], ["nA", 0], ["mV", 0], ["nS", 0]],     // "": channel unit
  number: [["", 1]],
};

// Friendly starting values for the required parameters (the form can change them).
export const PRESETS = {
  dc: { dur: [500, "ms"], level: 100 },
  ramp: { dur: [1, "s"], to: 100 },
  relax: { dur: [500, "ms"], to: 100, tau: [50, "ms"] },
  sine: { dur: [1, "s"], amp: 50, freq: [8, "Hz"] },
  square: { dur: [1, "s"], amp: 50, freq: [5, "Hz"] },
  saw: { dur: [1, "s"], amp: 50, freq: [5, "Hz"] },
  triangle: { dur: [1, "s"], amp: 50, freq: [5, "Hz"] },
  chirp: { dur: [5, "s"], amp: 50, f0: [1, "Hz"], f1: [20, "Hz"] },
  biexp: { dur: [200, "ms"], amp: 30, tau_rise: [2, "ms"], tau_decay: [20, "ms"] },
  alpha: { dur: [200, "ms"], amp: 30, tau: [10, "ms"] },
  file: { dur: [1, "s"], path: "data.txt", rate: [1, "kHz"] },
  pulses: { dur: [500, "ms"], amp: 1000, rate: [20, "Hz"], width: [1, "ms"] },
  ou: { dur: [2, "s"], mean: 0, sd: 20, tau: [5, "ms"] },
  wnoise: { dur: [1, "s"], mean: 0, sd: 10 },
  unoise: { dur: [1, "s"], mean: 0, sd: 10 },
  cnoise: { dur: [5, "s"], mean: 0, sd: 10 },
};

export const GROUPS = [
  ["Levels", ["dc", "ramp", "relax"]],
  ["Waves", ["sine", "square", "saw", "triangle", "chirp"]],
  ["Synaptic", ["biexp", "alpha", "pulses"]],
  ["Noise", ["ou", "wnoise", "unoise", "cnoise"]],
  ["Data", ["file"]],
];

const fmt = (v) => String(+(+v).toPrecision(10));
const el = (tag, attrs = {}, ...kids) => {
  const e = document.createElement(tag);
  for (const [k, v] of Object.entries(attrs)) k === "class" ? (e.className = v) : k.startsWith("on") ? (e[k] = v) : e.setAttribute(k, v);
  for (const k of kids) e.append(k);
  return e;
};

// A number with a unit selector.  value: [number, unit] or number (canonical unit).
function quantity(dim, value, onInput) {
  const units = UNITS[dim] || UNITS.number;
  const num = el("input", { type: "text", class: "num", spellcheck: "false" });
  const sel = el("select", { class: "unit" });
  for (const [u] of units) sel.add(new Option(u || (dim === "amplitude" ? "(channel)" : dim === "fraction" ? "0..1" : "—"), u));
  if (Array.isArray(value)) { num.value = fmt(value[0]); sel.value = value[1]; }
  else if (value !== undefined && value !== "") {
    // canonical value: choose a readable unit
    let u = units[0][0], f = units[0][1];
    if (dim === "time") [u, f] = Math.abs(value) >= 1 ? ["s", 1] : Math.abs(value) >= 1e-3 || value === 0 ? ["ms", 1e-3] : ["us", 1e-6];
    if (dim === "frequency") [u, f] = Math.abs(value) >= 1e3 ? ["kHz", 1e3] : ["Hz", 1];
    if (dim === "phase") [u, f] = ["rad", 1];
    if (dim === "fraction") [u, f] = ["%", 0.01];
    if (dim === "amplitude") [u, f] = ["", 1];
    num.value = fmt(value / f); sel.value = u;
  }
  if (units.length === 1) sel.style.display = "none";
  num.oninput = sel.onchange = onInput;
  const wrap = el("span", { class: "qty" }, num, sel);
  wrap.text = () => (num.value.trim() === "" ? "" : num.value.trim() + sel.value);
  wrap.num = num;
  return wrap;
}

// ------------------------------------------------------------------ forms
// prim: an entry of sg.prims(); values: {dur:[v,u] | seconds, label, args:{name: value}}
// (args come from parseCanonLine: canonical units, or keyword / "prev" / text).
export function makeForm(prim, values, onChange) {
  const preset = PRESETS[prim.name] || {}, given = values?.args || {};
  const form = el("div", { class: "genform" });
  const rows = [];
  const changed = () => onChange?.();
  const head = el("div", { class: "row head" });
  const dur = quantity("time", values?.dur ?? preset.dur ?? [1, "s"], changed);
  const label = el("input", { type: "text", class: "label", placeholder: "label (optional)", value: values?.label || "", spellcheck: "false" });
  label.oninput = changed;
  head.append(el("b", {}, prim.name), el("span", { class: "help" }, prim.help), el("br"),
              el("label", {}, "duration ", dur), el("label", {}, " @", label));
  form.append(head);
  const more = el("details", { class: "more" }, el("summary", {}, "more options"));

  // a value read back from the canonical form that equals the default is
  // treated as not given, so that an edited line stays as short as written
  const isDefault = (q, v) =>
    (q.kind === "number" && typeof v === "number" && v === q.default) ||
    ((q.kind === "keyword" || q.kind === "keyword-or-amplitude") && v === q.keywords.split("|")[0]) ||
    (q.kind === "amplitude-or-prev" && v === "prev");
  for (const q of prim.params) {
    if (q.kind === "waveform") continue;
    const has = q.name in given && !isDefault(q, given[q.name]), start = has ? given[q.name] : preset[q.name];
    const row = el("div", { class: "row" }, el("span", { class: "pname", title: q.help }, q.name));
    let get;
    if (q.kind === "keyword") {
      const s = el("select");
      q.keywords.split("|").forEach((k) => s.add(new Option(k, k)));
      if (has) s.value = start;
      const dflt = q.keywords.split("|")[0];
      s.onchange = changed; row.append(s);
      get = () => (s.value === dflt && !has ? "" : `${q.name}=${s.value}`);
    } else if (q.kind === "keyword-or-amplitude") {               // ou init
      const s = el("select"), v = quantity("amplitude", undefined, changed);
      [...q.keywords.split("|"), "value"].forEach((k) => s.add(new Option(k, k)));
      if (has) { if (q.keywords.split("|").includes(start)) s.value = start; else { s.value = "value"; v.num.value = fmt(start); } }
      const sync = () => { v.style.display = s.value === "value" ? "" : "none"; };
      s.onchange = () => { sync(); changed(); }; sync(); row.append(s, v);
      get = () => (s.value === "stationary" && !has ? "" : `init=${s.value === "value" ? v.text() || "0" : s.value}`);
    } else if (q.kind === "string" || q.kind === "list") {
      const t = el("input", { type: "text", class: "wide", spellcheck: "false",
        placeholder: q.kind === "list" ? "e.g. 10ms, 25ms, 70ms" : "file name", value: has ? start : (start || "") });
      t.oninput = changed; row.append(t);
      get = () => (t.value.trim() === "" ? "" : q.kind === "list" ? `${q.name}=[${t.value.trim()}]` : `${q.name}="${t.value.trim()}"`);
    } else if (q.kind === "seed") {
      const t = el("input", { type: "text", class: "num", placeholder: "none", value: has ? start : "" });
      t.oninput = changed; row.append(t, el("span", { class: "hint" }, "fixed seed: frozen noise"));
      get = () => (t.value.trim() === "" ? "" : `seed=${t.value.trim()}`);
    } else {                                                       // numbers
      const dim = q.dim === "number" ? "number" : q.dim;
      const v = quantity(dim, start, changed);
      row.append(v);
      let prev = null;
      if (q.kind === "amplitude-or-prev") {
        prev = el("input", { type: "checkbox" });
        prev.checked = has ? start === "prev" : true;
        if (has && start === "prev") v.num.value = "";
        const sync = () => { v.style.opacity = prev.checked ? 0.35 : 1; };
        prev.onchange = () => { sync(); changed(); }; sync();
        row.append(el("label", { class: "hint" }, prev, " prev (continue from the previous segment)"));
      }
      const required = q.kind === "required";
      get = () => {
        if (prev && prev.checked) return "";                       // the default is prev
        const t = v.text();
        if (t === "") return required ? `${q.name}=1` : "";
        if (!required && !has && preset[q.name] === undefined && q.kind === "number" && +v.num.value === q.default && v.querySelector("select").value === (UNITS[dim]?.[0]?.[0] ?? "")) return "";
        return `${q.name}=${t}`;
      };
    }
    (q.kind === "required" || has || preset[q.name] !== undefined ? form : more).append(row);
    rows.push(get);
  }
  if (more.children.length > 1) form.append(more);
  form.line = () => {
    const args = rows.map((g) => g()).filter(Boolean);
    const lab = label.value.trim().replace(/[^A-Za-z0-9_]/g, "");
    return `${lab ? "@" + lab + " " : ""}${dur.text() || "1s"} ${prim.name}(${args.join(", ")})`;
  };
  form.expr = () => form.line().replace(/^(@\w+\s+)?\S+\s+/, "");   // without label and duration
  return form;
}

// "1.5s sine(amp=50, freq=8Hz, phase=0rad, offset=0, clock=local)" -> fields.
// Returns null for anything that is not a single generator call.
export function parseCanonLine(line) {
  const m = /^(?:@(\w+) )?([0-9.eE+-]+)s ([a-z]+)\((.*)\)$/.exec(line.trim());
  if (!m || /[()]/.test(m[4].replace(/"[^"]*"/g, ""))) return null;
  const args = {};
  for (const part of m[4].match(/(\w+)=("[^"]*"|\[[^\]]*\]|[^,]+)/g) || []) {
    const [, k, v] = /^(\w+)=(.*)$/.exec(part);
    let val = v.trim();
    if (val.startsWith('"')) val = val.slice(1, -1);
    else if (val.startsWith("[")) val = val.slice(1, -1).split(",").map((x) => fmt(parseFloat(x) * 1000) + "ms").join(", ");
    else if (/^-?[0-9.eE+-]+(s|Hz|rad)?$/.test(val)) val = parseFloat(val);
    args[k] = val;
  }
  return { label: m[1] || "", dur: parseFloat(m[2]), name: m[3], args };
}

// ------------------------------------------------------------------ timeline
const TUNIT = { s: 1, ms: 1e-3, us: 1e-6, "µs": 1e-6, ns: 1e-9, min: 60 };

function splitTop(s, sep) {                     // split at sep outside brackets
  const out = []; let depth = 0, cur = "";
  for (const ch of s) {
    if ("({[".includes(ch)) depth++;
    if (")}]".includes(ch)) depth--;
    if (ch === sep && depth === 0) { out.push(cur); cur = ""; } else cur += ch;
  }
  out.push(cur);
  return out;
}

// Top-level segments of a waveform: [{t0, t1, label, line}].  Only simple
// "duration expression" segments; anything else (blocks over several lines,
// repeat, a stimulus or protocol) gives no timeline.
export function segmentsOf(text) {
  const lines = text.split("\n"), segs = [];
  let t = 0;
  for (let i = 0; i < lines.length; i++) {
    const raw = lines[i].replace(/#.*/, "").trim();
    if (!raw) continue;
    if (i === lines.findIndex((l) => l.replace(/#.*/, "").trim()) && /^sg\s/.test(raw)) {
      if (!/^sg\s+2\s+waveform/.test(raw)) return [];
      continue;
    }
    for (let part of splitTop(raw, ";")) {
      part = part.trim();
      if (!part) continue;
      if (/^@\w+$/.test(part)) continue;
      const m = /^(?:@\w+\s+)?([0-9]*\.?[0-9]+(?:[eE][+-]?[0-9]+)?)(s|ms|us|µs|ns|min)\s+(.+)$/.exec(part);
      if (!m) return [];
      const d = parseFloat(m[1]) * TUNIT[m[2]];
      const name = (/^[-(\s]*([a-z]+)\s*\(/.exec(m[3]) || [, "expr"])[1];
      const ops = /[+*/]|\)\s*-/.test(m[3].replace(/\([^()]*\)/g, "")) ? "…" : "";
      segs.push({ t0: t, t1: t + d, label: name + ops, line: i });
      t += d;
    }
  }
  return segs;
}

// ------------------------------------------------------------------ text edits
export function lineRange(text, a, b) {          // line indices of a selection
  const l0 = text.slice(0, a).split("\n").length - 1;
  let l1 = text.slice(0, b).split("\n").length - 1;
  if (b > a && text[b - 1] === "\n") l1--;
  return [l0, Math.max(l0, l1)];
}

const indentOf = (s) => /^\s*/.exec(s)[0];

export const edits = {
  insertAfter(text, line, newLine) {             // after line (or at the start when line < 0)
    const L = text.split("\n");
    if (L.length === 1 && L[0] === "") return newLine + "\n";
    const ind = line >= 0 && L[line] !== undefined ? indentOf(L[line]) : "";
    L.splice(line + 1, 0, ind + newLine.trim());
    return L.join("\n");
  },
  replace(text, line, newLine) {
    const L = text.split("\n");
    L[line] = indentOf(L[line]) + newLine.trim();
    return L.join("\n");
  },
  repeat(text, l0, l1, n) {
    const L = text.split("\n"), ind = indentOf(L[l0]);
    const body = L.slice(l0, l1 + 1).map((s) => "    " + s);
    L.splice(l0, l1 - l0 + 1, `${ind}repeat ${n} {`, ...body, `${ind}}`);
    return L.join("\n");
  },
  // combine the selected segment(s) with a generator: op is + - * /
  combine(text, l0, l1, op, expr) {
    const L = text.split("\n"), ind = indentOf(L[l0]);
    if (l0 === l1) {
      const m = /^(\s*(?:@\w+\s+)?[0-9.eE+-]+(?:s|ms|us|µs|ns|min)\s+)(.*?)(\s*#.*)?$/.exec(L[l0]);
      if (m) { L[l0] = `${m[1]}(${m[2].trim()}) ${op} ${expr}${m[3] || ""}`; return L.join("\n"); }
    }
    const body = L.slice(l0, l1 + 1).map((s) => "    " + s);
    L.splice(l0, l1 - l0 + 1, `${ind}{`, ...body, `${ind}} ${op} ${expr}`);
    return L.join("\n");
  },
  comment(text, l0, l1) {
    const L = text.split("\n"), sel = L.slice(l0, l1 + 1);
    const all = sel.every((s) => /^\s*#/.test(s) || !s.trim());
    for (let i = l0; i <= l1; i++) L[i] = all ? L[i].replace(/^(\s*)# ?/, "$1") : L[i].replace(/^(\s*)/, "$1# ");
    return L.join("\n");
  },
  marker(text, line, name) {
    const L = text.split("\n");
    L.splice(Math.max(0, line), 0, indentOf(L[line] || "") + "@" + name);
    return L.join("\n");
  },
  toStimulus(text, unit) {
    const body = text.replace(/^\s*sg\s+2\s+waveform\s*\n/, "").trimEnd().split("\n").map((s) => "    " + s).join("\n");
    return `sg 2 stimulus\n\nchannel out unit=${unit || "1"} {\n${body}\n}\n`;
  },
  addChannel(text, name, unit) {
    return text.trimEnd() + `\n\nchannel ${name} unit=${unit || "1"} {\n    1s dc(0)\n}\n`;
  },
  // replace the selected value by $name and make (or extend) a protocol
  sweep(text, a, b, name, values) {
    const body = text.slice(0, a) + "$" + name + text.slice(b);
    if (/^\s*sg\s+2\s+protocol/.test(text)) {
      return body.replace(/^(\s*sg\s+2\s+protocol[^\n]*\n)/, `$1sweep  ${name} = [${values}]\n`);
    }
    const inner = body.replace(/^\s*sg\s+2\s+(waveform|stimulus)\s*\n/, "").trimEnd().split("\n").map((s) => "    " + s).join("\n");
    return `sg 2 protocol\nsweep  ${name} = [${values}]\nrepeat 1\norder  sequential\nperiod 5s\n\nstimulus {\n${inner}\n}\n`;
  },
};
