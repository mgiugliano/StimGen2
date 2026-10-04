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

// test_web.mjs -- the web planner (web/) in a real browser: headless Chrome,
// driven through the DevTools protocol (no extra packages; Node >= 22).
//
//   node tests/test_web.mjs ./src/sg [screenshot-directory]
//   SG_WEB_URL=https://blog.giugliano.info/StimGen2/ node tests/test_web.mjs ./src/sg
//
// Serves web/ on a local port, loads every example of the planner, checks
// that each renders (status "ok", one plot per channel), that errors point
// at their line, and that the samples in the browser equal those of the
// native sg for the same text, rate and seed (within Level B).

import { spawn, execFileSync } from "node:child_process";
import fs from "node:fs";
import http from "node:http";
import os from "node:os";
import path from "node:path";

const ROOT = path.resolve(path.dirname(new URL(import.meta.url).pathname), "..");
const SG = path.resolve(process.argv[2] || "src/sg");
const SHOTS = process.argv[3];
const CHROME = process.env.CHROME || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
let fails = 0;
const check = (name, ok, detail = "") => { console.log((ok ? "ok    " : "FAIL  ") + name + (ok ? "" : "  " + detail)); if (!ok) fails++; };
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

// --- a static server for web/
const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm" };
const server = http.createServer((req, res) => {
  const f = path.join(ROOT, "web", decodeURIComponent(req.url.split("?")[0]).replace(/^\/$/, "/index.html"));
  if (!f.startsWith(path.join(ROOT, "web")) || !fs.existsSync(f)) { res.writeHead(404); return res.end(); }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(f)] || "application/octet-stream" });
  fs.createReadStream(f).pipe(res);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
// SG_WEB_URL tests a published copy instead (e.g. the GitHub Pages site)
const URL0 = process.env.SG_WEB_URL || `http://127.0.0.1:${server.address().port}/index.html`;

// --- headless Chrome with a throwaway profile
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "sgweb"));
const chrome = spawn(CHROME, ["--headless=new", "--disable-gpu", "--no-first-run", "--remote-debugging-port=0",
  `--user-data-dir=${profile}`, "--window-size=1400,900", "about:blank"], { stdio: ["ignore", "ignore", "pipe"] });
const wsUrl = await new Promise((resolve, reject) => {
  let buf = "";
  chrome.stderr.on("data", (d) => { buf += d; const m = /DevTools listening on (ws:\/\/\S+)/.exec(buf); if (m) resolve(m[1]); });
  setTimeout(() => reject(new Error("Chrome did not start")), 20000);
});
const port = new globalThis.URL(wsUrl).port;
const targets = await (await fetch(`http://127.0.0.1:${port}/json/list`)).json();
const ws = new WebSocket(targets.find((t) => t.type === "page").webSocketDebuggerUrl);
await new Promise((r) => (ws.onopen = r));
let seq = 0;
const waiting = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m); waiting.delete(m.id); } };
const cdp = (method, params = {}) => new Promise((r) => { const id = ++seq; waiting.set(id, r); ws.send(JSON.stringify({ id, method, params })); });
const evaluate = async (expr) => {
  const r = await cdp("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true });
  if (r.result.exceptionDetails) throw new Error(JSON.stringify(r.result.exceptionDetails));
  return r.result.result.value;
};
async function shot(name) {
  if (!SHOTS) return;
  const r = await cdp("Page.captureScreenshot", { format: "png" });
  fs.mkdirSync(SHOTS, { recursive: true });
  fs.writeFileSync(path.join(SHOTS, name + ".png"), Buffer.from(r.result.data, "base64"));
}
// wait until a new rendering has finished (status ok / warn / err)
async function settled(prevGen) {
  for (let i = 0; i < 200; i++) {
    const s = await evaluate(`({ cls: document.getElementById("status").className,
      text: document.getElementById("status").textContent, gen: window.sgPlanner?.generation ?? -1 })`);
    if (s.gen > prevGen && ["ok", "warn", "err"].includes(s.cls)) { await sleep(150); return s; }
    await sleep(100);
  }
  return { cls: "timeout", text: "" };
}

try {
  await cdp("Page.enable"); await cdp("Runtime.enable");
  await cdp("Emulation.setDeviceMetricsOverride", { width: 1400, height: 900, deviceScaleFactor: 1, mobile: false });
  await cdp("Page.navigate", { url: URL0 });
  let st = await settled(0);
  check("the planner loads and renders the first example (" + st.text.split("\n")[0].slice(0, 70) + ")", st.cls === "ok");
  const G = () => evaluate("window.sgPlanner.generation");
  const ed = () => evaluate(`document.getElementById("editor").value`);
  const setEditor = async (t) => { const g = await G();
    await evaluate(`{ const e = document.getElementById("editor"); e.value = ${JSON.stringify(t)}; e.dispatchEvent(new Event("input")); }`);
    return settled(g); };
  const clickSel = async (sel) => { const g = await G(); await evaluate(`document.querySelector(${JSON.stringify(sel)}).click()`); return g; };
  // dialogs: fill the fields, then OK
  const answer = async (values) => {
    await sleep(150);
    await evaluate(`{ const d = document.getElementById("dlg"); ${Object.entries(values).map(([k, v]) =>
      `d.querySelector("input[name=${k}]").value = ${JSON.stringify(v)};`).join(" ")} document.getElementById("dlgok").click(); }`);
  };
  // real mouse events through the DevTools protocol
  const mouse = async (type, x, y, extra = {}) => cdp("Input.dispatchMouseEvent", { type, x, y, button: "left", clickCount: 1, ...extra });
  const rect = (sel, i = 0) => evaluate(`(() => { const r = document.querySelectorAll(${JSON.stringify(sel)})[${i}].getBoundingClientRect();
    return { x: r.x, y: r.y, w: r.width, h: r.height }; })()`);

  // 1. every example of the Examples menu
  const names = await evaluate(`[...document.querySelectorAll("[data-example]")].map(b => b.dataset.example)`);
  for (const [i, name] of names.entries()) {
    const g = await clickSel(`[data-example="${name}"]`);
    st = await settled(g);
    const info = await evaluate(`({ canvases: document.querySelectorAll("#plots canvas").length,
      channels: window.sgPlanner.current?.header.channels.length, trials: document.getElementById("trial").options.length,
      trialbar: getComputedStyle(document.getElementById("trialbar")).display,
      segments: window.sgPlanner.plotter.segments.length })`);
    const etext = await ed(), edir = fs.mkdtempSync(path.join(os.tmpdir(), "sgex"));
    fs.writeFileSync(path.join(edir, "ex.sg"), etext);
    let native = "";
    try { native = execFileSync(SG, ["check", "-q", "-r", "20kHz", "-u", "pA", "ex.sg"], { cwd: edir }).toString().trim(); }
    catch (err) { native = "FAILED: " + err.stderr; }
    check(`example "${name}": ${st.cls}, ${info.canvases} plot(s), ${info.segments} segment(s)` +
          (info.trialbar !== "none" ? `, ${info.trials} trials` : "") + "; native sg: " + native.replace(/^ex\.sg: /, "").split(",")[0],
          (st.cls === "ok" || st.cls === "warn") && info.canvases === info.channels && info.canvases > 0 && native.includes("ok"), st.text + " " + native);
    if (i % 6 === 0) await shot("ex" + String(i + 1).padStart(2, "0") + "-" + name.replace(/[^a-z0-9]+/gi, "-").toLowerCase());
  }

  // 2. errors point at their line
  st = await setEditor("500ms dc(0)\n1s sine(1, 10ms)\n");
  const bad = await evaluate(`document.querySelector("#gutter .bad")?.textContent`);
  check("an error is shown and its line (2) is marked: " + st.text.slice(0, 60), st.cls === "err" && bad === "2");
  await shot("20-error");

  // 3. palette -> builder with live preview -> insert
  st = await setEditor("500ms dc(0)\n1s dc(300)\n500ms dc(0)\n");
  await evaluate(`{ const e = document.getElementById("editor"); e.setSelectionRange(14, 14); }`);   // cursor on line 2
  let g = await G();
  await evaluate(`[...document.querySelectorAll("#palette button")].find(b => b.textContent === "sine").click()`);
  st = await settled(g);
  const before = await ed();
  check("palette: the builder opens and previews the new segment, editor unchanged",
        st.text.startsWith("preview") && before === "500ms dc(0)\n1s dc(300)\n500ms dc(0)\n" &&
        (await evaluate(`window.sgPlanner.current.header.samples`)) === 60000, st.text);
  g = await G();
  await evaluate(`{ const n = document.querySelector("#bform .genform .row:nth-of-type(2) input.num"); n.value = "75"; n.dispatchEvent(new Event("input")); }`);
  st = await settled(g);
  const pv = await evaluate(`document.getElementById("bpreview").textContent`);
  check("builder: a changed field updates the preview line (" + pv + ")", pv === "1s sine(amp=75, freq=8Hz)", pv);
  await shot("30-builder");
  g = await clickSel("#bapply");
  st = await settled(g);
  check("builder: Insert writes the line after the cursor line, and it renders",
        (await ed()) === "500ms dc(0)\n1s dc(300)\n1s sine(amp=75, freq=8Hz)\n500ms dc(0)\n" && st.cls === "ok", await ed());

  // 4. timeline: click the sine segment -> edit it -> apply
  const segs = await evaluate(`window.sgPlanner.plotter.segments.map(s => s.label).join(",")`);
  check("timeline: one band per segment (" + segs + ")", segs === "dc,dc,sine,dc");
  const tl = await rect("#timeline");
  const durTotal = 3.0, xOf = (t) => tl.x + 62 + (t / durTotal) * (tl.w - 62 - 12);
  await mouse("mousePressed", xOf(2.0), tl.y + tl.h / 2); await mouse("mouseReleased", xOf(2.0), tl.y + tl.h / 2);
  await sleep(600);
  const bmode = await evaluate(`document.getElementById("bmode").textContent`);
  const amp = await evaluate(`document.querySelector("#bform .genform .row:nth-of-type(2) input.num")?.value`);
  check("timeline click opens the segment in the builder with its values (" + bmode + ", amp " + amp + ")",
        bmode === "Edit line 3" && amp === "75");
  g = await G();
  await evaluate(`{ const n = document.querySelector("#bform .genform .row:nth-of-type(3) input.num"); n.value = "20"; n.dispatchEvent(new Event("input")); }`);
  await settled(g);
  g = await clickSel("#bapply");
  st = await settled(g);
  check("builder: Apply rewrites that line", (await ed()).split("\n")[2] === "1s sine(amp=75, freq=20Hz)" && st.cls === "ok", await ed());

  // 5. combine line 2 with noise
  await evaluate(`{ const e = document.getElementById("editor"); e.setSelectionRange(14, 14); }`);
  await clickSel(`[data-act="combine"]`);
  await sleep(300);
  await evaluate(`{ const s = document.getElementById("bgen"); s.value = "ou"; s.dispatchEvent(new Event("change")); }`);
  await evaluate(`{ const s = document.getElementById("opsel"); s.value = "+"; s.dispatchEvent(new Event("change")); }`);
  await sleep(500);
  g = await clickSel("#bapply");
  st = await settled(g);
  check("Edit → Combine: line 2 plus OU noise, and it renders", (await ed()).split("\n")[1] === "1s (dc(300)) + ou(mean=0, sd=20, tau=5ms)" && st.cls === "ok", (await ed()).split("\n")[1]);

  // 6. repeat and marker through dialogs
  st = await setEditor("1s dc(0)\n2ms dc(1000)\n48ms dc(0)\n1s dc(0)\n");
  await evaluate(`{ const e = document.getElementById("editor"); e.setSelectionRange(9, 32); }`);  // lines 2-3
  g = await G();
  await clickSel(`[data-act="repeat"]`); await answer({ n: "10" });
  st = await settled(g);
  check("Edit → Repeat: lines 2–3 ten times (2.5 s), and it renders",
        (await ed()).includes("repeat 10 {\n    2ms dc(1000)\n    48ms dc(0)\n}") && (await evaluate(`window.sgPlanner.current.header.samples`)) === 50000, await ed());
  await evaluate(`{ const e = document.getElementById("editor"); e.setSelectionRange(0, 0); }`);
  g = await G();
  await clickSel(`[data-act="marker"]`); await answer({ m: "start" });
  st = await settled(g);
  check("Edit → Marker: @start before line 1, stored as a marker",
        (await ed()).startsWith("@start\n1s dc(0)") && (await evaluate(`window.sgPlanner.current.header.markers.map(m => m.name).join()`)) === "out.start");

  // 7. sweep a selected value -> protocol with trials
  st = await setEditor("500ms dc(0)\n1s dc(300pA)\n500ms dc(0)\n");
  await evaluate(`{ const e = document.getElementById("editor"); const i = e.value.indexOf("300pA"); e.setSelectionRange(i, i + 5); }`);
  g = await G();
  await clickSel(`[data-act="sweep"]`); await answer({ name: "amp", values: "100pA, 200pA, 300pA" });
  st = await settled(g);
  const ntr = await evaluate(`document.getElementById("trial").options.length`);
  check("Make → Sweep: a protocol with 3 trials, and the trial renders", (await ed()).startsWith("sg 2 protocol\nsweep  amp = [100pA, 200pA, 300pA]") && ntr === 3 && st.cls === "ok", await ed());
  await shot("40-sweep");

  // 8. waveform -> stimulus -> second channel
  st = await setEditor("500ms dc(0)\n1s dc(300)\n500ms dc(0)\n");
  g = await clickSel(`[data-act="to-stimulus"]`); st = await settled(g);
  g = await G(); await clickSel(`[data-act="add-channel"]`); await answer({ n: "led", u: "V" });
  st = await settled(g);
  const mk = await evaluate(`({ canvases: document.querySelectorAll("#plots canvas").length, panels: window.sgPlanner.plotter.panels.length,
    traces: window.sgPlanner.plotter.traces.length, mode: window.sgPlanner.plotter.mode, text: document.getElementById("editor").value })`);
  check("Make → stimulus and Add channel: two plots (a warning: different durations)", mk.canvases === 2 && (st.cls === "ok" || st.cls === "warn"), JSON.stringify(mk));

  // 9. axes: fixed by typing, by dragging, back to automatic by double-click
  st = await setEditor("2s sine(amp=50, freq=2Hz)\n");
  await evaluate(`{ const x = document.getElementById("xauto"); x.checked = false; x.dispatchEvent(new Event("change"));
    const a = document.getElementById("x0"), b = document.getElementById("x1"); a.value = "0.5"; b.value = "1";
    a.dispatchEvent(new Event("change")); b.dispatchEvent(new Event("change"));
    const y = document.getElementById("yauto"); y.checked = false; y.dispatchEvent(new Event("change"));
    const c = document.getElementById("y0"), d = document.getElementById("y1"); c.value = "-100"; d.value = "100";
    c.dispatchEvent(new Event("change")); d.dispatchEvent(new Event("change")); }`);
  let view = await evaluate(`JSON.stringify(window.sgPlanner.plotter.view)`);
  check("axes: time 0.5–1 s and values −100…100 fixed by typing", view === '{"x":{"auto":false,"lo":0.5,"hi":1},"y":{"out":{"auto":false,"lo":-100,"hi":100}}}', view);
  st = await setEditor("2s sine(amp=50, freq=3Hz)\n");
  view = await evaluate(`JSON.stringify(window.sgPlanner.plotter.view.x)`);
  check("axes: fixed limits survive editing the text", view === '{"auto":false,"lo":0.5,"hi":1}', view);
  await shot("50-fixed-axes");
  await evaluate(`document.getElementById("resetview").click()`);
  const c0 = await rect("#plots canvas");
  const px = (t) => c0.x + 62 + (t / 2) * (c0.w - 62 - 12), py = c0.y + c0.h / 2;
  await mouse("mousePressed", px(0.5), py); await mouse("mouseMoved", px(0.9), py); await mouse("mouseReleased", px(1.0), py);
  await sleep(200);
  view = await evaluate(`window.sgPlanner.plotter.view.x`);
  check(`axes: dragging zooms the time axis (${view.lo}–${view.hi} s)`, !view.auto && Math.abs(view.lo - 0.5) < 0.02 && Math.abs(view.hi - 1.0) < 0.02);
  await mouse("mousePressed", px(0.7), py, { clickCount: 2 }); await mouse("mouseReleased", px(0.7), py, { clickCount: 2 });
  await sleep(200);
  check("axes: double-click returns to automatic", await evaluate(`window.sgPlanner.plotter.view.x.auto`));

  // 10. help panel -> builder
  st = await setEditor("500ms dc(0)\n");
  await evaluate(`document.querySelector('[data-act="help"]').click()`);
  await sleep(300);
  await evaluate(`[...document.querySelectorAll("#primlist button")].find(b => b.textContent === "chirp").click()`);
  await sleep(300);
  const helpText = await evaluate(`document.getElementById("helptext").textContent`);
  g = await G();
  await evaluate(`document.getElementById("insert").click()`);
  await settled(g);
  g = await clickSel("#bapply");
  st = await settled(g);
  check("help: chirp help shown, opened in the builder, inserted and rendered",
        helpText.startsWith("chirp:") && (await ed()).includes("5s chirp(amp=50, f0=1Hz, f1=20Hz)") && st.cls === "ok", await ed());

  // 11. protocols: several trials (overlay, stack), the drop-down, animation
  const proto = "sg 2 protocol\nsweep amp = from -300pA to 300pA step 100pA\nrepeat 2\norder shuffled-blocks\nperiod 3s\nseed 4\n\n" +
                "stimulus {\n    channel Iinj unit=pA {\n        200ms dc(0)\n        300ms dc($amp)\n        200ms dc(0)\n    }\n}\n";
  st = await setEditor(proto);
  let pinfo = await evaluate(`({ traces: window.sgPlanner.plotter.traces.length, panels: document.querySelectorAll("#plots canvas").length,
    cur: window.sgPlanner.plotter.current, first: window.sgPlanner.plotter.traces[0].label, h: document.querySelector("#plots canvas").clientHeight,
    interval: document.getElementById("interval").value })`);
  check(`overlay: 8 trials in one panel (${pinfo.first} in bold), panel fills the height (${pinfo.h}px), interval from the protocol (${pinfo.interval} s)`,
        st.cls === "ok" && pinfo.traces === 8 && pinfo.panels === 1 && pinfo.cur === 0 && pinfo.h > 300 && pinfo.interval === "2.3");
  await shot("60-overlay");
  g = await G();
  await evaluate(`{ const s = document.getElementById("trial"); s.value = "5"; s.dispatchEvent(new Event("change")); }`);
  st = await settled(g);
  const first5 = await evaluate(`window.sgPlanner.plotter.traces[0].label`);
  check("the trial drop-down chooses the highlighted trial (" + first5 + ")", first5.startsWith("0005"));
  g = await G();
  await evaluate(`{ const s = document.getElementById("tview"); s.value = "stack"; s.dispatchEvent(new Event("change"));
    const c = document.getElementById("tcount"); c.value = "4"; c.dispatchEvent(new Event("change")); }`);
  st = await settled(g);
  pinfo = await evaluate(`({ panels: document.querySelectorAll("#plots canvas").length, mode: window.sgPlanner.plotter.mode,
    labels: [...document.querySelectorAll("#plots .name")].map(e => e.textContent.split(" ")[0]).join() })`);
  check(`stack: 4 trials, one panel each (${pinfo.labels})`, pinfo.panels === 4 && pinfo.mode === "stack" && pinfo.labels === "0005,0006,0007,0008");
  await shot("61-stack");
  g = await G();
  await evaluate(`{ const s = document.getElementById("tview"); s.value = "one"; s.dispatchEvent(new Event("change")); }`);
  st = await settled(g);
  check("'this trial': one trace, as before", (await evaluate(`window.sgPlanner.plotter.traces.length`)) === 1);
  // animation in overlay mode, fast
  g = await G();
  await evaluate(`{ const s = document.getElementById("tview"); s.value = "overlay"; s.dispatchEvent(new Event("change"));
    const c = document.getElementById("tcount"); c.value = "3"; c.dispatchEvent(new Event("change")); }`);
  st = await settled(g);
  await evaluate(`{ document.getElementById("speed").value = "1"; document.getElementById("interval").value = "0.2";
    document.getElementById("interval").dispatchEvent(new Event("input")); document.getElementById("play").click(); }`);
  await sleep(250);
  const a1 = await evaluate(`({ on: window.sgPlanner.anim.on, ph: window.sgPlanner.plotter.playhead, k: window.sgPlanner.anim.k })`);
  await shot("62-animation");
  await evaluate(`document.getElementById("speed").value = "5"`);
  await sleep(1300);
  const a2 = await evaluate(`({ on: window.sgPlanner.anim.on, ph: window.sgPlanner.plotter.playhead, k: window.sgPlanner.anim.k,
    info: document.getElementById("playinfo").textContent })`);
  await evaluate(`document.getElementById("play").click()`);
  await sleep(100);
  const a3 = await evaluate(`({ on: window.sgPlanner.anim.on, ph: window.sgPlanner.plotter.playhead })`);
  check(`animation: the playhead moves (${a1.ph?.toFixed(2)} s), trials follow (trial ${a2.k}: ${a2.info}), pause stops it`,
        a1.on && a1.k === 0 && a1.ph > 0.1 && a1.ph < 0.6 && a2.k >= 1 && !a3.on && a3.ph === null);
  // waveform animation and the splitter
  st = await setEditor("500ms dc(0)\n1s sine(50, 4Hz)\n500ms dc(0)\n");
  await evaluate(`{ document.getElementById("speed").value = "2"; document.getElementById("play").click(); }`);
  await sleep(500);
  const a4 = await evaluate(`({ ph: window.sgPlanner.plotter.playhead, sel: window.sgPlanner.plotter.segments.findIndex(s =>
    window.sgPlanner.plotter.playhead >= s.t0 && window.sgPlanner.plotter.playhead < s.t1) })`);
  await evaluate(`document.getElementById("play").click()`);
  check(`animation of a waveform (playhead ${a4.ph?.toFixed(2)} s, in segment ${a4.sel})`, a4.ph > 0.6 && a4.ph < 1.3 && a4.sel === 1);
  const sp = await rect("#split"), w0 = await evaluate(`document.querySelector("#plots canvas").clientWidth`);
  await mouse("mousePressed", sp.x + 3, sp.y + 200); await mouse("mouseMoved", sp.x - 100, sp.y + 200);
  await mouse("mouseMoved", sp.x - 200, sp.y + 200); await mouse("mouseReleased", sp.x - 200, sp.y + 200);
  await sleep(300);
  const w1 = await evaluate(`document.querySelector("#plots canvas").clientWidth`), lw = await evaluate(`document.getElementById("left").clientWidth`);
  check(`splitter: dragging resizes both sides (plot ${w0} -> ${w1} px, editor ${lw} px)`, w1 > w0 + 150 && Math.abs(lw - (sp.x - 200)) < 4);
  await shot("63-split");

  // 12. live off: typing marks the plot out of date; Plot renders
  await evaluate(`{ const l = document.getElementById("live"); l.checked = false; l.dispatchEvent(new Event("change")); }`);
  g = await G();
  await evaluate(`{ const e = document.getElementById("editor"); e.value = "3s ramp(from=0, to=10)\\n"; e.dispatchEvent(new Event("input")); }`);
  await sleep(600);
  const lv = await evaluate(`({ cls: document.getElementById("status").className, gen: window.sgPlanner.generation,
    stale: document.getElementById("plots").classList.contains("stale") })`);
  await evaluate(`document.getElementById("plotbtn").click()`);
  st = await settled(g);
  check("live off: typing only marks the plot out of date; Plot renders it",
        lv.cls === "stale" && lv.gen === g && lv.stale && st.cls === "ok" &&
        (await evaluate(`window.sgPlanner.current.header.samples`)) === 60000);
  await evaluate(`{ const l = document.getElementById("live"); l.checked = true; l.dispatchEvent(new Event("change")); }`);
  await sleep(400);

  // 13. seeds (spec Sec. 11.3): none -> a new realisation at every rendering
  const seedOf = () => evaluate(`({ seed: window.sgPlanner.current.header.provenance.master_seed,
    sha: window.sgPlanner.current.header.provenance.samples_sha256, status: document.getElementById("status").textContent })`);
  await evaluate(`document.getElementById("seed").value = ""`);
  st = await setEditor("1s ou(mean=0, sd=10, tau=5ms)\n1s ou(mean=0, sd=10, tau=5ms, seed=17)\n");
  const r1 = await seedOf(), x1 = await evaluate(`Array.from(window.sgPlanner.current.channels[0])`);
  g = await G(); await evaluate(`document.getElementById("plotbtn").click()`); st = await settled(g);
  const r2 = await seedOf(), x2 = await evaluate(`Array.from(window.sgPlanner.current.channels[0])`);
  const frozenSame = x1.slice(20000).every((v, k) => v === x2[20000 + k]), freeSame = x1.slice(0, 20000).every((v, k) => v === x2[k]);
  check(`no seed: each rendering draws a new master seed (${r1.seed} -> ${r2.seed}), the status says so`,
        r1.seed !== r2.seed && r1.sha !== r2.sha && r2.status.includes("(drawn: new at every rendering)"), r2.status);
  check("no seed: the unseeded segment changes, the seed=17 segment stays identical", !freeSame && frozenSame);
  g = await G(); await evaluate(`document.getElementById("keepseed").click()`); st = await settled(g);
  const r3 = await seedOf();
  g = await G(); await evaluate(`document.getElementById("plotbtn").click()`); st = await settled(g);
  const r4 = await seedOf();
  check(`keep: the seed shown is kept (${r3.seed}), and renderings repeat`,
        (await evaluate(`document.getElementById("seed").value`)) === r2.seed && r3.seed === r2.seed && r4.sha === r3.sha && r4.status.includes("(given)"));
  await evaluate(`document.getElementById("seed").value = ""`);
  st = await setEditor("sg 2 stimulus\nseed 77\nchannel a unit=pA { 1s ou(0, 10, 5ms) }\n");
  const r5 = await seedOf();
  check("a seed stated in the stimulus is used (77) and reported", r5.seed === "77" && r5.status.includes("(stated in the stimulus)"), r5.status);
  st = await setEditor("sg 2 protocol\nseed 4\nrepeat 2\nstimulus { 1s ou(0, 10, 5ms) }\n");
  const r6 = await seedOf();
  check("a seed stated in the protocol is used (protocol seed 4)", r6.status.includes("protocol seed 4 (stated in the protocol)"), r6.status);
  st = await setEditor("sg 2 protocol\nrepeat 2\nstimulus { 1s ou(0, 10, 5ms) }\n");
  const p1 = await evaluate(`document.getElementById("trialinfo").textContent`);
  g = await G(); await evaluate(`document.getElementById("plotbtn").click()`); st = await settled(g);
  const p2 = await evaluate(`document.getElementById("trialinfo").textContent`);
  check("a protocol without seed draws a new protocol seed at every rendering", p1 !== p2 && p2.includes("protocol seed"), p1 + " | " + p2);

  // the browser's samples equal those of the native sg (same text, rate, seed)
  const text = "500ms dc(0)\n1s sine(50, 8Hz) + ou(0, 20, 5ms)\n200ms chirp(30, 2Hz, 40Hz, law=exp)\n";
  let gen = await evaluate("window.sgPlanner.generation");
  await evaluate(`{ document.getElementById("rate").value = "10kHz"; document.getElementById("seed").value = "42";
    const e = document.getElementById("editor"); e.value = ${JSON.stringify(text)}; e.dispatchEvent(new Event("input")); }`);
  st = await settled(gen);
  const web = await evaluate(`({ seed: window.sgPlanner.current.header.provenance.master_seed,
    x: Array.from(window.sgPlanner.current.channels[0]) })`);
  const dir = fs.mkdtempSync(path.join(os.tmpdir(), "sgweb"));
  fs.writeFileSync(path.join(dir, "editor"), text);
  execFileSync(SG, ["render", "-q", "-r", "10kHz", "-s", "42", "-u", "pA", "-o", "n.sgb", "editor"], { cwd: dir });
  const b = fs.readFileSync(path.join(dir, "n.sgb"));
  const L = Number(b.readBigUInt64LE(8));
  const nat = new Float64Array(b.buffer.slice(b.byteOffset + 16 + L, b.byteOffset + b.length));
  let maxd = 0, same = 0;
  for (let k = 0; k < nat.length; k++) { const d = Math.abs(nat[k] - web.x[k]); if (d > maxd) maxd = d; if (d === 0) same++; }
  check(`browser = native sg: ${nat.length} samples, ${(100 * same / nat.length).toFixed(1)}% bit-identical, ` +
        `largest difference ${maxd.toExponential(1)} (amplitude ~80)`,
        web.seed === "42" && web.x.length === nat.length && maxd < 1e-12 * 80);
  await shot("91-compare");
} catch (e) {
  check("browser session", false, e.message);
} finally {
  ws.close(); chrome.kill(); server.close();
}
console.log(fails ? `${fails} web test(s) FAILED` : "all web tests passed");
process.exit(fails ? 1 : 0);
