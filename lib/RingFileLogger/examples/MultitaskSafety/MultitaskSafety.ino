/*
    Пример: потокобезопасность логгера.

    RingFileLogger защищён внутренним мьютексом (см. MutexRAII в RingFileLogger.h) —
    в него можно безопасно писать одновременно из разных задач FreeRTOS / разных ядер ESP32,
    не разрушая содержимое файлов. Здесь две задачи пишут в лог одновременно на высокой скорости,
    третья — периодически проверяет целостность через totalBytesUsed()/currentFileNum().
*/

#include <Arduino.h>
#include <LittleFS.h>
#include "RingFileLogger.h"

RingFileLogger logger;

// Задача, которая быстро и много пишет в лог от своего имени.
void writerTask(void* param) {
    const char* taskName = (const char*)param;

    for (;;) {
        // Одновременный вызов logger.printf() из двух задач безопасен: пока одна задача
        // держит внутренний мьютекс библиотеки, вторая просто дождётся своей очереди внутри write().
        logger.printf("[%s] tick=%lu, core=%d\n", taskName, millis(), xPortGetCoreID());
        vTaskDelay(pdMS_TO_TICKS(5));   // намеренно часто — чтобы гонки было проще спровоцировать
    }
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

    // Две задачи-писателя на разных ядрах ESP32 (0 и 1), плюс встроенный loop() на ядре 1 —
    // итого минимум два независимых потока исполнения одновременно пишут в один и тот же логгер.
    xTaskCreatePinnedToCore(writerTask, "writerA", 4096, (void*)"writerA", 1, nullptr, 0);
    xTaskCreatePinnedToCore(writerTask, "writerB", 4096, (void*)"writerB", 1, nullptr, 1);
}

void loop() {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck < 1000) return;
    lastCheck = millis();

    // Если бы записи из writerA/writerB не были синхронизированы мьютексом, здесь можно было бы
    // увидеть повреждённые данные или рассинхронизацию currentFileSize. При корректной работе
    // мьютекса totalBytesUsed() стабильно растёт, а currentFileNum()/currentGenCount() меняются
    // предсказуемо, без "рваных" значений.
    Serial.print("totalBytesUsed()  = ");
    Serial.println(logger.totalBytesUsed());
    Serial.print("currentFileNum()  = ");
    Serial.println(logger.currentFileNum());
    Serial.print("currentGenCount() = ");
    Serial.println(logger.currentGenCount());
    Serial.println("---");
}
