/*
  BLAKE2 reference source code package - reference C implementations
  Copyright 2012, Samuel Neves <sneves@dei.uc.pt>.  You may use this under the
  terms of the CC0, the OpenSSL Licence, or the Apache Public License 2.0, at
  your option.  The terms of these licenses can be found at:
  - CC0 1.0 Universal : http://creativecommons.org/publicdomain/zero/1.0
  - OpenSSL license   : https://www.openssl.org/source/license.html
  - Apache 2.0        : http://www.apache.org/licenses/LICENSE-2.0

  More information about the BLAKE2 hash function can be found at
  https://blake2.net.

  Modified for hash-wasm by Dani Biró
*/

#include <emscripten.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BLAKE2_PACKED(x) x __attribute__((packed))

uint8_t array[16 * 1024];

EMSCRIPTEN_KEEPALIVE
uint8_t *Hash_GetBuffer() {
  return array;
}

enum blake2s_constant {
  BLAKE2S_BLOCKBYTES = 64,
  BLAKE2S_OUTBYTES = 32,
  BLAKE2S_KEYBYTES = 32,
  BLAKE2S_SALTBYTES = 8,
  BLAKE2S_PERSONALBYTES = 8
};

typedef struct blake2s_state__ {
  uint32_t h[8];
  uint32_t t[2];
  uint32_t f[2];
  uint8_t buf[BLAKE2S_BLOCKBYTES];
  size_t buflen;
  size_t outlen;
  uint8_t last_node;
} blake2s_state;

blake2s_state S[1];

BLAKE2_PACKED(struct blake2s_param__ {
  uint8_t digest_length;                   /* 1 */
  uint8_t key_length;                      /* 2 */
  uint8_t fanout;                          /* 3 */
  uint8_t depth;                           /* 4 */
  uint32_t leaf_length;                    /* 8 */
  uint32_t node_offset;                    /* 12 */
  uint16_t xof_length;                     /* 14 */
  uint8_t node_depth;                      /* 15 */
  uint8_t inner_length;                    /* 16 */
  uint8_t salt[BLAKE2S_SALTBYTES];         /* 24 */
  uint8_t personal[BLAKE2S_PERSONALBYTES]; /* 32 */
});

typedef struct blake2s_param__ blake2s_param;

blake2s_param P[1];

static __inline__ uint32_t load32(const void *src) {
  return *(uint32_t *)src;
}

static __inline__ void store32(void *dst, uint32_t w) {
  *(uint32_t *)dst = w;
}

static __inline__ uint64_t rotr32(const uint32_t w, const unsigned c) {
  return (w >> c) | (w << (32 - c));
}

static const uint32_t blake2s_IV[8] = {
  0x6A09E667UL, 0xBB67AE85UL, 0x3C6EF372UL, 0xA54FF53AUL,
  0x510E527FUL, 0x9B05688CUL, 0x1F83D9ABUL, 0x5BE0CD19UL
};

static const uint8_t blake2s_sigma[10][16] = {
  {  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15 } ,
  { 14, 10,  4,  8,  9, 15, 13,  6,  1, 12,  0,  2, 11,  7,  5,  3 } ,
  { 11,  8, 12,  0,  5,  2, 15, 13, 10, 14,  3,  6,  7,  1,  9,  4 } ,
  {  7,  9,  3,  1, 13, 12, 11, 14,  2,  6,  5, 10,  4,  0, 15,  8 } ,
  {  9,  0,  5,  7,  2,  4, 10, 15, 14,  1, 11, 12,  6,  8,  3, 13 } ,
  {  2, 12,  6, 10,  0, 11,  8,  3,  4, 13,  7,  5, 15, 14,  1,  9 } ,
  { 12,  5,  1, 15, 14, 13,  4, 10,  0,  7,  6,  3,  9,  2,  8, 11 } ,
  { 13, 11,  7, 14, 12,  1,  3,  9,  5,  0, 15,  4,  8,  6,  2, 10 } ,
  {  6, 15, 14,  9, 11,  3,  0,  8, 12,  2, 13,  7,  1,  4, 10,  5 } ,
  { 10,  2,  8,  4,  7,  6,  1,  5, 15, 11,  9, 14,  3, 12, 13 , 0 }
};

static __inline__ void blake2s_set_lastnode() { S->f[1] = (uint32_t)-1; }

/* Some helper functions, not necessarily useful */
static __inline__ int blake2s_is_lastblock() { return S->f[0] != 0; }

static __inline__ void blake2s_set_lastblock() {
  if (S->last_node) blake2s_set_lastnode();

  S->f[0] = (uint32_t)-1;
}

static __inline__ void blake2s_increment_counter(const uint32_t inc) {
  S->t[0] += inc;
  S->t[1] += (S->t[0] < inc);
}

#define G(r, i, a, b, c, d)                     \
  do {                                          \
    a = a + b + m[blake2s_sigma[r][2 * i + 0]]; \
    d = rotr32(d ^ a, 16);                      \
    c = c + d;                                  \
    b = rotr32(b ^ c, 12);                      \
    a = a + b + m[blake2s_sigma[r][2 * i + 1]]; \
    d = rotr32(d ^ a, 8);                       \
    c = c + d;                                  \
    b = rotr32(b ^ c, 7);                       \
  } while (0)

#define ROUND(r)                       \
  do {                                 \
    G(r, 0, v[0], v[4], v[8], v[12]);  \
    G(r, 1, v[1], v[5], v[9], v[13]);  \
    G(r, 2, v[2], v[6], v[10], v[14]); \
    G(r, 3, v[3], v[7], v[11], v[15]); \
    G(r, 4, v[0], v[5], v[10], v[15]); \
    G(r, 5, v[1], v[6], v[11], v[12]); \
    G(r, 6, v[2], v[7], v[8], v[13]);  \
    G(r, 7, v[3], v[4], v[9], v[14]);  \
  } while (0)

static void blake2s_compress(const uint8_t block[BLAKE2S_BLOCKBYTES]) {
  uint32_t m[16];
  uint32_t v[16];

  for (size_t i = 0; i < 16; ++i) {
    m[i] = load32(block + i * sizeof(m[i]));
  }

  for (size_t i = 0; i < 8; ++i) {
    v[i] = S->h[i];
  }

  v[8] = blake2s_IV[0];
  v[9] = blake2s_IV[1];
  v[10] = blake2s_IV[2];
  v[11] = blake2s_IV[3];
  v[12] = blake2s_IV[4] ^ S->t[0];
  v[13] = blake2s_IV[5] ^ S->t[1];
  v[14] = blake2s_IV[6] ^ S->f[0];
  v[15] = blake2s_IV[7] ^ S->f[1];

  ROUND(0);
  ROUND(1);
  ROUND(2);
  ROUND(3);
  ROUND(4);
  ROUND(5);
  ROUND(6);
  ROUND(7);
  ROUND(8);
  ROUND(9);

  for (size_t i = 0; i < 8; ++i) {
    S->h[i] = S->h[i] ^ v[i] ^ v[i + 8];
  }
}

#undef G
#undef ROUND

void blake2s_update(const void *pin, size_t inlen) {
  const unsigned char *in = (const unsigned char *)pin;
  if (inlen > 0) {
    size_t left = S->buflen;
    size_t fill = BLAKE2S_BLOCKBYTES - left;
    if (inlen > fill) {
      S->buflen = 0;
      /* Fill buffer */
      for (uint8_t i = 0; i < fill; i++) {
        S->buf[left + i] = in[i];
      }
      blake2s_increment_counter(BLAKE2S_BLOCKBYTES);
      blake2s_compress(S->buf); /* Compress */
      in += fill;
      inlen -= fill;
      while (inlen > BLAKE2S_BLOCKBYTES) {
        blake2s_increment_counter(BLAKE2S_BLOCKBYTES);
        blake2s_compress(in);
        in += BLAKE2S_BLOCKBYTES;
        inlen -= BLAKE2S_BLOCKBYTES;
      }
    }
    for (uint8_t i = 0; i < inlen; i++) {
      S->buf[S->buflen + i] = in[i];
    }
    S->buflen += inlen;
  }
}

EMSCRIPTEN_KEEPALIVE
void Hash_Final() {
  size_t outlen = S->outlen;
  uint8_t buffer[BLAKE2S_OUTBYTES] = {0};

  if (blake2s_is_lastblock()) {
    return;
  }

  blake2s_increment_counter(S->buflen);
  blake2s_set_lastblock();
  memset(S->buf + S->buflen, 0, BLAKE2S_BLOCKBYTES - S->buflen); /* Padding */
  blake2s_compress(S->buf);

  for (size_t i = 0; i < 8; ++i) {
    /* Output full hash to temp buffer */
    store32(buffer + sizeof(S->h[i]) * i, S->h[i]);
  }

  for (uint8_t i = 0; i < S->outlen; i++) {
    array[i] = buffer[i];
  }
}

static void blake2s_init0() {
  size_t i;
  memset(S, 0, sizeof(blake2s_state));

  for (i = 0; i < 8; ++i) {
    S->h[i] = blake2s_IV[i];
  }
}

/* init xors IV with input parameter block */
void blake2s_init_param() {
  const uint8_t *p = (const uint8_t *)(P);
  size_t i;

  blake2s_init0();

  /* IV XOR ParamBlock */
  for (i = 0; i < 8; ++i) {
    S->h[i] ^= load32(p + sizeof(S->h[i]) * i);
  }

  S->outlen = P->digest_length;
}

void blake2s_init_key(size_t outlen, const uint8_t *key, size_t keylen) {
  P->digest_length = (uint8_t)outlen;
  P->key_length = (uint8_t)keylen;
  P->fanout = 1;
  P->depth = 1;
  // P->leaf_length = 0;
  // P->node_offset = 0;
  // P->xof_length = 0;
  // P->node_depth = 0;
  // P->inner_length = 0;
  // memset(P->reserved, 0, sizeof(P->reserved));
  // memset(P->salt,     0, sizeof(P->salt));
  // memset(P->personal, 0, sizeof(P->personal));

  blake2s_init_param();

  if (keylen > 0) {
    uint8_t block[BLAKE2S_BLOCKBYTES];
    memset(block, 0, BLAKE2S_BLOCKBYTES);
    for (uint8_t i = 0; i < keylen; i++) {
      block[i] = key[i];
    }
    blake2s_update(block, BLAKE2S_BLOCKBYTES);
  }
}

EMSCRIPTEN_KEEPALIVE
void Hash_Init(uint32_t bits) {
  size_t outlen = bits & 0xFFFF;
  size_t keylen = bits >> 16;
  blake2s_init_key(outlen / 8, array, keylen);
}

EMSCRIPTEN_KEEPALIVE
void Hash_Update(uint32_t size) {
  blake2s_update(array, size);
}

EMSCRIPTEN_KEEPALIVE
void Hash_Calculate(uint32_t length, uint32_t initParam) {
  Hash_Init(initParam);
  Hash_Update(length);
  Hash_Final();
}