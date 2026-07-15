/*
    Пример: геттеры состояния логгера — totalBytesUsed(), currentGenCount(), currentFileNum()
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "RingFileLogger.h"

RingFileLogger logger;

void printStats() {
    // totalBytesUsed() — суммарный размер ВСЕХ файлов кольца на ФС (не только текущего).
    // Полезно, например, чтобы прикинуть, сколько места логи реально занимают на носителе.
    Serial.print("totalBytesUsed()  = ");
    Serial.print(logger.totalBytesUsed());
    Serial.println(" байт");

    // currentGenCount() — сквозной счётчик "поколений" файла: растёт при каждой ротации
    // и не сбрасывается в 0 при переходе на следующий файл (в отличие от currentFileNum()).
    Serial.print("currentGenCount() = ");
    Serial.println(logger.currentGenCount());

    // currentFileNum() — индекс файла кольца, с которым логгер работает прямо сейчас (0 .. maxFilesNum-1).
    Serial.print("currentFileNum()  = ");
    Serial.println(logger.currentFileNum());

    Serial.println("---");
}

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

    Serial.println("Состояние сразу после begin():");
    printStats();
}

void loop() {
    static uint32_t lastWrite = 0;
    if (millis() - lastWrite < 1000) return;
    lastWrite = millis();

    logger.printf("stats demo line, millis=%lu\n", millis());
    logger.flush();

    Serial.println("Состояние после очередной записи:");
    printStats();
}
