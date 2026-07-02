// функция переключения соответствующего пин реле номера num(0-3) в состояние status(true/false)
void SwitchRelayPin(byte num, bool status) {
  if (num == 0) {
    digitalWrite(RELE1, !status);
  }
  else if (num == 1) {
    digitalWrite(RELE2, !status);
  }
  else if (num == 2) {
    digitalWrite(RELE3, !status);
  }
  else if (num == 3) {
    digitalWrite(RELE4, !status);
  }
}
