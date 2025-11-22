#include "kernel/types.h"
#include "user/user.h"
#include "sha256.h"

/* rotate right  */
static inline unsigned int rotr(unsigned int x, int n) {
  return (x >> n) | (x << (32 - n));
}

/* SHA-256 helper functions */
#define CH(x,y,z)  ((x & y) ^ (~x & z))
#define MAJ(x,y,z) ((x & y) ^ (x & z) ^ (y & z))
#define BSIG0(x) (rotr((x),2) ^ rotr((x),13) ^ rotr((x),22))
#define BSIG1(x) (rotr((x),6) ^ rotr((x),11) ^ rotr((x),25))
#define SSIG0(x) (rotr((x),7) ^ rotr((x),18) ^ ((x) >> 3))
#define SSIG1(x) (rotr((x),17) ^ rotr((x),19) ^ ((x) >> 10))

/* SHA-256 round constants */
static const unsigned int K[64] = {
  0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
  0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
  0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
  0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
  0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
  0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
  0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
  0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u
};

/* Helper: convert 4 bytes at p (big-endian) to uint32 */
static unsigned int be32(const unsigned char *p) {
  return ((unsigned int)p[0] << 24) |
         ((unsigned int)p[1] << 16) |
         ((unsigned int)p[2] <<  8) |
         ((unsigned int)p[3]      );
}

/* Helper: write uint32 to 4 bytes (big-endian) at p */
static void write_be32(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)((v >> 24) & 0xff);
  p[1] = (unsigned char)((v >> 16) & 0xff);
  p[2] = (unsigned char)((v >> 8) & 0xff);
  p[3] = (unsigned char)(v & 0xff);
}

/* Main SHA-256 function */
void sha256_hash(const unsigned char *data, unsigned int len, unsigned char hash[32]) {
  /* initial hash values (first 32 bits of the fractional parts of the square roots of the first 8 primes) */
  unsigned int H[8] = {
    0x6a09e667u,
    0xbb67ae85u,
    0x3c6ef372u,
    0xa54ff53au,
    0x510e527fu,
    0x9b05688cu,
    0x1f83d9abu,
    0x5be0cd19u
  };

  unsigned int bitlen_hi = 0;        /* high 32 bits of message length in bits */
  unsigned int bitlen_lo = 0;        /* low 32 bits of message length in bits */

  /* compute 64-bit bit-length = len * 8 */
  unsigned long long total_bits = ((unsigned long long)len) * 8ULL;
  bitlen_hi = (unsigned int)((total_bits >> 32) & 0xFFFFFFFFULL);
  bitlen_lo = (unsigned int)(total_bits & 0xFFFFFFFFULL);

  unsigned int i;
  /* process each 64-byte block */
  unsigned int idx = 0;
  while (len >= 64) {
    unsigned int W[64];
    /* prepare message schedule */
    for (i = 0; i < 16; ++i) {
      W[i] = be32(data + idx + 4*i);
    }
    for (i = 16; i < 64; ++i) {
      W[i] = SSIG1(W[i-2]) + W[i-7] + SSIG0(W[i-15]) + W[i-16];
    }
    /* initialize working variables */
    unsigned int a = H[0];
    unsigned int b = H[1];
    unsigned int c = H[2];
    unsigned int d = H[3];
    unsigned int e = H[4];
    unsigned int f = H[5];
    unsigned int g = H[6];
    unsigned int h = H[7];

    for (i = 0; i < 64; ++i) {
      unsigned int T1 = h + BSIG1(e) + CH(e,f,g) + K[i] + W[i];
      unsigned int T2 = BSIG0(a) + MAJ(a,b,c);
      h = g;
      g = f;
      f = e;
      e = d + T1;
      d = c;
      c = b;
      b = a;
      a = T1 + T2;
    }
    H[0] += a;
    H[1] += b;
    H[2] += c;
    H[3] += d;
    H[4] += e;
    H[5] += f;
    H[6] += g;
    H[7] += h;

    idx += 64;
    len -= 64;
  }

  /* Now handle final block(s) with padding.
     We need to append 0x80, zeros, and then 64-bit big-endian bit-length.
     That will require 1 or 2 blocks.
  */

  unsigned char block[128]; /* up to two blocks (128 bytes) */
  unsigned int rem = len;   /* remaining bytes (<64) */
  /* copy remaining bytes into block */
  for (i = 0; i < rem; ++i) {
    block[i] = data[idx + i];
  }
  /* append 0x80 */
  block[rem] = 0x80;
  /* zeros after that */
  unsigned int pad_zero_start = rem + 1;
  /* Determine how many padding zeros and whether we need two blocks */
  /* When we append 64-bit length, it should occupy the last 8 bytes of the final 64-byte block.
     So if rem + 1 + 8 > 64 we need two blocks.
  */
  if (pad_zero_start + 8 > 64) {
    /* need two blocks */
    unsigned int z;
    for (z = pad_zero_start; z < 128 - 8; ++z) block[z] = 0;
    /* write 64-bit big-endian length into last 8 bytes of block */
    /* first high 32 bits then low 32 bits */
    write_be32(block + 128 - 8, bitlen_hi);
    write_be32(block + 128 - 4, bitlen_lo);

    /* process first final block (first 64) */
    {
      unsigned int W[64];
      unsigned int j;
      for (j = 0; j < 16; ++j) {
        W[j] = be32(block + 4*j);
      }
      for (j = 16; j < 64; ++j) W[j] = SSIG1(W[j-2]) + W[j-7] + SSIG0(W[j-15]) + W[j-16];

      unsigned int a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (j = 0; j < 64; ++j) {
        unsigned int T1 = h + BSIG1(e) + CH(e,f,g) + K[j] + W[j];
        unsigned int T2 = BSIG0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
      }
      H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }

    /* process second final block (bytes 64..127) */
    {
      unsigned int W[64];
      unsigned int j;
      for (j = 0; j < 16; ++j) {
        W[j] = be32(block + 64 + 4*j);
      }
      for (j = 16; j < 64; ++j) W[j] = SSIG1(W[j-2]) + W[j-7] + SSIG0(W[j-15]) + W[j-16];

      unsigned int a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (j = 0; j < 64; ++j) {
        unsigned int T1 = h + BSIG1(e) + CH(e,f,g) + K[j] + W[j];
        unsigned int T2 = BSIG0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
      }
      H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }
  } else {
    /* fits in one block */
    unsigned int z;
    for (z = pad_zero_start; z < 64 - 8; ++z) block[z] = 0;
    /* write length at last 8 bytes */
    write_be32(block + 64 - 8, bitlen_hi);
    write_be32(block + 64 - 4, bitlen_lo);

    /* process single final block */
    {
      unsigned int W[64];
      unsigned int j;
      for (j = 0; j < 16; ++j) {
        W[j] = be32(block + 4*j);
      }
      for (j = 16; j < 64; ++j) W[j] = SSIG1(W[j-2]) + W[j-7] + SSIG0(W[j-15]) + W[j-16];

      unsigned int a = H[0], b = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
      for (j = 0; j < 64; ++j) {
        unsigned int T1 = h + BSIG1(e) + CH(e,f,g) + K[j] + W[j];
        unsigned int T2 = BSIG0(a) + MAJ(a,b,c);
        h = g; g = f; f = e; e = d + T1; d = c; c = b; b = a; a = T1 + T2;
      }
      H[0] += a; H[1] += b; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
    }
  }

  /* produce final digest (big-endian) */
  for (i = 0; i < 8; ++i) {
    write_be32(hash + 4*i, H[i]);
  }
}

/* ---------------------- Utilities for test output ---------------------- */

/* print 32-byte hash as hex with padding for xv6 printf */
static void print_hex(const unsigned char *h) {
  int i;
  for (i = 0; i < 32; i++) {
    int hi = (h[i] >> 4) & 0xF;
    int lo = h[i] & 0xF;

    // convert to hex character
    char c[2];
    c[0] = (hi < 10) ? ('0' + hi) : ('a' + hi - 10);
    c[1] = (lo < 10) ? ('0' + lo) : ('a' + lo - 10);
    printf("%c%c", c[0], c[1]);
  }
}

/* compare two 32-byte arrays */
static int cmp32(const unsigned char *a, const unsigned char *b) {
  int i;
  for (i = 0; i < 32; ++i) if (a[i] != b[i]) return 0;
  return 1;
}

/* hex string to bytes (expects lowercase hex, 64 hex chars) */
static void hexstr_to_bytes(const char *hex, unsigned char out[32]) {
  int i;
  for (i = 0; i < 32; ++i) {
    char hi = hex[2*i];
    char lo = hex[2*i + 1];
    unsigned int vhi = (hi >= 'a') ? (hi - 'a' + 10) : (hi >= 'A' ? (hi - 'A' + 10) : (hi - '0'));
    unsigned int vlo = (lo >= 'a') ? (lo - 'a' + 10) : (lo >= 'A' ? (lo - 'A' + 10) : (lo - '0'));
    out[i] = (unsigned char)((vhi << 4) | vlo);
  }
}

/* ---------------------- Tests in main ---------------------- */

int main(int argc, char *argv[]) {
  /* test vectors and their expected hashes (computed using a standard SHA-256) */
  struct {
    const char *s;
    const char *expected_hex;
  } tests[] = {
    { "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855" },
    { "a", "ca978112ca1bbdcafac231b39a23dc4da786eff8147c4e72b9807785afee48bb" },
    { "hello world", "b94d27b9934d3e08a52e52d7da7dabfac484efe37a5380ee9088f7ace2efcde9" },
    /* EXACT string provided in prompt (note the space between 'P' and 'Q') */
    { "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP QRSTUVWXYZ0123456",
      "1b4ac209f013956efb1854f976f69a3809873f8d10e549ef3dafecf0a73a9d10" },
    { "The quick brown fox jumps over the lazy dog. This is a longer test string that spans multiple blocks.",
      "65dab9c0a2772f0ea4654aabc5cb63c83a6ee018249ef5d104bed2ad7141a9e1" }
  };

  int ntests = sizeof(tests) / sizeof(tests[0]);
  int ti;
  for (ti = 0; ti < ntests; ++ti) {
    const char *s = tests[ti].s;
    const char *exp_hex = tests[ti].expected_hex;
    unsigned char got[32];
    unsigned char expect[32];
    sha256_hash((const unsigned char*)s, (unsigned int)strlen(s), got);
    hexstr_to_bytes(exp_hex, expect);

    printf("Test %d: \"%s\"\n", ti+1, s);
    printf("  Expected: ");
    print_hex(expect);
    printf("\n  Computed: ");
    print_hex(got);
    printf("\n  Result: %s\n\n", cmp32(got, expect) ? "PASS" : "FAIL");
  }

  exit(1);
  return 0;
}