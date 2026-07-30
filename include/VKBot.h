#include <ArduinoJson.h>
#include <cstdarg>

// Чтобы обязятельные параметры обязательно влезли в размеры буферов, лучше при нехватке места урезать само сообщение, заранее вычитая
// из свободного места размеры оставшихся параметров. Это позволит запросу отправится, даже при нехватке места для всего текста
// "&random_id=" + до 10 цифр (21) + "&access_token=" + токен + "&v=5.199" (14 + strlen(VK_TOKEN) + 8) + '\0'
const size_t VK_SUFFIX_RESERVE = 21 + 14 + strlen(VK_TOKEN) + 8 + 1;


char server[128] = "", key[256] = "";
uint32_t ts = 0;
int32_t dashboardMsgID = 0;

String VKPoll();

// Дописывает форматированную строку в buf, начиная с позиции len
// Возвращает false и НЕ меняет len, если строка не влезла целиком
bool appendChecked(char* buf, size_t bufSize, size_t& len, const char* fmt, ...) {
    size_t avail = bufSize - len;

    va_list args;
    va_start(args, fmt);
    int written = vsnprintf(buf + len, avail, fmt, args);
    va_end(args);

    if (written < 0 || (size_t)written >= avail) {
        return false;
    }

    len += written;
    return true;
}

// Структура ответа функции, работающей с API
struct VKApiResult {
    bool ok;                        // true, если запрос успешен и API не вернул ошибку
    int httpCode;                   // код от http
    int vkErrorCode = 0;            // код ошибки от ВК, если был
    String vkErrorMsg = "";         // сообщение ошибки от ВК, если был
    JsonDocument doc;               // распарсенное тело ответа
};

VKApiResult vkApiCall(const char* method, char* payload, size_t payloadCapacity, bool isPost) {
    VKApiResult result;
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(12000);

    size_t len = strlen(payload);           // сколько уже записано

    if (!appendChecked(payload, payloadCapacity, len, "&access_token=%s&v=5.199", VK_TOKEN)) {
        ESP_LOGE("VK_API", "[vkApiCall:access_token] Не хватило места в payload");
        result.ok = false;
        return result;
    }

    char url[400];
    DeserializationError err;

    for (uint8_t request_try = 0; request_try < 3; request_try++) {
        String returned_body;
        if (isPost) {
            if (snprintf(url, sizeof(url), "https://api.vk.com/method/%s", method) >= sizeof(url)) {
                ESP_LOGE("VK_API", "[vkApiCall:url:POST] Не хватило места в буфере [%d]", sizeof(url));
                result.ok = false;
                return result;
            }
            http.begin(client, url);
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            result.httpCode = http.POST(payload);
        }

        else {
            if (snprintf(url, sizeof(url), "https://api.vk.com/method/%s?%s", method, payload) >= sizeof(url)) {
                ESP_LOGE("VK_API", "[vkApiCall:url:GET] Не хватило места в буфере [%d]", sizeof(url));
                result.ok = false;
                return result;
            }
            http.begin(client, url);
            result.httpCode = http.GET();
        }

        returned_body = http.getString();
        http.end();
        err = deserializeJson(result.doc, returned_body);

        if (err || result.httpCode <= 0) {                // при сетевой ошибке пытаемся отправить запрос еще 2 раза
            result.ok = false;                            // ошибку десериализации тоже считаем обрывом связи
            ESP_LOGE("VK_API", "Метод %s: сетевая ошибка, http_code=%d", method, result.httpCode);
            /*ESP_LOGD("DIAG", "In vkApiCall: uptime=%lus, freeHeap=%u, maxAlloc=%u, minFreeHeap=%u, rssi=%d",
                        millis()/1000, ESP.getFreeHeap(), ESP.getMaxAllocHeap(), ESP.getMinFreeHeap(), WiFi.RSSI());
            */
            delay(200);
        }
        else break;
    }

    if (!err && !result.doc["error"].isNull())   {        // если есть ошибки
        result.vkErrorCode = result.doc["error"]["error_code"].as<int>();
        result.vkErrorMsg = result.doc["error"]["error_msg"].as<String>();
        result.ok = false;
        ESP_LOGE("VK_API", "Метод %s: VK вернул ошибку %d (%s)", method, result.vkErrorCode, result.vkErrorMsg.c_str());
    }

    else if (result.httpCode > 0) result.ok = true;

    return result;
}

// URLencode`ирование сообщения без String
size_t urlEncodeTo(const char* src, char* dst, size_t dstSize) {   // src - что преобразовываем, dst - куда записываем
    if (!dstSize)    return 0;
    size_t pos = 0;

    for (size_t src_iter = 0; src[src_iter] != '\0'; src_iter++) {
        char c = src[src_iter];

        // Проверка "безопасности" символа и преобразовывание его при надобности
        if (isalnum((unsigned char)c) || c == '-' || c == '_' || c == '.' || c == '~') {
            if (dstSize - 1 - pos >= 1) {           // есть как минимум свободный байт для записи
                dst[pos++] = c;
            }
            else {
                ESP_LOGE("VK_API", "[urlEncodeTo] Не хватило места в буфере");
                break;
            }

        } else {
            if (dstSize - 1 - pos >= 3) {           // есть как минимум 3 свободных байта для записи
                snprintf(dst + pos, dstSize - pos, "%%%02X", (uint8_t)c);
                pos += 3;
            }
            else {
                ESP_LOGE("VK_API", "[urlEncodeTo] Не хватило места в буфере");
                break;
            }
        }
    }

    dst[pos] = '\0';
    return pos;
}

// Отправка простого текстового сообщения
VKApiResult VKSendMessage(String data) {
    // --- Собираем тело запроса ---

    char payload[768];
    size_t len = snprintf(payload, sizeof(payload), "peer_id=%s&message=", VK_PEER_ID);
    len += urlEncodeTo(data.c_str(), payload + len, sizeof(payload) - len - VK_SUFFIX_RESERVE);             // записываем закодированную строку прямо в payload. -22 - чтобы следующие параметры гарантированно влезли

    if (!appendChecked(payload, sizeof(payload), len, "&random_id=%u", (unsigned)(esp_random() & 0x7FFFFFFF))) {
        ESP_LOGE("VK_API", "[VKSendMessage:random_id] Не хватило места в payload");
        VKApiResult result;
        result.ok = false;
        return result;
    }

    // --- Обращение к API и возврат результата ---
    return vkApiCall("messages.send", payload, sizeof(payload), true);
}

// инициализация Long Poll соединения
bool VKLongPollInit() {
    char payload[300];
    snprintf(payload, sizeof(payload), "group_id=%s", VK_GROUP_ID);

    VKApiResult result = vkApiCall("groups.getLongPollServer", payload, sizeof(payload), false);

    if (result.ok)    {
        const char* s = result.doc["response"]["server"] | "";
        strncpy(server, s, sizeof(server) - 1);
        server[sizeof(server) - 1] = '\0';

        const char* k = result.doc["response"]["key"] | "";
        strncpy(key, k, sizeof(key) - 1);
        key[sizeof(key) - 1] = '\0';

        ts = result.doc["response"]["ts"].as<uint32_t>();
        if (server[0] != '\0') return true;

        return false;
    }
    return false;
}

// работа с LongPoll
void vkLongPollTask(void* params) {
    static uint8_t failed_parsing = 0;  // сколько подряд ошибок поддержания Long Poll сессии произошло

    while (true) {
        if (server[0] == '\0') {              // инициализация не была запущена ИЛИ мы самостоятельно его инвалидировали при ошибках соединения
            if (!VKLongPollInit()) {                // если не удалось - попробуем после таймаута
                vTaskDelay(pdMS_TO_TICKS(7000));
                continue;
            }
        }


        String body = VKPoll();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, body);

        // Возникает, когда сервер ВК специально хочет заново инициализировать сессию/обновить ключ/ts
        if (!doc["failed"].isNull()) {
            uint8_t fail_code = doc["failed"].as<uint8_t>();
            switch (fail_code) {
                case 1:                     // ts устарел, берем новый из этого же ответа
                    ts = doc["ts"].as<uint32_t>();
                    break;

                case 2:                     // истек key, нужен новый VKPollInit()
                case 3:                     // инвалидна вся сессия, инициализируем заново
                    server[0] = '\0';            // для 2 и 3 случаев нужна инициализация, для этого сбрасываем сервер
                    break;
            }
            continue;
        }

        if (err)    {
            vTaskDelay(pdMS_TO_TICKS(3000));
            if (++failed_parsing >= 3)  {       // ошибок больше допустимого кол-ва - инвалидируем ранее инициализироанную сессию и прогоняем Init заново
                server[0] = '\0';
                failed_parsing = 0;
            }
            continue;
        }
        failed_parsing = 0;                     // парсинг удался - значит ответ от сервера пришел корректный - значит сессия Long Poll в норме
        
        ts = doc["ts"].as<uint32_t>();
        JsonArray updates = doc["updates"].as<JsonArray>();

        // Объект с данными ивента

        for (uint8_t upds = 0; upds < updates.size(); upds++) {         // если событий нет - цикл просто не вызывается и очередь не пополняется
            VKEvent event;
            
            JsonObject upd = updates[upds];
            strncpy(event.type, upd["type"], sizeof(event.type) - 1);    // message_new или message_event
            event.type[sizeof(event.type) - 1] = '\0';                  

            if (strcmp(event.type, "message_new") == 0) {
                // текстовое сообщение - вложенный объект
                strncpy(event.text, upd["object"]["message"]["text"], sizeof(event.text)-1);
                event.text[sizeof(event.text) - 1] = '\0';
                event.user_id = upd["object"]["message"]["from_id"];
            }

            else if (strcmp(event.type, "message_event") == 0) {
                // нажата кнопка - payload это строка
                serializeJson(upd["object"]["payload"], event.text, sizeof(event.text));
                event.user_id = upd["object"]["user_id"];
                event.peer_id = upd["object"]["peer_id"];
                strncpy(event.event_id, upd["object"]["event_id"], sizeof(event.event_id)-1);
                event.event_id[sizeof(event.event_id)-1] = '\0';
            }

            if (xQueueSend(vkEventQueue, &event, 0) != pdTRUE) ESP_LOGE("EVENT_QUEUE", "Ошибка отправки события в очередь! Возможно, она переполнена.");
        }
    }
}

// Long Polling запрос и возврат ответа от сервера
String VKPoll() {
    char url[400];
    snprintf(url, sizeof(url), "%s?act=a_check&key=%s&ts=%u&wait=25", server, key, ts);

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(30000);

    int return_code = http.GET();

    if (return_code > 0)    {
        String returned_body = http.getString();
        http.end();
        return returned_body;
    }
    http.end();
    return "Error, code: " + String(return_code);
}

// Строит JSON-клавиатуру в dst. Возвращает длину, или 0 — если не поместилось.
size_t buildKeyboardTo(char* dst, size_t dstSize) {
    size_t len = 0;
    if (!appendChecked(dst, dstSize, len, "{\"inline\":true,\"buttons\":[[")) return 0;

    for (uint8_t i = 0; i < 4; i++) {
        const char* icon = Relays[i] ? "✅" : "❌";
        if (!appendChecked(dst, dstSize, len,
                "{\"action\":{\"type\":\"callback\",\"label\":\"P%u%s\",\"payload\":\"{\\\"a\\\":\\\"switch_relay\\\",\\\"n\\\":%u}\"}}%s",
                i + 1, icon, i, (i < 3) ? "," : "")) {
            ESP_LOGE("VK_API", "[buildKeyboardTo:relay] Не хватило места в буфере [%u]", (unsigned)dstSize);
            return 0;
        }
    }

    if (!appendChecked(dst, dstSize, len, "],[")) return 0;

    for (uint8_t i = 0; i < 4; i++) {
        const char* mode = auto_mode[i] ? "авто" : "ручн";
        if (!appendChecked(dst, dstSize, len,
                "{\"action\":{\"type\":\"callback\",\"label\":\"%s\",\"payload\":\"{\\\"a\\\":\\\"switch_relay_mode\\\",\\\"n\\\":%u}\"}}%s",
                mode, i, (i < 3) ? "," : "")) {
            ESP_LOGE("VK_API", "[buildKeyboardTo:mode] Не хватило места в буфере [%u]", (unsigned)dstSize);
            return 0;
        }
    }

    if (!appendChecked(dst, dstSize, len, "]]}")) return 0;

    return len;
}


// функция для ответа snackbar-ом на нажатие кнопки
void VKAnswerCallback(VKEvent &event, String snackbar_text) {
    char payload[1536] = "";
    char eventData[256] = "";

    snprintf(eventData, sizeof(eventData), "{\"type\":\"show_snackbar\",\"text\":\"%s\"}", snackbar_text.c_str());
    size_t len = snprintf(payload, sizeof(payload), "event_id=");
    len += urlEncodeTo(event.event_id, payload + len, sizeof(payload) - len);
    len += snprintf(payload + len, sizeof(payload) - len, "&user_id=%d&peer_id=%d&event_data=", event.user_id, event.peer_id);

    char encoded_eventData[768] = "";       // худший сценарий: строка полностью из русских символов (x3 первоначального размера после кодирования)
    urlEncodeTo(eventData, encoded_eventData, sizeof(encoded_eventData));
    if (!appendChecked(payload, sizeof(payload) - VK_SUFFIX_RESERVE, len, "%s", encoded_eventData)) {
        ESP_LOGE("VK_API", "[VKAnswerCallback:event_data] Не хватило места в payload");
        return;
    }

    VKApiResult result = vkApiCall("messages.sendMessageEventAnswer", payload, sizeof(payload), true);
}

// функция редактирования сообщений
void VKEditMessage(const char* text) {
    if (!dashboardMsgID)    return;

    static char payload[2560] = "", urlencoded_keyboard[1600] = "";
    payload[0] = '\0';
    urlencoded_keyboard[0] = '\0';

    size_t keyboard_size = buildKeyboardTo(payload, sizeof(payload));

    if (!keyboard_size)     {
        ESP_LOGE("VK_API", "[VKEditMessage:keyboard_build] Не хватило места в буфере");
        ESP_LOGE("VK_API", "Размер keyboard: %ld", keyboard_size);
        return;
    }


    size_t keyboard_size_url = urlEncodeTo(payload, urlencoded_keyboard, sizeof(urlencoded_keyboard));
    if (keyboard_size_url >= sizeof(urlencoded_keyboard) - 4) {             // записалось под завязку
        ESP_LOGE("VK_API", "[VKEditMessage:keyboard_encode] Не хватило места в буфере");
        ESP_LOGE("VK_API", "Размер keyboard_url: %ld", keyboard_size_url);
        return;
    }

    payload[0] = '\0';                                              // очищаем перед повторным использованием

    size_t len = snprintf(payload, sizeof(payload), "peer_id=%s&message_id=%d&message=", VK_PEER_ID, dashboardMsgID);
    len += urlEncodeTo(text, payload + len, sizeof(payload) - VK_SUFFIX_RESERVE - keyboard_size_url - len);
    len += snprintf(payload + len, sizeof(payload) - len, "&keyboard=");
    if (!appendChecked(payload, sizeof(payload), len, "%s", urlencoded_keyboard)) {
        ESP_LOGE("VK_API", "[VKEditMessage:keyboard_append] Не хватило места в payload");
        return;
    }

    VKApiResult result = vkApiCall("messages.edit", payload, sizeof(payload), true);

    if (result.httpCode <= 0)   dashboardMsgID = 0;            // сетевая ошибка - сообщение точно нужно пересоздавать
    
    else if (!result.doc["error"].isNull()) {
        int error_code = result.doc["error"]["error_code"];
        if (error_code != 9)   dashboardMsgID = 0;             // 9 = flood control - не пересоздаём, просто подождём
    }
}


size_t buildDashboardTextTo(char* dst, size_t dstSize) {
    size_t len = 0;
    bool success = appendChecked(dst, dstSize, len, "Воздух: %.2f°C\nХолодная вода: %.2f°C\nТеплая вода: %.2f°C\nРазница: %.2f°C", temp[0], temp[1], temp[2], temp[2] - temp[1]);
    
    if (!success) {
        ESP_LOGE("VK_API", "[buildDashboardTextTo] Не хватило места в буфере");
        return 0;
    }
    
    return len;
}