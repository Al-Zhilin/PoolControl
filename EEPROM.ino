void INIT_EEPROM() {
  EEPROM.put(1, 0);   //Relays
  EEPROM.put(5, 1111);   //auto_mode

  //bot.sendMessage("EEPROM инициализирована!");
}

void START_EEPROM() {
  int baza = 0;
  EEPROM.get(1, baza);
  String str = "" + String(baza);

  while (str.length() < 4) {
    str = "0" + str;
  }
  
  Relays[0] = str[0] - '0';
  Relays[1] = str[1] - '0';
  Relays[2] = str[2] - '0';
  Relays[3] = str[3] - '0';

  baza = 0;
  EEPROM.get(5, baza);
  str = "" + String(baza);

  while (str.length() < 4) {
    str = "0" + str;
  }
  
  auto_mode[0] = str[0] - '0';
  auto_mode[1] = str[1] - '0';
  auto_mode[2] = str[2] - '0';
  auto_mode[3] = str[3] - '0';
}

void WriteEepromRele() {
  String str = "" + String(Relays[0]) + String(Relays[1]) + String(Relays[2]) + String(Relays[3]);
  EEPROM.put(1, str.toInt());
  EEPROM.commit();
  //bot.sendMessage("ИИПРОМ реле записаль!!");
}

void WriteEepromAuto() {
  String str = "" + String(auto_mode[0]) + String(auto_mode[1]) + String(auto_mode[2]) + String(auto_mode[3]);
  EEPROM.put(5, str.toInt());
  EEPROM.commit();
  //bot.sendMessage("ИИПРОМ авто записаль!!");
}
