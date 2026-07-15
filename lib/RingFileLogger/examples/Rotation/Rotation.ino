/*
    Пример: наблюдаем ротацию файлов вживую.

    Берём заведомо маленький maxFileSize, чтобы файлы заполнялись за пару записей,
    и следим за currentFileNum() / currentGenCount() — видно, как логгер по кругу
    переключается между файлами и увеличивает счётчик "поколений".
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "RingFileLogger.h"

RingFileLogger logger;

uint16_t lastFileNum = 0;
uint32_t lastGenCount = 0;

void setup() {
    Serial.begin(115200);
    delay(300);

    if (!LittleFS.begin(true)) {
        Serial.println("Не удалось смонтировать LittleFS!");
        return;
    }

    // Специально маленькие значения — только для демонстрации ротации.
    // В реальном проекте maxFileSize обычно намного больше (килобайты-десятки килобайт).
    RingFileLogger::Config cfg;
    cfg.maxFileSize = 64;     // очень маленький файл — заполнится буквально за пару строк
    cfg.maxFilesNum = 3;      // кольцо из 3 файлов
    cfg.dirName = "rotdemo";

    // Чтобы демонстрация начиналась "с чистого листа" при каждой прошивке — сотрём старые файлы.
    // кольца вручную (имена файлов  - складываются как dirName/filePrefix+N+fileExtension) - используем механику для удаления
    // Закомментируйте цикл, чтобы проверить сохранность логов и корректное продолжение работы после перезагрузки
    for (uint16_t i = 0; i < cfg.maxFilesNum; i++) {
        String path = String("/") + cfg.dirName + "/" + cfg.filePrefix + i + cfg.fileExtension;
        LittleFS.remove(path);
    }

    if (logger.begin(LittleFS, cfg) != RingFileLogger::Status::SUCCESS) {
        Serial.println("Ошибка инициализации логгера!");
        return;
    }

    lastFileNum = logger.currentFileNum();
    lastGenCount = logger.currentGenCount();
    Serial.print("Стартовое состояние: файл #");
    Serial.print(lastFileNum);
    Serial.print(", поколение ");
    Serial.println(lastGenCount);
}

void loop() {
    static uint32_t lastWrite = 0;
    if (millis() - lastWrite < 300) return;
    lastWrite = millis();

    // Каждая строка занимает заметную часть от 64 байт maxFileSize —
    // достаточно нескольких записей, чтобы вызвать переход на следующий файл кольца.
    logger.printf("line millis=%lu\n", millis());

    //logger.flush();       // Интересно! При перезагрузке, begin() следующего старта подхватывает не последнее поколение, которое
                            // вы можете увидеть в Serial, а предыдущее (при условии закомментирования цикла очистки выше). Это не баг: 
                            // просто физически новые данные в новый файл после rotation еще не успели долететь, попали только в буфер ФС
                            // раскомментируйте flush() метод, который будет принудительно переносить данные из буфера в файл после каждой записи
                            // и вы увидите "правильный подхват" файла после перезагрузки, ценой "отключения" буферизации данных, созданной для экономии ресурсов ФС
    
    // currentFileNum() — номер файла, с которым логгер работает сейчас (0 .. maxFilesNum-1).
    // currentGenCount() — сквозной счётчик "поколений": увеличивается при каждой ротации
    // и используется библиотекой, чтобы при перезапуске понять, какой файл самый свежий.
    uint16_t fileNum = logger.currentFileNum();
    uint32_t genCount = logger.currentGenCount();

    if (fileNum != lastFileNum || genCount != lastGenCount) {
        Serial.print(">>> РОТАЦИЯ: перешли на файл #");
        Serial.print(fileNum);
        Serial.print(", поколение ");
        Serial.println(genCount);
        lastFileNum = fileNum;
        lastGenCount = genCount;
    }
}
