#include <Wire.h>
#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>

#include "RTClib.h"
#include "wifi_credentials.h"

#define UTC_OFFSET_SECONDS (3600 * 3)

RTC_DS3231 rtc;
WiFiUDP udp;
NTPClient ntpClient(udp, "pool.ntp.org", UTC_OFFSET_SECONDS);


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

  WiFi.begin(SSID, PKEY);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");

  ntpClient.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  DateTime dt = rtc.now();
  Serial.printf("RTC\t%04d/%02d/%02d - %02d:%02d:%02d\n",
    dt.year(), dt.month(), dt.day(), 
    dt.hour(), dt.minute(), dt.second());

  ntpClient.update();
  dt = DateTime(ntpClient.getEpochTime());
  Serial.printf("NTP\t%04d/%02d/%02d - %02d:%02d:%02d\n",
    dt.year(), dt.month(), dt.day(), 
    dt.hour(), dt.minute(), dt.second());  

  Serial.printf("RTC raw: %08u\tNTP raw: %08u\n", 
    rtc.now().unixtime(), ntpClient.getEpochTime());
  if (rtc.now().unixtime() != ntpClient.getEpochTime()) {
    Serial.println("Adjust RTC with NTP value");
    rtc.adjust(DateTime(ntpClient.getEpochTime()));
  }

  Serial.printf("RTC: temperature %0.2f celsius\n", rtc.getTemperature());

  delay(1000);
}
