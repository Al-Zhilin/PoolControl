#pragma once

#include "Print.h"
#include <FS.h>

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
        };

        Status begin(fs::FS& filesys, const Config& cfg = Config{});
        void flush() override;
        size_t write(uint8_t) override;
        size_t write(const uint8_t*, size_t) override;
        Status clear();
        Status dumpTo(Print&);

        // Геттеры
        size_t totalBytesUsed() const;                   // суммарный размер лог-файлов в ФС
        uint32_t currentGenCount() const;                // номер текущего "поколения"
        uint16_t currentFileNum() const;                 // номер файла, с которым класс сейчас работает
        

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
        fs::File _file;
        
        Config _config;
        uint32_t _currentGenCount = 0;
        uint16_t _currentFileNum = 0;
        uint32_t _currentFileSize = 0;

        void rotate();
        String makeFilePath(uint16_t) const;
        bool readHeader(uint16_t, FileHeader&);
        bool writeHeader(fs::File&, uint32_t generation);
};