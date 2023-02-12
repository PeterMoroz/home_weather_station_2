#include <Wire.h>
#include <AHTxx.h>
#include <Adafruit_BMP085.h>


AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR);
Adafruit_BMP085 bmp;


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Wire.begin();
  delay(1000);

  if (!aht10.begin()) {
    Serial.println("setup: AHT10 begin failed.");
    return;
  }

  if (!bmp.begin()) {
    Serial.println("setup: BMP085 begin failed.");
    return;
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
    }  
  }
  
  {
    float t = bmp.readTemperature();
    float p = bmp.readPressure();
    Serial.printf("BMP180: pressure %0.2f Pascal, temperature %0.2f Celsius\n", p, t);
  }  

  delay(10000);
}
