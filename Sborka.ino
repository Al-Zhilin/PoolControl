
void SborkaMenu(byte is_eeprom) {
  String mainMenu = F("");

  switch (fazaMenu) {
    case 0:   //релюшки, обновление состояний реле и вызов настроек
      for (byte nomer = 0; nomer < 4; nomer++) {
        mainMenu += "Реле";
        mainMenu = mainMenu + String(nomer + 1);

        mainMenu += " [";
        if (auto_mode[nomer])  mainMenu += "?";
        if (Relays[nomer])  mainMenu += "✅]";
        else  mainMenu += "❌]";

        if (nomer == 1) {
          mainMenu += "\n";
        }

        else {
          mainMenu += "\t";
        }
        Rele(nomer+1, Relays[nomer]);
      }
      mainMenu += "\t Обновить \n Настройки";
      break;

    case 1:   //auto mode и кнопка на главную
      for (byte nomer = 0; nomer < 4; nomer++) {
        mainMenu += "P";
        mainMenu = mainMenu + String(nomer + 1);

        if (auto_mode[nomer])  mainMenu += "(автом.)";
        else  mainMenu += " (ручн.)";

        if (nomer == 1) {
          mainMenu += "\n";
        }

        else {
          mainMenu += "\t";
        }
        Rele(nomer+1, Relays[nomer]);
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
  
  bot.showMenuText("Меню ------------------------------> ⬇️", mainMenu);
  if (mainMenuID != 0)  bot.deleteMessage(mainMenuID);
  mainMenuID = bot.lastBotMsg();
}
