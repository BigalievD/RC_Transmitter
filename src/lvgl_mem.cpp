#include "lvgl_mem.h"
#include <esp_heap_caps.h>
#include "lvgl.h"

extern "C" void lv_mem_init(void){
}

extern "C" void lv_mem_deinit(void){
}

extern "C" lv_mem_pool_t lv_mem_add_pool(void *mem,size_t bytes){
    LV_UNUSED(mem);
    LV_UNUSED(bytes);
    return nullptr;
}

extern "C" void lv_mem_remove_pool(lv_mem_pool_t pool){
    LV_UNUSED(pool);
}

extern "C" void *lv_malloc_core(size_t size){
    if(size==0) return nullptr;
    return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}

extern "C" void *lv_realloc_core(void *ptr,size_t new_size){
    if(new_size==0){
        if(ptr) heap_caps_free(ptr);
        return nullptr;
    }
    return heap_caps_realloc(ptr,new_size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
}

extern "C" void lv_free_core(void *ptr){
    if(ptr) heap_caps_free(ptr);
}

extern "C" void lv_mem_monitor_core(lv_mem_monitor_t *mon){
    if(!mon) return;
    mon->total_size=heap_caps_get_total_size(MALLOC_CAP_SPIRAM);
    mon->free_size=heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    mon->free_biggest_size=heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);
    mon->used_pct=mon->total_size?100-(uint64_t)100*mon->free_size/mon->total_size:0;
    mon->frag_pct=mon->free_size?100-(uint64_t)100*mon->free_biggest_size/mon->free_size:0;
}

extern "C" lv_result_t lv_mem_test_core(void){
    return LV_RESULT_OK;
}