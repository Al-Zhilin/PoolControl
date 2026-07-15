/*
    Пример разбора структуры Config и обработки некорректного конфига (Status::INCORRECT_CONFIG)
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

    // --- Свой конфиг вместо конфига "по умолчанию" ---
    RingFileLogger::Config cfg;
    cfg.maxFileSize = 4096;              // файлы поменьше — 4кБ каждый
    cfg.maxFilesNum = 4;                 // и их побольше — 4 штуки в кольце
    cfg.dirName = "mylogs";              // своя директория вместо "logs"
    cfg.filePrefix = "session_";         // файлы будут называться session_0.log, session_1.log ...
    cfg.fileExtension = ".log";

    RingFileLogger::Status st = logger.begin(LittleFS, cfg);
    Serial.print("begin() со своим конфигом -> ");
    Serial.println(st == RingFileLogger::Status::SUCCESS ? "SUCCESS" : "ОШИБКА");
    // На ФС появится директория /mylogs с файлами session_0.log ... session_3.log

    // --- Теперь намеренно передаём некорректный конфиг ---
    // Библиотека проверяет конфиг в begin(): maxFileSize должен быть больше размера
    // служебного заголовка файла (9 байт), а maxFilesNum не может быть нулевым.
    RingFileLogger::Config badCfg;
    badCfg.maxFileSize = 5;              // слишком мал — даже заголовок не влезет
    badCfg.maxFilesNum = 0;              // ноль файлов в кольце — бессмысленно

    RingFileLogger::Status badSt = logger.begin(LittleFS, badCfg);
    Serial.print("begin() с некорректным конфигом -> ");
    Serial.println(badSt == RingFileLogger::Status::INCORRECT_CONFIG ? "INCORRECT_CONFIG (как и ожидалось)" : "неожиданный статус!");

    // Важно: после неудачного begin() логгер считается неинициализированным —
    // предыдущее успешное состояние (с cfg выше) не сохраняется, работать с логгером нельзя,
    // пока begin() не будет вызван повторно с корректным конфигом.
    Serial.println("Повторная инициализация с корректным конфигом:");
    st = logger.begin(LittleFS, cfg);
    Serial.println(st == RingFileLogger::Status::SUCCESS ? "SUCCESS" : "ОШИБКА");
}

void loop() {
}
