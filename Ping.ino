void Pingyem() {
  internet = Ping.ping("www.yandex.ru", 3);
  if (!internet)  internet = Ping.ping("www.google.com", 5);
}
