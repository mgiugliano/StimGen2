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

# StimGen 2 -- top-level Makefile.
#   make           build the renderer src/sg
#   make test      run all tests (needs python3 with numpy)
#   make figures   regenerate the figures of the document (needs matplotlib)
#   make docs      build docs/build/stimgen2-spec.pdf and sgb-access.pdf
#   make clean

all:
	$(MAKE) -C src

test: all
	$(MAKE) -C src test

figures: all
	$(MAKE) -C docs figures

docs: all
	$(MAKE) -C docs pdf access

clean:
	$(MAKE) -C src clean
	$(MAKE) -C docs clean

.PHONY: all test figures docs clean
