
void TempReading() {
  static bool error_flag[3] = {true, true, true};
  float new_znach[3] = {0, 0, 0};
  for (int i = 0; i < 3; i++) {
      if (!ds.readTemp(i)) {
          if (error_flag[i]) {
              switch (i) {
                  case 0:
                      bot.sendMessage("Ошибка чтения датчика температуры воздуха!");
                      break;
                  case 1:
                      bot.sendMessage("Ошибка чтения датчика температуры входной (холодной) воды!");
                      break;
                  case 2:
                      bot.sendMessage("Ошибка чтения датчика температуры выходной (теплой) воды!");
                      break;
              }
              error_flag[i] = false;
          }
          new_znach[i] = 0;
      }
      else {
          new_znach[i] = ds.getTemp();
      }
  }
  ds.requestTemp();
  Filtration(new_znach);
}


void Filtration(float new_znach[3]) {
  new_znach[0] -= 0.1;
  new_znach[1] += 0.19;
  
  for (int i = 0; i < 3; i++) {

    if (!Relays[0] && i > 0 && temp[i] != 0) break;
    
    if (new_znach[i] == 0) {
      temp[i] = 0;
      continue;
    }
      
    float k;
    if (abs(new_znach[i] - temp[i]) > 0.7) k = 1.0;
    else k = 0.7;
  
    temp[i] += (new_znach[i] - temp[i]) * k;
  }
}
