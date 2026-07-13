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

String RingFileLogger::makeDirPath() const {
    return "/" + String(_config.dirName);
}

bool RingFileLogger::readHeader(fs::File& file, FileHeader& out) {
    if (!file) return false;                         // не открылся / не существует
    size_t got = file.read((uint8_t*)&out, sizeof(out));  // структуру читаем как массив байт
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
    if (got != sizeof(fh)) return false;            // ошибка записи
    
    return true;
}

RingFileLogger::Status RingFileLogger::begin(fs::FS& filesystem, const Config &cfg) {
    // --- Сохраняем настройки ---
    if (cfg.maxFileSize <= 9 ||!cfg.maxFilesNum) return Status::INCORRECT_CONFIG;

    _config = cfg;
    _filesystem = &filesystem;

    // --- Проверка существования директории ---
    if (_filesystem->exists(makeDirPath())) {                 // Существует, проверим файлы
        uint32_t max_generation = 0;
        for (uint16_t file_iter = 0; file_iter < _config.maxFilesNum; file_iter++) {
            fs::File t_file = _filesystem->open(makeFilePath(file_iter), FILE_READ);

            if(!t_file)  continue;       // файл не существует
            FileHeader fh;
            if (!readHeader(t_file, fh) || fh.version != HEADER_VERSION) {   // файл некорректен - удаляем. В будущем создадим заново
                t_file.close();
                _filesystem->remove(makeFilePath(file_iter));
                continue;
            }

            // --- Выискиваем файл с наибольшим поколением ---
            if (fh.generation > max_generation) {
                max_generation = fh.generation;
                _currentFileNum = file_iter;
            }

            t_file.close();
        }

        if (max_generation) {                                 // false если все невалидны
            // --- Открываем файл с наибольшим поколением ---
            if (!(_file = _filesystem->open(makeFilePath(_currentFileNum), FILE_APPEND)))    return Status::FILE_OPEN_ERROR;
            _currentFileSize = _file.size();
            _currentGenCount = max_generation;
            return Status::SUCCESS;
        }
    }

    // --- Директория не существует ИЛИ все файлы невалидны ---
    if (!_filesystem->exists(makeDirPath())) _filesystem->mkdir(makeDirPath());
    if(!(_file = _filesystem->open(makeFilePath(0), FILE_APPEND))) return Status::FILE_OPEN_ERROR;
    if (!writeHeader(_file, 1)) return Status::FILE_WRITE_ERROR;
    _currentFileNum = 0;
    _currentGenCount = 1;
    _currentFileSize = sizeof(FileHeader);
    return Status::SUCCESS;

}

size_t RingFileLogger::write(const uint8_t* buffer, size_t size) {
    if (!_file) return 0;

    if (size > (_config.maxFileSize - sizeof(FileHeader)))  return 0;           // данные физически не влезают даже в пустой файл
    if (_currentFileSize + size > _config.maxFileSize)  {
        if (rotate() != Status::SUCCESS) return 0;
    }

    size_t wrote = _file.write(buffer, size);
    _currentFileSize += wrote;

    return wrote;
}

size_t RingFileLogger::write(uint8_t c) {
    return write(&c, 1);
}

void RingFileLogger::flush() {
    if (!_file) return;
    _file.flush();
}

RingFileLogger::Status RingFileLogger::rotate() {
    _currentGenCount++;

    // --- Вычисляем следующий файл для записи ---
    _currentFileNum = (_currentFileNum + 1) % _config.maxFilesNum;

    // --- Заканчиваем работу с текущим и переходим к следующему файлу ---
    // --- С обработкой ошибок: при возникновении пытаемся пересоздать, если проблема не решилась - возврат ошибки ---
    
    for (uint8_t attemp = 0; attemp < 2; ++attemp) {            // максимум одна попытка пересоздания
        if (_file)  _file.close();
        if (attemp) _filesystem->remove(makeFilePath(_currentFileNum));

        _file = _filesystem->open(makeFilePath(_currentFileNum), FILE_WRITE);
        bool success = _file && writeHeader(_file, _currentGenCount);

        if (success) {
            _currentFileSize = sizeof(FileHeader);
            return Status::SUCCESS;
        }
    }
    
    return Status::FILE_OPEN_ERROR;
}

RingFileLogger::Status RingFileLogger::dumpTo(Print& out) {
    flush();
    _file.close();
    uint8_t output_buf[257];

    uint16_t slot_iter = (_currentFileNum+1) % _config.maxFilesNum;
    for (;;slot_iter = (slot_iter + 1) % _config.maxFilesNum) {             // Обход со следующего файла по очереди (мин. поколение) до текущего (дамп всего)
        _file = _filesystem->open(makeFilePath(slot_iter), FILE_READ);
        if (_file) {
            _file.seek(sizeof(FileHeader));                                            // Header не читаем
        
            size_t read = 0;
            while (read = _file.read(output_buf, 256)) {                               // читаем пока read() не вернет 0 прочитанных байт
                if (out.write(output_buf, read) != read) {
                    _file = _filesystem->open(makeFilePath(_currentFileNum), FILE_APPEND);
                    return Status::DUMP_OUT_ERROR;
                }
            }
        }

        if (slot_iter == _currentFileNum) {                                        // успешно прочитали все файлы
            _file = _filesystem->open(makeFilePath(_currentFileNum), FILE_APPEND);
            return Status::SUCCESS;
        }
    }   
}

