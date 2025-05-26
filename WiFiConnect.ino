void ConnectWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  uint32_t start_time = millis();
  
  while (WiFi.status() != WL_CONNECTED && millis() - start_time < 45000) {        
    delay(1000);
  }

}
