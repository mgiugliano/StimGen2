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

"""
refstream.py -- the random streams of the StimGen 2 specification (Sec. 11),
implemented independently of sg: SHA-256 from hashlib, Philox4x64-10 from
numpy, and the variate formulas of the specification.
"""
import hashlib, math
import numpy as np


class Stream:
    def __init__(self, keystring):
        d = hashlib.sha256(keystring.encode()).digest()
        k0 = int.from_bytes(d[0:8], "big")
        k1 = int.from_bytes(d[8:16], "big")
        self.key = k0 | (k1 << 64)          # numpy packs key words low first
        self.block, self.buf, self.cache = 0, [], None

    def word(self):
        if not self.buf:
            # numpy increments the counter before use: start one below
            g = np.random.Philox(counter=(self.block - 1) % (1 << 256), key=self.key)
            self.buf = [int(w) for w in g.random_raw(4)]
            self.block += 1
        return self.buf.pop(0)

    def uniform(self):
        return (self.word() >> 11) * 2.0 ** -53

    def gauss(self):
        if self.cache is not None:
            g, self.cache = self.cache, None
            return g
        u1, u2 = self.uniform(), self.uniform()
        r = math.sqrt(-2.0 * math.log(1.0 - u1))
        self.cache = r * math.sin(2 * math.pi * u2)
        return r * math.cos(2 * math.pi * u2)

    def expo(self):
        return -math.log(1.0 - self.uniform())
