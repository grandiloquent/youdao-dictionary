#ifndef _taos_md5_header_
#define _taos_md5_header_

#include <stdint.h>

typedef struct {
  uint32_t i[2];       /* number of _bits_ handled mod 2^64 */
  uint32_t buf[4];     /* scratch buffer */
  uint8_t  in[64];     /* input buffer */
  uint8_t  digest[16]; /* actual digest after MD5Final call */
} MD5_CTX;

void MD5Init(MD5_CTX *mdContext);
void MD5Update(MD5_CTX *mdContext, uint8_t *inBuf, unsigned int inLen);
void MD5Final(MD5_CTX *mdContext);

#endif