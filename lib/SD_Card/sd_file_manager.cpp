#include "ui/styles.h"
#include "ui/ui.h"
#include "sd_file_manager.h"
#include "sd_card.h"
#include <FS.h>
#include <SD_MMC.h>

static std::string current_path = "/";
static lv_obj_t * selected_item = nullptr; // Храним ссылку на выбранный контейнер

struct PathUpdateData {
    std::string path;
};

// Асинхронная отложенная загрузка папки (защита от краша при клике)
struct OpenDirData {
    std::string path;
};

static void show_file_preview(const char * path) {
    if (!objects.sd_card_viewer_panel) return;
    
    // Безопасная очистка правой панели
    lv_obj_clean(objects.sd_card_viewer_panel);
    
    std::string p = path;
    bool is_text = (p.rfind(".txt") != std::string::npos || 
                    p.rfind(".log") != std::string::npos || 
                    p.rfind(".ini") != std::string::npos || 
                    p.rfind(".json") != std::string::npos ||
                    p.rfind(".csv") != std::string::npos);
                    
    if (!is_text) {
        lv_obj_t * lbl = lv_label_create(objects.sd_card_viewer_panel);
        lv_label_set_text_fmt(lbl, "Файл: %s\n\n(Бинарный формат)", path);
        return;
    }

    File f = SD_MMC.open(path, FILE_READ);
    if (!f || f.isDirectory()) {
        if (f) f.close();
        lv_obj_t * lbl = lv_label_create(objects.sd_card_viewer_panel);
        lv_label_set_text(lbl, "Ошибка чтения файла");
        return;
    }

    char buffer[2049];
    size_t bytes_read = f.readBytes(buffer, 2048);
    buffer[bytes_read] = '\0';
    f.close();

    lv_obj_t * ta = lv_textarea_create(objects.sd_card_viewer_panel);
    lv_obj_set_size(ta, lv_pct(100), lv_pct(100));
    lv_textarea_set_text(ta, buffer);
    lv_obj_clear_flag(ta, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_set_style_bg_opa(ta, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ta, 0, 0);
}

static void open_dir_async_cb(void * param) {
    auto * data = static_cast<OpenDirData *>(param);
    if (data) {
        sd_fm_open_dir(data->path.c_str());
        //lv_label_set_text(objects.disk_content_lbl, data->path .c_str());
        delete data;
    }
}

static void on_item_clicked(lv_event_t * e) {
    lv_obj_t * target_obj = static_cast<lv_obj_t *>(lv_event_get_target(e));
    auto * data = static_cast<SdFileItemData *>(lv_event_get_user_data(e));
    if (!data || !target_obj) return;

    // 1. ПЕРВЫЙ КЛИК (Выделение + Предпросмотр)
    if (selected_item != target_obj) {
        if (selected_item && lv_obj_is_valid(selected_item)) {
            lv_obj_clear_state(selected_item, LV_STATE_CHECKED);
        }
        selected_item = target_obj;
        lv_obj_add_state(selected_item, LV_STATE_CHECKED);

        if (data->type == SD_ITEM_FILE) {
            show_file_preview(data->full_path.c_str()); 
        } else {
            if (objects.sd_card_viewer_panel) {
                lv_obj_clean(objects.sd_card_viewer_panel);
            }
        }
        return;
    }

    // 2. ВТОРОЙ КЛИК (Действие) — вызываем через lv_async_call для безопасного перестроения UI!
    switch (data->type) {
        case SD_ITEM_PARENT_DIR: {
            selected_item = nullptr;
            std::string new_path = current_path;
            size_t last_slash = new_path.find_last_of('/');
            if (last_slash != std::string::npos && last_slash > 0) {
                new_path = new_path.substr(0, last_slash);
            } else {
                new_path = "/";
            }
            auto * async_data = new OpenDirData{ new_path };
            lv_async_call(open_dir_async_cb, async_data);
            Serial.println("SD_ITEM_PARENT_DIR");
            break;
        }
        case SD_ITEM_FOLDER: {
            selected_item = nullptr;
            auto * async_data = new OpenDirData{ data->full_path };
            lv_async_call(open_dir_async_cb, async_data);
            Serial.printf("Подтвержден выбор папки: %s\n", data->full_path.c_str());
            break;
        }
        case SD_ITEM_FILE: {
            Serial.printf("Подтвержден выбор файла: %s (%u байт)\n", data->full_path.c_str(), data->size);
            break;
        }
    }
}

void sd_fm_init() {
    if (objects.sd_card_fm_panel) {
        lv_obj_set_scroll_dir(objects.sd_card_fm_panel, LV_DIR_VER);
    }
    sd_fm_open_dir("/");
}

void sd_fm_open_dir(const char * path) {
    if (!objects.sd_card_fm_panel || !sd_card_is_ready()) return;

    selected_item = nullptr;
    current_path = path;

    // 1. Очищаем панель со списком перед созданием новых объектов
    lv_obj_clean(objects.sd_card_fm_panel);

    // 2. Очищаем правую панель предпросмотра
    if (objects.sd_card_viewer_panel) {
        lv_obj_clean(objects.sd_card_viewer_panel);
    }

    File dir = SD_MMC.open(current_path.c_str());
    if (!dir || !dir.isDirectory()) {
        if (dir) dir.close();
        return;
    }

    int item_index = 0;

    // 1. Пункт ".." (Назад)
    if (current_path != "/") {
        lv_obj_t * cont = lv_obj_create(objects.sd_card_fm_panel);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, 544, 45);

        add_style_container1_1(cont);
        lv_obj_set_style_radius(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t * lbl = lv_label_create(cont);
        lv_obj_set_size(lbl, 530, 35);
        lv_obj_set_pos(lbl, 5, 3);
        lv_label_set_text(lbl, LV_SYMBOL_LEFT "  ..");

        auto * item_data = new SdFileItemData{"..", current_path, SD_ITEM_PARENT_DIR, 0};

        lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cont, on_item_clicked, LV_EVENT_CLICKED, item_data);

        lv_obj_add_event_cb(cont, [](lv_event_t * e) {
            delete static_cast<SdFileItemData *>(lv_event_get_user_data(e));
        }, LV_EVENT_DELETE, item_data);

        item_index++;
    }

    // 2. Сканирование файлов
    File entry;
    while ((entry = dir.openNextFile())) {
        bool is_dir = entry.isDirectory();
        std::string name = entry.name();
        uint32_t size = entry.size();
        entry.close();

        std::string full_path = current_path;
        if (full_path.back() != '/') full_path += "/";
        full_path += name;

        lv_obj_t * cont = lv_obj_create(objects.sd_card_fm_panel);
        lv_obj_remove_style_all(cont);
        lv_obj_set_size(cont, 544, 45);

        if (item_index % 2 == 0) {
            add_style_container1_1(cont);
        } else {
            add_style_container1(cont);
        }

        lv_obj_set_style_radius(cont, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t * lbl = lv_label_create(cont);
        lv_obj_set_size(lbl, 530, 35);
        lv_obj_set_pos(lbl, 5, 3);

        if (is_dir) {
            lv_label_set_text_fmt(lbl, "[%s]", name.c_str());
        } else {
            lv_label_set_text(lbl, name.c_str());
        }

        auto * item_data = new SdFileItemData{
            name, full_path, is_dir ? SD_ITEM_FOLDER : SD_ITEM_FILE, size
        };

        lv_obj_add_flag(cont, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(cont, on_item_clicked, LV_EVENT_CLICKED, item_data);

        lv_obj_add_event_cb(cont, [](lv_event_t * e) {
            delete static_cast<SdFileItemData *>(lv_event_get_user_data(e));
        }, LV_EVENT_DELETE, item_data);

        item_index++;
    }

    dir.close();
}