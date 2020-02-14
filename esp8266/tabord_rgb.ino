    // пины подключения RGB светодиодной ленты через ключи
    #define LED_RED_PIN     10
    #define LED_GREEN_PIN   9
    #define LED_BLUE_PIN    11

    #define FADE_SPEED      10
    #define KILLMAILS_SIZE  3

    unsigned long killMails[KILLMAILS_SIZE] = {
      0x00fff200ul,
      0x0000ff00ul,
      0x00ff0000ul,
    };
     
    void setup()
    {
      // пины в режим выхода
      pinMode(LED_RED_PIN, OUTPUT);
      pinMode(LED_GREEN_PIN, OUTPUT);
      pinMode(LED_BLUE_PIN, OUTPUT);
      pinMode(LED_BUILTIN, OUTPUT);
    }
     
    void loop()
    {
        for (int k = 0; k < KILLMAILS_SIZE; ++k)
        {
          unsigned long red   = (killMails[k] & 0x00ff0000ul) >> 16;
          unsigned long green = (killMails[k] & 0x0000ff00ul) >> 8;
          unsigned long blue  =  killMails[k] & 0x000000fful;
          for (int i=1;i<256; i++)
          {
            float power = (float)i / 255.0;
            analogWrite(LED_RED_PIN, power * red);
            analogWrite(LED_GREEN_PIN, power * green);
            analogWrite(LED_BLUE_PIN, power * blue);
            delay(FADE_SPEED);
          }
          for (int i = 255; i >= 1; --i)
          {
            float power = (float)i / 255.0;
            analogWrite(LED_RED_PIN, power * red);
            analogWrite(LED_GREEN_PIN, power * green);
            analogWrite(LED_BLUE_PIN, power * blue);
            delay(FADE_SPEED);
          }
        }
      /*
        digitalWrite(LED_GREEN_PIN, HIGH);   //включить зеленый светодиод на максимум
        digitalWrite(LED_RED_PIN, HIGH);    // включить красный светодиод на максимум
        digitalWrite(LED_BLUE_PIN, HIGH);   // включить синий светодиод на максимум
        digitalWrite(LED_BUILTIN, HIGH);
        delay(1000);                       // подождать 5 секунд
        digitalWrite(LED_GREEN_PIN, LOW);  // выключаем зеленый светодиод
        digitalWrite(LED_RED_PIN, LOW); // выключаем красный светодиод
        digitalWrite(LED_BLUE_PIN, LOW);// выключаем синий светодиод
        digitalWrite(LED_BUILTIN, LOW);
        delay(1000);

        // цикл, чтобы зажечь светодиод со скоростью затухания
        for (int i=0;i<256; i++)
        {
            analogWrite(LED_RED_PIN, i);
            delay(FADE_SPEED);
        }
        digitalWrite(LED_RED_PIN, LOW);  // выключаем зеленый светодиод
        for (int i=0;i<256; i++)
        {
            analogWrite(LED_GREEN_PIN, i);
            delay(FADE_SPEED);
        }
        digitalWrite(LED_GREEN_PIN, LOW);  // выключаем зеленый светодиод
        for (int i=0;i<256; i++)
        {
            analogWrite(LED_BLUE_PIN, i);
            delay(FADE_SPEED);
        }
        digitalWrite(LED_BLUE_PIN, LOW);  // выключаем зеленый светодиод
    */
    }
