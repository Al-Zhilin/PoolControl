void Pinging() {
    bool prev = internet;

    internet = Ping.ping("api.vk.com", 3);
    if (!internet)  internet = Ping.ping("www.yandex.ru", 5);

    if (prev != internet) {
        if (internet)   ESP_LOGD("PING_CHECK", "Связь восстановлена");
        else ESP_LOGE("PING_CHECK", "Связь потеряна");
    }
}
