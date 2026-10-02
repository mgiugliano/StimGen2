/*
 * StimGen 2 -- SPDX-License-Identifier: MIT
 *
 * Copyright (c) 2026 Michele Giugliano
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

/*
 * rng.c -- random numbers for StimGen 2 (spec, Sec. 11).
 *
 * Everything here is self-contained, so that the samples do not depend on
 * the C library of the platform:
 *
 *   1. SHA-256 (FIPS 180-4), used to turn a text string into a 128-bit key.
 *   2. Philox4x64-10 (Salmon et al., SC'11), a counter-based generator: block
 *      j of a stream is computed directly from (key, j), without stepping
 *      through blocks 0..j-1.  Streams with different keys are independent.
 *   3. The transformations from 64-bit words to uniform, Gaussian and
 *      exponential variates, fixed by the specification (spec Sec. 11.4).
 *
 * Why not the classic generators (e.g. "Ran" of Numerical Recipes, 3rd ed.)?
 * They are excellent sequential generators, but every segment of a waveform
 * would then have to be generated in a fixed global order, and changing one
 * segment would change the noise of all later ones.  With a counter-based
 * generator each noise segment owns a stream, keyed by its position in the
 * description (or by an explicit seed), and is reproducible on its own.
 *
 * Known-answer tests for both algorithms are run by "sg selftest".
 */
#include <math.h>
#include <string.h>
#include <time.h>
#include "sg.h"

/* ================================================================== */
/* 1. SHA-256                                                          */
/* ================================================================== */

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2 };

/* Process one 64-byte block into the state h[8]. */
static void sha256_block(uint32_t h[8], const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, k, t1, t2;
    int i;
    for (i = 0; i < 16; i++)                       /* big-endian words */
        w[i] = (uint32_t)p[4*i] << 24 | (uint32_t)p[4*i+1] << 16 |
               (uint32_t)p[4*i+2] << 8 | (uint32_t)p[4*i+3];
    for (i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3);
        uint32_t s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = h[0]; b = h[1]; c = h[2]; d = h[3];
    e = h[4]; f = h[5]; g = h[6]; k = h[7];
    for (i = 0; i < 64; i++) {
        t1 = k + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g))
               + K256[i] + w[i];
        t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & c) ^ (b & c));
        k = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += k;
}

void sha256(const void *data, size_t len, uint8_t out[32])
{
    uint32_t h[8] = { 0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                      0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19 };
    const uint8_t *p = (const uint8_t *)data;
    uint8_t tail[128];
    size_t i, rest = len % 64, ntail;
    uint64_t bits = (uint64_t)len * 8;

    for (i = 0; i + 64 <= len; i += 64)            /* whole blocks      */
        sha256_block(h, p + i);
    /* Padding: the bytes left, 0x80, zeros, and the length in bits.     */
    memset(tail, 0, sizeof tail);
    memcpy(tail, p + len - rest, rest);
    tail[rest] = 0x80;
    ntail = (rest < 56) ? 64 : 128;
    for (i = 0; i < 8; i++)
        tail[ntail - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (i = 0; i < ntail; i += 64)
        sha256_block(h, tail + i);
    for (i = 0; i < 8; i++) {                      /* big-endian output */
        out[4*i]   = (uint8_t)(h[i] >> 24); out[4*i+1] = (uint8_t)(h[i] >> 16);
        out[4*i+2] = (uint8_t)(h[i] >> 8);  out[4*i+3] = (uint8_t)h[i];
    }
}

void sha256_hex(const void *data, size_t len, char hex[65])
{
    uint8_t d[32];
    int i;
    sha256(data, len, d);
    for (i = 0; i < 32; i++)
        sprintf(hex + 2 * i, "%02x", d[i]);
}

/* ================================================================== */
/* 2. Philox4x64-10                                                    */
/* ================================================================== */

/* Full 128-bit product of two 64-bit numbers, from 32-bit halves, so that
 * no compiler extension (such as __int128) is needed.                   */
void mulhilo64(uint64_t a, uint64_t b, uint64_t *hi, uint64_t *lo)
{
    uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
    uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
    uint64_t p00 = a0 * b0, p01 = a0 * b1, p10 = a1 * b0, p11 = a1 * b1;
    uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
    *lo = (mid << 32) | (p00 & 0xffffffffu);
    *hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
}

/* One block: 4 words of output from a 256-bit counter and a 128-bit key.
 * Ten rounds of multiply-and-xor; the key is bumped by two Weyl constants
 * between rounds (Salmon et al. 2011, Random123 reference code).          */
void philox4x64_10(const uint64_t ctr[4], const uint64_t key[2], uint64_t out[4])
{
    const uint64_t M0 = 0xD2E7470EE14C6C93u, M1 = 0xCA5A826395121157u;
    const uint64_t W0 = 0x9E3779B97F4A7C15u, W1 = 0xBB67AE8584CAA73Bu;
    uint64_t c0 = ctr[0], c1 = ctr[1], c2 = ctr[2], c3 = ctr[3];
    uint64_t k0 = key[0], k1 = key[1], hi0, lo0, hi1, lo1;
    int r;
    for (r = 0; r < 10; r++) {
        if (r > 0) { k0 += W0; k1 += W1; }
        mulhilo64(M0, c0, &hi0, &lo0);
        mulhilo64(M1, c2, &hi1, &lo1);
        c0 = hi1 ^ c1 ^ k0;  c1 = lo1;
        c2 = hi0 ^ c3 ^ k1;  c3 = lo0;
    }
    out[0] = c0; out[1] = c1; out[2] = c2; out[3] = c3;
}

/* ================================================================== */
/* 3. Streams and variates                                             */
/* ================================================================== */

/* A stream is keyed by the first 128 bits of SHA-256(keystring), read as
 * two big-endian 64-bit words (spec Rule 6).  The keystring is
 * "fixed:<seed>" for an explicit seed, else "<master seed>:<address>".   */
void stream_init(Stream *s, const char *keystring)
{
    uint8_t d[32];
    int i;
    sha256(keystring, strlen(keystring), d);
    s->key[0] = s->key[1] = 0;
    for (i = 0; i < 8; i++) {
        s->key[0] = (s->key[0] << 8) | d[i];
        s->key[1] = (s->key[1] << 8) | d[8 + i];
    }
    s->block = 0;
    s->nbuf = 0;
    s->has_gauss = 0;
}

/* Next word: block j is Philox(counter = (j,0,0,0)); its 4 words are used
 * in order (spec Rule 5).                                               */
uint64_t stream_word(Stream *s)
{
    if (s->nbuf == 0) {
        uint64_t ctr[4] = { 0, 0, 0, 0 };
        ctr[0] = s->block++;
        philox4x64_10(ctr, s->key, s->buf);
        s->nbuf = 4;
    }
    return s->buf[4 - s->nbuf--];
}

/* U = floor(w / 2^11) * 2^-53: exact, in [0,1), with 53 random bits. */
double stream_uniform(Stream *s)
{
    return (double)(stream_word(s) >> 11) * (1.0 / 9007199254740992.0);
}

/* sin and cos of the same angle, evaluated as two separate library calls.
 * Optimising compilers may otherwise merge them into one sincos() call,
 * whose last bit can differ, so that the samples would depend on the
 * optimisation level.  Reading the angle through a volatile prevents it.  */
void sin_cos(double x, double *s, double *c)
{
    volatile double v = x;
    *s = sin(v);
    *c = cos(v);
}

/* Box-Muller from two consecutive uniforms; the cosine branch is returned
 * first, the sine branch is kept for the next call.  1-U lies in (0,1],
 * so the logarithm is always finite.                                     */
double stream_gauss(Stream *s)
{
    const double TWO_PI = 6.283185307179586476925286766559;
    double u1, u2, r;
    if (s->has_gauss) { s->has_gauss = 0; return s->gauss; }
    u1 = stream_uniform(s);
    u2 = stream_uniform(s);
    double sn, cs;
    r = sqrt(-2.0 * log(1.0 - u1));
    sin_cos(TWO_PI * u2, &sn, &cs);
    s->gauss = r * sn;
    s->has_gauss = 1;
    return r * cs;
}

double stream_exponential(Stream *s)
{
    return -log(1.0 - stream_uniform(s));
}

/* A master seed from the operating system, used only when none is given.
 * The seed actually used is always written to the provenance record.     */
uint64_t entropy_seed(void)
{
    uint64_t seed = 0;
    FILE *f = fopen("/dev/urandom", "rb");
    if (f) {
        uint8_t b[8];
        if (fread(b, 1, 8, f) == 8) {
            int i;
            for (i = 0; i < 8; i++) seed = (seed << 8) | b[i];
        }
        fclose(f);
    }
    if (seed == 0)                      /* no /dev/urandom: time and clock */
        seed = (uint64_t)time(NULL) * 6364136223846793005u + (uint64_t)clock();
    return seed;
}
