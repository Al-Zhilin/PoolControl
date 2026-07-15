/*
    Пример: RingFileLogger наследуется от Print, поэтому в него можно писать
    теми же способами, что и в Serial — write(), print(), println(), printf()
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "RingFileLogger.h"

RingFileLogger logger;

void setup() {
    Serial.begin(115200);
    delay(300);

    if (!LittleFS.begin(true)) {
        Serial.println("Не удалось смонтировать LittleFS!");
        return;
    }

    if (logger.begin(LittleFS) != RingFileLogger::Status::SUCCESS) {
        Serial.println("Ошибка инициализации логгера!");
        return;
    }

    // --- write(uint8_t) — запись одного байта ---
    // Возвращает 1, если байт записан, и 0 при ошибке.
    size_t r1 = logger.write((uint8_t)'A');
    Serial.print("write(byte) записал байт: ");
    Serial.println(r1);

    // --- write(const uint8_t*, size_t) — запись произвольного буфера ---
    // Возвращает реальное количество записанных байт (может быть меньше size, если что-то пошло не так).
    const char msg[] = "raw buffer\n";
    size_t r2 = logger.write((const uint8_t*)msg, strlen(msg));
    Serial.print("write(buf, len) записал байт: ");
    Serial.println(r2);

    // --- print()/println() — все стандартные перегрузки класса Print ---
    size_t r3 = logger.print("Число: ");
    r3 += logger.println(42);
    Serial.print("print()+println(int) записали байт: ");
    Serial.println(r3);

    size_t r4 = logger.println(3.14159, 2);        // println(float, кол-во знаков после запятой)
    Serial.print("println(float) записал байт: ");
    Serial.println(r4);

    // --- printf() — форматированный вывод, как в printf() из stdio ---
    size_t r5 = logger.printf("Форматированная строка: temp=%.1fC, millis=%lu\n", 23.7, millis());
    Serial.print("printf() записал байт: ");
    Serial.println(r5);

    logger.flush();
    Serial.println("Все данные сброшены на ФС (flush).");
}

void loop() {
}
