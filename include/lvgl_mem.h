#pragma once
#include <stddef.h>
#include "lvgl.h"
#ifdef __cplusplus
extern "C" {
#endif
void lv_mem_init(void);
void lv_mem_deinit(void);
lv_mem_pool_t lv_mem_add_pool(void *mem,size_t bytes);
void lv_mem_remove_pool(lv_mem_pool_t pool);
void *lv_malloc_core(size_t size);
void *lv_realloc_core(void *ptr,size_t new_size);
void lv_free_core(void *ptr);
void lv_mem_monitor_core(lv_mem_monitor_t *mon);
lv_result_t lv_mem_test_core(void);
#ifdef __cplusplus
}
#endif