#include "polyhash.h"

uint32_t POLYHASH_NEGATOR;

uint32_t polyhash_compute(const uint8_t *data, size_t len) {
    uint32_t res = 0;
    uint32_t pw = 1;
    for (size_t i = 0; i < len; i++) {
        res = (((uint64_t)res) * POLYHASH_BASE + data[i]) % POLYHASH_MODULO;
        pw = (((uint64_t)pw) * POLYHASH_BASE) % POLYHASH_MODULO;
    }
    pw = POLYHASH_MODULO - pw;
    if (pw == POLYHASH_MODULO) pw = 0;
    POLYHASH_NEGATOR = pw;
    return res;
}
