#ifndef _TDM_BUZHASH_H_237623_
#define _TDM_BUZHASH_H_237623_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const uint32_t buzhash_table[256];

//note: len must be divisible by 32!
uint32_t buzhash_compute(const uint8_t *data, size_t len);

static inline uint32_t buzhash_fast_update(uint32_t value, uint8_t added, uint8_t removed) {
  uint32_t shift = (value << 1) ^ (value >> 31);
  uint32_t update = buzhash_table[removed] ^ buzhash_table[added];
  return shift ^ update;
}

#ifdef __cplusplus
}
#endif

#endif
