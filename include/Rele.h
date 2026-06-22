void Rele(byte num, bool satatus) {
  if (num == 1) {
    digitalWrite(RELE1, !satatus);
  }
  else if (num == 2) {
    digitalWrite(RELE2, !satatus);
  }
  else if (num == 3) {
    digitalWrite(RELE3, !satatus);
  }
  else if (num == 4) {
    digitalWrite(RELE4, !satatus);
  }
}
