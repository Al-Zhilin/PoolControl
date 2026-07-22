// Все для логирования, сохранения логов и управления выводом
#include <ESPmDNS.h>
#include <WebServer.h>

WebServer web_server(80);
RingFileLogger logger;

// --- Перенаправление стандартного потока вывода логов в RingFileLogger + Serial/Telnet по выбору ---
int my_log_vprintf(const char* fmt, va_list args) {
    // --- Собираем строку в буфер ---
    char log_buf[256];
    int len = vsnprintf(log_buf, sizeof(log_buf), fmt, args);

    // --- Подготавливаем временной штамп для лога ---
    char time_buf[32];
    time_t now = time(NULL);

    if (now > 100000000) {                    // время синхронизировано
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);

        strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    }

    else snprintf(time_buf, 32, "NO-NTP");
    char result_buf[288];
    snprintf(result_buf, sizeof(result_buf), "%s: %s", time_buf, log_buf);
    
    // --- Логируем в ФС ---
    logger.print(result_buf);

    // --- Логируем в Serial/Telnet по выбору ---
    #if LOG_ENABLED
        #ifdef USING_TELNET_LOG
            SerialAndTelnet.print(log_buf);
        #else
            Serial.print(log_buf);
        #endif
    #endif

    return len;
}

// --- Проверка пароля клиента ---
bool checkAuth() {
    if (!web_server.authenticate("admin", WEB_SECRET_TOKEN)) {
        web_server.requestAuthentication();
        return false;
    }
    return true;
}

// --- Обработчики запросов ---
void handleRoot() {
    if (!checkAuth())   return;
    web_server.send(200, "text/html", R"rawliteral(<!DOCTYPE html>
        <html><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>ESP32 Logs</title><style>
        *{margin:0;padding:0;box-sizing:border-box}
        body{display:flex;justify-content:center;align-items:center;min-height:100vh;background:linear-gradient(145deg,#f5f7fa 0%,#e4e9f2 100%);font-family:system-ui,-apple-system,sans-serif;padding:20px}
        .card{background:#ffffff;border-radius:32px;padding:48px 56px;max-width:520px;width:100%;box-shadow:0 20px 60px rgba(0,0,0,0.08),0 8px 24px rgba(0,0,0,0.04);text-align:center;transition:box-shadow .3s}
        .card:hover{box-shadow:0 30px 80px rgba(0,0,0,0.12)}
        h1{font-size:24px;font-weight:600;letter-spacing:-0.5px;color:#1e293b;margin-bottom:12px}
        h1 small{display:block;font-size:14px;font-weight:400;color:#94a3b8;letter-spacing:0.3px;margin-top:4px}
        .divider{height:1px;background:linear-gradient(to right,transparent,#d1d9e6,transparent);margin:20px 0 28px}
        .btn-group{display:flex;flex-direction:column;gap:12px}
        .btn{display:block;padding:18px 24px;border-radius:60px;font-size:16px;font-weight:500;text-decoration:none;text-align:center;transition:all .25s;cursor:pointer;border:none;outline:none}
        .btn-download{background:linear-gradient(135deg,#6366f1,#8b5cf6);color:#fff;box-shadow:0 8px 24px rgba(99,102,241,0.30)}
        .btn-download:hover{transform:translateY(-3px);box-shadow:0 12px 32px rgba(99,102,241,0.45)}
        .btn-clear{background:#f1f5f9;color:#334155;border:1px solid #e2e8f0}
        .btn-clear:hover{background:#e2e8f0;border-color:#cbd5e1;transform:translateY(-2px)}
        .status{display:flex;justify-content:center;gap:12px;margin-top:24px;font-size:13px;color:#94a3b8}
        .status span{display:inline-block;width:8px;height:8px;border-radius:50%;background:#22c55e;box-shadow:0 0 0 3px rgba(34,197,94,0.2)}
        @media(max-width:480px){.card{padding:32px 20px}.btn-group{flex-direction:column}}
        </style></head><body>
        <div class=card>
        <h1>⚡ ESP32: <small>управление логами</small></h1>
        <div class=divider></div>
        <div class=btn-group>
            <a href="/download" class="btn btn-download">Скачать логи</a>
            <a href="/clear" class="btn btn-clear">Очистить логи</a>
        </div>
        <div class=status><span></span> система активна</div>
        </div>
        </body></html>)rawliteral");
}

void handleDownload() {
    if (!checkAuth()) return;

    char filename[48];
    time_t now = time(NULL);
    if (now > 100000000) {
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        strftime(filename, sizeof(filename), "esp32_logs_%Y-%m-%d_%H-%M-%S.txt", &timeinfo);
    } else {
        snprintf(filename, sizeof(filename), "esp32_logs_no-ntp_%lu.txt", millis());
    }


    WiFiClient client = web_server.client();
    client.print("HTTP/1.1 200 OK\r\n");
    client.print("Content-Type: text/plain; charset=utf-8\r\n");
    client.print("Content-Disposition: attachment; filename=\"" + String(filename) + "\"\r\n");   // filename
    client.print("Connection: close\r\n\r\n");
    logger.dumpTo(client);
    client.stop();
}

void handleClear() {
    if (!checkAuth())   return;
    logger.clear();
    web_server.sendHeader("Location", "/");
    web_server.send(302, "text/plain", "");
}