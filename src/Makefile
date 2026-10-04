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

LIB = util.o help.o render.o rng.o parse.o prims.o eval.o protocol.o sgb.o
OBJ = main.o $(LIB)

sg: $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(OBJ) sg_api.o: sg.h

# Native driver of the library interface, for tests/test_api.py.
api_main: $(LIB) sg_api.o ../tests/api_main.c
	$(CC) $(CFLAGS) -o $@ ../tests/api_main.c $(LIB) sg_api.o $(LDLIBS)

test: sg api_main
	./sg selftest
	python3 ../tests/run_tests.py ./sg
	python3 ../tests/test_thorough.py ./sg
	python3 ../tests/test_access.py ./sg
	python3 ../tests/test_examples.py ./sg
	python3 ../tests/test_api.py ./sg ./api_main

# WebAssembly build for the web app (needs Emscripten's emcc; the result,
# web/sg.js and web/sg.wasm, is committed, so the web app needs no compiler).
WASM_EXPORTS = _sg_api_render,_sg_api_render_trial,_sg_api_check,_sg_api_canon,_sg_api_kind,_sg_api_expand,_sg_api_help,_sg_api_prims,_sg_api_error,_sg_api_text,_sg_api_warnings,_sg_api_result,_sg_api_result_size,_sg_api_version,_malloc,_free
LIBSRC = util.c help.c render.c rng.c parse.c prims.c eval.c protocol.c sgb.c sg_api.c

wasm: ../web/sg.js

../web/sg.js: $(LIBSRC) sg.h
	emcc -O2 -std=c99 -Wall -Wextra -ffp-contract=off $(LIBSRC) -o ../web/sg.js \
	  -sMODULARIZE=1 -sEXPORT_ES6=1 -sEXPORT_NAME=createSG -sENVIRONMENT=web,worker,node \
	  -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sFORCE_FILESYSTEM=1 \
	  -sEXPORTED_FUNCTIONS=$(WASM_EXPORTS) \
	  -sEXPORTED_RUNTIME_METHODS=UTF8ToString,stringToNewUTF8,FS,HEAPU8

# The WebAssembly build against the native command line (needs node), and
# the web planner in headless Chrome (needs Google Chrome; CHROME=path to override).
test-wasm: sg
	python3 ../tests/test_api.py --level-b ./sg node ../web/node_driver.mjs
	node ../tests/test_web.mjs ./sg

clean:
	rm -f sg api_main $(OBJ) sg_api.o

.PHONY: test test-wasm wasm clean
