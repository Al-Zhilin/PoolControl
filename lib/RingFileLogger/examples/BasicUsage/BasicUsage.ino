/*
    Пример базовой работы с библиотекой
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "RingFileLogger.h"

RingFileLogger logger;                   // создание объекта библиотеки

void setup() {
    Serial.begin(115200);
    delay(300);

    // Файловая система должна быть смонтирована до вызова begin() библиотеки —
    // сама RingFileLogger файловую систему не монтирует, только работает поверх неё.
    if (!LittleFS.begin(true)) {          // true = отформатировать ФС, если не удалось смонтировать
        Serial.println("Не удалось смонтировать LittleFS!");
        return;
    }

    RingFileLogger::Config cfg = {
        .maxFileSize = 16384,            // максимальный размер одного файла в байтах. По умолчанию 16кБ
        .maxFilesNum = 2,                // максимальное количество файлов. По умолчанию 2
        .dirName = "logs",               // имя директории хранения файлов. По умолчанию "logs"
        .filePrefix = "log",             // префикс имени файла. По умолчанию "log", следовательно будут создаваться файлы log0.txt, log1.txt ...
        .fileExtension = ".txt",         // расширение хранимых файлов. По умолчанию ".txt"
    };

    // Инициализация. Принимает ссылки на используемую ФС (fs::FS&) и конфиг. При отсутствии передаваемого конфига
    // будут использованы настройки, указанные выше.
    RingFileLogger::Status st = logger.begin(LittleFS, cfg);
    if (st != RingFileLogger::Status::SUCCESS) {
        // begin() вернёт код ошибки, если, например, конфиг некорректен или недоступна ФС.
        // Полный разбор кодов Status и некорректного конфига — в примере CustomConfig.
        Serial.println("Ошибка инициализации логгера!");
        return;
    }

    // RingFileLogger наследуется от Print — писать в лог можно теми же методами, что и в Serial.
    logger.println("Логгер инициализирован, начинаем работу");
}

void loop() {
    // Раз в секунду пишем в лог одну строку с меткой времени и сбрасываем буфер на ФС.
    static uint32_t lastWrite = 0;
    if (millis() - lastWrite >= 1000) {
        lastWrite = millis();

        // print()/println() возвращают количество реально записанных байт —
        // удобно сверять с ожидаемой длиной строки, чтобы заметить ошибку записи.
        size_t wrote = logger.print("Отметка времени, millis() = ");
        wrote += logger.println(millis());

        // flush() гарантированно сбрасывает буферизованные данные драйвера ФС на носитель.
        // Не обязателен на каждую запись, но полезен перед событиями вроде перезагрузки.
        logger.flush();

        Serial.print("Записано байт: ");
        Serial.println(wrote);
    }
}