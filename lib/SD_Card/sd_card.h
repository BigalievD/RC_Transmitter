#ifndef SD_CARD_H
#define SD_CARD_H
#include <Arduino.h>
#include <FS.h>
bool sd_card_init();
bool sd_card_is_ready();
bool sd_card_read(File &file,void *dst,size_t size);
bool sd_card_write(File &file,const void *src,size_t size);
void sd_card_release_dma_reserve();
void sd_card_restore_dma_reserve();
#endif
