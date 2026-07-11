#include "RingFileLogger.h"

String RingFileLogger::makeFilePath(uint16_t slotNum) const {
    String path = "/";
    path += _config.dirName;
    path += "/";
    path += _config.filePrefix;
    path += slotNum;
    path += _config.fileExtension;
    return path;
}

bool RingFileLogger::readHeader(uint16_t slotNum, FileHeader& out) {
    fs::File file = _filesystem->open(makeFilePath(slotNum), FILE_READ);
    if (!file) return false;                          // не открылся / не существует
    size_t got = file.read((uint8_t*)&out, sizeof(out));  // структуру читаем как массив байт
    file.close();
    if (got != sizeof(out)) return false;            // файл короче, чем заголовок

    if (out.magic != HEADER_MAGIC) return false;     // magic число не совпало - файл либо не наш, либо поврежден
    
    return true;
}

bool RingFileLogger::writeHeader(fs::File& file, uint32_t generation) {
    FileHeader fh = {
        .magic = HEADER_MAGIC,
        .generation = generation,
        .version = HEADER_VERSION
    };

    size_t got = file.write((uint8_t*)&fh, sizeof(fh));
    file.close();
    if (got != sizeof(fh)) return false;            // ошибка записи
    
    return true;
}

RingFileLogger::Status RingFileLogger::begin(fs::FS& filesys, const Config &cfg) {
    
}