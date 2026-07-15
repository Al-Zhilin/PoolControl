/*
    Пример: чтение всей истории логов через dumpTo() и полная очистка через clear()
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

    // Маленький конфиг, чтобы гарантированно накопить несколько файлов кольца за пару записей
    // и увидеть, что dumpTo() действительно читает их все по порядку, а не только последний.
    RingFileLogger::Config cfg;
    cfg.maxFileSize = 128;
    cfg.maxFilesNum = 3;
    cfg.dirName = "dumpdemo";

    if (logger.begin(LittleFS, cfg) != RingFileLogger::Status::SUCCESS) {
        Serial.println("Ошибка инициализации логгера!");
        return;
    }

    // Пишем с запасом, чтобы точно произошла хотя бы одна ротация файлов.
    for (int i = 0; i < 15; i++) {
        logger.printf("record #%d, millis=%lu\n", i, millis());
    }
    logger.flush();

    // --- dumpTo(Print&) ---
    // Читает все файлы кольца по порядку от самого старого поколения к самому новому
    // и пишет их содержимое в переданный Print (тут — в Serial). Возвращает Status:
    // NOT_INITIALIZED, если begin() не вызывался/не удался, DUMP_OUT_ERROR — если запись
    // в переданный Print оборвалась (например, устройство назначения перестало принимать данные).
    Serial.println("----- dumpTo(Serial): вся история логов от старой записи к новой -----");
    RingFileLogger::Status dumpSt = logger.dumpTo(Serial);
    Serial.println("----- конец дампа -----");
    Serial.print("dumpTo() -> ");
    Serial.println(dumpSt == RingFileLogger::Status::SUCCESS ? "SUCCESS" : "ОШИБКА");

    Serial.print("totalBytesUsed() до очистки: ");
    Serial.println(logger.totalBytesUsed());

    // --- clear() ---
    // Удаляет все файлы кольца и создаёт первый файл заново (поколение сбрасывается в 1).
    // Используйте, когда данные из лога уже выгружены (например, dumpTo() выше отправил их
    // на ПК/в облако) и локальное хранилище пора освободить.
    RingFileLogger::Status clearSt = logger.clear();
    Serial.print("clear() -> ");
    Serial.println(clearSt == RingFileLogger::Status::SUCCESS ? "SUCCESS" : "ОШИБКА");

    Serial.print("totalBytesUsed() после очистки: ");
    Serial.println(logger.totalBytesUsed());   // останется только служебный заголовок первого файла
}

void loop() {
}
