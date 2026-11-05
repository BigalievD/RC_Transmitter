#ifndef SD_FILE_MANAGER_H
#define SD_FILE_MANAGER_H

#include "lvgl.h"
#include <string>

// Типы элементов
enum SdItemType {
    SD_ITEM_PARENT_DIR, // ".."
    SD_ITEM_FOLDER,     // Папка
    SD_ITEM_FILE        // Файл
};

// Данные каждого элемента для привязки к UI
struct SdFileItemData {
    std::string name;      // Имя
    std::string full_path; // Полный путь
    SdItemType type;       // Тип
    uint32_t size;         // Размер в байтах
};

// Функция инициализации и открывания пути
void sd_fm_init();
void sd_fm_open_dir(const char * path = "/");

#endif // SD_FILE_MANAGER_H