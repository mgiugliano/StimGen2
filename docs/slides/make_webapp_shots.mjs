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

// make_webapp_shots.mjs -- screenshots of the planner for the web-app deck.
//
//   node docs/slides/make_webapp_shots.mjs
//
// Serves web/, drives headless Chrome through the scenarios of the tutorial,
// and writes docs/slides/webapp/*.png (2x) plus shots.json with the screen
// rectangles of the elements the slides point at.

import { spawn } from "node:child_process";
import fs from "node:fs";
import http from "node:http";
import os from "node:os";
import path from "node:path";

const HERE = path.dirname(new URL(import.meta.url).pathname);
const ROOT = path.resolve(HERE, "..", "..");
const OUT = path.join(HERE, "webapp");
const CHROME = process.env.CHROME || "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
const W = 1440, H = 860;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
fs.mkdirSync(OUT, { recursive: true });

const TYPES = { ".html": "text/html", ".js": "text/javascript", ".mjs": "text/javascript", ".wasm": "application/wasm" };
const server = http.createServer((req, res) => {
  const f = path.join(ROOT, "web", decodeURIComponent(req.url.split("?")[0]).replace(/^\/$/, "/index.html"));
  if (!fs.existsSync(f)) { res.writeHead(404); return res.end(); }
  res.writeHead(200, { "Content-Type": TYPES[path.extname(f)] || "application/octet-stream" });
  fs.createReadStream(f).pipe(res);
});
await new Promise((r) => server.listen(0, "127.0.0.1", r));
const profile = fs.mkdtempSync(path.join(os.tmpdir(), "sgshots"));
const chrome = spawn(CHROME, ["--headless=new", "--disable-gpu", "--no-first-run", "--hide-scrollbars", "--remote-debugging-port=0",
  `--user-data-dir=${profile}`, "about:blank"], { stdio: ["ignore", "ignore", "pipe"] });
const wsUrl = await new Promise((res, rej) => {
  let b = ""; chrome.stderr.on("data", (d) => { b += d; const m = /DevTools listening on (ws:\/\/\S+)/.exec(b); if (m) res(m[1]); });
  setTimeout(() => rej(new Error("Chrome did not start")), 20000);
});
const port = new URL(wsUrl).port;
const page = (await (await fetch(`http://127.0.0.1:${port}/json/list`)).json()).find((t) => t.type === "page");
const ws = new WebSocket(page.webSocketDebuggerUrl);
await new Promise((r) => (ws.onopen = r));
let seq = 0; const waiting = new Map();
ws.onmessage = (e) => { const m = JSON.parse(e.data); if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m); waiting.delete(m.id); } };
const cdp = (method, params = {}) => new Promise((r) => { const id = ++seq; waiting.set(id, r); ws.send(JSON.stringify({ id, method, params })); });
const js = async (expr) => (await cdp("Runtime.evaluate", { expression: expr, awaitPromise: true, returnByValue: true })).result.result?.value;
const mouse = (type, x, y, extra = {}) => cdp("Input.dispatchMouseEvent", { type, x, y, button: "left", clickCount: 1, ...extra });
async function settled() {
  const g0 = await js("window.sgPlanner?.generation ?? -1");
  for (let i = 0; i < 150; i++) {
    await sleep(100);
    const s = await js(`({ g: window.sgPlanner?.generation ?? -1, c: document.getElementById("status").className })`);
    if (["ok", "warn", "err", "stale"].includes(s.c) && s.g >= g0) { await sleep(400); return; }
  }
}
const rects = {};
async function shot(name, sels) {
  await sleep(250);
  const r = await cdp("Page.captureScreenshot", { format: "png" });
  fs.writeFileSync(path.join(OUT, name + ".png"), Buffer.from(r.result.data, "base64"));
  rects[name] = await js(`(() => { const o = {}; for (const [k, s] of Object.entries(${JSON.stringify(sels || {})})) {
    const e = document.querySelector(s); if (!e) continue; const b = e.getBoundingClientRect();
    o[k] = { x: b.x, y: b.y, w: b.width, h: b.height }; } return o; })()`);
  console.log("wrote", name + ".png");
}
const setText = async (t, sel) => {
  await js(`{ const e = document.getElementById("editor"); e.value = ${JSON.stringify(t)}; e.dispatchEvent(new Event("input"));
    ${sel ? `const i = e.value.indexOf(${JSON.stringify(sel)}); e.focus(); e.setSelectionRange(i, i + ${sel.length});` : ""} }`);
  await settled();
};
const example = async (name) => { await js(`document.querySelector('[data-example="${name}"]').click()`); await settled(); };
const fill = (sel, v) => js(`{ const n = document.querySelector(${JSON.stringify(sel)}); n.value = ${JSON.stringify(v)}; n.dispatchEvent(new Event("input")); n.dispatchEvent(new Event("change")); }`);

try {
  await cdp("Page.enable"); await cdp("Runtime.enable");
  await cdp("Emulation.setDeviceMetricsOverride", { width: W, height: H, deviceScaleFactor: 2, mobile: false });
  await cdp("Page.navigate", { url: `http://127.0.0.1:${server.address().port}/index.html` });
  await settled();
  await js(`document.querySelector("main").style.gridTemplateColumns = "560px 6px 1fr"`);
  const step = "# a 1 s step of 300 pA between two pauses\n500ms  dc(0)\n1s     dc(300)\n500ms  dc(0)\n";

  // 1. overview
  await setText(step);
  await shot("overview", { menus: "header .menu", settings: "#rate", palette: "#palette", editor: "#editwrap",
    status: "#status", axes: "#axes", timeline: "#timeline", plots: "#plots", play: "#playbar", save: "#dl-sgb" });
  // 2. the Examples menu
  await js(`document.querySelectorAll(".menu > button")[4].click()`); await sleep(300);
  await shot("examples-menu", { menu: "#examplemenu" });
  await js(`document.body.click()`);
  // 3. palette -> builder with preview
  await setText(step);
  await js(`{ const e = document.getElementById("editor"); const i = e.value.indexOf("1s"); e.focus(); e.setSelectionRange(i, i); }`);
  await js(`[...document.querySelectorAll("#palette button")].find(b => b.textContent === "sine").click()`); await settled();
  await fill("#bform .genform .row:nth-of-type(2) input.num", "75"); await settled();
  await shot("builder", { palette: "#palette", builder: "#builder", preview: "#bpreview", apply: "#bapply", plots: "#plots", status: "#status" });
  await js(`document.getElementById("bapply").click()`); await settled();
  // 4. timeline -> edit a segment
  const tl = await js(`(() => { const r = document.getElementById("timeline").getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; })()`);
  const tx = tl.x + 62 + (2.0 / 3.0) * (tl.w - 74);
  await mouse("mousePressed", tx, tl.y + tl.h / 2); await mouse("mouseReleased", tx, tl.y + tl.h / 2); await settled();
  await fill("#bform .genform .row:nth-of-type(3) input.num", "20"); await settled();
  await shot("timeline-edit", { timeline: "#timeline", builder: "#builder", gutter: "#gutter" });
  await js(`document.getElementById("bcancel").click()`); await settled();
  // 5. combine: a step plus noise
  await setText("1s  dc(0)\n3s  dc(100)\n1s  dc(0)\n", "3s  dc(100)");
  await js(`document.querySelector('[data-act="combine"]').click()`); await sleep(300);
  await js(`{ const s = document.getElementById("bgen"); s.value = "ou"; s.dispatchEvent(new Event("change")); }`); await sleep(200);
  await js(`{ const s = document.getElementById("opsel"); s.value = "+"; s.dispatchEvent(new Event("change")); }`); await settled();
  await shot("combine", { builder: "#builder", op: "#bop", editor: "#editwrap", plots: "#plots" });
  await js(`document.getElementById("bapply").click()`); await settled();
  await shot("combine-done", { editor: "#editwrap", plots: "#plots" });
  // 6. sweep: select 300pA -> dialog -> protocol
  await setText("500ms  dc(0)\n1s     dc(300pA)\n500ms  dc(0)\n", "300pA");
  await js(`document.querySelector('[data-act="sweep"]').click()`); await sleep(400);
  await js(`{ const d = document.getElementById("dlg"); d.querySelector("input[name=name]").value = "amp";
    d.querySelector("input[name=values]").value = "-200pA, -100pA, 100pA, 200pA, 300pA"; }`);
  await shot("sweep-dialog", { dialog: "#dlg", editor: "#editwrap" });
  await js(`document.getElementById("dlgok").click()`); await settled();
  await shot("sweep-done", { editor: "#editwrap", trialbar: "#trialbar", plots: "#plots" });
  // 7. protocols: overlay, stack
  await example("A family of steps");
  await shot("protocol-overlay", { editor: "#editwrap", trial: "#trial", tview: "#tview", tcount: "#tcount", trialinfo: "#trialinfo", plots: "#plots" });
  await fill("#tview", "stack"); await fill("#tcount", "5"); await settled();
  await shot("protocol-stack", { tview: "#tview", plots: "#plots" });
  await fill("#tview", "overlay"); await fill("#tcount", "8"); await settled();
  // 8. animation
  await example("Paired-pulse ratio at several intervals");
  await fill("#tview", "overlay"); await fill("#tcount", "4"); await settled();
  await js(`{ document.getElementById("speed").value = "0.25"; document.getElementById("play").click(); }`);
  await sleep(700);
  await shot("animate", { play: "#play", speed: "#speed", interval: "#interval", plots: "#plots", info: "#playinfo" });
  await js(`document.getElementById("play").click()`); await sleep(200);
  // 9. noise and seeds
  await example("Noise: new each time, and frozen");
  await shot("noise-seed", { seed: "#seed", keep: "#keepseed", status: "#status", plots: "#plots", plot: "#plotbtn" });
  // 10. axes: drag to zoom
  await example("Sinusoids at several frequencies");
  const c0 = await js(`(() => { const r = document.querySelector("#plots canvas").getBoundingClientRect(); return { x: r.x, y: r.y, w: r.width, h: r.height }; })()`);
  const px = (t) => c0.x + 62 + (t / 8) * (c0.w - 74), py = c0.y + c0.h / 2;
  await mouse("mousePressed", px(4.0), py); await mouse("mouseMoved", px(5.0), py); await mouse("mouseMoved", px(6.0), py); await mouse("mouseReleased", px(6.0), py);
  await sleep(400);
  await shot("axes", { axes: "#axes", plots: "#plots" });
  await js(`document.getElementById("resetview").click()`);
  // 11. an error
  await setText("500ms  dc(0)\n1s     sine(50, 8ms)\n500ms  dc(0)\n");
  await shot("error", { gutter: "#gutter", status: "#status", editor: "#editwrap" });
  // 12. help
  await setText(step);
  await js(`document.querySelector('[data-act="help"]').click()`); await sleep(300);
  await js(`[...document.querySelectorAll("#primlist button")].find(b => b.textContent === "pulses").click()`); await sleep(400);
  await shot("help", { help: "#help", insert: "#insert" });
  await js(`document.getElementById("closehelp").click()`);
  // 13. a multichannel stimulus
  await example("Two cells and a camera");
  await shot("stimulus", { editor: "#editwrap", plots: "#plots" });
  // 14. the File menu
  await js(`document.querySelectorAll(".menu > button")[0].click()`); await sleep(300);
  await shot("file-menu", { menu: "header .menu .items" });
  await js(`document.body.click()`);
  fs.writeFileSync(path.join(OUT, "shots.json"), JSON.stringify({ width: W, height: H, rects }, null, 1));
} finally { ws.close(); chrome.kill(); server.close(); }
