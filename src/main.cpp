// подключение библиотек
#include <Arduino.h>
#include <GyverDS18Array.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoOTA.h>
#include <ESP32Ping.h>
#include <time.h>
#include "Config.h"
#include "pass.h"


#define INIT_ADDR 0                                                                 //ячейка с ключом
#define INIT_KEY 52                                                                 //ключ для проведения инициальзации всех байтов EEPROM (0 - 255)
#define RELE1 27                                                                    //пин 1 реле
#define RELE2 26                                                                    //пин 2 реле
#define RELE3 25                                                                    //пин 3 реле
#define RELE4 33                                                                    //пин 4 реле
#define GET_PERIOD 1000 * 60 * 1.5                                                  //период отправки данных на сервер (всегда должен быть > 1 минуты)
#define UPD_PERIOD 7 * 1000                                                         //период между обновлениями температуры в чате телеграм
#define EE_WritePeriod 10 * 1000                                                    //выдержка перед записью данных в еепром (экономия ресурсов памяти), секунды
#define ON_PERIOD 5 * 60 * 1000                                                     //период выдержки перед новой попыткой запустить нагрев бассейна
#define OFF_PERIOD1 20 * 1000                                                       //период выдержки перед выключением нагрева бассейна (малый период работы)
#define OFF_PERIOD2 2 * 60 * 1000                                                   //период выдержки перед выключением нагрева бассейна (бОльший период работы)
#define RAZN_TO_OFF1 0.3                                                            //разница температур, при которой обогрев бассейна будет выключаться (малый период работы)
#define RAZN_TO_OFF2 1.8                                                            //разница температур, при которой обогрев бассейна будет выключаться (бОльший период работы)
#define POOL_PERIOD 1000                                                            //период между прогонами функции управления бассейном
#define PING_PERIOD 60 * 1000                                                       //период между пинг запросами
#define RECONNECT_PERIOD 2 * 60 * 1000                                              //период между попытками переподключения WiFi при его потере

// очередь между задачей, обрабатывающей LongPoll (ядро 0) и основным кодом (ядро 1)
QueueHandle_t vkEventQueue;

struct VKEvent {
    char type[16];          // тип: message_new / message_event
    char text[64];          // текст команды или payload кнопок
    int32_t user_id;
};

uint32_t startUnix;
int32_t mainMenuID = 0, TempID = 0, eerele_timer = 0, eeauto_timer = 0;
float temp[3] = {};
bool internet = true, auto_mode[] = {true, true, true, true}, Relays[] = {false, false, false, false}, eerele_flag = false, eeauto_flag = false;
byte fazaMenu = 0;

uint64_t addr[] = {
    0x760000006AB3CC28,     //1 датчик
    0x950000006B043C28,     //2 датчик
    0x4D0000006AB84428,     //3 датчик
};

GyverDS18Array ds(14, addr, 3);
//FastBot bot(bot_token);

// файлы проекта — после глобальных переменных, иначе функции не видят их
// Rele.h первым: его вызывают Pool.h и Sborka.h
#include "include/Rele.h"
#include "include/WiFiConnect.h"
#include "include/Pool.h"
#include "include/MenuBuild.h"
#include "include/FilesManager.h"
#include "include/TempReading.h"
#include "include/HTTPGET.h"
#include "include/PingCheck.h"
#include "include/VKBot.h"


// newMsg определён ниже setup(), поэтому нужен прототип
//void newMsg(FB_msg& msg);

void setup() {
  // --- Настройки OTA ---
  ArduinoOTA.setHostname(OTA_NAME);
  ArduinoOTA.setPassword(OTA_PASS);
  
  // --- WiFi и Serial для отладки ---
  ConnectWiFi();
  Serial.begin(115200);

  // --- Начинаем LongPolling с VK ---
  VKLongPollInit();

  // --- Настраиваем задачу с LongPoll на ядро 0 и осздаем очередь для ивентов ВК
  vkEventQueue = xQueueCreate(5, sizeof(VKEvent));              // 5 объектов структуры VKEvent
  xTaskCreatePinnedToCore(
    vkLongPollTask,
    "VKLongPoll",
    8192,
    NULL,
    1,
    NULL,
    0
  );

  // --- Синхронизация NTP с ожиданием или выходом по истечении попыток ---
  configTime(10800, 0, "pool.ntp.org");
  struct tm t;
  uint8_t wait_sync_retry = 0;
  Serial.print("Wait NTP sync ");
  while (!getLocalTime(&t) && wait_sync_retry < 10)   {         // 10 * 200 = 2000мс хватит, чтобы первая попытки синхронизации завершилась. Далее - автоматически реконнект под капотом
    delay(200);
    Serial.print(".");
    wait_sync_retry++;
  }
  if (wait_sync_retry == 10)    Serial.println("Sync attemps in over!");
  else Serial.println("Successfully NTP sync!");


  //EEPROM.begin(9);             //0 - 4096 байт

  //bot.setChatID(chat_id);
  /*bot.setPeriod(500);
  bot.setLimit(1);
  bot.clearServiceMessages(1);*/

  pinMode(RELE1, OUTPUT);
  pinMode(RELE2, OUTPUT);
  pinMode(RELE3, OUTPUT);
  pinMode(RELE4, OUTPUT);
  digitalWrite(RELE1, HIGH);
  digitalWrite(RELE2, HIGH);
  digitalWrite(RELE3, HIGH);
  digitalWrite(RELE4, HIGH);

  ds.requestTemp();

  /*.attach(newMsg);
  bot.unpinAll();
  bot.sendMessage("Здравствуйте!\nПосмотреть графики: " + String(OPEN_M_LINK));
  bot.sendMessage("скоро здесь будет температура");
  TempID = bot.lastBotMsg();
  startUnix = bot.getUnix();
  bot.pinMessage(bot.lastBotMsg());*/

  /*if (EEPROM.read(INIT_ADDR) != INIT_KEY) {
    EEPROM.put(INIT_ADDR, INIT_KEY);
    INIT_EEPROM();
    EEPROM.commit();
  }*/

  //START_EEPROM();
  ArduinoOTA.begin();
  // SborkaMenu(0);
  Pinging();

}

void loop() {
  static uint32_t pool_timer = millis(), ping_timer = millis(), reconnect_timer = millis();
  static bool int_res = !internet;
  //FB_Time t = bot.getTime(3);

  ArduinoOTA.handle();

  VKEvent event;
  if (xQueueReceive(vkEventQueue, &event, 0)) {
    // Обрабатываем пришедшие события
  }

  if (!internet)  {
    if (WiFi.status() != WL_CONNECTED && millis() - reconnect_timer >= RECONNECT_PERIOD) {
      WiFi.disconnect();
      ConnectWiFi();
      reconnect_timer = millis();
    }
  }

  if (int_res && internet)  {
      ESP.restart();
  }

  if (auto_mode[0] && millis() - pool_timer >= POOL_PERIOD) {
    pool_timer = millis();
    Pool();
  }

  if (millis() - ping_timer >= PING_PERIOD || (!internet && millis() - ping_timer >= 10 * 1000))  {
    ping_timer = millis();
    Pinging();
  }

  if (eerele_flag && millis() - eerele_timer >= EE_WritePeriod) {
    eerele_flag = false;
    //WriteEepromRele();
  }

  if (eeauto_flag && millis() - eeauto_timer >= EE_WritePeriod) {
    eeauto_flag = false;
    //WriteEepromAuto();
  }

  static uint32_t get_per = 0, upd_per = 0;
  //bot.tick();

  if (millis() - upd_per >= UPD_PERIOD) {
    upd_per = millis();
    //bot.editMessage(TempID,"Воздух: " + String(temp[0]) + "\nХолодная вода: " + String(temp[1]) + "\nТеплая вода: " + String(temp[2]) + "\nРазница: " + String(temp[2] - temp[1]));
  }

  if (millis() - get_per >= GET_PERIOD) {
    get_per = millis();
    http_get();
  }
  if (ds.ready()) {
   TempReading();      //0 - датчик темп. воздуха, 1 - холодной воды, 2 - теплой воды
  }
}
