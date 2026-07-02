void SborkaMenu(byte is_eeprom) {
  String mainMenu = F("");

  switch (fazaMenu) {
    case 0:   //релюшки, обновление состояний реле и вызов настроек
      for (byte number = 0; number < 4; number++) {
        mainMenu += "Реле";
        mainMenu = mainMenu + String(number + 1);

        mainMenu += " [";
        if (auto_mode[number])  mainMenu += "?";
        if (Relays[number])  mainMenu += "✅]";
        else  mainMenu += "❌]";

        if (number == 1) {
          mainMenu += "\n";
        }

        else {
          mainMenu += "\t";
        }
        SwitchRelayPin(number, Relays[number]);
      }
      mainMenu += "\t Обновить \n Настройки";
      break;

    case 1:   //auto mode и кнопка на главную
      for (byte number = 0; number < 4; number++) {
        mainMenu += "P";
        mainMenu = mainMenu + String(number + 1);

        if (auto_mode[number])  mainMenu += "(автом.)";
        else  mainMenu += " (ручн.)";

        if (number == 1) {
          mainMenu += "\n";
        }

        else {
          mainMenu += "\t";
        }
        SwitchRelayPin(number, Relays[number]);
      }
      mainMenu += "\t";
      mainMenu += "На главную";
      break;
  }

  if (is_eeprom == 1) {
    eerele_flag = true;
    eerele_timer = millis();
  }

  if (is_eeprom == 2) {
    eeauto_flag = true;
    eeauto_timer = millis();
  }

  //bot.showMenuText("Меню ------------------------------> ⬇️", mainMenu);
  //if (mainMenuID != 0)  bot.deleteMessage(mainMenuID);
  //mainMenuID = bot.lastBotMsg();
}
