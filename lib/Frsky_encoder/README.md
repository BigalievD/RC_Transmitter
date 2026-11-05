# FrSky Encoder

Arduino-библиотека для rotary encoder FrSky с кнопкой.

## Возможности

- Определение вращения влево/вправо.
- Отслеживание позиции энкодера.
- Короткое нажатие кнопки.
- Долгое нажатие кнопки.
- `PRESSED` и `LONG PRESS` взаимоисключающие.
- После `LONG PRESS` отпускание не генерирует отдельное событие.
- Настраиваемое время долгого нажатия.
- Программный debounce кнопки.
- Получение текущего состояния и времени удержания.

## Подключение

| Сигнал | GPIO |
|--------|------|
|   A    |  18  |
|   B    |  19  |
|   SW   |  21  |
|   GND  |  GND |

```cpp
FrSkyEncoder encoder(18, 19, 21);
```

Входы используют `INPUT_PULLUP`.

## Быстрый старт

```cpp
#include <Arduino.h>
#include "FrSkyEncoder.h"

FrSkyEncoder encoder(18, 19, 21);

void setup()
{
    Serial.begin(115200);
    encoder.begin();

    // Долгое нажатие через 800 мс
    encoder.setLongPressTime(800);
}

void loop()
{
    encoder.update();

    if (encoder.left())
        Serial.println("LEFT");

    if (encoder.right())
        Serial.println("RIGHT");

    if (encoder.pressed())
        Serial.println("PRESSED");

    if (encoder.longPressed())
        Serial.println("LONG PRESS");
}
```

## Логика кнопки

### Короткое нажатие

```text
НАЖАЛ → ОТПУСТИЛ
             ↓
          PRESSED
```

### Долгое нажатие

```text
НАЖАЛ → 800 мс → LONG PRESS → ОТПУСТИЛ
```

После `LONG PRESS` при отпускании дополнительного события нет.

Таким образом:

```text
Короткое нажатие → PRESSED
Долгое нажатие   → LONG PRESS
```

## Настройка времени Long Press

По умолчанию:

```cpp
800
```

мс.

Изменение:

```cpp
encoder.setLongPressTime(1000);
```

Примеры:

```cpp
encoder.setLongPressTime(500);   // 0,5 сек
encoder.setLongPressTime(800);   // 0,8 сек
encoder.setLongPressTime(1000);  // 1 сек
encoder.setLongPressTime(1500);  // 1,5 сек
encoder.setLongPressTime(2000);  // 2 сек
```

Получить установленное значение:

```cpp
uint32_t time = encoder.getLongPressTime();
```

## API

### `FrSkyEncoder(pinA, pinB, pinSW)`

Создание объекта:

```cpp
FrSkyEncoder encoder(18, 19, 21);
```

### `begin()`

Инициализация GPIO:

```cpp
encoder.begin();
```

### `update()`

Обновление состояния энкодера и кнопки. Вызывать регулярно в `loop()`:

```cpp
encoder.update();
```

### `left()`

Возвращает `true` при одном шаге вращения влево:

```cpp
if (encoder.left())
    Serial.println("LEFT");
```

### `right()`

Возвращает `true` при одном шаге вращения вправо:

```cpp
if (encoder.right())
    Serial.println("RIGHT");
```

### `pressed()`

Возвращает `true` только для короткого нажатия:

```cpp
if (encoder.pressed())
    Serial.println("PRESSED");
```

При длинном нажатии не срабатывает.

### `longPressed()`

Возвращает `true` один раз после достижения установленного времени:

```cpp
if (encoder.longPressed())
    Serial.println("LONG PRESS");
```

### `isPressed()`

Показывает текущее состояние:

```cpp
if (encoder.isPressed())
    Serial.println("BUTTON IS DOWN");
```

### `pressDuration()`

Возвращает время текущего удержания в миллисекундах:

```cpp
Serial.println(encoder.pressDuration());
```

При отпущенной кнопке возвращает `0`.

### `setLongPressTime(uint32_t time)`

Устанавливает время долгого нажатия в миллисекундах:

```cpp
encoder.setLongPressTime(1200);
```

### `getLongPressTime()`

Возвращает установленное время:

```cpp
uint32_t time = encoder.getLongPressTime();
```

### `position()`

Возвращает текущую позицию:

```cpp
int pos = encoder.position();
```

### `reset()`

Сбрасывает позицию в `0`:

```cpp
encoder.reset();
```

## Пример меню

```cpp
#include <Arduino.h>
#include "FrSkyEncoder.h"

FrSkyEncoder encoder(18, 19, 21);

int menuItem = 0;

void setup()
{
    Serial.begin(115200);
    encoder.begin();

    encoder.setLongPressTime(1000);
}

void loop()
{
    encoder.update();

    if (encoder.left())
    {
        menuItem--;
        if (menuItem < 0)
            menuItem = 0;
    }

    if (encoder.right())
    {
        menuItem++;
        if (menuItem > 9)
            menuItem = 9;
    }

    if (encoder.pressed())
    {
        Serial.print("SELECT: ");
        Serial.println(menuItem);
    }

    if (encoder.longPressed())
    {
        Serial.println("BACK");
    }
}
```

## Debounce

Для кнопки используется программный debounce:

```cpp
static const uint32_t BUTTON_DEBOUNCE = 50;
```

Значение находится в `FrSkyEncoder.h`.

## Структура

```text
src/
├── main.cpp
├── FrSkyEncoder.h
└── FrSkyEncoder.cpp
```

## Принцип работы энкодера

Используются два квадратурных сигнала A и B:

```text
A ──┐    ┌────┐    ┌────
    └────┘    └────┘

B ─────┐    ┌────┐    ┌──
       └────┘    └────┘
```

Направление определяется по последовательности переходов A/B.

Один обработанный физический щелчок соответствует одному изменению позиции:

```text
вправо → position + 1
влево  → position - 1
```

## Требования

- Arduino framework.
- ESP32 или совместимая плата с цифровыми GPIO.
- `Arduino.h`.
- Дополнительные библиотеки не требуются.

## Лицензия

Использование и модификация библиотеки разрешены для собственных проектов.
