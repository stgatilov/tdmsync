#ifndef _TDM_POLYHASH_H_665168_
#define _TDM_POLYHASH_H_665168_

#include <stdint.h>

#ifdef _MSC_VER
#define INLINE __inline
#else
#define INLINE inline
#endif


#ifdef __cplusplus
extern "C" {
#endif

static const uint32_t POLYHASH_MODULO = 0x7FFFFFFF;
static const uint32_t POLYHASH_BASE = 103945823;
extern uint32_t POLYHASH_NEGATOR;

uint32_t polyhash_compute(const uint8_t *data, size_t len);

static INLINE uint32_t polyhash_fast_update(uint32_t value, uint8_t added, uint8_t removed) {
  value = (((uint64_t)value) * POLYHASH_BASE + added + ((uint64_t)removed) * POLYHASH_NEGATOR) % POLYHASH_MODULO;
  return value;
}

#ifdef __cplusplus
}
#endif

#endif
