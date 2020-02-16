#include <ESP8266WiFi.h>
#include <EEPROM.h>
#include <Ticker.h>

// пины подключения RGB через транзисторную сборку
#define LED_BLUE_PIN   14
#define LED_GREEN_PIN  12
#define LED_RED_PIN    13
#define WPS_BUTTON_PIN  4 // на самом деле у Troyka Wifi модуля шелкография помечена 5й ногой (до ревизии №8 включительно)
// макрос для кодирования состояний машины состояний
// r,g,b - соответветственно цвета со значениями от 0x0 до 0xf
// f     - признак мерцания, 0 или 1
#define FSM_STATE(r,g,b,f)   (unsigned int)(((r&0xf)<<8)|((g&0xf)<<4)|(b&0xf)|(f?0x1000:0))
#define FSM_STATE_RED(s)     (unsigned char)(((s&0xf00)>>4)*17)
#define FSM_STATE_GREEN(s)   (unsigned char)((s&0x0f0)*17)
#define FSM_STATE_BLUE(s)    (unsigned char)(((s&0x00f)<<4)*17)

enum state_t
{
  sWIFI_INACTIVE   = FSM_STATE(0xf,0xf,0xf,0), // белый, НЕ мерцающий
  sWPS_MODE_ACTIVE = FSM_STATE(0x0,0x0,0xf,0), // синий, НЕ мерцающий
  sSTA_MODE_ACTIVE = FSM_STATE(0xf,0xe,0x0,1), // жёлтый, мерцающий
  sWIFI_ACTIVE     = FSM_STATE(0x0,0xf,0x0,0), // зелёный, НЕ мерцающий
};

const char * g_ssid = NULL;
const char * g_pswd = NULL;
byte g_eeprom_data[512];
Ticker g_leds_control;
volatile state_t g_fsm_active_state;
volatile state_t g_fsm_prev_state;
volatile bool g_setup_finished; // признак того, что работа метода setup завершена

void readEEPROM();
void parseEEPROM();
void renewEEPROM(const char * ssid, const char * pswd);
bool startWPS();
void resetTabordFSM();
void changeTabordState(state_t state);
void ledsControl();

void setup()
{
  // настройка машины состояний для сигнализации процесса настройки и работы
  g_setup_finished = false;

  // пины в режим выхода
  pinMode(LED_BLUE_PIN, OUTPUT);
  pinMode(LED_GREEN_PIN, OUTPUT);
  pinMode(LED_RED_PIN, OUTPUT);
  // настройка отладочного выхода
  Serial.begin(115200);

  resetTabordFSM();
  g_leds_control.attach_ms(50, ledsControl);
  // настройка прочих необходимых ресурсов
  pinMode(LED_BUILTIN, OUTPUT);
  // настройка работы с EEPROM
  EEPROM.begin(512);

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
  for (int i = 0; i < 512; ++i)
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

// see https://github.com/esp8266/Arduino/issues/1958#issue-150097589
bool startWPS()
{
  changeTabordState(sWPS_MODE_ACTIVE);
  Serial.print("Press WPS button on your router ...");

  // подготавливаемся к переходу в sta режим
  for (int i = 0; i < 10; ++i) { delay(500); Serial.print("."); } // задержка 5 секунд
  // WPS works in STA (Station mode) only -> not working in WIFI_AP_STA !!! 
  WiFi.mode(WIFI_STA);
  for (int i = 0; i < 2; ++i) { delay(500); Serial.print("."); } // задержка 1 секунда
  WiFi.begin("tabord_rgb",""); // make a failed connection
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

void resetTabordFSM()
{
  // чёрный цвет не используется в машине состояний, т.ч. он будет заменён при первом же запуске таймера
  //зануляется в начале работы: g_fsm_prev_state = (state_t)0xfffff000;

  digitalWrite(LED_BLUE_PIN, LOW);
  digitalWrite(LED_GREEN_PIN, LOW);
  digitalWrite(LED_RED_PIN, LOW);
}

void changeTabordState(state_t state)
{
  Serial.printf("FSM state changed %08x -> %08x\n", (int)g_fsm_active_state, (int)state);
  g_fsm_active_state = state;
}

void ledsControl()
{
  if (g_fsm_prev_state != g_fsm_active_state)
  {
    // требуется произвести замену цвета
    analogWrite(LED_RED_PIN, FSM_STATE_RED(g_fsm_active_state));
    analogWrite(LED_GREEN_PIN, FSM_STATE_GREEN(g_fsm_active_state));
    analogWrite(LED_BLUE_PIN, FSM_STATE_BLUE(g_fsm_active_state));
    g_fsm_prev_state = g_fsm_active_state;
    Serial.printf("debug 4: %02x %02x %02x\n", (int)FSM_STATE_RED(g_fsm_active_state), (int)FSM_STATE_GREEN(g_fsm_active_state), (int)FSM_STATE_BLUE(g_fsm_active_state));
  }
}

void loop()
{
  // работа метода разрешается тлько после того, как будет настроен WiFi-объект, с тем чтобы корректно начать проверять его состояния
  if (!g_setup_finished) return;

  if (digitalRead(WPS_BUTTON_PIN))
  {
    Serial.print("Configure button pressed...");
    // если обнаруживаем, что кнопка WPS/STA нажата, то проверяем что она удерживается 2сек непрерывно
    int cnt = 0;
    do
    {
      delay(300);
      if (!digitalRead(WPS_BUTTON_PIN)) break;
      cnt++;
      Serial.print(".");
      if (i == 7) changeTabordState(sWPS_MODE_ACTIVE); // спустя 2.1 сек включем синюю подсветку режима WPS
      else if (i == 15) changeTabordState(sSTA_MODE_ACTIVE); // спустя 4.5 сек включем жёлтую подсветку режима STA
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
    else if (cnt < 15)
    {
      Serial.println(" and STA activated");
    }
  }

  // пока нет подключения к wifi (или оно внезапно утрачено) отображаем состояние "нет подключения"
  if (WiFi.status() != WL_CONNECTED)
  {
    if (g_fsm_active_state != sWIFI_INACTIVE)
      changeTabordState(sWIFI_INACTIVE); // будет включена сигнализация "нет соединения"
    delay(500);
    return;
  }
  if (g_fsm_active_state == sWIFI_INACTIVE)
  {
    changeTabordState(sWIFI_ACTIVE); // будет включена сигнализация "есть подключение к wifi"
    Serial.print("WiFi connected, ip address: ");
    Serial.println(WiFi.localIP());
  }

  digitalWrite(LED_BUILTIN, HIGH);
  //Serial.println("on");
  delay(1000);
  digitalWrite(LED_BUILTIN, LOW);
  //Serial.println("off");
  delay(1000);
}
