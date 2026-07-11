// Все для логирования, сохранения логов и управления выводом

int my_log_vprintf(const char* fmt, va_list args) {
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);

    // здесь пишем в кольцевой буфер по условию. Буфер сохраняет логи всегда, Telet или Serial - по флагу

    #if LOG_ENABLED
        #ifdef USING_TELNET_LOG
            SerialAndTelnet.print(buf);
        #else
            Serial.print(buf);
        #endif
    #endif

    return len;
}