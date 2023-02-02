#include <Wire.h>
#include "ccs811.h"

CCS811 ccs811(-1, 0x5A);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Wire.begin();
  delay(1000);
  ccs811.set_i2cdelay(50);

  if (!ccs811.begin()) {
    Serial.println("setup: CCS811 init failed.");
    return ;
  }

  if (!ccs811.start(CCS811_MODE_1SEC)) {
    Serial.println("setup: CCS811 start failed.");
    return;
  }

}

void loop() {
  // put your main code here, to run repeatedly:

  uint16_t eco2, etvoc, errstat, raw;
  ccs811.read(&eco2, &etvoc, &errstat, &raw);

  if (errstat == CCS811_ERRSTAT_OK)
  {
    float co2 = eco2;
    float tvoc = etvoc;
    Serial.printf("CCS811: CO2 %02.f ppm, TVOC %02f ppb\n", co2, tvoc);
  }
  else if (errstat == CCS811_ERRSTAT_OK_NODATA)
  {
    Serial.println("CCS811: waiting for (new) data...");
  }
  else if (errstat & CCS811_ERRSTAT_I2CFAIL)
  {
    Serial.println("CCS811: I2C error");
  }
  else if (errstat & CCS811_ERRSTAT_ERRORS)
  {
    Serial.print("CCS811: error ");
    Serial.print(errstat, HEX);
    Serial.print(" ");
    Serial.println(ccs811.errstat_str(errstat));
  }

  delay(1000);
}
