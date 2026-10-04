// StimGen 2 -- SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michele Giugliano. See LICENSE for the full text.
//
// build_webapp_deck.js -- the tutorial deck for the StimGen 2 planner (web app).
//
//   node make_webapp_shots.mjs           # screenshots of the planner (webapp/*.png)
//   node build_webapp_deck.js            # writes StimGen2-planner-tutorial.pptx
//
// Needs the npm packages pptxgenjs, react, react-dom, react-icons, sharp (and jszip, which comes with pptxgenjs).
// SKILL_DIR may point to a directory with scripts/apply_theme.js (optional).

const path = require("path");
const fs = require("fs");
const pptxgen = require("pptxgenjs");
const React = require("react");
const ReactDOMServer = require("react-dom/server");
const sharp = require("sharp");
const fa = require("react-icons/fa");

const HERE = __dirname;
const SHOTS = path.join(HERE, "webapp");
const META = JSON.parse(fs.readFileSync(path.join(SHOTS, "shots.json"), "utf8"));
const OUT = path.join(HERE, "StimGen2-planner-tutorial.pptx");
const URL = "blog.giugliano.info/StimGen2";

const THEME = {
  name: "StimGen 2",
  headFontFace: "Cambria",
  bodyFontFace: "Calibri",
  colors: {
    dk1: "1B2A41", lt1: "FFFFFF", dk2: "0B7A75", lt2: "EEF4F3",
    accent1: "0B7A75", accent2: "F26419", accent3: "1B2A41",
    accent4: "7FC8C2", accent5: "F6AE2D", accent6: "5C6B7A",
    hlink: "0B7A75", folHlink: "5C6B7A",
  },
};
const HEX = THEME.colors;
const MONO = "Courier New";

const pres = new pptxgen();
pres.layout = "LAYOUT_WIDE";                         // 13.33 x 7.5 in
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
pres.author = "Michele Giugliano";
pres.title = "Plan your stimuli in the browser: the StimGen 2 planner";
pres.subject = "Tutorial on the StimGen 2 web planner";
const C = pres.SchemeColor;

pres.defineSlideMaster({
  title: "TITLE",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.7, y: 1.7, w: 6.4, h: 1.9,
        fontSize: 44, bold: true, color: C.background1, valign: "bottom", align: "left", margin: 0 }, text: "" } },
    { placeholder: { options: { name: "body", type: "body", x: 0.7, y: 3.8, w: 6.2, h: 1.2,
        fontSize: 20, color: C.accent4, valign: "top", align: "left", margin: 0 }, text: "" } },
  ],
});
pres.defineSlideMaster({
  title: "CONTENT",
  background: { color: C.background1 },
  margin: [0.5, 0.6, 0.6, 0.6],
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.6, y: 0.3, w: 12.1, h: 0.8,
        fontSize: 30, bold: true, color: C.text1, valign: "middle", align: "left", margin: 0 }, text: "" } },
    { text: { text: "StimGen 2 planner · " + URL, options: { x: 0.6, y: 7.02, w: 6, h: 0.3,
        fontSize: 10, color: C.accent6, margin: 0 } } },
  ],
  slideNumber: { x: 12.2, y: 7.02, w: 0.5, h: 0.3, fontSize: 10, color: C.accent6, align: "right" },
});
pres.defineSlideMaster({
  title: "CLOSING",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.7, y: 0.6, w: 11.9, h: 1.0,
        fontSize: 40, bold: true, color: C.background1, align: "left", margin: 0 }, text: "" } },
  ],
});

// ------------------------------------------------------------------ helpers
async function icon(Comp, hex) {
  const svg = ReactDOMServer.renderToStaticMarkup(React.createElement(Comp, { color: "#" + hex, size: 256 }));
  return "image/png;base64," + (await sharp(Buffer.from(svg)).png().toBuffer()).toString("base64");
}
async function badge(slide, Comp, x, y, d, fill, fg, name) {
  slide.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color: fill }, line: { color: fill }, objectName: name + " circle" });
  slide.addImage({ data: await icon(Comp, fg), x: x + d * 0.25, y: y + d * 0.25, w: d * 0.5, h: d * 0.5,
    objectName: name + " icon", altText: name });
}
function text(slide, t, x, y, w, h, o = {}) {
  slide.addText(t, Object.assign({ x, y, w, h, fontSize: 16, color: C.text1, margin: 0, valign: "top", isTextBox: true }, o));
}
function card(slide, x, y, w, h, lines, opts = {}) {
  const size = opts.size || 15, name = opts.name || "code";
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.12,
    fill: { color: C.text1 }, line: { color: C.text1 }, objectName: name + " card" });
  if (opts.label) text(slide, opts.label, x + 0.25, y + 0.12, w - 0.5, 0.3, { fontSize: 11, color: C.accent4, fontFace: MONO });
  const runs = [];
  lines.forEach((ln, i) => {
    const last = i === lines.length - 1;
    let code = ln, comment = "";
    const k = ln.indexOf("#");
    if (k >= 0) { code = ln.slice(0, k); comment = ln.slice(k); }
    if (code.startsWith("$ ")) { runs.push({ text: "$ ", options: { color: C.accent2, bold: true } }); code = code.slice(2); }
    runs.push({ text: code || " ", options: { color: C.background1, breakLine: last && !comment ? false : !comment } });
    if (comment) runs.push({ text: comment, options: { color: C.accent4, breakLine: !last } });
  });
  const top = opts.label ? 0.45 : 0.2;
  slide.addText(runs, { x: x + 0.25, y: y + top, w: w - 0.5, h: h - top - 0.15, fontFace: MONO, fontSize: size,
    valign: "top", margin: 0, paraSpaceAfter: 2, isTextBox: true, objectName: name + " text" });
}

// A screenshot in a thin frame, fitted into a box; returns its placement so
// that callouts can be put on the elements recorded in shots.json.
function shot(slide, name, x, y, w, h, alt, crop) {
  const SW = META.width, SH = META.height;
  const c = crop || { x: 0, y: 0, w: SW, h: SH };            // CSS-pixel region shown
  let fw = w, fh = w * c.h / c.w;
  if (fh > h) { fh = h; fw = h * c.w / c.h; }
  const px = x + (w - fw) / 2, py = y + (h - fh) / 2;
  const sc = fw / c.w;                                         // inches per CSS pixel
  const img = { path: path.join(SHOTS, name + ".png"), x: px, y: py, altText: alt, objectName: "screenshot " + name };
  if (crop) Object.assign(img, { w: SW * sc, h: SH * sc,       // full image size; crop shows the region
    sizing: { type: "crop", x: c.x * sc, y: c.y * sc, w: fw, h: fh } });
  else Object.assign(img, { w: fw, h: fh });
  slide.addImage(img);
  slide.addShape(pres.shapes.RECTANGLE, { x: px, y: py, w: fw, h: fh, fill: { type: "none" },
    line: { color: HEX.lt2, width: 1 }, objectName: "frame " + name });
  return { name, px, py, s: fw / c.w, c };
}
// numbered orange marker at an element of the screenshot
function callout(slide, place, key, n, dx = 0, dy = 0) {
  const r = META.rects[place.name][key];
  if (!r) throw new Error(`no rect ${place.name}.${key}`);
  const d = 0.36;                                              // dx, dy: offsets in CSS pixels
  const x = +(place.px + (r.x - place.c.x + dx) * place.s - d / 2).toFixed(3);
  const y = +(place.py + (r.y - place.c.y + dy) * place.s - d / 2).toFixed(3);
  slide.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color: C.accent2 }, line: { color: HEX.lt1, width: 1.5 },
    objectName: `callout ${n}` });
  slide.addText(String(n), { x, y, w: d, h: d, fontSize: 13, bold: true, color: C.background1, align: "center",
    valign: "middle", margin: 0, isTextBox: true, objectName: `callout ${n} number` });
}
function legend(slide, items, x, y, w, rowH = 0.72, size = 15, first = 1) {
  items.forEach(([head, body], k) => {
    const i = k + first - 1;
    const yy = y + k * rowH, d = 0.36;
    slide.addShape(pres.shapes.OVAL, { x, y: yy, w: d, h: d, fill: { color: C.accent2 }, line: { color: C.accent2 }, objectName: `legend ${i + 1}` });
    text(slide, String(i + 1), x, yy, d, d, { fontSize: 13, bold: true, color: C.background1, align: "center", valign: "middle" });
    text(slide, [{ text: head, options: { bold: true } }, { text: body ? "  " + body : "" }], x + 0.5, yy, w - 0.5, rowH - 0.05,
      { fontSize: size, valign: "top" });
  });
}

// ------------------------------------------------------------------ slides
async function build() {
  let s, p;

  // 1 -------------------------------------------------------------------
  pres.addSection({ title: "Introduction" });
  s = pres.addSlide({ masterName: "TITLE", sectionTitle: "Introduction" });
  s.addText("Plan your stimuli in the browser", { placeholder: "title" });
  s.addText("A hands-on tutorial for the StimGen 2 planner: from a first current step to shuffled, repeated protocols",
            { placeholder: "body" });
  card(s, 0.7, 5.35, 6.2, 0.75, [URL], { size: 20, name: "url" });
  text(s, "Michele Giugliano  ·  MIT License", 0.7, 6.55, 6, 0.35, { fontSize: 13, color: C.accent6 });
  shot(s, "overview", 7.5, 1.4, 5.3, 4.8, "The StimGen 2 planner: text editor on the left, plot on the right");
  s.addNotes("This tutorial shows how to plan electrophysiology stimuli in a web browser with the StimGen 2 " +
    "planner. Nothing needs to be installed: open the address on the slide. By the end you will be able to build " +
    "a family of current steps, repeated and shuffled, by clicking or by writing a few readable lines.");

  // 2 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Introduction" });
  s.addText("A protocol is a sentence", { placeholder: "title" });
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.6, y: 1.5, w: 5.4, h: 4.3, rectRadius: 0.12,
    fill: { color: C.background2 }, line: { color: C.background2 }, objectName: "plain english card" });
  await badge(s, fa.FaComment, 0.9, 1.75, 0.7, HEX.accent1, HEX.lt1, "words");
  text(s, "What you would tell a colleague", 1.8, 1.85, 4, 0.5, { fontSize: 18, bold: true });
  text(s, "“Inject current steps from −300 to +300 pA, every 50 pA. Each step lasts 1 s, with half a second of rest " +
       "before and after. Play every step 3 times, in a shuffled order, one trial every 5 seconds.”",
       0.9, 2.6, 4.8, 3.0, { fontSize: 18, italic: true, color: C.text1 });
  s.addShape(pres.shapes.RIGHT_ARROW, { x: 6.15, y: 3.4, w: 0.55, h: 0.5, fill: { color: C.accent2 }, line: { color: C.accent2 },
    objectName: "arrow" });
  card(s, 6.85, 1.5, 5.85, 4.3, [
    "sg 2 protocol",
    "sweep  amp = from -300pA to 300pA step 50pA",
    "repeat 3",
    "order  shuffled-blocks",
    "period 5s",
    "stimulus {",
    "    500ms dc(0)",
    "    1s    dc($amp)",
    "    500ms dc(0)",
    "}",
  ], { label: "steps.sg — what you write", size: 15, name: "protocol" });
  text(s, [
    { text: "The same information, almost word for word. ", options: { bold: true } },
    { text: "Ten short lines describe 39 trials, cannot be misread, and can be checked, plotted and reused. The planner helps you write them — by typing or by clicking." },
  ], 0.6, 6.05, 12.1, 0.8, { fontSize: 16 });
  s.addNotes("Every protocol you run can be said in one or two sentences. StimGen 2 lets you write it almost as " +
    "you would say it: sweep the amplitude, repeat three times, shuffle, one trial every five seconds. Ten lines " +
    "describe thirty-nine trials. Nobody has to guess what was played, and the file is saved with every recording.");

  // 3 -------------------------------------------------------------------
  pres.addSection({ title: "Getting started" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Getting started" });
  s.addText("What you see when you open it", { placeholder: "title" });
  p = shot(s, "overview", 0.6, 1.3, 8.3, 5.5, "The planner window with its parts numbered");
  callout(s, p, "menus", 1, 6, 10); callout(s, p, "palette", 2, 0, 10); callout(s, p, "editor", 3, 18, 18);
  callout(s, p, "status", 4, 12, 10); callout(s, p, "timeline", 5, 0, 12); callout(s, p, "plots", 6, 20, 30);
  callout(s, p, "play", 7, 0, 12); callout(s, p, "save", 8, 0, 4);
  legend(s, [["Menus", "file, edit, insert, examples, help"], ["Palette", "a button per generator"],
    ["Text", "the description itself"], ["Status", "ok, warnings, errors"], ["Segments", "click one to edit it"],
    ["Plot", "updates as you type"], ["Play", "animate the stimulus"], ["Save", "the .sg text, the .sgb samples"]],
    9.2, 1.4, 3.6, 0.66, 14);
  s.addNotes("The window has two sides. On the left, the text of the stimulus, with a palette of generators " +
    "above it and a status line below. On the right, the plot: it is redrawn as you type. Above the plot, a strip " +
    "of segments that you can click, and a play bar to animate the stimulus. The menus at the top do the rest.");

  // 4 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Getting started" });
  s.addText("Start from an example", { placeholder: "title" });
  p = shot(s, "examples-menu", 0.6, 1.3, 7.6, 5.5, "The Examples menu", { x: 0, y: 0, w: 900, h: 760 });
  await badge(s, fa.FaListUl, 8.6, 1.5, 0.75, HEX.accent1, HEX.lt1, "examples");
  text(s, "43 ready-made stimuli", 9.55, 1.62, 3.2, 0.5, { fontSize: 20, bold: true, fontFace: THEME.headFontFace });
  text(s, [
    { text: "Basics", options: { bold: true, breakLine: true } }, { text: "steps, rheobase ramp, sag, membrane time constant", options: { breakLine: true } },
    { text: "Waves · Synaptic · Noise", options: { bold: true, breakLine: true } }, { text: "ZAP chirps, EPSC trains, Poisson barrages, frozen noise", options: { breakLine: true } },
    { text: "Stimuli", options: { bold: true, breakLine: true } }, { text: "two cells, voltage clamp, dynamic clamp, optogenetics", options: { breakLine: true } },
    { text: "Protocols", options: { bold: true, breakLine: true } }, { text: "f–I curve, reliability, paired-pulse ratio, I–V" },
  ], 8.6, 2.5, 4.1, 3.4, { fontSize: 15, paraSpaceAfter: 4 });
  text(s, "Pick one, change a number, watch the plot. Your text is kept in the browser.", 8.6, 6.0, 4.1, 0.8,
    { fontSize: 15, italic: true, color: C.accent2 });
  s.addNotes("The quickest start is the Examples menu: forty-three complete stimuli, grouped from basic steps to " +
    "full protocols. Choose one that resembles what you need, then change its numbers. The planner remembers your " +
    "text, so you can close the tab and come back.");

  // 5 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Getting started" });
  s.addText("Type, and the plot follows", { placeholder: "title" });
  card(s, 0.6, 1.4, 5.3, 1.75, ["500ms  dc(0)      # rest", "1s     dc(300)    # 300 pA", "500ms  dc(0)      # rest"],
       { label: "three lines = a current step", size: 17, name: "step" });
  const parts = [["500ms", "how long", "always with a unit: ms, s, min"], ["dc", "what", "a generator: dc, ramp, sine, pulses, ou…"],
                 ["(300)", "how much", "parameters, by position or by name"]];
  parts.forEach(([a, b, c], i) => {
    const y = 3.45 + i * 0.95;
    text(s, a, 0.6, y, 1.3, 0.5, { fontSize: 18, fontFace: MONO, bold: true, color: C.accent2 });
    text(s, b, 1.95, y, 1.4, 0.5, { fontSize: 17, bold: true });
    text(s, c, 3.35, y, 2.6, 0.8, { fontSize: 14, color: C.accent6 });
  });
  text(s, [{ text: "live", options: { bold: true } }, { text: " on: the plot follows every key. Off: press " },
           { text: "Plot ⟳", options: { bold: true } }, { text: " (⌘/Ctrl+Enter) when you are ready." }],
       0.6, 6.35, 5.3, 0.6, { fontSize: 14 });
  p = shot(s, "overview", 6.2, 1.35, 6.5, 5.4, "A step typed in the editor and its plot", { x: 560, y: 160, w: 880, h: 700 });
  s.addNotes("Each line is a segment: how long, then what the output does. dc means a constant level. Write three " +
    "lines and the plot shows a current step. Units are part of the language: 500 ms, 1 s. If live is switched off, " +
    "nothing is redrawn until you press Plot, which is handy for very long stimuli.");

  // 6 -------------------------------------------------------------------
  pres.addSection({ title: "Building" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Building" });
  s.addText("Build a segment without typing", { placeholder: "title" });
  p = shot(s, "builder", 0.6, 1.3, 8.3, 5.5, "The segment builder with its preview");
  callout(s, p, "palette", 1, -34, 40); callout(s, p, "builder", 2, 360, 13); callout(s, p, "preview", 3, 300, 12);
  callout(s, p, "apply", 4, 175, 22); callout(s, p, "plots", 5, 60, 70);
  legend(s, [["Click a generator", "here: sine"], ["Fill in the form", "units from the menus; extras under “more options”"],
    ["Read the line", "this is what will be written"], ["Insert", "or Cancel (Esc)"], ["See it first", "the plot previews the new segment"]],
    9.2, 1.4, 3.6, 1.0, 15);
  s.addNotes("You never have to remember a parameter name. Click a generator in the palette and a form opens under " +
    "the text, with sensible starting values and units chosen from menus. The plot already shows the result, and " +
    "the line that will be written is printed in green. Insert writes it into the text; Cancel leaves everything as it was.");

  // 7 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Building" });
  s.addText("Change a segment: just click it", { placeholder: "title" });
  p = shot(s, "timeline-edit", 0.6, 1.3, 8.3, 5.5, "Editing a segment chosen on the timeline");
  callout(s, p, "timeline", 1, 470, 10); callout(s, p, "builder", 2, 24, 18); callout(s, p, "gutter", 3, 12, 30);
  legend(s, [["Click a segment", "in the strip above the plot"], ["Edit its values", "the same form, already filled in"],
             ["Apply", "only that line changes"]], 9.2, 1.4, 3.6, 1.0, 15);
  text(s, "Or put the cursor on a line and choose Edit → Edit the segment at the cursor.", 9.2, 4.7, 3.6, 1.2,
       { fontSize: 14, italic: true, color: C.accent6 });
  s.addNotes("To change a segment, click it in the strip above the plot. The same form opens with the current values, " +
    "read back from the text. Change the frequency, press Apply, and only that line of text is rewritten. The text " +
    "always remains the master copy.");

  // 8 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Building" });
  s.addText("Combine: a step with noise on top", { placeholder: "title" });
  card(s, 0.6, 1.4, 4.9, 1.5, ["1s  dc(0)", "3s  dc(100)", "1s  dc(0)"], { label: "before: select line 2", size: 16, name: "before" });
  text(s, "Edit → Combine the selection with a generator → ou, operation + add", 0.6, 3.05, 4.9, 0.7, { fontSize: 14, bold: true, color: C.accent2 });
  card(s, 0.6, 3.75, 4.9, 1.6, ["1s  dc(0)", "3s  (dc(100)) + ou(mean=0,", "        sd=20, tau=5ms)", "1s  dc(0)"],
       { label: "after", size: 16, name: "after" });
  text(s, "The same way: multiply by an envelope, Edit → Repeat lines N times, add a marker for the analysis.",
       0.6, 5.6, 4.9, 1.0, { fontSize: 14 });
  shot(s, "combine-done", 5.8, 1.35, 6.9, 5.45, "A step with noise added", { x: 560, y: 160, w: 880, h: 700 });
  s.addNotes("Real stimuli are often combinations: a step with noise on top, a sine under an envelope. Select the " +
    "line, choose Edit, Combine, pick a generator and the operation. The planner writes the combined line for you. " +
    "Repeat and markers work the same way, from the Edit menu.");

  // 9 -------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Building" });
  s.addText("From one step to a whole protocol", { placeholder: "title" });
  const steps9 = [["Select a value", "in the text: 300pA"], ["Make → Sweep", "give the values"], ["Done", "a protocol, one trial per value"]];
  steps9.forEach(([a, b], i) => {
    const x = 0.6 + i * 4.15;
    s.addShape(pres.shapes.OVAL, { x, y: 1.35, w: 0.45, h: 0.45, fill: { color: C.accent2 }, line: { color: C.accent2 }, objectName: "step " + (i + 1) });
    text(s, String(i + 1), x, 1.35, 0.45, 0.45, { fontSize: 15, bold: true, color: C.background1, align: "center", valign: "middle" });
    text(s, [{ text: a, options: { bold: true } }, { text: "  " + b }], x + 0.6, 1.4, 3.4, 0.45, { fontSize: 16 });
  });
  shot(s, "sweep-dialog", 0.6, 2.05, 6.0, 4.75, "The sweep dialog", { x: 200, y: 160, w: 860, h: 500 });
  shot(s, "sweep-done", 6.75, 2.05, 5.95, 4.75, "The resulting protocol with its trials");
  s.addNotes("This is the most useful trick. Select a number in the text, for example the amplitude of a step, and " +
    "choose Make, Sweep. Give the values you want. The planner turns your stimulus into a protocol: one trial per value, " +
    "with the value replaced by a variable. Repetitions, order and timing are one line each, as on the next slide.");

  // 10 ------------------------------------------------------------------
  pres.addSection({ title: "Protocols" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Protocols" });
  s.addText("The protocol language reads like English", { placeholder: "title" });
  const H = (t) => ({ text: t, options: { bold: true, color: C.background1, fill: { color: C.accent1 } } });
  const M = (t) => ({ text: t, options: { fontFace: MONO, color: C.accent1, bold: true } });
  s.addTable([
    [H("you write"), H("it means")],
    [M("sweep amp = from -300pA to 300pA step 50pA"), "amplitudes −300 … +300 pA, every 50 pA"],
    [M("sweep f = [5Hz, 10Hz, 20Hz]"), "these three frequencies"],
    [M("repeat 3"), "play every condition 3 times"],
    [M("order shuffled-blocks"), "random order; every condition once before any repeats"],
    [M("period 5s     ·     gap 2s"), "one trial every 5 s  ·  2 s of pause between trials"],
    [M("noise per-condition"), "repetitions get the same noise (frozen noise)"],
    [M("let charge = amp * dur"), "a quantity computed from other variables"],
    [M("dc($amp)"), "$amp is replaced by the value of each trial"],
  ], { x: 0.6, y: 1.35, w: 12.1, colW: [6.2, 5.9], fontSize: 15, fontFace: THEME.bodyFontFace, color: C.text1,
       border: { type: "solid", pt: 0.75, color: HEX.lt2 }, rowH: 0.52, valign: "middle", objectName: "language table" });
  text(s, "Several sweeps combine into every combination. Help → protocol lists everything.", 0.6, 6.35, 12.1, 0.5,
       { fontSize: 14, italic: true, color: C.accent6 });
  s.addNotes("These are the words of the protocol language. Sweep gives a variable its values, as a list or a range. " +
    "Repeat sets the repetitions. Order says how trials are shuffled; shuffled-blocks plays every condition once " +
    "before any is repeated, which protects against slow drifts. Period or gap set the timing. Noise per-condition " +
    "freezes the noise across repetitions, for reliability experiments.");

  // 11 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Protocols" });
  s.addText("See all the trials, or one at a time", { placeholder: "title" });
  p = shot(s, "protocol-overlay", 0.6, 1.3, 6.0, 4.6, "Eight trials overlaid", { x: 560, y: 60, w: 880, h: 800 });
  callout(s, p, "trial", 1, 14, 10); callout(s, p, "tview", 2, 14, 10); callout(s, p, "tcount", 3, 14, 10);
  shot(s, "protocol-stack", 6.75, 1.3, 5.95, 4.6, "Five trials stacked", { x: 560, y: 60, w: 880, h: 800 });
  [["Trial", "pick any trial from the list"], ["Show", "this trial · overlay · stack"], ["Trials", "how many, from the selected one"]]
    .forEach(([a, b], i) => legend(s, [[a, b]], 0.6 + i * 4.05, 6.1, 3.95, 0.6, 14, i + 1));
  s.addNotes("A protocol produces many trials. Choose any trial from the list to see it alone, or show several: " +
    "overlay draws them on the same axes, coloured by condition, with the selected trial in bold; stack gives each " +
    "trial its own row. The number of trials shown is up to you.");

  // 12 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Protocols" });
  s.addText("Watch it play", { placeholder: "title" });
  p = shot(s, "animate", 0.6, 1.3, 8.3, 5.5, "The animation of a paired-pulse protocol");
  callout(s, p, "play", 1, 4, 6); callout(s, p, "speed", 2, 4, 6); callout(s, p, "interval", 3, 4, 6); callout(s, p, "plots", 4, 470, 60);
  legend(s, [["Play / Pause", "a cursor runs through the stimulus"], ["Speed", "¼× to 20×; 1× is real time"],
             ["Interval", "time between trials, taken from the protocol"], ["Trial after trial", "highlighted in turn; loop if you like"]],
         9.2, 1.4, 3.6, 1.05, 15);
  s.addNotes("Press Play and a cursor runs through the stimulus at the speed you choose. After each trial the " +
    "planner waits the interval, taken from the protocol's period, and moves to the next trial. It is the easiest " +
    "way to explain a protocol to a student, or to check its timing before an experiment.");

  // 13 ------------------------------------------------------------------
  pres.addSection({ title: "Checking" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Checking" });
  s.addText("Noise: new every time, or frozen", { placeholder: "title" });
  p = shot(s, "noise-seed", 0.6, 1.3, 7.6, 5.5, "Fresh and frozen noise");
  callout(s, p, "seed", 1, 6, 8); callout(s, p, "keep", 2, 4, 8); callout(s, p, "status", 3, 12, 8);
  const rows13 = [[fa.FaDice, "No seed", "a new realisation at every rendering (Plot ⟳ draws again)"],
                  [fa.FaLock, "seed=17 in the text", "that segment is identical, always (frozen noise)"],
                  [fa.FaThumbtack, "keep", "freeze the noise you are looking at"]];
  for (let i = 0; i < 3; i++) {
    const y = 1.5 + i * 1.45;
    await badge(s, rows13[i][0], 8.6, y, 0.7, HEX.accent1, HEX.lt1, "noise " + (i + 1));
    text(s, rows13[i][1], 9.45, y, 3.3, 0.4, { fontSize: 17, bold: true });
    text(s, rows13[i][2], 9.45, y + 0.42, 3.3, 0.9, { fontSize: 14, color: C.accent6 });
  }
  text(s, "The status line always says which seed was used, and where it came from.", 8.6, 5.9, 4.1, 0.8,
       { fontSize: 14, italic: true, color: C.accent2 });
  s.addNotes("Noise follows the rules of the specification. Without a seed, every rendering draws a new realisation, " +
    "exactly as in an experiment. A seed written in the text freezes that noise, wherever it appears. The keep button " +
    "copies the seed you are looking at, so the same noise stays on the screen. The status line tells you which seed was used.");

  // 14 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Checking" });
  s.addText("Zoom, check, fix", { placeholder: "title" });
  shot(s, "axes", 0.6, 1.3, 6.0, 4.4, "Zooming the time axis", { x: 560, y: 60, w: 880, h: 800 });
  shot(s, "error", 6.75, 1.3, 5.95, 4.4, "An error marked on its line", { x: 0, y: 160, w: 780, h: 700 });
  text(s, [{ text: "Axes. ", options: { bold: true } }, { text: "Drag on the plot to zoom time, Shift+drag for values, double-click to go back. Or type the limits; fixed limits are kept while you edit." }],
       0.6, 5.9, 6.0, 1.0, { fontSize: 14 });
  text(s, [{ text: "Errors. ", options: { bold: true } }, { text: "A mistake marks its line in orange and says what is wrong (here: a frequency written as a time). Nothing is played until it is fixed." }],
       6.75, 5.9, 5.95, 1.0, { fontSize: 14 });
  s.addNotes("To look closer, drag across the plot to zoom in time; hold Shift to zoom the values; double-click to " +
    "return to automatic. If you make a mistake, the line turns orange and the status line explains it, here a " +
    "frequency written in milliseconds. A wrong stimulus can never be played by accident.");

  // 15 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Checking" });
  s.addText("Several outputs at once", { placeholder: "title" });
  p = shot(s, "stimulus", 0.6, 1.3, 8.3, 5.5, "A stimulus with two cells and a camera trigger");
  const ch = [["channel pre unit=pA", "a current for the first cell"], ["channel post unit=pA", "a current for the second"],
              ["digital camera", "TTL pulses for a camera"], ["@train", "a marker saved with the data"]];
  ch.forEach(([a, b], i) => {
    text(s, a, 9.2, 1.45 + i * 1.05, 3.6, 0.4, { fontSize: 15, fontFace: MONO, bold: true, color: C.accent1 });
    text(s, b, 9.2, 1.85 + i * 1.05, 3.6, 0.5, { fontSize: 14 });
  });
  text(s, "Make → Waveform → stimulus and Make → Add a channel write these lines for you.", 9.2, 5.75, 3.6, 0.9,
       { fontSize: 14, italic: true, color: C.accent6 });
  s.addNotes("Many experiments drive several outputs: two cells in a pair, a camera trigger, a light source. A " +
    "stimulus file lists them as channels with their units, on one time base. The Make menu converts a waveform into " +
    "a stimulus and adds channels; markers label the moments you will want to find in the analysis.");

  // 16 ------------------------------------------------------------------
  pres.addSection({ title: "To the rig" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "To the rig" });
  s.addText("Take it to the rig", { placeholder: "title" });
  const flow = [[fa.FaGlobe, "Planner", "plan, check, animate"], [fa.FaSave, "Save .sg", "a few lines of text"],
                [fa.FaTerminal, "sg · Gecko", "rendered, then played at the rig"], [fa.FaMicrochip, "NI board", "the cell receives it"]];
  for (let i = 0; i < flow.length; i++) {
    const x = 0.6 + i * 3.1;
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y: 1.5, w: 2.7, h: 2.3, rectRadius: 0.12,
      fill: { color: i === 1 ? C.accent1 : C.background2 }, line: { color: i === 1 ? C.accent1 : C.background2 }, objectName: "flow " + i });
    await badge(s, flow[i][0], x + 0.95, 1.7, 0.8, i === 1 ? HEX.lt1 : HEX.accent1, i === 1 ? HEX.accent1 : HEX.lt1, "flow " + i);
    text(s, flow[i][1], x + 0.1, 2.65, 2.5, 0.45, { fontSize: 18, bold: true, align: "center", color: i === 1 ? C.background1 : C.text1 });
    text(s, flow[i][2], x + 0.1, 3.1, 2.5, 0.5, { fontSize: 14, align: "center", color: i === 1 ? C.background1 : C.accent6 });
    if (i < flow.length - 1) s.addShape(pres.shapes.RIGHT_ARROW, { x: x + 2.73, y: 2.45, w: 0.34, h: 0.4,
      fill: { color: C.accent2 }, line: { color: C.accent2 }, objectName: "flow arrow " + i });
  }
  card(s, 0.6, 4.2, 6.6, 2.4, ["$ sg check steps.sg          # 39 trials", "$ sg render -r 20kHz steps.sg # samples",
                               "# Gecko, the rig program, plays the trials"], { label: "the same file, on the command line", size: 15, name: "cli" });
  text(s, [{ text: "Save .sg ", options: { bold: true } }, { text: "keeps the text: it is the protocol. " },
           { text: "Save .sgb ", options: { bold: true } }, { text: "keeps the rendered samples with a full record (text, seed, rate), so any trial can be regenerated years later." }],
       7.5, 4.3, 5.2, 2.2, { fontSize: 15 });
  s.addNotes("When the stimulus is right, save it. The .sg file is the protocol itself: a few lines that the command-" +
    "line program sg checks and renders it, and Gecko, the separate program that drives the National Instruments board, plays it at the rig. The .sgb file keeps the " +
    "samples together with the record needed to regenerate them.");

  // 17 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "To the rig" });
  s.addText("Why this works so well in a browser: WebAssembly", { placeholder: "title" });
  text(s, "The planner does not imitate StimGen 2: it runs the very same C program as the command line, compiled to " +
          "WebAssembly, a format every modern browser executes at near-native speed.", 0.6, 1.3, 12.1, 0.9, { fontSize: 17 });
  const why = [[fa.FaDownload, "Nothing to install", "any computer, any system: open the page"],
               [fa.FaUserShield, "Nothing uploaded", "everything runs on your machine; it even works offline"],
               [fa.FaEquals, "The same results", "same seeds, same samples as the sg command line — checked by 76 automatic tests"],
               [fa.FaFeather, "Small and fast", "170 KB; minutes of stimulus render in about a second"]];
  for (let i = 0; i < 4; i++) {
    const x = 0.6 + (i % 2) * 6.15, y = 2.45 + Math.floor(i / 2) * 2.15;
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w: 5.95, h: 1.9, rectRadius: 0.12,
      fill: { color: C.background2 }, line: { color: C.background2 }, objectName: "why " + i });
    await badge(s, why[i][0], x + 0.3, y + 0.35, 0.8, HEX.accent1, HEX.lt1, "why " + i);
    text(s, why[i][1], x + 1.35, y + 0.35, 4.4, 0.5, { fontSize: 19, bold: true });
    text(s, why[i][2], x + 1.35, y + 0.9, 4.4, 0.9, { fontSize: 15, color: C.accent6 });
  }
  s.addNotes("One technical point worth knowing. The planner is not a separate imitation of StimGen: it is the same C " +
    "program as the command-line tool, compiled to WebAssembly so that the browser runs it directly. That is why there " +
    "is nothing to install, why your stimuli never leave your computer, and why what you see is exactly what the rig plays.");

  // 18 ------------------------------------------------------------------
  pres.addSection({ title: "Wrap-up" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Wrap-up" });
  s.addText("Cheat sheet", { placeholder: "title" });
  const K = (t) => ({ text: t, options: { color: C.accent1, bold: true } });
  s.addTable([
    [H("I want to …"), H("do this")],
    ["start quickly", K("Examples menu")],
    ["add a segment", K("click a generator in the palette → Insert")],
    ["change a segment", K("click it in the segment strip → Apply")],
    ["add noise or an envelope", K("select the line → Edit → Combine")],
    ["repeat part of a stimulus", K("select the lines → Edit → Repeat")],
    ["vary a value across trials", K("select the value → Make → Sweep")],
    ["see several trials", K("show: overlay or stack")],
    ["freeze the noise", K("keep (or seed=17 in the text)")],
    ["zoom", K("drag on the plot · double-click to reset")],
    ["take it to the rig", K("File → Save .sg")],
  ], { x: 0.6, y: 1.3, w: 12.1, colW: [4.2, 7.9], fontSize: 15, fontFace: THEME.bodyFontFace, color: C.text1,
       border: { type: "solid", pt: 0.75, color: HEX.lt2 }, rowH: 0.47, valign: "middle", objectName: "cheat sheet" });
  s.addNotes("Everything on one page. Keep it next to the computer for the first sessions.");

  // 19 ------------------------------------------------------------------
  s = pres.addSlide({ masterName: "CLOSING", sectionTitle: "Wrap-up" });
  s.addText("Open it and try", { placeholder: "title" });
  card(s, 0.7, 1.9, 7.2, 1.0, [URL], { size: 28, name: "url" });
  const next = [["1. Examples → A family of steps", "press Play"], ["2. Change 50pA into 100pA", "watch the trials change"],
                ["3. Make it yours", "Save .sg and bring it to the rig"]];
  next.forEach(([a, b], i) => {
    text(s, a, 0.7, 3.3 + i * 0.95, 7.2, 0.44, { fontSize: 19, bold: true, color: C.background1 });
    text(s, b, 0.7, 3.78 + i * 0.95, 7.2, 0.4, { fontSize: 15, color: C.accent4 });
  });
  shot(s, "protocol-overlay", 8.3, 1.9, 4.4, 4.4, "A protocol in the planner", { x: 560, y: 60, w: 880, h: 800 });
  text(s, "StimGen 2 · Michele Giugliano · MIT License · Help → Generators and topics", 0.7, 6.65, 11, 0.4,
       { fontSize: 13, color: C.accent6 });
  s.addNotes("Open the planner now and try three things: load the family of steps and press Play; change the step " +
    "size and watch the trials change; then make a protocol of your own and save it for the rig.");

  await pres.writeFile({ fileName: OUT });
  if (process.env.SKILL_DIR) {
    const { applyTheme } = require(path.join(process.env.SKILL_DIR, "scripts", "apply_theme.js"));
    await applyTheme(OUT, THEME);
  }
  await require("./fix_ids.js").fixIds(OUT);   // pptxgenjs reuses shape id 25
  console.log("wrote", OUT);
}

build().catch((e) => { console.error(e); process.exit(1); });
