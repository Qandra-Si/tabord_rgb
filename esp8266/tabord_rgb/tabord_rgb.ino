// технические сведения о Wi-Fi модуле см. здесь: https://amperka.ru/product/troyka-wi-fi

#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Ticker.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <NTPClient.h> // Look NTPClient by Fabrice Weinberg (Sketch > Include Library > Manage Libraries...)
#include <WiFiUdp.h> // for esp8266

#include "options.h"


// серийный номер изделия (он же номер игрока, которому он достался)
#define HTTP_USER_AGENT    "TabordRGB/" TABORD_RGB_VER " (esp8266)"

// ВНИМАНИЕ! СЛЕДУЮЩИЕ ЗНАЧЕНИЯ ДОЛЖНЫ БЫТЬ ЦЕЛЫМИ ЧИСЛАМИ И ДЕЛИТЬСЯ БЕЗ ОСТАТКОВ !!!!!
// max количество последовательно отображаемых итогов сражений
#define MAX_KILLMAILS      10
// интервал (сек) обращения на web-сервер за обновлениями killmails
#define UPLOAD_INTERVAL    60
// интервал (ms) fading-цикла для одного из killmail-а (является частным от UPLOAD_INTERVAL/MAX_KILLMAILS)
#define FADING_INTERVAL    6000
// интервал смены частоты ШИМ на выходе транзисторной сборки (не слишком большой и не слишком маленький)
#define PWM_INTERVAL       50
// число нарастаний и затуханий свечения индикаторов за один fading-цикл (является частным от FADING_INTERVAL/PWM_INTERVAL)
#define PWM_FADE_TIMES     120
#define PWM_FADE_HALFTIMES (PWM_FADE_TIMES / 2)

// аппаратно-зависимые настройки (свободные ноги, ёмкость eeprom и т.п.)
// пины подключения RGB через транзисторную сборку
#define EEPROM_SIZE       512
#define LED_BLUE_PIN       14
#define LED_GREEN_PIN      12
#define LED_RED_PIN        13
#define WPS_BUTTON_PIN      4 // на самом деле у Troyka Wifi модуля шелкография помечена 5й ногой (до ревизии №8 включительно)

// макрос для кодирования состояний машины состояний
// r,g,b - соответветственно цвета со значениями от 0x0 до 0xf
// f     - признак мерцания, 0 или 1
#define FSM_STATE(r,g,b,f)   (unsigned int)(((r&0xf)<<8)|((g&0xf)<<4)|(b&0xf)|(f?0x1000:0))
#define FSM_STATE_RED(s)     (unsigned char)(((s&0xf00)>>4)*17)
#define FSM_STATE_GREEN(s)   (unsigned char)((s&0x0f0)*17)
#define FSM_STATE_BLUE(s)    (unsigned char)(((s&0x00f)<<4)*17)
// состояния модуля
enum state_t
{
  sWIFI_INACTIVE    = FSM_STATE(0xf,0xf,0xf,0), // белый, НЕ мерцающий
  sWPS_MODE_ACTIVE  = FSM_STATE(0x0,0x0,0xf,0), // синий, НЕ мерцающий
  sAP_MODE_ACTIVE   = FSM_STATE(0xf,0xe,0x0,1), // жёлтый, мерцающий
  sWIFI_ACTIVE      = FSM_STATE(0x0,0xf,0x0,0), // зелёный, НЕ мерцающий
  sWAITING_NTP      = FSM_STATE(0x2,0xe,0x4,0), // оливково-зелёный, НЕ мерцающий (не требуется иной подсветки относительно sWIFI_ACTIVE, но нужен другой числовой fsm-код)
  sINET_AVAILABLE   = FSM_STATE(0x0,0xe,0x0,0), // зелёный, НЕ мерцающий (не требуется иной подсветки относительно sWIFI_ACTIVE, но нужен другой числовой fsm-код)
  sAP_MODE_FINISHED = FSM_STATE(0x0,0x0,0x0,1), // нет свечений, мерцающий (исключительная ситуация для перезагрузки)
};

// глобальные параметры
const char       *g_cfg_access_point = "tabord_rgb";
// данные из EEPROM
byte              g_eeprom_data[EEPROM_SIZE];
const char       *g_ssid = NULL;
const char       *g_pswd = NULL;
// прерывания для управления светодиодами
Ticker            g_leds_control;
// машина состояний (finite state machine) для управления алгоритмом модуля и его индикацией
volatile state_t  g_fsm_active_state;
volatile state_t  g_fsm_prev_state;
volatile bool     g_setup_finished = false; // признак того, что работа метода setup завершена
volatile bool     g_need_reboot = false; // признак того, что требуется перезагрузка платы
volatile bool     g_ntp_client_started = false; // признак того, что ntp-клиент запущен
unsigned short    g_timeout_to_load_data = 0;
// локальный вер-сервер для настройки содержимого EEPROM с помощью браузера
// используется только в режиме локальной точки доступа (sAP_MODE_ACTIVE)
ESP8266WebServer  g_web_server(80);
// данные с результатами боевых действий (killmails) для отображения и индикации 
unsigned int      g_killmails[MAX_KILLMAILS];
unsigned int      g_killmails_stored = 0;
unsigned int      g_killmails_current = 0;
unsigned int      g_killmails_fading = 0;
// ntp-клиент, который получает текущую дату-время, необходимую для формирования запросов
// непосредственно на zkillboard, где 0 - это смещение в секундах относительно UTC (т.е. Eve Time)
WiFiUDP           g_ntp_udp;
NTPClient         g_time_client(g_ntp_udp, "pool.ntp.org", 0);
unsigned int      g_time_client_step;


// ---------------------------
// forward method declarations
void readEEPROM();
void parseEEPROM();
void renewEEPROM(const char * ssid, const char * pswd);
void clearEEPROM();
void handleWPSAPBtnPressed();
bool startWPS();
bool startAP();
void resetTabordFSM();
void changeTabordState(state_t state, int line);
void ledsControl();
void handleWebRoot();
void handleWebNotFound();
void handleWebCommit();
void handleWebReset();
void requestURI();
// ---------------------------


void setup()
{
  // пины в режим выхода
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  // настройка отладочного выхода
  Serial.begin(115200);

  // настройка машины состояний для сигнализации процесса настройки и работы
  resetTabordFSM();
  g_leds_control.attach_ms(PWM_INTERVAL, ledsControl); // 50ms => 120 раз в 6 сек меняется частота ШИМ, т.о. fading-цикл состоит из 60 шагов нарастания и 60 шагов затухания
  // настройка прочих необходимых ресурсов
  pinMode(LED_BUILTIN, OUTPUT);
  // настройка работы с EEPROM
  EEPROM.begin(EEPROM_SIZE);

  digitalWrite(LED_BUILTIN, LOW);   // Turn the LED on (Note that LOW is the voltage level)
                                    // but actually the LED is on; this is because
                                    // it is active low on the ESP-01)

  Serial.println("\nReading SSID and password from EEPROM ...");
  readEEPROM();
  parseEEPROM();
  if (g_ssid && g_pswd)
  {
    Serial.printf("EEPROM read. SSID '%s' and password '%s'\n", g_ssid, g_pswd);
    WiFi.begin(g_ssid, g_pswd);
  }
  else // if (g_ssid == null || g_pswd == null)
  {
    Serial.println("EEPROM empty or corrupted");
    if (startWPS())
    {
      parseEEPROM();
      if (g_ssid && g_pswd)
      {
        Serial.printf("WPS finished sucessfully. SSID '%s' and password '%s'\n", g_ssid, g_pswd);
      }
    }
  }

  g_setup_finished = true;
  digitalWrite(LED_BUILTIN, HIGH);  // Turn the LED off by making the voltage HIGH
}

void readEEPROM()
{
  for (int i = 0; i < EEPROM_SIZE; ++i)
    g_eeprom_data[i] = EEPROM.read(i);
}

void parseEEPROM()
{
  // max lengths | 0 ... 31 | '\0' | 33 .. 96 | '\0' |
  g_ssid = NULL;
  g_pswd = NULL;
  if (g_eeprom_data[0] == '\0') return;
  for (int i = 1; i <= 32; ++i) // max ssid len = 32
  {
    if (g_eeprom_data[i] == '\0')
    {
      if (i == 1)
      {
        Serial.println("ERROR! SSID corrupted");
        break;
      }
      g_ssid = (const char*)g_eeprom_data;
      Serial.printf("SSID '%s' found\n", g_ssid);
      for (int j = i + 1; j < i + 64; ++j) // max pswd len = 64
      {
        if (g_eeprom_data[j] == '\0')
        {
          if (j == (i + 1))
          {
            Serial.println("ERROR! Password corrupted");
            break;
          }
          g_pswd = (const char*)&g_eeprom_data[i+1];
          Serial.printf("Password '%s' found\n", g_pswd);
          break;
        }
      }
      break;
    }
  }
}

void renewEEPROM(const char * ssid, const char * pswd)
{
  int len_ssid = strlen(ssid);
  int len_pswd = strlen(pswd);
  if (len_ssid && len_pswd && (len_ssid <= 32) && (len_pswd <= 64))
  {
    strcpy((char*)g_eeprom_data, ssid);
    g_eeprom_data[len_ssid] = '\0';
    strcpy((char*)&g_eeprom_data[len_ssid+1], pswd);
    g_eeprom_data[len_ssid+1+len_pswd] = '\0';

    for (int i = 0, cnt = len_ssid+1+len_pswd+1; i < cnt; ++i)
      EEPROM.write(i, g_eeprom_data[i]);
    if (EEPROM.commit())
      Serial.println("EEPROM successfully committed");
    else
      Serial.println("ERROR! EEPROM commit failed");
  }
}

void clearEEPROM()
{
  for (int i = 0; i < EEPROM_SIZE; ++i)
    EEPROM.write(i, 0);
  if (EEPROM.commit())
    Serial.println("EEPROM successfully reset");
  else
    Serial.println("ERROR! EEPROM reset failed");
}

void handleWPSAPBtnPressed()
{
  // если обнаруживаем, что кнопка WPS/AP нажата, то проверяем что она удерживается 2сек непрерывно
  int cnt = 0;
  do
  {
    delay(300);
    if (!digitalRead(WPS_BUTTON_PIN)) break;
    cnt++;
    Serial.print(".");
    if (cnt == 7) changeTabordState(sWPS_MODE_ACTIVE, __LINE__); // спустя 2.1 сек включем синюю подсветку режима WPS
    else if (cnt == 15) changeTabordState(sAP_MODE_ACTIVE, __LINE__); // спустя 4.5 сек включем жёлтую подсветку режима STA
  } while (cnt < 20);
  if (cnt < 7)
    Serial.println(" and aborted");
  else if (cnt < 15)
  {
    Serial.println(" and WPS activated");
    // бесконечно пытаемся найти точку доступа
    while (1)
    {
      if (startWPS())
      {
        parseEEPROM();
        if (g_ssid && g_pswd)
        {
          Serial.printf("WPS finished sucessfully. SSID '%s' and password '%s'\n", g_ssid, g_pswd);
          break;
        }
      }
    }
  }
  else // if (cnt >= 15)
  {
    Serial.println(" and AP activated");
    if (!startAP())
    {
      // если не удалось настроить локальную точку доступа, то "программно перезагрузим плату",
      // т.к. из-за подвисания loop контроллер перезапустится
      g_need_reboot = true;
    }
    //перенесено из main:return;
  }
}

// see https://github.com/esp8266/Arduino/issues/1958#issue-150097589
bool startWPS()
{
  changeTabordState(sWPS_MODE_ACTIVE, __LINE__);
  Serial.print("Press WPS button on your router ...");

  // подготавливаемся к переходу в sta режим
  for (int i = 0; i < 10; ++i) { delay(500); Serial.print("."); } // задержка 5 секунд
  // WPS works in STA (Station mode) only -> not working in WIFI_AP_STA !!! 
  WiFi.mode(WIFI_STA);
  for (int i = 0; i < 2; ++i) { delay(500); Serial.print("."); } // задержка 1 секунда
  WiFi.begin(g_cfg_access_point,""); // make a failed connection
  while (WiFi.status() == WL_DISCONNECTED) { delay(500); Serial.print("."); } // задержка, пока не выключится wifi sta
  Serial.print("\n");

  // начинаем процедуру WPS-конфигурирования
  bool wpsSuccess = WiFi.beginWPSConfig();
  if (wpsSuccess)
  {
      // Well this means not always success :-/ in case of a timeout we have an empty ssid
      String newSSID = WiFi.SSID();
      if (newSSID.length() > 0)
      {
        // WPSConfig has already connected in STA mode successfully to the new station. 
        Serial.printf("WPS finished. Connected successfull to SSID '%s' with '%s'", newSSID.c_str(), WiFi.psk().c_str());
        // save to config and use next time or just use - WiFi.begin(WiFi.SSID().c_str(),WiFi.psk().c_str());
        renewEEPROM(newSSID.c_str(), WiFi.psk().c_str());
      }
      else
      {
        Serial.println("Unable to configure in WPS mode");
        wpsSuccess = false;
      }
  }
  return wpsSuccess; 
}

bool startAP()
{
  changeTabordState(sAP_MODE_ACTIVE, __LINE__);
  Serial.printf("Configuring open access point SSID '%s' ... ", g_cfg_access_point);

  WiFi.mode(WIFI_AP);
  bool ap_ready = WiFi.softAP(g_cfg_access_point,"");
  Serial.println(ap_ready ? "ready" : "failed!");
  if (!ap_ready) return false;
  
  IPAddress apip = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(apip);

  g_web_server.on("/", handleWebRoot);
  g_web_server.on("/commit", handleWebCommit);
  g_web_server.on("/reset", handleWebReset);
  g_web_server.onNotFound(handleWebNotFound);
  g_web_server.begin();
  Serial.println("HTTP server started");

  return true;
}

void resetTabordFSM()
{
  // чёрный цвет не используется в машине состояний, т.ч. он будет заменён при первом же запуске таймера
  //зануляется в начале работы: g_fsm_prev_state = (state_t)0xfffff000;

  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
}

void changeTabordState(state_t state, int line)
{
  Serial.printf("FSM state changed:%d %08x -> %08x\n", line, (int)g_fsm_active_state, (int)state);
  g_fsm_active_state = state;
}

void ledsControl()
{
  // переходы по машине состояний - это системные воздействия (нажатия на тактовые кнопки,
  // сбросы устройства, его перезагрузки, потеря Wi-Fi сигнала и соединения с интернетом)
  // переход в состемное (технологическое) состояние происходит мгновенно, без ожидания
  // когда завершится fading-цикл
  if (g_fsm_prev_state != g_fsm_active_state)
  {
    // требуется произвести замену цвета
    analogWrite(LED_RED_PIN, FSM_STATE_RED(g_fsm_active_state));
    analogWrite(LED_GREEN_PIN, FSM_STATE_GREEN(g_fsm_active_state));
    analogWrite(LED_BLUE_PIN, FSM_STATE_BLUE(g_fsm_active_state));
    g_fsm_prev_state = g_fsm_active_state;
    //Serial.printf("debug 4: %02x %02x %02x\n", (int)FSM_STATE_RED(g_fsm_active_state), (int)FSM_STATE_GREEN(g_fsm_active_state), (int)FSM_STATE_BLUE(g_fsm_active_state));
    g_killmails_fading = 0;
    g_killmails_current = 0;
    return;
  }

  // если СИСТЕМНЫЕ состояния не меняются, и модуль находится в режиме sINET_AVAILABLE, то это
  // означает что он находится в "боевом" состоянии и готов к отображению результатов боёв
  // ...осталось только дождаться когда они подгрузятся
  if (sINET_AVAILABLE == g_fsm_active_state)
  {
    if (g_killmails_stored == 0) return;
    // всякая смена цвета выполняется только когда завершён fading-цикл
    // метод вызывается каждые 50ms, таким образом 120 раз в 6 сек меняется частота ШИМ,
    // т.о. fading-цикл состоит из 60 шагов нарастания и 60 шагов затухания
    g_killmails_fading++;
    if (g_killmails_fading == 1) // первый шаг fading-цикла
    {
      // поправка (с сервера внезапно пришло меньше данных, чем приходило ранее? ...раки!)
      if (g_killmails_current > g_killmails_stored) g_killmails_current = 0;
      // вычисляем цветовую составляющую
      const unsigned int rgb = g_killmails[g_killmails_current];
      const unsigned char r = (unsigned char)((rgb & 0x00ff0000) >> 16);
      const unsigned char g = (unsigned char)((rgb & 0x0000ff00) >> 8);
      const unsigned char b = (unsigned char)rgb;
      // расчёт текущей мощности сечения
      const float power = (float)1.0 / (float)PWM_FADE_HALFTIMES;
      // некоторые индикаторы будут отключены на весь fading-цикл, если их цвета = 0
      if (r == 0) digitalWrite(LED_RED_PIN, LOW);
      else        analogWrite(LED_RED_PIN, power * r);
      if (g == 0) digitalWrite(LED_GREEN_PIN, LOW);
      else        analogWrite(LED_GREEN_PIN, power * g);
      if (b == 0) digitalWrite(LED_BLUE_PIN, LOW);
      else        analogWrite(LED_BLUE_PIN, power * b);
    }
    else if (g_killmails_fading == PWM_FADE_TIMES) // последний шаг fading-цикла
    {
      // выкл. всех каналов
      digitalWrite(LED_RED_PIN, LOW);
      digitalWrite(LED_GREEN_PIN, LOW);
      digitalWrite(LED_BLUE_PIN, LOW);
      // поправка (с сервера внезапно пришло 0 данных, хотя раньше было не 0? ...раки!)
      if (g_killmails_stored == 0)
        g_killmails_current = 0;
      // переход к следующему killmail-у
      else
        g_killmails_current = (g_killmails_current + 1) % g_killmails_stored;
      // перезапуск fading-цикла
      g_killmails_fading = 0;
    }
    else
    {
      // вычисляем цветовую составляющую
      const unsigned int rgb = g_killmails[g_killmails_current];
      const unsigned char r = (unsigned char)((rgb & 0x00ff0000) >> 16);
      const unsigned char g = (unsigned char)((rgb & 0x0000ff00) >> 8);
      const unsigned char b = (unsigned char)rgb;
      if (g_killmails_fading <= PWM_FADE_HALFTIMES) // значения от 2 до 60 включительно
      {
        // расчёт текущей мощности сечения (значения от 0.0333 до 1.0)
        const float power = (float)g_killmails_fading / (float)(PWM_FADE_HALFTIMES);
        // некоторые индикаторы будут отключены на весь fading-цикл, если их цвета = 0
        if (r) analogWrite(LED_RED_PIN, power * r);
        if (g) analogWrite(LED_GREEN_PIN, power * g);
        if (b) analogWrite(LED_BLUE_PIN, power * b);
      }
      else if (g_killmails_fading < PWM_FADE_TIMES) // значения от 61 до 119 включительно
      {
        // инверсия текущей мощности сечения (раньше нарастало, теперь будет затухать, значения от 0.9833 до 0.0333)
        const float power = (float)(PWM_FADE_TIMES - g_killmails_fading) / (float)(PWM_FADE_HALFTIMES);
        // некоторые индикаторы будут отключены на весь fading-цикл, если их цвета = 0
        if (r) analogWrite(LED_RED_PIN, power * r);
        if (g) analogWrite(LED_GREEN_PIN, power * g);
        if (b) analogWrite(LED_BLUE_PIN, power * b);
      }
    }
  }
}

void handleWebRoot()
{
  g_web_server.send(200, "text/html",
  "<h1>Tabord RGB configure</h1>"
  "<form name='auth' method='post' action='commit'>"
    "<p>SSID : <input type='text' size='31' name='ssid'></p>"
    "<p>Password : <input type='text' size='63' name='pwd'></p>"
    "<input type='submit' value='Commit'>"
  "</form>\n"
  "<form name='rst' method='post' action='reset'>"
    "<p>Clear data from device.</p>"
    "<input type='submit' value='Erase'>"
  "</form>"
  );
}

void handleWebNotFound()
{
  String message = "Not Found\n\n";
  message += "URI: ";
  message += g_web_server.uri();
  message += "\nMethod: ";
  message += (g_web_server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += g_web_server.args();
  message += "\n";
  for (uint8_t i = 0; i < g_web_server.args(); i++)
    message += " " + g_web_server.argName(i) + ": " + g_web_server.arg(i) + "\n";
  g_web_server.send(404, "text/plain", message);
}

void handleWebCommit()
{
  if (g_web_server.args() < 2) return; // явная ошибка и подмена form-ы
  String ssid, pswd;
  for (uint8_t i = 0; i < g_web_server.args(); i++)
  {
    if (g_web_server.argName(i) == "ssid")
      ssid = g_web_server.arg(i);
    else if (g_web_server.argName(i) == "pwd")
      pswd = g_web_server.arg(i);
  }
  if (ssid.length() && pswd.length())
  {
    Serial.printf("AP finished. Sumbit successfull for SSID '%s' with '%s'", ssid.c_str(), pswd.c_str());
    // save to config and use next time or just use - WiFi.begin(WiFi.SSID().c_str(),WiFi.psk().c_str());
    renewEEPROM(ssid.c_str(), pswd.c_str());
    parseEEPROM();
    if (g_ssid && g_pswd)
    {
      Serial.printf("EEPROM read. SSID '%s' and password '%s'\n", g_ssid, g_pswd);
      WiFi.mode(WIFI_STA);
      changeTabordState(sAP_MODE_FINISHED, __LINE__);
      g_need_reboot = true;
    }
    else
    {
      handleWebNotFound();
    }
  }
  else
  {
    handleWebNotFound();
  }
}

void handleWebReset()
{
  Serial.println("AP finished. Resetting EEPROM");
  clearEEPROM();
  WiFi.mode(WIFI_STA);
  changeTabordState(sAP_MODE_FINISHED, __LINE__);
  g_need_reboot = true;
}

void requestURI()
{
  WiFiClient client;
  HTTPClient http;
  http.setUserAgent(HTTP_USER_AGENT);
  http.setReuse(false);
  if (http.begin(client, KILLMAILS_URL))
  {
    //debug:Serial.print("[HTTP] GET...\n");
    // если ошибка, то http-код будет отрицательный
    int httpCode = http.GET();
    if (httpCode > 0)
    {
      // file found at server
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY)
      {
        // ожидается строка в формате: "1 2 3 4 5"
        String payload = http.getString();
        debug:Serial.println(payload);
        
        long nums[MAX_KILLMAILS];
        int idx = 0;
        while (payload.length())
        {
          long num = payload.toInt();
          if (num == 0) break; // если в начальной позиции не число, то выход
          nums[idx++] = num;
          if (idx == MAX_KILLMAILS) break; // забираем не больше 10 элементов из .txt файла
          int pos = payload.indexOf(' ');
          if (pos == -1) break; // если нет пробела, то строка кончилась
          payload.remove(0, pos+1);
        }

        // копируем распакованные из .txt документа данные, с тем чтобы их начать отображать на световодах
        for (int i = 0; i < idx; ++i) g_killmails[i] = nums[i];
        g_killmails_stored = idx;
      }
      else
      {
        Serial.printf("[HTTP] GET... unsupported, code: %d\n", httpCode);
      }
    }
    else
    {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  }
  else
  {
    Serial.printf("[HTTP] Unable to connect\n");
  }
}

void loop()
{
  // работа метода разрешается только после того, как будет настроен WiFi-объект, с тем чтобы корректно начать проверять его состояния
  if (!g_setup_finished) return;
  // приведёт к вызову watch dog-а, который перезагрузит плату
  if (g_need_reboot) while (1);

  // при включённом режиме локальной точки доступа обрабатываем http-запросы
  if (g_fsm_active_state == sAP_MODE_ACTIVE)
  {
    g_web_server.handleClient();
    return;
  }
  else if (g_fsm_active_state == sAP_MODE_FINISHED)
  {
    //WiFi.mode(WIFI_STA);
    //for (int i = 0; i < 2; ++i) { delay(500); Serial.print("."); } // задержка 1 секунда
    //WiFi.begin(g_cfg_access_point,""); // make a failed connection
    //while (WiFi.status() == WL_DISCONNECTED) { delay(500); Serial.print("."); } // задержка, пока не выключится wifi sta
    //Serial.print("\n");
    g_need_reboot = true; // вынуждаем выполнить программный reset платы из-за зависания loop-метода
    return;
  }

  if (digitalRead(WPS_BUTTON_PIN))
  {
    Serial.print("Configure button pressed...");
    handleWPSAPBtnPressed();
    return;
  }

  // пока нет подключения к wifi (или оно внезапно утрачено) отображаем состояние "нет подключения"
  if (WiFi.status() != WL_CONNECTED)
  {
    if (g_fsm_active_state != sWIFI_INACTIVE)
      changeTabordState(sWIFI_INACTIVE, __LINE__); // будет включена сигнализация "нет соединения"
    if (g_ntp_client_started)
    {
      g_time_client.end();
      g_ntp_client_started = false;
      g_time_client_step = 0;
    }
    delay(500);
    return;
  }
  if (g_fsm_active_state == sWIFI_INACTIVE)
  {
    changeTabordState(sWIFI_ACTIVE, __LINE__); // будет включена сигнализация "есть подключение к wifi"
    Serial.print("WiFi connected, ip address: ");
    Serial.println(WiFi.localIP());
  }
  if (g_fsm_active_state == sWIFI_ACTIVE)
  {
    changeTabordState(sWAITING_NTP, __LINE__); // будет включена сигнализация "ожидание данных от ntp-сервера"
    if (!g_ntp_client_started)
    {
      g_time_client.begin();
      g_ntp_client_started = true;
      g_time_client_step = 0;
    }
  }
  if (g_fsm_active_state == sWAITING_NTP)
  {
    if (!g_time_client.forceUpdate())
    {
      delay(500);
      return;
    }
    Serial.println("Network time received, ET: " + g_time_client.getFormattedTime());
    changeTabordState(sINET_AVAILABLE, __LINE__); // будет включена сигнализация "есть подключение к internet-у"
  }

  // мигающий встроенный светодиод вкупе с зелёным индикатором состояния сигнализирует о том, что система работает
  digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  delay(1000);

  // обновление текущей даты времени (если успешно выполнено, то был получен ответ на запрос)
  // запросы к ntp-серверам отправляются не чаще раз в 10 мин, в промежутках время считается арифметичеcки
  g_time_client_step = g_time_client_step + 1;
  if (g_time_client_step >= 600)
  {
    g_time_client_step = 0;
    if (g_time_client.update())
    {
      // подъехало новое время от ntp-сервера
      Serial.println("Network time updated, ET: " + g_time_client.getFormattedTime());
    }
  }

  // раз в минуту пытаемся получить доступ к web-серверу с информацией
  if (0 == g_timeout_to_load_data++)
  {
    requestURI();
  }
  // интервал получения информация от сервера: 60 секунд
  if (g_timeout_to_load_data >= 60) g_timeout_to_load_data = 0;
}
