#pragma once

#include "Print.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <FS.h>

/*
TODO:
- Полная проверка валидности переданного конфига в begin()



*/

class RingFileLogger : public Print {
    public:
        struct Config {
            uint32_t maxFileSize = 16384;           // максимальный размер одного файла. default = 16KB
            uint16_t maxFilesNum = 2;               // максимальное кол-во файлов. default = 2 штуки
            const char* dirName = "logs";           // имя директории(папки) с логами
            const char* filePrefix = "log";         // префикс имени файлов
            const char* fileExtension = ".txt";     // расширение файлов
        };

        enum class Status {
            SUCCESS = 0,
            NOT_INITIALIZED,                        // отсутствует инициализация (begin() не был вызван или завершился с ошибкой)
            FILESYSTEM_ERROR,                       // недоступность ФС
            FILE_OPEN_ERROR,                        // не удалось открыть файл
            FILE_CREATE_ERROR,                      // не удалось создать файл
            FILE_WRITE_ERROR,                       // ошибка записи данных в файл
            INCORRECT_CONFIG,                       // в переданном полььзовательском конфиге содержатся инвалидные данные
            DUMP_OUT_ERROR,                         // ошибка вывода в переданный Print& в функции dumpTo()
            MUTEX_CREATE_ERROR,                     // ошибка создания мьютекса
            MUTEX_TIMEOUT_ERROR,                    // ошибка времени ожидания захвата мьютекса
        };

        Status begin(fs::FS& filesys, const Config& cfg = Config{});
        size_t write(uint8_t) override;
        size_t write(const uint8_t*, size_t) override;
        void flush() override;
        Status clear();
        Status dumpTo(Print&);

        // Геттеры
        size_t totalBytesUsed() const;                   // суммарный размер лог-файлов в ФС
        uint32_t currentGenCount() const {               // номер текущего "поколения"
            MutexRAII guard(_mutex);
            if (!guard.acquired()) return 0;
            return _currentGenCount;
        }   
        uint16_t currentFileNum() const {                // номер файла, с которым класс сейчас работает
            MutexRAII guard(_mutex);
            if (!guard.acquired()) return 0;
            return _currentFileNum;
        }
        

    private:
        static constexpr uint32_t HEADER_MAGIC  = 0x52464C47;    // "RFLG" для примера
        static constexpr uint8_t  HEADER_VERSION = 1;

        struct __attribute__((packed)) FileHeader {
            uint32_t magic;
            uint32_t generation;
            uint8_t version;
        };
        static_assert(sizeof(FileHeader) == 9, "FileHeader must be packed! Or check the size of the FileHeader struct!");

        fs::FS* _filesystem = nullptr;
        mutable fs::File _file;

        SemaphoreHandle_t _mutex = nullptr;
        
        Config _config;
        uint32_t _currentGenCount = 0;
        uint16_t _currentFileNum = 0;
        uint32_t _currentFileSize = 0;
        bool _ready = false;

        Status rotate();
        Status createFirstFile();
        String makeFilePath(uint16_t) const;
        String makeDirPath() const;
        bool readHeader(fs::File&, FileHeader&);
        bool writeHeader(fs::File&, uint32_t generation);

        // маленькая обертка для реализации принципа RAII на мьютексе класса логирования
        class MutexRAII {
            public:
                MutexRAII(SemaphoreHandle_t mutex) {
                    handle = mutex;
                    _acquired = false;
                    _acquired = (handle != nullptr) && (xSemaphoreTake(handle, MUTEX_TIMEOUT) == pdTRUE);
                }

                ~MutexRAII() {
                    if (_acquired) xSemaphoreGive(handle);
                }

                // Запрещаем копирование через конструктор и оператор
                MutexRAII(const MutexRAII&) = delete;
                MutexRAII& operator=(const MutexRAII&) = delete;

                // Метод проверки успешности захвата мьютекса
                bool acquired() const {
                    return _acquired;
                }

            private:
                static constexpr TickType_t MUTEX_TIMEOUT = pdMS_TO_TICKS(1000);            // таймаут ожидания мьютекса
                bool _acquired = false;
                SemaphoreHandle_t handle;
        };
};