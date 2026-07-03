void Pinging() {
  internet = Ping.ping("api.vk.com", 3);
  if (!internet)  internet = Ping.ping("www.yandex.ru", 5);
}
