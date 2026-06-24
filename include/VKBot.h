#include <ArduinoJson.h>

String server = "", key = "";
uint32_t ts = 0;

String VKPoll();

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
String VKSendMessage(String data) {
    WiFiClientSecure client;                // для защищенного https
    client.setInsecure();                   // отключаем проверку сертификата

    HTTPClient http;
    http.begin(client, "https://api.vk.com/method/messages.send");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    String payload = "peer_id=";
    payload += VK_PEER_ID;
    payload += "&message=";
    payload += data;
    payload += "&random_id=";
    payload += String(esp_random() & 0x7FFFFFFF);
    payload += "&access_token=";
    payload += VK_TOKEN;
    payload += "&v=5.199";

    int return_code = http.POST(payload);
    String returned_body = "";

    if (return_code > 0)    returned_body = http.getString();

    http.end();
    return returned_body;
}

// инициализация Long Poll соединения
bool VKLongPollInit() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, "https://api.vk.com/method/groups.getLongPollServer?group_id=" + String(VK_GROUP_ID) + "&access_token=" + VK_TOKEN + "&v=5.199");

    int return_code = http.GET();
    String returned_body = "";

    if (return_code > 0)    {
        returned_body = http.getString();
        http.end();

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, returned_body);

        if (!err) {
            server = doc["response"]["server"].as<String>();
            key = doc["response"]["key"].as<String>();
            ts = doc["response"]["ts"].as<uint32_t>();
            return true;
        }

        return false;
    }
    http.end();
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
                strncpy(event.text, upd["object"]["payload"], sizeof(event.text)-1);
                event.text[sizeof(event.text) - 1] = '\0';
                event.user_id = upd["object"]["user_id"];
                event.peer_id = upd["object"]["peer_id"];
                strncpy(event.event_id, upd["object"]["event_id"], sizeof(event.event_id)-1);
                event.event_id[sizeof(event.event_id)-1] = '\0';
            }

            xQueueSend(vkEventQueue, &event, 0);
        }
    }
}

String VKPoll() {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, server  + "?act=a_check&key=" + key + "&ts=" + ts + "&wait=25");

    int return_code = http.GET();

    if (return_code > 0)    {
        String returned_body = http.getString();
        http.end();
        return returned_body;
    }
    http.end();
    return "Error, code: " + String(return_code);
}