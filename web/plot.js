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

// plot.js -- the plots of the planner.
//
//   plotter.setData(data, traces, mode)
//     data:   the trial shown in full (header, channels), for markers and readout
//     traces: [{channels, color, width, label}]  (default: data itself)
//     mode:   "overlay" -- one panel per channel, all traces in it
//             "stack"   -- one compact panel per trace and channel
//   plotter.view      {x: {auto, lo, hi}, y: {channelName: {auto, lo, hi}}}
//   plotter.playhead  time of the animation cursor, or null
//   plotter.current   index of the trace being animated (drawn on top)
//
// Panels fill the available height.  Drag horizontally to zoom time,
// Shift+drag vertically to zoom values, double-click to return to automatic.

const L = 62, R = 12, T = 24, B = 22;                 // margins (CSS pixels)
const INK = "#1B2A41", MUTED = "#5C6B7A", GRID = "#E3EAE9", FRAME = "#C1CFCC", ORANGE = "#F26419";
const BANDS = ["#EEF4F3", "#DCEBE9"];

export function niceTicks(lo, hi, n = 4) {
  if (!(hi > lo)) { hi = lo + 1; lo -= 1; }
  const raw = (hi - lo) / n, p = Math.pow(10, Math.floor(Math.log10(raw)));
  const step = [1, 2, 2.5, 5, 10].map((m) => m * p).find((s) => s >= raw);
  const ticks = [];
  for (let v = Math.ceil(lo / step - 1e-9) * step; v <= hi + step * 1e-9; v += step) ticks.push(+v.toPrecision(12));
  return ticks;
}

// Min/max of the samples under each pixel column, cached per array and range.
const envCache = new WeakMap();
function envelope(x, fs, a, b, pw) {
  const key = `${a}|${b}|${pw}|${fs}`, hit = envCache.get(x);
  if (hit && hit.key === key) return hit.env;
  const mn = new Float64Array(pw), mx = new Float64Array(pw), N = x.length;
  for (let p = 0; p < pw; p++) {
    const ka = Math.max(0, Math.floor((a + p / pw * (b - a)) * fs));
    const kb = Math.min(N, Math.max(ka + 1, Math.floor((a + (p + 1) / pw * (b - a)) * fs)));
    let lo = Infinity, hi = -Infinity;
    for (let k = ka; k < kb; k++) { if (x[k] < lo) lo = x[k]; if (x[k] > hi) hi = x[k]; }
    mn[p] = lo; mx[p] = hi;
  }
  const env = { mn, mx };
  envCache.set(x, { key, env });
  return env;
}

export class Plotter {
  constructor(plotsEl, timelineEl, readoutEl) {
    this.el = plotsEl; this.tl = timelineEl; this.readout = readoutEl;
    this.data = null; this.traces = []; this.mode = "overlay"; this.panels = [];
    this.view = { x: { auto: true, lo: 0, hi: 1 }, y: {} };
    this.segments = []; this.selected = -1;
    this.playhead = null; this.current = -1;
    this.onView = () => {}; this.onSegment = () => {};
    this.tl.addEventListener("click", (e) => {
      const t = this.timeAt(e.offsetX, this.tl.clientWidth);
      const i = this.segments.findIndex((s) => t >= s.t0 && t < s.t1);
      if (i >= 0) this.onSegment(this.segments[i], i);
    });
    new ResizeObserver(() => { this.layout(); this.draw(); }).observe(this.el);
  }

  duration() {
    if (!this.data) return 1;
    const fs = this.data.header.rate;
    return Math.max(...this.traces.map((t) => t.channels[0].length / fs), this.data.header.samples / fs);
  }
  xRange() { const x = this.view.x; return x.auto ? [0, this.duration()] : [x.lo, x.hi]; }
  timeAt(px, W) { const [a, b] = this.xRange(); return a + (px - L) / (W - L - R) * (b - a); }

  // values shown in a panel: fixed per channel, or automatic over its traces
  yRange(panel) {
    const name = this.data.header.channels[panel.j].name, y = this.view.y[name];
    if (y && !y.auto) return [y.lo, y.hi];
    const [a, b] = this.xRange(), fs = this.data.header.rate;
    let lo = Infinity, hi = -Infinity;
    for (let ti = 0; ti < this.traces.length; ti++) {         // all trials: one scale per channel
      const x = this.traces[ti].channels[panel.j];
      const k0 = Math.max(0, Math.floor(a * fs)), k1 = Math.min(x.length, Math.ceil(b * fs) + 1);
      for (let k = k0; k < k1; k++) { if (x[k] < lo) lo = x[k]; if (x[k] > hi) hi = x[k]; }
    }
    if (!isFinite(lo)) { lo = -1; hi = 1; }
    if (!(hi > lo)) { lo -= 1; hi += 1; }
    const pad = (hi - lo) * 0.08;
    return [lo - pad, hi + pad];
  }
  yRangeOfChannel(j) { return this.yRange(this.panels.find((p) => p.j === j) || { j, traces: [0] }); }

  setData(data, traces, mode = "overlay") {
    this.data = data;
    this.traces = traces || [{ channels: data.channels, color: INK, width: 1.2, label: "" }];
    this.mode = this.traces.length > 1 ? mode : "overlay";
    const J = data.channels.length;
    this.panels = [];
    if (this.mode === "stack")
      this.traces.forEach((t, ti) => { for (let j = 0; j < J; j++) this.panels.push({ j, traces: [ti] }); });
    else
      for (let j = 0; j < J; j++) this.panels.push({ j, traces: this.traces.map((_, i) => i) });
    if (this.el.children.length !== this.panels.length) {
      this.el.innerHTML = "";
      this.panels.forEach((p, i) => {
        const d = document.createElement("div"); d.className = "chan";
        d.innerHTML = '<span class="name"></span><canvas></canvas><div class="zoombox"></div>';
        this.el.appendChild(d);
        this.attachMouse(d, i);
      });
    }
    this.layout();
    this.draw();
  }

  // panel heights that fill the visible area
  layout() {
    const n = this.panels.length;
    if (!n) return;
    const avail = this.el.clientHeight - 24, stack = this.mode === "stack";
    const h = Math.round(Math.max(stack ? 72 : 120, Math.min(stack ? 220 : 900, avail / n - 6)));
    for (const d of this.el.children) d.querySelector("canvas").style.height = h + "px";
  }

  draw() {
    if (!this.data) return;
    this.panels.forEach((p, i) => this.drawPanel(this.el.children[i], p));
    this.drawTimeline();
  }

  canvas(cv) {
    const dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = cv.clientHeight;
    cv.width = Math.max(1, W * dpr); cv.height = Math.max(1, H * dpr);
    const g = cv.getContext("2d"); g.scale(dpr, dpr);
    return [g, W, H];
  }

  drawTimeline() {
    const cv = this.tl;
    cv.style.display = this.segments.length ? "block" : "none";
    if (!this.segments.length || !this.data) return;
    const [g, W, H] = this.canvas(cv), [a, b] = this.xRange(), pw = W - L - R;
    const X = (t) => L + (t - a) / (b - a) * pw;
    g.font = "11px ui-monospace, Menlo, monospace"; g.textBaseline = "middle";
    g.save(); g.beginPath(); g.rect(L, 0, pw, H); g.clip();
    this.segments.forEach((s, i) => {
      const x0 = X(s.t0), x1 = X(s.t1);
      if (x1 < L || x0 > L + pw) return;
      const on = i === this.selected || (this.playhead !== null && this.playhead >= s.t0 && this.playhead < s.t1);
      g.fillStyle = on ? "#FCD9C6" : BANDS[i % 2];
      g.fillRect(x0, 2, Math.max(1, x1 - x0 - 1), H - 4);
      if (x1 - x0 > 24) {
        g.fillStyle = on ? ORANGE : INK;
        let t = s.label;
        while (t.length > 1 && g.measureText(t).width > x1 - x0 - 8) t = t.slice(0, -1);
        g.fillText(t === s.label ? t : t.slice(0, -1) + "…", Math.max(x0, L) + 4, H / 2);
      }
    });
    g.restore();
    g.fillStyle = MUTED; g.textAlign = "right"; g.fillText("segments", L - 6, H / 2);
  }

  drawPanel(div, panel) {
    const h = this.data.header, c = h.channels[panel.j], fs = h.rate;
    const tr0 = this.traces[panel.traces[0]];
    const unit = c.unit !== "1" ? ` (${c.unit})` : c.digital ? " (digital)" : "";
    div.querySelector(".name").textContent = this.mode === "stack" ? `${tr0.label} · ${c.name}${unit}`
      : `${c.name}${unit}` + (this.traces.length > 1 ? ` · ${this.traces.length} trials` : "");
    const [g, W, H] = this.canvas(div.querySelector("canvas"));
    const pw = Math.max(1, Math.floor(W - L - R)), ph = H - T - B;
    const [a, b] = this.xRange(), [lo, hi] = this.yRange(panel);
    const X = (t) => L + (t - a) / (b - a) * pw, Y = (v) => T + (1 - (v - lo) / (hi - lo)) * ph;
    g.font = "11px ui-monospace, Menlo, monospace"; g.lineWidth = 1;
    for (const v of niceTicks(lo, hi, ph < 80 ? 2 : 4)) {
      g.strokeStyle = GRID; g.beginPath(); g.moveTo(L, Math.round(Y(v)) + 0.5); g.lineTo(L + pw, Math.round(Y(v)) + 0.5); g.stroke();
      g.fillStyle = MUTED; g.textAlign = "right"; g.fillText(String(v), L - 6, Y(v) + 4);
    }
    const xt = niceTicks(a, b, 6);
    for (const t of xt) { g.fillStyle = MUTED; g.textAlign = "center"; g.fillText(String(t) + (t === xt[0] ? " s" : ""), X(t), H - 6); }
    const yFixed = this.view.y[c.name] && !this.view.y[c.name].auto;
    g.strokeStyle = !this.view.x.auto || yFixed ? ORANGE : FRAME;
    g.strokeRect(L + 0.5, T + 0.5, pw, ph);
    g.save(); g.beginPath(); g.rect(L, T, pw, ph); g.clip();
    g.setLineDash([4, 3]); g.strokeStyle = ORANGE;
    for (const m of h.markers) {
      if (m.name.includes(".") && !m.name.startsWith(c.name + ".")) continue;
      const px = Math.round(X(m.sample / fs)) + 0.5;
      g.beginPath(); g.moveTo(px, T); g.lineTo(px, T + ph); g.stroke();
      g.fillStyle = ORANGE; g.textAlign = "left"; g.fillText(m.name, px + 3, T + 10);
    }
    g.setLineDash([]);
    // traces: the animated one last; with a playhead, the part not yet played is faded
    const order = panel.traces.filter((i) => i !== this.current).concat(panel.traces.includes(this.current) ? [this.current] : []);
    const stroke = (ti, alpha) => {
      const tr = this.traces[ti], x = tr.channels[panel.j];
      g.globalAlpha = alpha; g.strokeStyle = ti === this.current ? INK : tr.color;
      g.lineWidth = ti === this.current ? 2 : tr.width; g.beginPath();
      const k0 = Math.max(0, Math.floor(a * fs) - 1), k1 = Math.min(x.length, Math.ceil(b * fs) + 2);
      if (k1 - k0 <= pw * 2) {
        for (let k = k0; k < k1; k++) { const px = X(k / fs), py = Y(x[k]); k === k0 ? g.moveTo(px, py) : g.lineTo(px, py); }
      } else {
        const { mn, mx } = envelope(x, fs, a, b, pw);
        let started = false;
        for (let p = 0; p < pw; p++) {
          if (!isFinite(mn[p])) continue;
          if (!started) { g.moveTo(L + p + 0.5, Y(mx[p])); started = true; }
          g.lineTo(L + p + 0.5, Y(mx[p])); g.lineTo(L + p + 0.5, Y(mn[p]));
        }
      }
      g.stroke();
    };
    if (this.playhead === null) order.forEach((ti) => stroke(ti, 1));
    else {
      const hx = X(this.playhead);
      order.forEach((ti) => stroke(ti, ti === this.current ? 0.25 : 0.2));
      g.save(); g.beginPath(); g.rect(L, T, Math.max(0, hx - L), ph); g.clip();
      order.forEach((ti) => stroke(ti, ti === this.current ? 1 : 0.55));
      g.restore();
      g.globalAlpha = 1; g.strokeStyle = ORANGE; g.lineWidth = 1.5;
      g.beginPath(); g.moveTo(Math.round(hx) + 0.5, T); g.lineTo(Math.round(hx) + 0.5, T + ph); g.stroke();
    }
    g.globalAlpha = 1;
    g.restore();
  }

  attachMouse(div, pi) {
    const cv = div.querySelector("canvas"), box = div.querySelector(".zoombox");
    let start = null;
    cv.addEventListener("mousedown", (e) => { start = { x: e.offsetX, y: e.offsetY, shift: e.shiftKey }; });
    cv.addEventListener("mousemove", (e) => {
      const panel = this.panels[pi];
      if (this.data && panel) {
        const h = this.data.header, t = this.timeAt(e.offsetX, cv.clientWidth), tr = this.traces[panel.traces[0]];
        const k = Math.min(tr.channels[0].length - 1, Math.max(0, Math.round(t * h.rate)));
        this.readout.textContent = `t = ${(k / h.rate).toPrecision(7)} s · sample ${k} · ` + (this.mode === "stack" ? tr.label + " · " : "") +
          h.channels.map((ch, i) => `${ch.name} = ${+tr.channels[i][k].toPrecision(7)}`).join(" · ");
      }
      if (!start) return;
      Object.assign(box.style, start.shift
        ? { display: "block", left: L + "px", width: (cv.clientWidth - L - R) + "px",
            top: Math.min(start.y, e.offsetY) + "px", height: Math.abs(e.offsetY - start.y) + "px" }
        : { display: "block", top: T + "px", height: (cv.clientHeight - T - B) + "px",
            left: Math.min(start.x, e.offsetX) + "px", width: Math.abs(e.offsetX - start.x) + "px" });
    });
    const finish = (e) => {
      if (!start) return;
      box.style.display = "none";
      const s = start, panel = this.panels[pi]; start = null;
      if (s.shift && Math.abs(e.offsetY - s.y) > 5) {
        const [lo, hi] = this.yRange(panel), ph = cv.clientHeight - T - B;
        const v = (py) => lo + (1 - (py - T) / ph) * (hi - lo);
        this.view.y[this.data.header.channels[panel.j].name] = { auto: false,
          lo: +v(Math.max(s.y, e.offsetY)).toPrecision(4), hi: +v(Math.min(s.y, e.offsetY)).toPrecision(4) };
      } else if (!s.shift && Math.abs(e.offsetX - s.x) > 5) {
        const a = this.timeAt(Math.min(s.x, e.offsetX), cv.clientWidth), b = this.timeAt(Math.max(s.x, e.offsetX), cv.clientWidth);
        this.view.x = { auto: false, lo: +Math.max(0, a).toPrecision(5), hi: +b.toPrecision(5) };
      } else return;
      this.draw(); this.onView();
    };
    cv.addEventListener("mouseup", finish);
    cv.addEventListener("mouseleave", () => { start = null; box.style.display = "none"; });
    cv.addEventListener("dblclick", () => {
      this.view.x.auto = true;
      delete this.view.y[this.data.header.channels[this.panels[pi].j].name];
      this.draw(); this.onView();
    });
  }
}
