#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  if (!rtc.begin()) {
    Serial.println("Could not init RTC.");
    return ;
  }

  if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime dt = rtc.now();
  Serial.printf("%04d/%02d/%02d - %02d:%02d:%02d\n",
    dt.year(), dt.month(), dt.day(), 
    dt.hour(), dt.minute(), dt.second());

  delay(1000);
}
