#include <Wire.h>
#include <AHTxx.h>
#include <ESP8266WiFi.h>
#include <Adafruit_BMP085.h>

#include "ThingSpeak.h"

#include "credentials.h"


AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR);
Adafruit_BMP085 bmp;

WiFiClient wifi;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Wire.begin();
  delay(1000);

  if (!aht10.begin()) {
    Serial.println("setup: AHT10 begin failed.");
    while (1) ;
  }

  if (!bmp.begin()) {
    Serial.println("setup: BMP085 begin failed.");
    while (1) ;
  }


  WiFi.mode(WIFI_STA);
  ThingSpeak.begin(wifi);

  unsigned attempt = 10;
  while (1) {
    WiFi.begin(SSID, PKEY);
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
    if (--attempt == 0) {
      break;
    }
    delay(5000);
  }

  if (attempt == 0) {
    Serial.println("setup: Could not connect to WiFi");
    while (1) ;
  }
  
}

void printStatusAHT10() {
  switch (aht10.getStatus()) {
    case AHTXX_NO_ERROR:
      Serial.println("AHTXX: no error");
      break;
    case AHTXX_BUSY_ERROR:
      Serial.println("AHTXX: sensor busy, increase polling time");
      break;
    case AHTXX_ACK_ERROR:
      Serial.println("AHTXX: sensor didn't return ACK, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)");
      break;
    case AHTXX_DATA_ERROR:
      Serial.println("AHTXX: received data smaller than expected, not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)");
      break;
    case AHTXX_CRC8_ERROR:
      Serial.println("AHTXX: computed CRC8 not match, received CRC8 (feature available only by AHT2xx sensors)");
      break;
    default:
      Serial.println("AHTXX: unknown status");
      break;
  }  
}

void loop() {
  // put your main code here, to run repeatedly:

  {
    float t = aht10.readTemperature();
    bool temperatureValid = true;
    if (t == AHTXX_ERROR)
    {
      Serial.println("AHT10: read temperature failed");
      printStatusAHT10();
      temperatureValid = false;
      if (!aht10.softReset()) {
        Serial.println("AHT10: soft reset failed");
      }
    }
  
    delay(2000);
  
    float h = aht10.readHumidity();
    bool humidityValid = true;
    if (h == AHTXX_ERROR)
    {
      Serial.println("AHT10: read humidity failed");
      printStatusAHT10();
      humidityValid = false;
      if (!aht10.softReset()) {
        Serial.println("AHT10: soft reset failed");
      }
    }
  
    if (temperatureValid && humidityValid) {
      Serial.printf("AHT10: humidity %0.2f %%, temperature %0.2f Celsius\n", h, t);
      // channel number, field number, value to publish, channel write API key
      int x = 0;      
      x = ThingSpeak.setField(1, h);
      if (x != 200) {
        Serial.printf("ThingSpeak: failed to set field 1 (AHT10/humidity). response: %d\n", x);
      }
      x = ThingSpeak.setField(2, t);
      if (x != 200) {
        Serial.printf("ThingSpeak: failed to set field 2 (AHT10/temperature). response: %d\n", x);
      }
      x = ThingSpeak.writeFields(1, AHT10_WRITE_API_KEY);
      if (x != 200) {
        Serial.printf("ThingSpeak: failed to write multiple fields of channel #1 (AHT10). response: %d\n", x);
      }
    }  
  }
  
  {
    float t = bmp.readTemperature();
    float p = bmp.readPressure();
    Serial.printf("BMP180: pressure %0.2f Pascal, temperature %0.2f Celsius\n", p, t);

    // channel number, field number, value to publish, channel write API key
    int x = 0;
    x = ThingSpeak.setField(1, p);
    if (x != 200) {
      Serial.printf("ThingSpeak: failed to set field 1 (BMP180/pressure). response: %d\n", x);
    }
    x = ThingSpeak.setField(2, t);
    if (x != 200) {
      Serial.printf("ThingSpeak: failed to set field 2 (BMP180/temperature). response: %d\n", x);
    }
    x = ThingSpeak.writeFields(2, BMP180_WRITE_API_KEY);
    if (x != 200) {
      Serial.printf("ThingSpeak: failed to write multiple fields of channel #1 (BMP180). response: %d\n", x);
    }
  }

  delay(30000);
}
