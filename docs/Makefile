# StimGen 2 -- SPDX-License-Identifier: MIT
#
# Copyright (c) 2026 Michele Giugliano
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# Build the StimGen 2 specification with Pandoc.
#   make pdf    -> build/stimgen2-spec.pdf
#   make html   -> build/stimgen2-spec.html
#   make tex    -> build/stimgen2-spec.tex (for debugging LaTeX errors)

SRC_DIR   := src
BUILD_DIR := build
NAME      := stimgen2-spec

# Figures are rendered by sg (../src/sg) from the texts in
# figures/make_figures.py; "make figures" regenerates them.

# Chapter order is the lexical order of the file names.
META    := $(SRC_DIR)/00-metadata.yaml
CHAPTERS := $(sort $(wildcard $(SRC_DIR)/[0-9][0-9]-*.md)) \
            $(sort $(wildcard $(SRC_DIR)/[A-Z]-*.md))

PANDOC_OPTS := --from=markdown+tex_math_dollars+raw_tex+fenced_divs \
               --filter pandoc-crossref \
               --lua-filter filters/boxes.lua \
               --citeproc \
               --resource-path=.:figures

.PHONY: all pdf html tex clean figures access

all: pdf access

# Companion note: reading .sgb files from C, Python and Julia.
access: $(BUILD_DIR)/sgb-access.pdf

$(BUILD_DIR)/sgb-access.pdf: access/sgb-access.md | $(BUILD_DIR)
	pandoc --from=markdown --filter pandoc-crossref --pdf-engine=xelatex -o $@ access/sgb-access.md

pdf: $(BUILD_DIR)/$(NAME).pdf
html: $(BUILD_DIR)/$(NAME).html
tex: $(BUILD_DIR)/$(NAME).tex

$(BUILD_DIR):
	mkdir -p $@

figures: figures/.stamp

# One run of the script writes all figures; the stamp file records it.
figures/.stamp: figures/make_figures.py ../src/sg ../tools/sgplot.py
	python3 figures/make_figures.py
	touch $@

../src/sg:
	$(MAKE) -C ../src

$(BUILD_DIR)/$(NAME).pdf: $(META) $(CHAPTERS) figures/.stamp references.bib filters/boxes.lua | $(BUILD_DIR)
	pandoc $(PANDOC_OPTS) --pdf-engine=xelatex -o $@ $(META) $(CHAPTERS)

$(BUILD_DIR)/$(NAME).tex: $(META) $(CHAPTERS) references.bib filters/boxes.lua | $(BUILD_DIR)
	pandoc $(PANDOC_OPTS) --standalone -o $@ $(META) $(CHAPTERS)

$(BUILD_DIR)/$(NAME).html: $(META) $(CHAPTERS) references.bib | $(BUILD_DIR)
	pandoc $(PANDOC_OPTS) --standalone --mathjax --toc -o $@ $(META) $(CHAPTERS)

clean:
	rm -rf $(BUILD_DIR)/*
