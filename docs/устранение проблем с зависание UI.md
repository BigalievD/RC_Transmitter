Что в итоге было изменено, что убрало зависание?
Одна правка в одном файле LVGL — .pio/libdeps/Waveshare/lvgl/src/widgets/slider/lv_slider.c.

Было (оригинал LVGL):


void lv_slider_set_value(lv_obj_t * obj, int32_t value, lv_anim_enable_t anim)
{
    lv_bar_set_value(obj, value, anim);
}
Стало (патч):


void lv_slider_set_value(lv_obj_t * obj, int32_t value, lv_anim_enable_t anim)
{
    lv_bar_t * bar = (lv_bar_t *)obj;
    value = LV_CLAMP(bar->min_value, value, bar->max_value);
    if(bar->cur_value == value) return;
    bar->cur_value = value;

    lv_display_t * disp = lv_obj_get_display(obj);
    if(disp) {
        lv_area_t coords;
        lv_obj_get_coords(obj, &coords);
        lv_inv_area(disp, &coords);
    }
}
Плюс добавлен #include "../../core/lv_refr_private.h" в начале файла.

Что изменилось по логике:

Оригинал	Патч
Запись значения	Через lv_bar_set_value → вызывает lv_obj_invalidate	Прямая запись в bar->cur_value
Инвалидация экрана	lv_obj_invalidate → 5 полных обходов дерева объектов (blur walk по всем слоям)	lv_inv_area → только помечает область dirty, без обходов
Почему зависало: при 17 слайдерах на экране настроек, каждый вызов lv_slider_set_value запускал 5 × обход ВСЕХ объектов на экране. Сотни объектов × 17 слайдеров = зависание ESP32-P4.