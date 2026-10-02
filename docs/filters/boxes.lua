-- StimGen 2 -- SPDX-License-Identifier: MIT
--
-- Copyright (c) 2026 Michele Giugliano
--
-- Permission is hereby granted, free of charge, to any person obtaining a copy
-- of this software and associated documentation files (the "Software"), to deal
-- in the Software without restriction, including without limitation the rights
-- to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
-- copies of the Software, and to permit persons to whom the Software is
-- furnished to do so, subject to the following conditions:
--
-- The above copyright notice and this permission notice shall be included in all
-- copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
-- IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
-- FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
-- AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
-- LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
-- OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
-- SOFTWARE.

-- Turn fenced divs with classes .todo, .note, .rationale
-- into tcolorbox environments (LaTeX) defined in 00-metadata.yaml.
-- Other output formats keep the div untouched.

local envs = {
  todo = "todobox",
  note = "notebox",
  rationale = "rationalebox",
}

function Div(el)
  if not FORMAT:match("latex") then
    return nil
  end
  for cls, env in pairs(envs) do
    if el.classes:includes(cls) then
      local blocks = { pandoc.RawBlock("latex", "\\begin{" .. env .. "}") }
      for _, b in ipairs(el.content) do
        table.insert(blocks, b)
      end
      table.insert(blocks, pandoc.RawBlock("latex", "\\end{" .. env .. "}"))
      return blocks
    end
  end
  return nil
end
