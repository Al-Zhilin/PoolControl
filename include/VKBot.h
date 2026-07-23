#include <ArduinoJson.h>

String server = "", key = "";
uint32_t ts = 0;
int32_t dashboardMsgID = 0;

String VKPoll();

// Структура ответа функции, работающей с API
struct VKApiResult {
    bool ok;                        // true, если запрос успешен и API не вернул ошибку
    int httpCode;                   // код от http
    int vkErrorCode = 0;            // код ошибки от ВК, если был
    String vkErrorMsg = "";         // сообщение ошибки от ВК, если был
    JsonDocument doc;               // распарсенное тело ответа
};

// Функция - обертка над API запросами
VKApiResult vkApiCall(const String& method, String payload, bool isPost) {
    VKApiResult result;
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.setTimeout(10000);

    // добавляем универсальную информацию к запросу
    payload += "&access_token=";
    payload += VK_TOKEN;
    payload += "&v=5.199";

    DeserializationError err;

    for (uint8_t request_try = 0; request_try < 3; request_try++) {
        String returned_body;

        if (isPost) {
            http.begin(client, "https://api.vk.com/method/" + method);
            http.addHeader("Content-Type", "application/x-www-form-urlencoded");
            result.httpCode = http.POST(payload);
        }

        else {
            http.begin(client, "https://api.vk.com/method/" + method + "?" + payload);
            result.httpCode = http.GET();
        }

        returned_body = http.getString();
        http.end();
        err = deserializeJson(result.doc, returned_body);

        if (err || result.httpCode <= 0) {                // при сетевой ошибке пытаемся отправить запрос еще 2 раза
            result.ok = false;                            // ошибку десериализации тоже считаем обрывом связи
            ESP_LOGE("VK_API", "Метод %s: сетевая ошибка, http_code=%d", method.c_str(), result.httpCode);
            delay(200);
        }
        else break;
    }

    if (!err && !result.doc["error"].isNull())   {        // если есть ошибки
        result.vkErrorCode = result.doc["error"]["error_code"].as<int>();
        result.vkErrorMsg = result.doc["error"]["error_msg"].as<String>();
        result.ok = false;
        ESP_LOGE("VK_API", "Метод %s: VK вернул ошибку %d (%s)", method.c_str(), result.vkErrorCode, result.vkErrorMsg.c_str());
    }

    else if (result.httpCode > 0) result.ok = true;

    return result;
}

// функция URLencode`ирования сообщения
String urlEncode(String str) {
    String encoded = "";
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded += c;
        } else {
            char buf[4];
            snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)c);
            encoded += buf;
        }
    }
    return encoded;
}

// Отправка простого текстового сообщения
VKApiResult VKSendMessage(String data) {
    
    // --- Собираем тело запроса ---
    String payload = "peer_id=";
    payload += VK_PEER_ID;
    payload += "&message=";
    payload += urlEncode(data);
    payload += "&random_id=";
    payload += String(esp_random() & 0x7FFFFFFF);

    // --- Обращение к API и возврат результата ---
    return vkApiCall("messages.send", payload, true);
}

// инициализация Long Poll соединения
bool VKLongPollInit() {
    String payload = "group_id=";
    payload += VK_GROUP_ID;

    VKApiResult result = vkApiCall("groups.getLongPollServer", payload, false);

    if (result.ok)    {
            server = result.doc["response"]["server"].as<String>();
            key = result.doc["response"]["key"].as<String>();
            ts = result.doc["response"]["ts"].as<uint32_t>();
            if (server != "") return true;

        return false;
    }
    return false;
}

// работа с LongPoll
void vkLongPollTask(void* params) {
    static uint8_t failed_parsing = 0;  // сколько подряд ошибок поддержания Long Poll сессии произошло

    while (true) {
        if (server == "") {              // инициализация не была запущена ИЛИ мы самостоятельно его инвалидировали при ошибках соединения
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
                    server = "";            // для 2 и 3 случаев нужна инициализация, для этого сбрасываем сервер
                    break;
            }
            continue;
        }

        if (err)    {
            vTaskDelay(pdMS_TO_TICKS(3000));
            if (++failed_parsing >= 3)  {       // ошибок больше допустимого кол-ва - инвалидируем ранее инициализироанную сессию и прогоняем Init заново
                server = "";
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
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, server  + "?act=a_check&key=" + key + "&ts=" + ts + "&wait=25");
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

// функция построения JSON клавиатуры из текущего состояния Relays[] и auto_mode[]
String buildKeyboard() {
    String keyboard = "{\"inline\":true,\"buttons\":[[";

    // Собираем кнопки управления Реле
    for (uint8_t i = 0; i < 4; i++) {
        String label = "P" + String(i+1) + "";
        label += Relays[i] ? "✅" : "❌";
        //label += "]";
        String payload = "{\\\"a\\\":\\\"switch_relay\\\",\\\"n\\\":" + String(i) + "}";

        keyboard += "{\"action\":{\"type\":\"callback\",\"label\":\"";
        keyboard += label;
        keyboard += "\",\"payload\":\"";
        keyboard += payload;
        keyboard += "\"}}";

        if (i < 3) keyboard += ",";
    }

        keyboard += "],[";

    // Кнопки переключения режимов работы Реле
    for (uint8_t i = 0; i < 4; i++) {
        String label = auto_mode[i] ? "авто" : "ручн";
        String payload = "{\\\"a\\\":\\\"switch_relay_mode\\\",\\\"n\\\":" + String(i) + "}";
        keyboard += "{\"action\":{\"type\":\"callback\",\"label\":\"";
        keyboard += label;
        keyboard += "\",\"payload\":\"";
        keyboard += payload;
        keyboard += "\"}}";

        if (i < 3) keyboard += ",";
    }

    keyboard += "]]}";
    return keyboard;
}

// функция для ответа snackbar-ом на нажатие кнопки
void VKAnswerCallback(VKEvent &event, String snackbar_text) {

    String payload = "event_id=", eventData = "{\"type\":\"show_snackbar\",\"text\":\"" + snackbar_text + "\"}";
    payload += event.event_id;
    payload += "&user_id=";
    payload += event.user_id;
    payload += "&peer_id=";
    payload += event.peer_id;
    payload += "&event_data=";
    payload += urlEncode(eventData);

    VKApiResult result = vkApiCall("messages.sendMessageEventAnswer", payload, true);

    // Возможно логика ретраев?
}

// функция редактирования сообщений
void VKEditMessage(String text) {
    if (!dashboardMsgID) return;                // если переменная не содержат корректного ID - нет смысла пытаться редактировать

    String payload = "peer_id=";
    payload += VK_PEER_ID;
    payload += "&message_id=";
    payload += dashboardMsgID;
    payload += "&message=";
    payload += urlEncode(text);
    payload += "&keyboard=";
    payload += urlEncode(buildKeyboard());

    VKApiResult result = vkApiCall("messages.edit", payload, true);

    if (result.httpCode <= 0)   dashboardMsgID = 0;            // сетевая ошибка - сообщение точно нужно пересоздавать
    
    else if (!result.doc["error"].isNull()) {
        int error_code = result.doc["error"]["error_code"];
        if (error_code != 9)   dashboardMsgID = 0;             // 9 = flood control - не пересоздаём, просто подождём
    }
}

String buildDashboardText() {
    String text = "";

    text += "Воздух: ";
    text += String(temp[0], 2);
    text += "°C\n";

    text += "Холодная вода: ";
    text += String(temp[1], 2);
    text += "°C\n";

    text += "Теплая вода: ";
    text += String(temp[2], 2);
    text += "°C\n";

    text += "Разница: ";
    text += String(temp[2] - temp[1], 2);
    text += "°C\n\n";

    // статусы Реле в дашборде - не нужны, т.к. чуть ниже есть кнопки, с такой же информативностью
    /*for (uint8_t i = 0; i < 4; i++) {
        text += "Реле ";
        text += i+1;
        text += ": ";
        text += Relays[i] ? "✅" : "❌";
        text += " | ";
        text += auto_mode[i] ? "авто" : "ручной";
        text += "\n";
    }*/

    return text;
}