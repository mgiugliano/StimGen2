// StimGen 2 -- SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michele Giugliano. See LICENSE for the full text.
//
// build_deck.js -- the beginner's tutorial deck "Stimuli as text".
//
//   python3 make_slide_figures.py        # figures rendered by sg
//   node build_deck.js                   # writes StimGen2-tutorial.pptx
//
// Needs the npm packages pptxgenjs, react, react-dom, react-icons, sharp.
// SKILL_DIR may point to a directory with scripts/apply_theme.js (optional).

const path = require("path");
const fs = require("fs");
const pptxgen = require("pptxgenjs");
const React = require("react");
const ReactDOMServer = require("react-dom/server");
const sharp = require("sharp");
const fa = require("react-icons/fa");

const HERE = __dirname;
const FIGS = path.join(HERE, "figs");
const SIZES = JSON.parse(fs.readFileSync(path.join(FIGS, "figs.json"), "utf8"));
const OUT = path.join(HERE, "StimGen2-tutorial.pptx");

// ------------------------------------------------------------------ theme
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
pres.layout = "LAYOUT_WIDE";                       // 13.33 x 7.5 in
pres.theme = { headFontFace: THEME.headFontFace, bodyFontFace: THEME.bodyFontFace };
pres.author = "Michele Giugliano";
pres.title = "Stimuli as text: a beginner's guide to StimGen 2";
pres.subject = "Tutorial on generating electrophysiology stimuli with sg";
const C = pres.SchemeColor;

// ------------------------------------------------------------------ layouts
pres.defineSlideMaster({
  title: "TITLE",
  background: { color: C.text1 },
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.7, y: 2.0, w: 7.4, h: 1.6,
        fontSize: 48, bold: true, color: C.background1, valign: "bottom", align: "left", margin: 0 }, text: "" } },
    { placeholder: { options: { name: "body", type: "body", x: 0.7, y: 3.75, w: 7.2, h: 1.2,
        fontSize: 20, color: C.accent4, valign: "top", margin: 0 }, text: "" } },
  ],
});
pres.defineSlideMaster({
  title: "CONTENT",
  background: { color: C.background1 },
  margin: [0.5, 0.6, 0.6, 0.6],
  objects: [
    { placeholder: { options: { name: "title", type: "title", x: 0.6, y: 0.35, w: 12.1, h: 0.85,
        fontSize: 32, bold: true, color: C.text1, valign: "middle", align: "left", margin: 0 }, text: "" } },
    { text: { text: "StimGen 2 · a beginner's tutorial", options: { x: 0.6, y: 7.0, w: 6, h: 0.3,
        fontSize: 10, color: C.accent6, margin: 0 } } },
  ],
  slideNumber: { x: 12.2, y: 7.0, w: 0.5, h: 0.3, fontSize: 10, color: C.accent6, align: "right" },
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
  const png = await sharp(Buffer.from(svg)).png().toBuffer();
  return "image/png;base64," + png.toString("base64");
}

// Icon in a filled circle: the recurring visual cue next to headings.
async function badge(slide, Comp, x, y, d, fill, fg, name) {
  slide.addShape(pres.shapes.OVAL, { x, y, w: d, h: d, fill: { color: fill }, line: { color: fill },
    objectName: name + " circle" });
  slide.addImage({ data: await icon(Comp, fg), x: x + d * 0.25, y: y + d * 0.25, w: d * 0.5, h: d * 0.5,
    objectName: name + " icon", altText: name });
}

// A terminal card: the text to type, on a dark rounded card.  Lines that
// start with "$ " are commands; "#" starts a comment (dimmed).
function card(slide, x, y, w, h, lines, opts = {}) {
  const size = opts.size || 15;
  slide.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w, h, rectRadius: 0.12,
    fill: { color: C.text1 }, line: { color: C.text1 }, objectName: (opts.name || "code") + " card" });
  if (opts.label) {
    slide.addText(opts.label, { x: x + 0.25, y: y + 0.12, w: w - 0.5, h: 0.3, fontSize: 11,
      color: C.accent4, fontFace: MONO, margin: 0, isTextBox: true, objectName: (opts.name || "code") + " label" });
  }
  const runs = [];
  lines.forEach((ln, i) => {
    const last = i === lines.length - 1;
    let code = ln, comment = "";
    const k = ln.indexOf("#");
    if (k >= 0) { code = ln.slice(0, k); comment = ln.slice(k); }
    if (code.startsWith("$ ")) {
      runs.push({ text: "$ ", options: { color: C.accent2, bold: true } });
      code = code.slice(2);
    }
    runs.push({ text: code || " ", options: { color: C.background1, breakLine: last && !comment ? false : !comment } });
    if (comment) runs.push({ text: comment, options: { color: C.accent4, breakLine: !last } });
  });
  const top = opts.label ? 0.45 : 0.2;
  slide.addText(runs, { x: x + 0.25, y: y + top, w: w - 0.5, h: h - top - 0.15, fontFace: MONO,
    fontSize: size, valign: "top", margin: 0, paraSpaceAfter: 2, isTextBox: true,
    objectName: (opts.name || "code") + " text" });
}

// A figure rendered by sg, fitted into a box with its own aspect ratio.
function figure(slide, name, x, y, w, h, alt) {
  const [pw, ph] = SIZES[name];
  let fw = w, fh = w * ph / pw;
  if (fh > h) { fh = h; fw = h * pw / ph; }
  slide.addImage({ path: path.join(FIGS, name + ".png"), x: x + (w - fw) / 2, y: y + (h - fh) / 2,
    w: fw, h: fh, objectName: "figure " + name, altText: alt });
}

function text(slide, t, x, y, w, h, o = {}) {
  slide.addText(t, Object.assign({ x, y, w, h, fontSize: 16, color: C.text1, margin: 0,
    valign: "top", isTextBox: true }, o));
}

// ------------------------------------------------------------------ slides
async function build() {
  let s;

  // 1. Title ---------------------------------------------------------------
  pres.addSection({ title: "Introduction" });
  s = pres.addSlide({ masterName: "TITLE", sectionTitle: "Introduction" });
  s.addText("Stimuli as text", { placeholder: "title" });
  s.addText("A beginner's guide to generating electrophysiology stimuli with StimGen 2",
    { placeholder: "body" });
  text(s, "Michele Giugliano  ·  sg 0.2  ·  MIT License", 0.7, 6.4, 7, 0.4,
    { fontSize: 14, color: C.accent6 });
  card(s, 8.6, 2.0, 4.1, 2.9, ["500ms  dc(0)", "1s     dc(300)", "500ms  dc(0)"],
    { label: "step.sg", size: 20, name: "title" });
  text(s, "three lines = a current step", 8.6, 5.05, 4.1, 0.4, { fontSize: 14, color: C.accent4, italic: true });
  s.addNotes("StimGen 2 lets you write a stimulus the way you would describe it to a colleague: " +
    "half a second at rest, one second at 300 pA, half a second at rest. This tutorial shows how to " +
    "write such texts, check them, look at them, and play them on the National Instruments board.");

  // 2. Pipeline ------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Introduction" });
  s.addText("From a line of text to a neuron", { placeholder: "title" });
  const steps = [
    [fa.FaFileAlt, "1. Write", "step.sg", "a short text file"],
    [fa.FaTerminal, "2. Check & look", "sg", "checks it, renders it, plots it"],
    [fa.FaPlay, "3. Play", "sgplay", "reads the text, drives the board"],
    [fa.FaMicrochip, "4. NI board", "AO · AI · DO", "converts numbers to volts"],
    [fa.FaBolt, "5. Amplifier", "→ cell", "current or voltage clamp"],
  ];
  for (let i = 0; i < steps.length; i++) {
    const x = 0.6 + i * 2.5, y = 1.9;
    const [ic, head, mono, sub] = steps[i];
    const dark = i === 2;
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w: 2.1, h: 3.1, rectRadius: 0.12,
      fill: { color: dark ? C.accent1 : C.background2 }, line: { color: dark ? C.accent1 : C.background2 },
      objectName: "step " + (i + 1) + " box" });
    await badge(s, ic, x + 0.6, y + 0.3, 0.9, dark ? HEX.lt1 : HEX.accent1, dark ? HEX.accent1 : HEX.lt1, "step " + (i + 1));
    text(s, head, x + 0.15, y + 1.35, 1.8, 0.4, { fontSize: 17, bold: true, align: "center",
      color: dark ? C.background1 : C.text1 });
    text(s, mono, x + 0.15, y + 1.8, 1.8, 0.4, { fontSize: 15, fontFace: MONO, align: "center",
      color: dark ? C.background1 : C.accent1 });
    text(s, sub, x + 0.15, y + 2.25, 1.8, 0.7, { fontSize: 13, align: "center",
      color: dark ? C.background1 : C.accent6 });
    if (i < steps.length - 1) {
      s.addShape(pres.shapes.RIGHT_ARROW, { x: x + 2.13, y: y + 1.35, w: 0.34, h: 0.4,
        fill: { color: C.accent2 }, line: { color: C.accent2 }, objectName: "arrow " + (i + 1) });
    }
  }
  text(s, [
    { text: "You only write step 1. ", options: { bold: true } },
    { text: "sg turns the text into samples and checks it; sgplay plays it on the board and records the response." },
  ], 0.6, 5.45, 12.1, 0.5, { fontSize: 17 });
  text(s, "sgplay stands for the acquisition program that talks to the National Instruments board. " +
    "Its name and options in this tutorial are a proposal.", 0.6, 6.15, 12.1, 0.5,
    { fontSize: 12, color: C.accent6, italic: true });
  s.addNotes("Only step one is your job: a text file. The program sg checks the text, turns it into " +
    "samples and lets you plot them before any cell is involved. The acquisition program, called sgplay " +
    "here, reads the same text, converts it to volts for the NI board, plays it and records. The name " +
    "sgplay is a placeholder for that program.");

  // 3. Why text ------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Introduction" });
  s.addText("Why write a stimulus as text", { placeholder: "title" });
  const why = [
    [fa.FaCompressAlt, "Short", "1 line", "10 minutes of noise fit in one line: the text does not grow with the duration."],
    [fa.FaRedo, "Repeatable", "same text + seed", "gives the same samples on any computer, today or in ten years."],
    [fa.FaPlug, "Portable", "no rate, no board", "inside the file: the same text plays at 10 kHz on one rig, 50 kHz on another."],
  ];
  for (let i = 0; i < 3; i++) {
    const x = 0.6 + i * 4.1, y = 1.7;
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w: 3.8, h: 4.6, rectRadius: 0.12,
      fill: { color: C.background2 }, line: { color: C.background2 }, objectName: "why card " + (i + 1) });
    await badge(s, why[i][0], x + 0.35, y + 0.35, 0.9, HEX.accent1, HEX.lt1, "why " + (i + 1));
    text(s, why[i][1], x + 1.45, y + 0.5, 2.2, 0.6, { fontSize: 24, bold: true, fontFace: THEME.headFontFace });
    text(s, why[i][2], x + 0.35, y + 1.6, 3.2, 0.7, { fontSize: 24, bold: true, color: C.accent2 });
    text(s, why[i][3], x + 0.35, y + 2.4, 3.2, 1.9, { fontSize: 16 });
  }
  s.addNotes("Three reasons. A description is short, because it describes the shape, not every sample. " +
    "It is repeatable: the noise comes from a seed that is always saved. And it is portable: nothing in " +
    "the file depends on the sampling rate or on the board.");

  // 4. First stimulus --------------------------------------------------------
  pres.addSection({ title: "Writing stimuli" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Writing stimuli" });
  s.addText("Your first stimulus in three lines", { placeholder: "title" });
  card(s, 0.6, 1.6, 5.2, 2.0, ["500ms  dc(0)      # rest", "1s     dc(300)    # step", "500ms  dc(0)      # rest"],
    { label: "step.sg", size: 17, name: "file" });
  card(s, 0.6, 3.85, 5.2, 1.5, ["$ sg render -r 20kHz -u pA step.sg", "# writes step.sgb"],
    { label: "terminal", size: 15, name: "command" });
  text(s, "One segment per line: a duration, then what the output does.", 0.6, 5.65, 5.2, 0.9, { fontSize: 16 });
  figure(s, "step", 6.2, 1.6, 6.5, 4.0, "The step waveform: 0 pA for 0.5 s, 300 pA for 1 s, 0 pA for 0.5 s");
  text(s, "the result, plotted with tools/sgplot.py", 6.2, 5.75, 6.5, 0.4, { fontSize: 13, color: C.accent6,
    italic: true, align: "center" });
  s.addNotes("Write these three lines in a file called step.sg. Each line is a segment: how long, and what. " +
    "dc means a constant level. Then run sg render with a sampling rate and the unit of the channel. " +
    "The result is step.sgb, which you can plot with python3 tools/sgplot.py step.sgb.");

  // 5. Anatomy of a line -----------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Writing stimuli" });
  s.addText("Reading one line", { placeholder: "title" });
  s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x: 0.6, y: 1.6, w: 12.1, h: 1.6, rectRadius: 0.12,
    fill: { color: C.text1 }, line: { color: C.text1 }, objectName: "big line card" });
  // 40 pt Courier New: one character is 0.6 em = 24 pt = 1/3 inch wide
  const X0 = 1.0, CW = 40 * 0.6 / 72;
  text(s, [
    { text: "1s  ", options: { color: C.accent5 } },
    { text: "sine(", options: { color: C.background1 } },
    { text: "amp=50, freq=8Hz", options: { color: C.accent4 } },
    { text: ")", options: { color: C.background1 } },
  ], X0, 1.95, 11.4, 0.9, { fontSize: 40, fontFace: MONO, bold: true, valign: "middle" });
  // notes in fixed columns, joined by slanted pointers to the part they explain
  const notes5 = [
    [X0 + 1 * CW, 0.9, "how long", "a duration, always with a unit: 500ms, 1s, 2min"],
    [X0 + 5.5 * CW, 3.9, "what", "a generator: dc, ramp, sine, pulses, ou, ..."],
    [X0 + 17 * CW, 6.9, "how", "parameters by name (or by position); units are checked"],
  ];
  for (const [cx, col, h, d] of notes5) {
    const tx = col + 0.3;
    const geo = tx >= cx ? { x: cx, w: tx - cx } : { x: tx, w: cx - tx, flipH: true };
    s.addShape(pres.shapes.LINE, Object.assign(geo, { y: 3.2, h: 0.5,
      line: { color: C.accent2, width: 2 }, objectName: "pointer " + h }));
    text(s, h, col, 3.8, 2.7, 0.45, { fontSize: 20, bold: true, color: C.accent2 });
    text(s, d, col, 4.3, 2.6, 1.0, { fontSize: 15 });
  }
  card(s, 9.9, 3.6, 2.8, 2.9, ["s ms us min", "Hz kHz", "rad deg  %", "pA nA mV nS"], { label: "units", size: 15, name: "units" });
  text(s, [
    { text: "Rules of thumb: ", options: { bold: true } },
    { text: "# starts a comment · ; separates segments on one line · a wrong unit (freq=10ms) is an error, never a guess" },
  ], 0.6, 5.7, 8.6, 0.8, { fontSize: 15 });
  s.addNotes("Every line has three parts. First how long, always with a unit. Then what: the name of a " +
    "generator. Then the parameters in parentheses, by name or by position. Units are part of the " +
    "language: writing a time where a frequency is expected is an error, not a silent mistake.");

  // 6. Generators -------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Writing stimuli" });
  s.addText("A toolbox of 16 generators", { placeholder: "title" });
  figure(s, "gallery", 0.6, 1.45, 7.9, 5.3, "Every generator of StimGen 2 rendered as a 400 ms segment");
  const groups = [["levels", "dc  ramp  relax"], ["waves", "sine  square  saw\ntriangle  chirp"],
    ["synaptic, pulses", "biexp  alpha  pulses"], ["noise", "ou  wnoise  unoise  cnoise"], ["recorded", "file"]];
  groups.forEach(([g, n], i) => {
    text(s, g, 8.9, 1.5 + i * 0.95, 3.8, 0.35, { fontSize: 15, bold: true, color: C.accent1 });
    text(s, n, 8.9, 1.83 + i * 0.95, 3.8, 0.6, { fontSize: 14, fontFace: MONO });
  });
  card(s, 8.9, 6.2, 3.8, 0.6, ["$ sg help sine"], { size: 14, name: "help" });
  s.addNotes("Sixteen generators cover almost every protocol: levels and ramps, periodic waves and " +
    "frequency sweeps, synaptic-like waveforms and pulse trains, four kinds of noise, and samples read " +
    "from a file. sg help followed by a name lists the parameters, their units and defaults.");

  // 7. Noise ----------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Writing stimuli" });
  s.addText("Noise you can repeat", { placeholder: "title" });
  figure(s, "noise", 0.6, 1.45, 7.2, 5.3, "Three different noise realisations, and two identical ones with seed=17");
  const rows = [
    [fa.FaDice, "new noise every time", "10s ou(100, 50, 5ms)"],
    [fa.FaRedo, "the same run again", "sg render -s 42 ..."],
    [fa.FaLock, "frozen noise, always", "10s ou(100, 50, 5ms, seed=17)"],
  ];
  for (let i = 0; i < 3; i++) {
    const y = 1.6 + i * 1.45;
    await badge(s, rows[i][0], 8.1, y, 0.8, HEX.accent1, HEX.lt1, "noise " + (i + 1));
    text(s, rows[i][1], 9.1, y, 3.6, 0.4, { fontSize: 17, bold: true });
    text(s, rows[i][2], 9.1, y + 0.42, 3.6, 0.7, { fontSize: 13, fontFace: MONO, color: C.accent1 });
  }
  text(s, "The seed actually used is always saved in the output file.", 8.1, 6.0, 4.6, 0.7,
    { fontSize: 15, italic: true, color: C.accent2 });
  s.addNotes("Without a seed, each playing gives new noise with the same statistics. With -s on the command " +
    "line you can reproduce a run. With seed= inside the text the noise is frozen: identical wherever " +
    "you use it, which is what you need to measure spike-time reliability.");

  // 8. Combining --------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Writing stimuli" });
  s.addText("Combine generators like arithmetic", { placeholder: "title" });
  card(s, 0.6, 1.6, 5.6, 3.6, [
    "5s sine(50, 8Hz) + ou(0, 20, 5ms)",
    "5s (1 + 0.5*sine(1, 2Hz)) * sine(80, 40Hz)",
    "10s pos(ou(10, 5, 3ms))   # never < 0",
    "repeat 10 {2ms dc(1000); 48ms dc(0)}",
    "{1s ramp(0,1); 3s dc(1); 1s ramp(1,0)}",
    "    * sine(80, 8Hz)       # envelope",
  ], { label: "examples", size: 14, name: "combine" });
  text(s, [
    { text: "+ − × ÷", options: { bold: true, fontFace: MONO } },
    { text: " act sample by sample.  " },
    { text: "{ … }", options: { bold: true, fontFace: MONO } },
    { text: " groups segments into a block you can multiply." },
  ], 0.6, 5.45, 5.6, 1.1, { fontSize: 15 });
  figure(s, "envelope", 6.5, 1.5, 6.2, 5.2, "A trapezoidal block, a sine carrier, and their product");
  s.addNotes("Generators can be added and multiplied like numbers, sample by sample. Functions such as pos " +
    "or abs transform them. repeat repeats part of a waveform, and braces make a block: here a " +
    "trapezoid multiplies a sine to make an oscillation that fades in and out.");

  // 9. Channels ---------------------------------------------------------------
  pres.addSection({ title: "Experiments" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Experiments" });
  s.addText("Several outputs at once", { placeholder: "title" });
  card(s, 0.6, 1.6, 6.0, 4.2, [
    "sg 2 stimulus",
    "channel pre unit=pA {",
    "    1s     dc(0)",
    "    @train                 # a marker",
    "    500ms  pulses(2000, 20Hz,",
    "                  width=1ms)",
    "    1s     dc(0)",
    "}",
    "channel post unit=pA { 2.5s dc(-50) }",
  ], { label: "pair.sg", size: 15, name: "pair" });
  figure(s, "pair", 6.9, 1.6, 5.8, 3.6, "Two channels on a common time base with the marker pre.train");
  text(s, [
    { text: "Channels have names, not wires. ", options: { bold: true } },
    { text: "Which board output each name uses is set once, in the rig file of sgplay. " },
    { text: "@train", options: { fontFace: MONO, bold: true } },
    { text: " saves a marker for the analysis." },
  ], 6.9, 5.4, 5.8, 1.3, { fontSize: 15 });
  s.addNotes("A stimulus file lists several channels on one time base, for example a presynaptic and a " +
    "postsynaptic cell. Channels have names; the mapping to physical outputs lives in the rig file of the " +
    "acquisition program, so the same file runs on any set-up. A label such as @train becomes a marker " +
    "stored with the data.");

  // 10. Protocols --------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Experiments" });
  s.addText("Families of trials: protocols", { placeholder: "title" });
  card(s, 0.6, 1.6, 5.4, 4.6, [
    "sg 2 protocol",
    "sweep  amp = from -300pA",
    "             to 300pA step 50pA",
    "repeat 3",
    "order  shuffled-blocks",
    "period 5s",
    "stimulus { channel Iinj unit=pA {",
    "  500ms dc(0); 1s dc($amp)",
    "  500ms dc(0) } }",
  ], { label: "steps.sg", size: 15, name: "protocol" });
  figure(s, "steps", 6.3, 1.6, 6.4, 3.4, "Thirteen current steps and the shuffled order of 39 trials");
  text(s, [
    { text: "One file, 39 trials: ", options: { bold: true } },
    { text: "13 amplitudes × 3 repetitions, in a random but recorded order, one every 5 s. " },
    { text: "$amp", options: { fontFace: MONO, bold: true } },
    { text: " is replaced by each value." },
  ], 6.3, 5.25, 6.4, 1.2, { fontSize: 15 });
  s.addNotes("A protocol describes a whole series of trials. sweep defines a variable and its values, " +
    "repeat the number of repetitions, order how they are shuffled, and period the time between trials. " +
    "In the stimulus, $amp is replaced by each value. sg expand writes all 39 trials as files if you " +
    "want to inspect them one by one.");

  // 11. Check, look, render -----------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Experiments" });
  s.addText("Before the cell: check, look, render", { placeholder: "title" });
  const cl = [
    [fa.FaCheck, "Check", ["$ sg check steps.sg", "steps.sg: ok, 39 trial(s)"], "Syntax, units, durations, every trial."],
    [fa.FaEye, "Look", ["$ sg render -r 20kHz step.sg", "$ python3 tools/sgplot.py \\", "    step.sgb"], "See the waveform before a cell sees it."],
    [fa.FaSave, "Keep", ["$ sg expand steps.sg steps", "steps/0000.sg ... 0038.sg"], "Every trial as a plain file, if you want."],
  ];
  for (let i = 0; i < 3; i++) {
    const x = 0.6 + i * 4.1;
    await badge(s, cl[i][0], x, 1.6, 0.8, HEX.accent2, HEX.lt1, cl[i][1]);
    text(s, cl[i][1], x + 1.0, 1.75, 2.8, 0.5, { fontSize: 22, bold: true, fontFace: THEME.headFontFace });
    card(s, x, 2.7, 3.8, 1.5, cl[i][2], { size: 13, name: cl[i][1] });
    text(s, cl[i][3], x, 4.45, 3.8, 0.8, { fontSize: 16 });
  }
  card(s, 0.6, 5.9, 12.1, 0.75, ["$ sg examples        # 40 ready-to-run commands, from a first step to protocols"],
    { size: 15, name: "examples" });
  s.addNotes("Three habits save experiments. Check every file with sg check: it reports the line of any " +
    "mistake. Look at the waveform with sgplot before a cell sees it. And if you like, expand a protocol " +
    "into plain files. sg examples prints forty commands you can paste into a terminal.");

  // 12. sgplay ------------------------------------------------------------------
  pres.addSection({ title: "On the rig" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "On the rig" });
  s.addText("Playing it on the NI board with sgplay", { placeholder: "title" });
  card(s, 0.6, 1.6, 6.6, 3.3, [
    "$ sgplay step.sg              # play once",
    "$ sgplay -n 10 -i 5s step.sg  # 10 times",
    "$ sgplay steps.sg             # a protocol",
    "$ sgplay steps/               # its directory",
    "$ sgplay --dry-run pair.sg    # show the map",
    "$ sgplay --rig rig2.cfg pair.sg",
  ], { label: "terminal (proposed interface)", size: 15, name: "sgplay" });
  const play = [
    [fa.FaFileAlt, "reads the .sg text, renders it with sg's rules at the board's rate"],
    [fa.FaProjectDiagram, "maps channel names to board lines with the rig file"],
    [fa.FaTachometerAlt, "converts to volts with the amplifier gains; refuses clipping"],
    [fa.FaWaveSquare, "streams AO, records AI, sends triggers on DO"],
    [fa.FaDatabase, "saves the recording together with the stimulus record"],
  ];
  for (let i = 0; i < play.length; i++) {
    const y = 1.6 + i * 0.85;
    await badge(s, play[i][0], 7.6, y, 0.6, HEX.accent1, HEX.lt1, "sgplay step " + (i + 1));
    text(s, play[i][1], 8.4, y + 0.05, 4.3, 0.6, { fontSize: 15, valign: "middle" });
  }
  text(s, "sgplay is the acquisition program assumed by this tutorial: it receives the same text files, " +
    "and is the only part that knows the National Instruments board.", 0.6, 6.0, 12.1, 0.7,
    { fontSize: 13, italic: true, color: C.accent6 });
  s.addNotes("This is how a session would look at the rig. sgplay receives the same text files; it renders " +
    "them with exactly the rules of sg at the sampling rate of the board, looks up which board output each " +
    "channel uses, converts the numbers to volts with the amplifier gains, refuses anything that would " +
    "clip, plays and records, and stores the recording with the full stimulus record. The interface " +
    "shown is a proposal for that program.");

  // 13. Rig file ----------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "On the rig" });
  s.addText("The rig file connects names to wires", { placeholder: "title" });
  const hdr = (t) => ({ text: t, options: { bold: true, color: C.background1, fill: { color: C.accent1 } } });
  const rowsT = [
    [hdr("channel in the .sg"), hdr("NI board line"), hdr("gain"), hdr("meaning")],
    ["Iinj", "Dev1/ao0", "400 pA/V", "current command"],
    ["Vcmd", "Dev1/ao1", "20 mV/V", "voltage command"],
    ["cam (digital)", "Dev1/port0/line0", "—", "camera trigger"],
    ["Vm (recorded)", "Dev1/ai0", "100 mV/V", "membrane potential"],
  ];
  s.addTable(rowsT, { x: 0.6, y: 1.65, w: 7.6, colW: [2.0, 2.3, 1.3, 2.0], fontSize: 14,
    fontFace: THEME.bodyFontFace, color: C.text1, border: { type: "solid", pt: 0.75, color: HEX.lt2 },
    fill: { color: C.background1 }, rowH: 0.55, valign: "middle", objectName: "rig table" });
  card(s, 8.6, 1.65, 4.1, 3.0, [
    "board  Dev1",
    "rate   20kHz",
    "Iinj   ao0  400pA/V",
    "Vcmd   ao1  20mV/V",
    "cam    port0/line0",
    "Vm     ai0  100mV/V",
  ], { label: "rig.cfg (proposed)", size: 14, name: "rig" });
  await badge(s, fa.FaExchangeAlt, 0.6, 5.0, 0.8, HEX.accent2, HEX.lt1, "portable");
  text(s, [
    { text: "Same stimulus, any rig. ", options: { bold: true } },
    { text: "Move to another set-up and change only the rig file: the .sg files, and the stimuli they describe, stay identical." },
  ], 1.6, 5.0, 11.1, 0.9, { fontSize: 16 });
  s.addNotes("The rig file is the only place where wires appear. It names the board, the sampling rate, " +
    "and for each channel name the board line and the gain of the amplifier. Change rigs, change this " +
    "file; the stimulus files stay as they are. The format of the rig file is a proposal.");

  // 14. Provenance --------------------------------------------------------------
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "On the rig" });
  s.addText("Nothing gets lost", { placeholder: "title" });
  const kept = [
    [fa.FaFileAlt, "the stimulus text", "every line you wrote"],
    [fa.FaDice, "the seed", "of every noise segment"],
    [fa.FaTachometerAlt, "the sampling rate", "and the program version"],
    [fa.FaFingerprint, "a digest", "to prove the samples are intact"],
  ];
  for (let i = 0; i < 4; i++) {
    const x = 0.6 + (i % 2) * 3.4, y = 1.7 + Math.floor(i / 2) * 2.2;
    s.addShape(pres.shapes.ROUNDED_RECTANGLE, { x, y, w: 3.1, h: 1.9, rectRadius: 0.12,
      fill: { color: C.background2 }, line: { color: C.background2 }, objectName: "kept " + (i + 1) });
    await badge(s, kept[i][0], x + 0.25, y + 0.3, 0.7, HEX.accent1, HEX.lt1, "kept " + (i + 1));
    text(s, kept[i][1], x + 1.1, y + 0.3, 1.9, 0.7, { fontSize: 17, bold: true });
    text(s, kept[i][2], x + 0.25, y + 1.15, 2.7, 0.6, { fontSize: 14, color: C.accent6 });
  }
  card(s, 7.6, 1.7, 5.1, 2.4, [
    "# regenerate any trial,",
    "# years later:",
    "$ sg render -r 20kHz -s SEED \\",
    "    trial.sg",
  ], { label: "from the saved record", size: 15, name: "regenerate" });
  text(s, "Every .sgb file, and every recording made by sgplay, carries this record in its header.",
    7.6, 4.4, 5.1, 1.2, { fontSize: 16 });
  s.addNotes("Each output file carries a record of how it was made: the full text, the seed, the rate, the " +
    "program version, and a digest of the samples. From that record alone, any trial can be regenerated " +
    "exactly, which is what makes old recordings re-analysable.");

  // 15. Cheat sheet -------------------------------------------------------------
  pres.addSection({ title: "Wrap-up" });
  s = pres.addSlide({ masterName: "CONTENT", sectionTitle: "Wrap-up" });
  s.addText("Cheat sheet", { placeholder: "title" });
  const cmds = [
    [hdr("I want to …"), hdr("type")],
    ["make a step", "500ms dc(0); 1s dc(300); 500ms dc(0)"],
    ["check a file", "sg check file.sg"],
    ["get the samples", "sg render -r 20kHz file.sg"],
    ["see them", "python3 tools/sgplot.py file.sgb"],
    ["repeat the same noise", "ou(..., seed=17)   or   sg render -s 42"],
    ["play it on the board", "sgplay file.sg"],
    ["know a generator", "sg help pulses"],
    ["see 40 examples", "sg examples"],
  ];
  const mono = cmds.map((r, i) => i === 0 ? r : [r[0], { text: r[1], options: { fontFace: MONO, color: C.accent1 } }]);
  s.addTable(mono, { x: 0.6, y: 1.55, w: 12.1, colW: [3.6, 8.5], fontSize: 15, fontFace: THEME.bodyFontFace,
    color: C.text1, border: { type: "solid", pt: 0.75, color: HEX.lt2 }, rowH: 0.52, valign: "middle",
    objectName: "cheat sheet" });
  s.addNotes("Everything in one table. Keep it next to the rig.");

  // 16. Closing -----------------------------------------------------------------
  s = pres.addSlide({ masterName: "CLOSING", sectionTitle: "Wrap-up" });
  s.addText("Start with one line", { placeholder: "title" });
  card(s, 0.7, 1.9, 6.2, 1.2, ["1s dc(300)"], { size: 36, name: "one line" });
  const next = [
    ["sg help", "all topics; sg help NAME for a generator"],
    ["sg examples", "forty commands to paste and run"],
    ["docs/build/stimgen2-spec.pdf", "the tutorial and the full reference"],
    ["docs/build/sgb-access.pdf", "reading .sgb files from C, Python, Julia"],
  ];
  next.forEach(([a, b], i) => {
    text(s, a, 7.4, 1.9 + i * 0.95, 5.3, 0.4, { fontSize: 17, fontFace: MONO, color: C.accent4, bold: true });
    text(s, b, 7.4, 2.3 + i * 0.95, 5.3, 0.4, { fontSize: 14, color: C.background1 });
  });
  text(s, "StimGen 2 and sg · Michele Giugliano · MIT License", 0.7, 6.6, 8, 0.4,
    { fontSize: 13, color: C.accent6 });
  s.addNotes("Start with a single line, check it, look at it, then build up. The built-in help and the " +
    "examples cover the rest; the specification explains every detail.");

  await pres.writeFile({ fileName: OUT });
  const skill = process.env.SKILL_DIR;
  if (skill) {
    const { applyTheme } = require(path.join(skill, "scripts", "apply_theme.js"));
    await applyTheme(OUT, THEME);
  }
  console.log("wrote", OUT);
}

build().catch((e) => { console.error(e); process.exit(1); });
