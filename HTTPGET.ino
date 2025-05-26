void http_get() {
    static bool get_failed = false, fail_flag = true;
    
    if (WiFi.status() == WL_CONNECTED) {
      
        String req = REG_START; 
        req += temp[0];
        req += "&p2=";
        req += temp[1];
        req += "&p3=";
        req += temp[2];
        req += "&p6=";
        req += temp[2] - temp[1];
        
        HTTPClient http;        // создаем объект для работы с HTTP
    
        http.begin(req);     // подключаемся к веб-странице
    
        int result = http.GET();      // делаем GET запрос
    
        if (result <= 0) {
           if (get_failed)  {
              if (fail_flag) {
                //bot.sendMessage("Ошибка HTTP-запроса");  
                fail_flag = false;
              }
           }
           get_failed = true;
        }
        else {
          fail_flag = true;
          get_failed = false;
        }
        
        http.end();     // освобождаем ресурсы микроконтроллера
    }
}
