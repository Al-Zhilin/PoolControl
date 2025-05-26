void Pool() {
  static uint32_t try_timer = 0, timer_delay = 0;
  static bool first_on = true;
  static float koeff = 1.0;

  FB_Time t = bot.getTime(3);
  
  if (!internet || (t.hour >= 7 && (t.hour < 19 || (t.hour == 19 && t.minute <= 30)))) {        //подходящий прайм тайм или нет интернета    
    if (!Relays[0] && (millis() - timer_delay >= ON_PERIOD * koeff || first_on)) {
      first_on = false;
      try_timer = millis();
      timer_delay = millis();
      Relays[0] = true;
      Rele(1, Relays[0]);
    }

    if (Relays[0]) {
      if (temp[2] - temp[1] <= RAZN_TO_OFF1 && millis() - try_timer >= OFF_PERIOD1) {
        if (first_on) first_on = false;
        koeff = ChechKoeff();
        timer_delay = millis();
        try_timer = millis();
        Relays[0] = false;
        Rele(1, Relays[0]);
      }

      if (temp[2] - temp[1] <= RAZN_TO_OFF2 && millis() - try_timer >= OFF_PERIOD2) {
        if (first_on) first_on = false;
        koeff = 1.0;
        timer_delay = millis();
        try_timer = millis();
        Relays[0] = false;
        Rele(1, Relays[0]);
      }
    }
  }

  else if (Relays[0]) {
    Relays[0] = false;
    Rele(1, Relays[0]);
  }
}

float ChechKoeff() {
  float razn = temp[2] - temp[1];

  if (razn > -2)  return 1.0;
  else if (razn <= -2 && razn > -4)  return 1.5;
  else if (razn <= -4 && razn > -7)  return 2.0;
  else if (razn <= -7)  return 3.0;

  return 1.0;
}
