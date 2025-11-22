#ifndef _SHA256_H_
#define _SHA256_H_

/*
 * Minimal public API for the xv6 user-level SHA-256 implementation.
 *
 * The function below computes the SHA-256 digest for `len` bytes at `data`
 * and writes the 32-byte digest into `hash`.
 *
 * types: use plain unsigned char and unsigned int to match xv6 user space types.
 */
void sha256_hash(const unsigned char *data, unsigned int len, unsigned char hash[32]);

#endif /* _SHA256_H_ */