#include "sd_card.h"
#include <SD_MMC.h>
#include <esp_heap_caps.h>
#include "lvgl.h"
#include "driver/sdmmc_host.h"
static bool sd_ready=false;
static constexpr size_t SD_IO_CHUNK=512;
static uint8_t *sd_io_buffer=nullptr;
bool sd_card_read(File &file,void *dst,size_t size){
    if(!file||!dst||!sd_io_buffer) return false;
    uint8_t *out=(uint8_t *)dst;
    size_t done=0;
    while(done<size){
        size_t chunk=(size-done)>SD_IO_CHUNK?SD_IO_CHUNK:(size-done);
        size_t count=file.read(sd_io_buffer,chunk);
        if(count!=chunk) return false;
        memcpy(out+done,sd_io_buffer,chunk);
        done+=chunk;
    }
    return true;
}
bool sd_card_write(File &file,const void *src,size_t size){
    if(!file||!src||!sd_io_buffer) return false;
    const uint8_t *in=(const uint8_t *)src;
    size_t done=0;
    while(done<size){
        size_t chunk=(size-done)>SD_IO_CHUNK?SD_IO_CHUNK:(size-done);
        memcpy(sd_io_buffer,in+done,chunk);
        size_t count=file.write(sd_io_buffer,chunk);
        if(count!=chunk) return false;
        done+=chunk;
    }
    return true;
}
static void *sd_fs_open(lv_fs_drv_t *drv,const char *path,lv_fs_mode_t mode){
    (void)drv;
    const char *flags=FILE_READ;
    if(mode==LV_FS_MODE_WR) flags=FILE_WRITE;
    else if(mode==(LV_FS_MODE_WR|LV_FS_MODE_RD)) flags=FILE_WRITE;
    File f=SD_MMC.open(path,flags);
    if(!f) return NULL;
    return (void *)new File(f);
}
static lv_fs_res_t sd_fs_close(lv_fs_drv_t *drv,void *file_p){
    (void)drv;
    File *f=(File *)file_p;
    if(f){
        f->close();
        delete f;
    }
    return LV_FS_RES_OK;
}
static lv_fs_res_t sd_fs_read(lv_fs_drv_t *drv,void *file_p,void *buf,uint32_t btr,uint32_t *br){
    (void)drv;
    File *f=(File *)file_p;
    if(!f||!*f||!br||!sd_io_buffer) return LV_FS_RES_UNKNOWN;
    if(btr==0){
        *br=0;
        return LV_FS_RES_OK;
    }
    uint8_t *out=(uint8_t *)buf;
    uint32_t done=0;
    while(done<btr){
        uint32_t chunk=(btr-done)>SD_IO_CHUNK?SD_IO_CHUNK:(btr-done);
        size_t count=f->read(sd_io_buffer,chunk);
        if(count==0) break;
        memcpy(out+done,sd_io_buffer,count);
        done+=(uint32_t)count;
        if(count<chunk) break;
    }
    *br=done;
    return LV_FS_RES_OK;
}
static lv_fs_res_t sd_fs_seek(lv_fs_drv_t *drv,void *file_p,uint32_t pos,lv_fs_whence_t whence){
    (void)drv;
    File *f=(File *)file_p;
    if(!f||!*f) return LV_FS_RES_UNKNOWN;
    SeekMode mode=SeekSet;
    if(whence==LV_FS_SEEK_CUR) mode=SeekCur;
    else if(whence==LV_FS_SEEK_END) mode=SeekEnd;
    return f->seek(pos,mode)?LV_FS_RES_OK:LV_FS_RES_UNKNOWN;
}
static lv_fs_res_t sd_fs_tell(lv_fs_drv_t *drv,void *file_p,uint32_t *pos_p){
    (void)drv;
    File *f=(File *)file_p;
    if(!f||!*f||!pos_p) return LV_FS_RES_UNKNOWN;
    *pos_p=f->position();
    return LV_FS_RES_OK;
}
static void *sd_fs_dir_open(lv_fs_drv_t *drv,const char *path){
    (void)drv;
    const char *target=(path==nullptr||path[0]=='\0')?"/":path;
    File dir=SD_MMC.open(target);
    if(!dir||!dir.isDirectory()) return NULL;
    return (void *)new File(dir);
}
static lv_fs_res_t sd_fs_dir_read(lv_fs_drv_t *drv,void *dir_p,char *fn,uint32_t fn_len){
    (void)drv;
    File *dir=(File *)dir_p;
    if(!dir||!*dir||!fn||fn_len==0) return LV_FS_RES_UNKNOWN;
    File entry=dir->openNextFile();
    if(!entry){
        fn[0]='\0';
        return LV_FS_RES_OK;
    }
    if(entry.isDirectory()) snprintf(fn,fn_len,"/%s",entry.name());
    else snprintf(fn,fn_len,"%s",entry.name());
    entry.close();
    return LV_FS_RES_OK;
}
static lv_fs_res_t sd_fs_dir_close(lv_fs_drv_t *drv,void *dir_p){
    (void)drv;
    File *dir=(File *)dir_p;
    if(dir){
        dir->close();
        delete dir;
    }
    return LV_FS_RES_OK;
}
bool sd_card_init(){
    sd_ready=false;
    SD_MMC.setPins(43,44,39,40,41,42);
    if(!SD_MMC.begin("/sdcard",false,false,BOARD_MAX_SDMMC_FREQ,2)) return false;
    if(!sd_io_buffer){
        sd_io_buffer=(uint8_t *)heap_caps_malloc(SD_IO_CHUNK,MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA|MALLOC_CAP_8BIT);
        if(!sd_io_buffer){
            Serial.println("SD ERROR: DMA buffer allocation failed");
            SD_MMC.end();
            return false;
        }
    }
    static lv_fs_drv_t fs_drv;
    lv_fs_drv_init(&fs_drv);
    fs_drv.letter='S';
    fs_drv.open_cb=sd_fs_open;
    fs_drv.close_cb=sd_fs_close;
    fs_drv.read_cb=sd_fs_read;
    fs_drv.seek_cb=sd_fs_seek;
    fs_drv.tell_cb=sd_fs_tell;
    fs_drv.dir_open_cb=sd_fs_dir_open;
    fs_drv.dir_read_cb=sd_fs_dir_read;
    fs_drv.dir_close_cb=sd_fs_dir_close;
    lv_fs_drv_register(&fs_drv);
    sd_ready=true;
    Serial.printf("SD DMA buffer: %u bytes, free DMA: %u, largest DMA: %u\n",(unsigned)SD_IO_CHUNK,(unsigned)heap_caps_get_free_size(MALLOC_CAP_DMA),(unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
    return true;
}
bool sd_card_is_ready(){
    return sd_ready;
}
void sd_card_release_dma_reserve(){
}
void sd_card_restore_dma_reserve(){
}
