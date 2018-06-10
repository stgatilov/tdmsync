#ifndef _TDM_BINARY_SEARCH_203876_H_
#define _TDM_BINARY_SEARCH_203876_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct tdm_bsb_info {
    intptr_t minus_one;
    intptr_t step_first;
    intptr_t step_second;
};

void binary_search_branchless_precompute(struct tdm_bsb_info *info, uint32_t num);
uint32_t binary_search_branchless_run (const struct tdm_bsb_info *info, const uint32_t *arr, uint32_t key);

#ifdef __cplusplus
}
#endif

#endif
