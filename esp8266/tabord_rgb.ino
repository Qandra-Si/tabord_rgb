#include <ESP8266WiFi.h>
#include <EEPROM.h>

const char * g_ssid = NULL;
const char * g_pswd = NULL;
byte g_eeprom_data[512];

void readEEPROM();
void parseEEPROM();
void renewEEPROM(const char * ssid, const char * pswd);
bool startWPS();

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as an output
  Serial.begin(115200);
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
    Serial.println("EEPROM empty or corrupted.\nPress WPS button on your router ...");
    delay(5000);
    if (startWPS())
    {
      parseEEPROM();
      if (g_ssid && g_pswd)
      {
        Serial.printf("WPS finished sucessfully. SSID '%s' and password '%s'\n", g_ssid, g_pswd);
      }
    }
  }

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
  Serial.println("WPS config start");
  // WPS works in STA (Station mode) only -> not working in WIFI_AP_STA !!! 
  WiFi.mode(WIFI_STA);
  delay(1000);
  WiFi.begin("foobar",""); // make a failed connection
  while (WiFi.status() == WL_DISCONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
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
        wpsSuccess = false;
      }
  }
  return wpsSuccess; 
}

void loop()
{
  // put your main code here, to run repeatedly:
}
