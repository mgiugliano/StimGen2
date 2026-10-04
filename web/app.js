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

// app.js -- the StimGen 2 planner: edit a description, see it rendered live.
//
// The text in the editor is the single source of truth.  Menus, the
// generator palette and the segment builder only write text; while the
// builder is open the plot previews the text *with* the pending change.
// Rendering is done by the real sg (WebAssembly) in worker.js.

import { parseSgb } from "./sglib.mjs";
import { Plotter } from "./plot.js";
import { makeForm, parseCanonLine, segmentsOf, edits, lineRange, GROUPS } from "./builder.js";
import { EXAMPLES } from "./examples.js";

const $ = (id) => document.getElementById(id);
const editor = $("editor"), gutter = $("gutter"), status = $("status");
const plotter = new Plotter($("plots"), $("timeline"), $("readout"));

// ------------------------------------------------------------------ worker
const worker = new Worker(new URL("./worker.js", import.meta.url), { type: "module" });
let nextId = 0;
const pending = new Map();
worker.onmessage = (e) => { pending.get(e.data.id)?.(e.data); pending.delete(e.data.id); };
const call = (op, data = {}) => new Promise((res) => { const id = ++nextId; pending.set(id, res); worker.postMessage({ id, op, ...data }); });

// ------------------------------------------------------------------ examples
const EXAMPLE_TEXT = Object.fromEntries(EXAMPLES.map(([, name, text]) => [name, text]));
{
  let group = "";
  for (const [g, name] of EXAMPLES) {
    if (g !== group) {
      const h = document.createElement("div"); h.className = "grp"; h.textContent = g; $("examplemenu").append(h);
      group = g;
    }
    const b = document.createElement("button"); b.textContent = name; b.dataset.example = name; $("examplemenu").append(b);
  }
}

// ------------------------------------------------------------------ editor
let badLine = 0, selLine = -1;
function drawGutter() {
  const n = editor.value.split("\n").length;
  gutter.innerHTML = Array.from({ length: n }, (_, i) =>
    i + 1 === badLine ? `<span class="bad">${i + 1}</span>` : i === selLine ? `<span class="sel">${i + 1}</span>` : String(i + 1)).join("\n");
  gutter.scrollTop = editor.scrollTop;
}
editor.addEventListener("scroll", () => { gutter.scrollTop = editor.scrollTop; });
editor.addEventListener("keydown", (e) => {
  if (e.key === "Tab") { e.preventDefault(); document.execCommand("insertText", false, "    "); }
  if ((e.metaKey || e.ctrlKey) && e.key === "/") { e.preventDefault(); act("comment"); }
  if ((e.metaKey || e.ctrlKey) && e.key === "Enter") { e.preventDefault(); $("plotbtn").click(); }
});
editor.addEventListener("input", () => { stopAnim(); if (builder) closeBuilder(); changed(); });
editor.addEventListener("click", () => { markCursorSegment(); });
editor.addEventListener("keyup", (e) => { if (e.key.startsWith("Arrow")) markCursorSegment(); });
for (const id of ["rate", "seed", "unit"]) $(id).addEventListener("input", () => changed());
$("trial").onchange = () => { stopAnim(); renderTrial(); };
$("tview").onchange = () => { stopAnim(); renderTrial(); };
$("tcount").onchange = () => { stopAnim(); renderTrial(); };

const cursorLine = () => editor.value.slice(0, editor.selectionStart).split("\n").length - 1;
function setText(t) { editor.value = t; changed(true); }   // menu actions render at once
function selectLine(i) {
  const L = editor.value.split("\n"), a = L.slice(0, i).join("\n").length + (i ? 1 : 0);
  editor.focus(); editor.setSelectionRange(a, a + (L[i] || "").length);
  editor.scrollTop = Math.max(0, i * 20 - editor.clientHeight / 3);
}

// ------------------------------------------------------------------ rendering
let timer = null, generation = 0, current = null, trials = null;
// Live: re-render shortly after each change.  Otherwise only on Plot
// (or Cmd/Ctrl+Enter); until then the plot is marked out of date.
function changed(force = false) {
  drawGutter();
  try { localStorage.setItem("sg-text", editor.value); } catch (e) { /* private mode */ }
  clearTimeout(timer);
  if ($("live").checked || force) { timer = setTimeout(update, force ? 0 : 200); return; }
  $("plots").classList.add("stale");
  setStatus("stale", "text changed · press Plot ⟳ (⌘/Ctrl+Enter) to render");
}
$("plotbtn").onclick = () => { stopAnim(); changed(true); };
$("keepseed").onclick = () => {                     // freeze the noise shown
  if (!current) return;
  $("seed").value = trials ? trials.protocol_seed : current.header.provenance.master_seed;
  changed(true);
};
$("live").onchange = () => {
  try { localStorage.setItem("sg-live", $("live").checked ? "1" : "0"); } catch (e) { /* private mode */ }
  if ($("live").checked) changed(true);
};
try { if (localStorage.getItem("sg-live") === "0") $("live").checked = false; } catch (e) { /* private mode */ }
function setStatus(kind, text) { status.className = kind; status.textContent = text; }
function errorLine(msg) { const m = /editor:(\d+):/.exec(msg) || /line (\d+):/.exec(msg); return m ? +m[1] : 0; }
const shownText = () => (builder ? builder.candidate() : editor.value);   // with the pending change

async function update() {
  const gen = ++generation, text = shownText();
  const k = await call("kind", { text });
  if (gen !== generation) return;
  if (k.error) return showError(k.error);
  if (k.kind === "protocol") {
    const ex = await call("expand", { text, seed: $("seed").value.trim() });
    if (gen !== generation) return;
    if (ex.error) return showError(ex.error);
    trials = ex;
    const sel = $("trial"), keep = sel.value;
    sel.innerHTML = "";
    ex.trials.forEach((t, j) => {
      const vars = Object.entries(t.info.variables || {}).map(([a, b]) => `${a}=${b}`).join(", ");
      sel.add(new Option(`${t.name}  (condition ${t.info.condition}, rep ${t.info.repetition})  ${vars}`, j));
    });
    if (keep && +keep < ex.trials.length) sel.value = keep;
    $("trialbar").style.display = "flex";
    $("trialinfo").textContent = `${ex.trials.length} trials · ${ex.timing} · start ${ex.start} · protocol seed ${ex.protocol_seed}`;
    plotter.segments = [];
    return renderTrial(gen);
  }
  trials = null;
  $("trialbar").style.display = "none";
  plotter.segments = segmentsOf(text);
  render(gen, text, { rate: $("rate").value.trim(), seed: $("seed").value.trim(), unit: $("unit").value });
}

// The selected trial, alone or with the next ones (overlay or stack).
function renderTrial(gen = ++generation) {
  if (!trials) return Promise.resolve();
  const j = +$("trial").value || 0, mode = $("tview").value;
  const n = mode === "one" ? 1 : Math.max(1, Math.min(trials.trials.length, parseInt($("tcount").value) || 8));
  const idx = Array.from({ length: n }, (_, i) => (j + i) % trials.trials.length);
  return render(gen, null, null, null, idx, mode);
}

// colours of trials by condition: teal -> orange
function conditionColor(c, nc) {
  const f = nc > 1 ? c / (nc - 1) : 0, A = [11, 122, 117], Bc = [242, 100, 25];
  return `rgb(${A.map((v, i) => Math.round(v + f * (Bc[i] - v))).join(",")})`;
}

async function render(gen, text, opts, name = "stimulus", idx = null, mode = "overlay") {
  let r, traces = null;
  if (idx) {                                         // protocol trials: render each one
    const ncond = Math.max(...trials.trials.map((t) => t.info.condition)) + 1;
    traces = [];
    for (const i of idx) {
      const t = trials.trials[i];
      const ri = await call("render", { text: t.text, opts: { rate: $("rate").value.trim(), seed: t.seed, unit: $("unit").value,
                                                             trial: JSON.stringify(t.info), protocol: shownText() } });
      if (gen !== generation) return;
      if (ri.error) return showError(ri.error, ri.warnings);
      const d = parseSgb(ri.bytes), vars = Object.entries(t.info.variables || {}).map(([a, b]) => `${a}=${b}`).join(" ");
      traces.push({ channels: d.channels, color: conditionColor(t.info.condition, ncond), width: 1, label: `${t.name} ${vars}`.trim() });
      if (!r) { r = ri; name = t.name; }
    }
  } else {
    r = await call("render", { text, opts });
    if (gen !== generation) return;
    if (r.error) return showError(r.error, r.warnings);
  }
  current = { ...parseSgb(r.bytes), bytes: r.bytes, name };
  badLine = builder ? 0 : 0; drawGutter();
  const h = current.header, dur = h.samples / h.rate;
  const w = r.warnings.length ? `\nwarning: ${r.warnings.join("\nwarning: ")}` : "";
  setStatus(w ? "warn" : "ok", (builder ? "preview · " : "ok · ") + `${h.channels.length} channel(s) × ${h.samples} samples · ` +
    `${h.rate} Hz · ${+dur.toPrecision(6)} s · ${seedSource(h)} · rendered in ${r.ms.toFixed(1)} ms` + w);
  $("dl-sgb").disabled = false;
  $("plots").classList.remove("stale");
  markCursorSegment(false);
  plotter.current = traces && traces.length > 1 ? 0 : -1;    // the selected trial in bold
  plotter.setData(current, traces, mode);
  if (!intervalEdited) $("interval").value = defaultInterval();
  syncAxes();
}

// The seed of what is shown, and where it came from (spec Sec. 11.3.3).
function seedSource(h) {
  const field = $("seed").value.trim(), text = shownText();
  if (trials) {
    const src = field ? "given" : /^\s*seed\s+\d+/m.test(text.split(/\bstimulus\b/)[0]) ? "stated in the protocol" : "drawn";
    return `protocol seed ${trials.protocol_seed} (${src}) · trial seed ${h.provenance.master_seed}`;
  }
  const src = field ? "given" : /^\s*seed\s+\d+/m.test(text) && /^\s*sg\s+2\s+stimulus/.test(text) ? "stated in the stimulus" : "drawn: new at every rendering";
  return `master seed ${h.provenance.master_seed} (${src})`;
}

function showError(msg, warnings = []) {
  badLine = builder ? 0 : errorLine(msg); drawGutter();
  setStatus("err", (builder ? "preview: " : "error: ") + msg + (warnings.length ? "\nwarning: " + warnings.join("\nwarning: ") : ""));
  $("dl-sgb").disabled = true;
  $("plots").classList.add("stale");                   // the last valid rendering, dimmed
}

// highlight the segment of the cursor line (or of the pending change)
function markCursorSegment(redraw = true) {
  const line = builder ? builder.line : cursorLine();
  plotter.selected = plotter.segments.findIndex((s) => s.line === line);
  selLine = builder ? -1 : plotter.selected >= 0 ? line : -1;
  if (redraw) { drawGutter(); plotter.drawTimeline(); }
}

// ------------------------------------------------------------------ axes
function syncAxes() {
  const v = plotter.view, h = current?.header;
  $("xauto").checked = v.x.auto;
  const [a, b] = plotter.xRange();
  $("x0").value = +a.toPrecision(6); $("x1").value = +b.toPrecision(6);
  $("x0").disabled = $("x1").disabled = v.x.auto;
  if (h) {
    const names = h.channels.map((c) => c.name), sel = $("ych"), keep = sel.value;
    if ([...sel.options].map((o) => o.value).join() !== names.join()) {
      sel.innerHTML = ""; names.forEach((n) => sel.add(new Option(n, n)));
      if (names.includes(keep)) sel.value = keep;
    }
    const j = Math.max(0, names.indexOf(sel.value)), y = v.y[names[j]];
    const [lo, hi] = plotter.yRangeOfChannel(j);
    $("yauto").checked = !y || y.auto;
    $("y0").value = +lo.toPrecision(5); $("y1").value = +hi.toPrecision(5);
    $("y0").disabled = $("y1").disabled = !y || y.auto;
  }
}
plotter.onView = syncAxes;
plotter.onSegment = (s) => { selectLine(s.line); editSegment(s.line); };
$("xauto").onchange = () => {
  const [a, b] = plotter.xRange();
  plotter.view.x = $("xauto").checked ? { auto: true, lo: a, hi: b } : { auto: false, lo: a, hi: b };
  plotter.draw(); syncAxes();
};
for (const id of ["x0", "x1"]) $(id).onchange = () => {
  const a = parseFloat($("x0").value), b = parseFloat($("x1").value);
  if (b > a) { plotter.view.x = { auto: false, lo: a, hi: b }; plotter.draw(); }
  syncAxes();
};
$("ych").onchange = syncAxes;
$("yauto").onchange = () => {
  const name = $("ych").value, j = current.header.channels.findIndex((c) => c.name === name);
  const [lo, hi] = plotter.yRangeOfChannel(j);
  if ($("yauto").checked) delete plotter.view.y[name]; else plotter.view.y[name] = { auto: false, lo, hi };
  plotter.draw(); syncAxes();
};
for (const id of ["y0", "y1"]) $(id).onchange = () => {
  const a = parseFloat($("y0").value), b = parseFloat($("y1").value);
  if (b > a) { plotter.view.y[$("ych").value] = { auto: false, lo: a, hi: b }; plotter.draw(); }
  syncAxes();
};
$("resetview").onclick = () => { plotter.view = { x: { auto: true, lo: 0, hi: 1 }, y: {} }; plotter.draw(); syncAxes(); };

// ------------------------------------------------------------------ builder
// builder: {mode: insert | replace | combine, line, l0, l1, prim, form, candidate()}
let prims = [], builder = null;
const primByName = (n) => prims.find((p) => p.name === n && !p.map);

function openBuilder(mode, primName, opts = {}) {
  const prim = primByName(primName);
  const L = editor.value.split("\n");
  const b = { mode, prim, line: opts.line ?? cursorLine(), l0: opts.l0, l1: opts.l1 };
  if (mode === "insert") {                              // after the cursor line, or at the end
    const cl = cursorLine();
    b.after = editor.value.trim() === "" ? -1 : (L[cl] !== undefined && L[cl].trim() === "" && cl === L.length - 1 ? cl - 1 : cl);
    b.line = b.after + 1;
  }
  $("bgen").innerHTML = "";
  for (const [g, names] of GROUPS) {
    const og = document.createElement("optgroup"); og.label = g;
    names.forEach((n) => og.append(new Option(n, n)));
    $("bgen").append(og);
  }
  $("bgen").value = primName;
  $("bmode").textContent = { insert: `New segment after line ${b.after + 1}`, replace: `Edit line ${b.line + 1}`,
    combine: `Combine lines ${b.l0 + 1}–${b.l1 + 1} with` }[mode];
  $("bop").style.display = mode === "combine" ? "" : "none";
  $("bapply").textContent = mode === "insert" ? "Insert" : mode === "replace" ? "Apply" : "Combine";
  b.form = makeForm(prim, opts.values, () => { showPreview(); });
  b.candidate = () => {
    const t = editor.value;
    if (b.mode === "insert") return edits.insertAfter(t, b.after, b.form.line());
    if (b.mode === "replace") return edits.replace(t, b.line, b.form.line());
    return edits.combine(t, b.l0, b.l1, $("opsel").value, b.form.expr());
  };
  $("bform").innerHTML = ""; $("bform").append(b.form);
  builder = b;
  $("builder").classList.add("open");
  showPreview();
}
function showPreview() {
  if (!builder) return;
  $("bpreview").textContent = builder.mode === "combine" ? `${$("opsel").value} ${builder.form.expr()}` : builder.form.line();
  clearTimeout(timer); timer = setTimeout(update, 150);       // a preview always renders
}
function closeBuilder() {
  builder = null;
  $("builder").classList.remove("open");
  changed(true);                                   // back to the plot of the editor text
}
$("bgen").onchange = () => {
  const b = builder;
  openBuilder(b.mode, $("bgen").value, { line: b.line, l0: b.l0, l1: b.l1 });
  if (b.mode === "insert") { builder.after = b.after; builder.line = b.line; showPreview(); }
};
$("opsel").onchange = showPreview;
$("bapply").onclick = () => {
  const t = builder.candidate(), line = builder.line, mode = builder.mode;
  builder = null; $("builder").classList.remove("open");
  setText(t);
  if (mode !== "combine") selectLine(line);
};
$("bcancel").onclick = closeBuilder;

// edit an existing line: read it back through sg's canonical form
async function editSegment(line) {
  const raw = (editor.value.split("\n")[line] || "").replace(/#.*/, "").trim();
  if (!raw || raw.includes(";")) { setStatus("warn", "Put the cursor on a line with one segment to edit it in the builder."); return; }
  const r = await call("canon", { text: raw + "\n", unit: $("unit").value });
  const parsed = r.error ? null : parseCanonLine(r.text.split("\n")[1] || "");
  if (!parsed || !primByName(parsed.name)) {
    setStatus("warn", "This segment is an expression (combined or transformed): edit it as text, or use Edit → Combine.");
    return;
  }
  openBuilder("replace", parsed.name, { line, values: parsed });
}

// ------------------------------------------------------------------ dialogs
function ask(title, text, fields) {
  return new Promise((resolve) => {
    const d = $("dlg");
    $("dlgtitle").textContent = title; $("dlgtext").textContent = text;
    $("dlgfields").innerHTML = "";
    for (const f of fields) {
      const lab = document.createElement("label"), inp = document.createElement("input");
      inp.type = "text"; inp.name = f.name; inp.value = f.value ?? "";
      lab.append(f.label, inp); $("dlgfields").append(lab);
    }
    d.onclose = () => {
      if (d.returnValue !== "ok") return resolve(null);
      resolve(Object.fromEntries(fields.map((f) => [f.name, d.querySelector(`input[name=${f.name}]`).value.trim()])));
    };
    d.showModal();
    d.querySelector("input")?.select();
  });
}

// ------------------------------------------------------------------ actions
function save(data, name, type) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(new Blob([data], { type }));
  a.download = name; a.click();
  setTimeout(() => URL.revokeObjectURL(a.href), 1000);
}
const docKind = () => (/^\s*(?:#[^\n]*\n\s*)*sg\s+2\s+(\w+)/.exec(editor.value) || [, "waveform"])[1];

async function act(name, arg) {
  const [l0, l1] = lineRange(editor.value, editor.selectionStart, editor.selectionEnd);
  switch (name) {
    case "new": setText(""); break;
    case "open": $("openfile").click(); break;
    case "save-sg": save(editor.value, "stimulus.sg", "text/plain"); break;
    case "save-sgb": if (current) save(current.bytes, current.name + ".sgb", "application/octet-stream"); break;
    case "edit-line": editSegment(cursorLine()); break;
    case "gen": openBuilder("insert", arg); break;
    case "repeat": {
      const v = await ask("Repeat", `Repeat lines ${l0 + 1}–${l1 + 1} several times (each copy gets its own noise).`,
                          [{ name: "n", label: "number of copies", value: "5" }]);
      if (v && /^\d+$/.test(v.n)) setText(edits.repeat(editor.value, l0, l1, v.n));
      break;
    }
    case "combine": openBuilder("combine", "sine", { l0, l1 }); break;
    case "marker": {
      const v = await ask("Marker", "A label before this line; it is stored with the data as a marker.",
                          [{ name: "m", label: "name", value: "on" }]);
      if (v && /^[A-Za-z_]\w*$/.test(v.m)) setText(edits.marker(editor.value, l0, v.m));
      break;
    }
    case "comment": setText(edits.comment(editor.value, l0, l1)); break;
    case "to-stimulus":
      if (docKind() !== "waveform") { setStatus("warn", "This is already a " + docKind() + "."); break; }
      setText(edits.toStimulus(editor.value, $("unit").value || "1")); break;
    case "add-channel": {
      if (docKind() !== "stimulus") { setStatus("warn", "Make → Waveform → stimulus first."); break; }
      const v = await ask("New channel", "A new output channel with its own waveform.",
                          [{ name: "n", label: "name", value: "ch2" }, { name: "u", label: "unit", value: $("unit").value || "pA" }]);
      if (v && /^[A-Za-z_]\w*$/.test(v.n)) setText(edits.addChannel(editor.value, v.n, v.u));
      break;
    }
    case "sweep": {
      const a = editor.selectionStart, b = editor.selectionEnd, sel = editor.value.slice(a, b).trim();
      if (!/^-?[0-9.]+[a-zA-Z%]*$/.test(sel)) { setStatus("warn", "Select a value in the text first (for example 300 or 300pA), then Make → Sweep."); break; }
      const v = await ask("Sweep a value", `Each trial uses one of these values in place of ${sel}.`,
                          [{ name: "name", label: "variable", value: "x" }, { name: "values", label: "values", value: `${sel}, ${sel}` }]);
      if (v && /^[A-Za-z_]\w*$/.test(v.name) && v.values) setText(edits.sweep(editor.value, a, b, v.name, v.values));
      break;
    }
    case "help": openHelp(""); break;
    case "help-syntax": openHelp("syntax"); break;
    case "about": openHelp("license"); break;
    case "example": setText(EXAMPLE_TEXT[arg]); break;
  }
}

// menus: click to open, click an item to act
document.querySelectorAll(".menu").forEach((m) => {
  m.querySelector(":scope > button").onclick = (e) => {
    e.stopPropagation();
    const was = m.classList.contains("open");
    document.querySelectorAll(".menu.open").forEach((x) => x.classList.remove("open"));
    if (!was) m.classList.add("open");
  };
  m.querySelector(".items").addEventListener("click", (e) => {
    const b = e.target.closest("button");
    if (!b) return;
    m.classList.remove("open");
    if (b.dataset.example) act("example", b.dataset.example);
    else if (b.dataset.gen) act("gen", b.dataset.gen);
    else act(b.dataset.act);
  });
});
document.addEventListener("click", () => document.querySelectorAll(".menu.open").forEach((x) => x.classList.remove("open")));
document.addEventListener("keydown", (e) => { if (e.key === "Escape" && builder) closeBuilder(); });
$("dl-sg").onclick = () => act("save-sg");
$("dl-sgb").onclick = () => act("save-sgb");
$("openfile").onchange = async (e) => { const f = e.target.files[0]; if (f) setText(await f.text()); e.target.value = ""; };

// ------------------------------------------------------------------ files
const dropped = [];
window.addEventListener("dragover", (e) => { e.preventDefault(); $("drop").style.display = "flex"; });
$("drop").addEventListener("dragleave", () => { $("drop").style.display = "none"; });
window.addEventListener("drop", async (e) => {
  e.preventDefault(); $("drop").style.display = "none";
  for (const f of e.dataTransfer.files) {
    if (/\.sg$/.test(f.name) && e.dataTransfer.files.length === 1 && !e.shiftKey) { setText(await f.text()); return; }
    await call("file", { name: f.name, data: await f.arrayBuffer() });
    if (!dropped.includes(f.name)) dropped.push(f.name);
  }
  $("files").textContent = "files: " + dropped.join(", ");
  changed();
});

// ------------------------------------------------------------------ help
let helpPrim = null;
const TOPICS = ["", "syntax", "units", "maps", "stimulus", "noise", "protocol", "examples", "output"];
async function showHelp(topic) {
  const r = await call("help", { topic });
  $("helptext").textContent = r.error || r.text;
  helpPrim = primByName(topic) || null;
  $("insert").disabled = !helpPrim;
}
function openHelp(topic) { $("help").classList.add("open"); showHelp(topic); }
$("closehelp").onclick = () => $("help").classList.remove("open");
$("insert").onclick = () => { if (helpPrim) { $("help").classList.remove("open"); openBuilder("insert", helpPrim.name); } };

// ------------------------------------------------------------------ splitter
const mainEl = document.querySelector("main");
function setLeftWidth(px) {
  const w = Math.max(300, Math.min(window.innerWidth - 320, px));
  mainEl.style.gridTemplateColumns = `${w}px 6px 1fr`;
  return w;
}
$("split").addEventListener("pointerdown", (e) => {
  e.preventDefault();
  $("split").setPointerCapture(e.pointerId);
  $("split").classList.add("drag"); document.body.classList.add("resizing");
  const move = (ev) => setLeftWidth(ev.clientX);
  const up = (ev) => {
    $("split").removeEventListener("pointermove", move); $("split").removeEventListener("pointerup", up);
    $("split").classList.remove("drag"); document.body.classList.remove("resizing");
    try { localStorage.setItem("sg-left", String(setLeftWidth(ev.clientX))); } catch (err) { /* private mode */ }
  };
  $("split").addEventListener("pointermove", move); $("split").addEventListener("pointerup", up);
});
$("split").addEventListener("dblclick", () => {
  mainEl.style.gridTemplateColumns = "";
  try { localStorage.removeItem("sg-left"); } catch (err) { /* private mode */ }
});
try { const w = +localStorage.getItem("sg-left"); if (w) setLeftWidth(w); } catch (err) { /* private mode */ }

// ------------------------------------------------------------------ animation
// A cursor plays the stimulus at the chosen speed; then, after the interval,
// the next trial (protocols: the trials shown, or the next in the list).
let intervalEdited = false;
$("interval").addEventListener("input", () => { intervalEdited = true; });
const seconds = (s) => { const m = /^([0-9.]+)\s*(ms|s|min)?$/.exec(s.trim()); return m ? parseFloat(m[1]) * ({ ms: 1e-3, min: 60 }[m[2]] || 1) : NaN; };
function defaultInterval() {                          // from the protocol timing
  if (!trials || !current) return 1;
  const dur = current.header.samples / current.header.rate, m = /^(period|gap)\s+(.+)$/.exec(trials.timing);
  if (!m) return 1;
  const v = seconds(m[2]);
  return +(m[1] === "period" ? Math.max(0, v - dur) : v).toPrecision(6);
}
const anim = { on: false, k: 0, phase: "play", t0: 0, raf: 0, loading: false };
function stopAnim() {
  if (!anim.on) return;
  anim.on = false; cancelAnimationFrame(anim.raf);
  $("play").textContent = "▶ Play"; $("play").classList.remove("on"); $("playinfo").textContent = "";
  plotter.playhead = null;
  plotter.current = plotter.traces.length > 1 ? 0 : -1;
  plotter.draw();
}
function startAnim() {
  if (!current) return;
  anim.on = true; anim.k = 0; anim.phase = "play"; anim.t0 = performance.now();
  $("play").textContent = "❚❚ Pause"; $("play").classList.add("on");
  plotter.current = plotter.traces.length > 1 ? 0 : -1;
  anim.raf = requestAnimationFrame(frame);
}
async function nextTrial() {                          // returns false at the end
  const many = plotter.traces.length > 1;
  if (many && anim.k + 1 < plotter.traces.length) { anim.k++; plotter.current = anim.k; return true; }
  if (trials && !many) {                              // "this trial" view: step the list
    const sel = $("trial"), j = +sel.value + 1;
    if (j >= trials.trials.length && !$("loop").checked) return false;
    sel.value = String(j % trials.trials.length);
    anim.loading = true; await renderTrial(); anim.loading = false;
    return anim.on;
  }
  if (!$("loop").checked) return false;
  anim.k = 0; plotter.current = many ? 0 : -1;
  return true;
}
async function frame(now) {
  if (!anim.on) return;
  if (!anim.loading) {
    const speed = parseFloat($("speed").value), elapsed = (now - anim.t0) / 1000 * speed;
    const dur = plotter.duration(), gap = Math.max(0, seconds($("interval").value) || 0);
    const label = plotter.traces.length > 1 ? plotter.traces[anim.k].label : trials ? trials.trials[+$("trial").value].name : "";
    if (anim.phase === "play") {
      plotter.playhead = Math.min(elapsed, dur);
      $("playinfo").textContent = `${label} · t = ${plotter.playhead.toFixed(3)} s`;
      if (elapsed >= dur) { anim.phase = "wait"; anim.t0 = now; }
    } else {
      $("playinfo").textContent = `${label} · interval ${Math.min(elapsed, gap).toFixed(2)} / ${gap} s`;
      if (elapsed >= gap) {
        if (!(await nextTrial())) { stopAnim(); return; }
        anim.phase = "play"; anim.t0 = performance.now(); plotter.playhead = 0;
      }
    }
    plotter.draw();
  }
  anim.raf = requestAnimationFrame(frame);
}
$("play").onclick = () => (anim.on ? stopAnim() : startAnim());

// read-only view of the planner, for tests/test_web.mjs
window.sgPlanner = { get current() { return current; }, get generation() { return generation; },
                     get builder() { return builder; }, plotter, anim };

// ------------------------------------------------------------------ start
(async () => {
  const info = await call("init");
  prims = info.prims;
  $("ver").textContent = `sg ${info.version} (WebAssembly) · MIT License · Michele Giugliano`;
  for (const [g, names] of GROUPS) {                      // palette and Insert menu
    const grp = document.createElement("span"); grp.className = "grp";
    grp.innerHTML = `<span>${g}</span>`;
    const head = document.createElement("div"); head.className = "grp"; head.textContent = g; $("insertmenu").append(head);
    for (const n of names) {
      const p = primByName(n), b = document.createElement("button");
      b.textContent = n; b.title = p.help; b.onclick = () => act("gen", n); grp.append(b);
      const mi = document.createElement("button"); mi.textContent = n; mi.dataset.gen = n;
      mi.innerHTML = `${n} <kbd>${p.help.split(/[.:(]/)[0].slice(0, 28)}</kbd>`; $("insertmenu").append(mi);
    }
    $("palette").append(grp);
  }
  $("insertmenu").insertAdjacentHTML("beforeend", '<hr><button data-act="marker">Marker (label)…</button><button data-act="add-channel">Channel…</button>');
  const list = $("primlist");
  for (const t of TOPICS) {
    const b = document.createElement("button"); b.textContent = t || "overview"; b.onclick = () => showHelp(t); list.appendChild(b);
  }
  for (const p of prims) {
    const b = document.createElement("button"); b.textContent = p.name; b.title = p.help;
    b.style.color = p.map ? "#5C6B7A" : "#0B7A75"; b.onclick = () => showHelp(p.name); list.appendChild(b);
  }
  let saved = null;
  try { saved = localStorage.getItem("sg-text"); } catch (e) { /* private mode */ }
  editor.value = saved ?? EXAMPLE_TEXT["A current step"];
  changed();
})();
