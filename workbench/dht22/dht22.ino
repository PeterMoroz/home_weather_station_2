#include "DHTesp.h"

DHTesp dht;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  dht.setup(D0, DHTesp::DHT22);
}

void loop() {
  // put your main code here, to run repeatedly:
  float h = dht.getHumidity();
  float t = dht.getTemperature();

  if (isnan(h)) {
    Serial.println("Invalid humidity");
  }

  if (isnan(t)) {
    Serial.println("Invalid temperature");
  }

  if (!isnan(h) && !isnan(t)) {
    Serial.printf("humidity: %.2f%% temperature: %.2f deg.\n", h, t);
  }

  delay(1000);
}
