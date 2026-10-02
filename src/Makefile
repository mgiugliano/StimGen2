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

# sg -- StimGen 2 reference renderer.
#   make          build ./sg
#   make test     self test, reference tests and thorough tests (../tests)
#   make clean

CC     ?= cc
CFLAGS ?= -std=c99 -pedantic -Wall -Wextra -O2
# -ffp-contract=off: no fused multiply-add, so that results do not depend on
# whether the compiler chooses to fuse operations (reproducibility).
CFLAGS += -ffp-contract=off
LDLIBS  = -lm

OBJ = main.o rng.o parse.o prims.o eval.o protocol.o sgb.o

sg: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(OBJ): sg.h

test: sg
	./sg selftest
	python3 ../tests/run_tests.py ./sg
	python3 ../tests/test_thorough.py ./sg
	python3 ../tests/test_access.py ./sg
	python3 ../tests/test_examples.py ./sg

clean:
	rm -f sg $(OBJ)

.PHONY: test clean
