// StimGen 2 -- SPDX-License-Identifier: MIT
// Copyright (c) 2026 Michele Giugliano. See LICENSE for the full text.
//
// fix_ids.js -- make shape ids unique on every slide of a .pptx.
//
// pptxgenjs gives the slide-number placeholder the fixed id 25, so on a slide
// with more than about 23 objects two shapes share an id. PowerPoint then
// reports the file as damaged and offers to repair it. This renumbers every
// repeated id (after the first) to a fresh one above the slide's maximum.
//
//   const { fixIds } = require("./fix_ids.js"); await fixIds("deck.pptx");
//   node fix_ids.js deck.pptx

const fs = require("fs");
const JSZip = require("jszip");

async function fixIds(file) {
  const zip = await JSZip.loadAsync(fs.readFileSync(file));
  let fixed = 0;
  for (const name of Object.keys(zip.files)) {
    if (!/^ppt\/slides\/slide\d+\.xml$/.test(name)) continue;
    const xml = await zip.file(name).async("string");
    const ids = [...xml.matchAll(/<p:cNvPr id="(\d+)"/g)].map((m) => +m[1]);
    let next = Math.max(0, ...ids) + 1;
    const seen = new Set();
    const out = xml.replace(/<p:cNvPr id="(\d+)"/g, (m, id) => {
      if (!seen.has(id)) { seen.add(id); return m; }
      fixed++;
      return `<p:cNvPr id="${next++}"`;
    });
    if (out !== xml) zip.file(name, out);
  }
  if (fixed) fs.writeFileSync(file, await zip.generateAsync({ type: "nodebuffer", compression: "DEFLATE" }));
  return fixed;
}

module.exports = { fixIds };

if (require.main === module) {
  fixIds(process.argv[2]).then((n) => console.log(`fix_ids: renumbered ${n} shape id(s)`));
}
