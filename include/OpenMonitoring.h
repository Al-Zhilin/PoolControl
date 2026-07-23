void OpenMonitoringSend() {
    static bool get_failed = false;

    if (WiFi.status() == WL_CONNECTED) {

        String req = REG_START;
        req += temp[0];
        req += "&p2=";
        req += temp[1];
        req += "&p3=";
        req += temp[2];
        req += "&p6=";
        req += temp[2] - temp[1];

        HTTPClient http;

        http.begin(req);

        int result = http.GET();

        if (result <= 0) {
           if (!get_failed) ESP_LOGE("OPEN_MONITORING", "Ошибка отправки запроса, код ответа: %d", result);
           get_failed = true;
        }
        else {
            if (get_failed) {
                ESP_LOGD("OPEN_MONITORING", "Запрос отправлен успешно после предыдущей неудачи");
                get_failed = false;
            }
        }

        http.end();
    }
}


