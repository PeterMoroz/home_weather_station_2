// #include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>
#include <EEPROM.h>
#include <FS.h>
#include <Arduino_JSON.h>


AsyncWebServer webServer(80);

#define EEPROM_SIZE 512
#define SSID_SIZE 32
#define PASS_SIZE 32

bool needRestart = false;

float temperature = 0.f;
float humidity = 0.f;

unsigned long lastUpdateReadings = 0;
#define UPDATE_READINGS_INTERVAL_MS 10000




void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  ::temperature = random(10, 30);
  ::humidity = random(20, 50);

  EEPROM.begin(EEPROM_SIZE);

  if (!SPIFFS.begin()) {
    Serial.println("Could not mount filesystem.");
    while (1) ;
  }

  char ssid[SSID_SIZE] = { 0 };
  char pass[PASS_SIZE] = { 0 };

  int idx = 0;
  for (int i = 0; i < SSID_SIZE; i++, idx++)
  {
    ssid[i] = char(EEPROM.read(idx));
  }
  ssid[SSID_SIZE - 1] = '\0';

  for (int i = 0; i < PASS_SIZE; i++, idx++)
  {
    pass[i] = char(EEPROM.read(idx));
  }
  pass[PASS_SIZE - 1] = '\0';
  
  WiFi.begin(ssid, pass);
  bool connected = false;
  int attempt = 0;
  while (attempt++ < 10) {
    if (WiFi.status() == WL_CONNECTED) {
      connected = true;
      break;
    }
    delay(1000);
  }

  if (!connected) {
    Serial.println("Start AP");
    WiFi.softAP("ESP8266");
    delay(100);
    Serial.print("SoftAP IP ");
    Serial.println(WiFi.softAPIP());

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/wifimanager.html", "text/html");      
    });

    webServer.on("/", HTTP_POST, [](AsyncWebServerRequest *request) {
      char ssid[SSID_SIZE] = { 0 };
      char pass[SSID_SIZE] = { 0 };
      const int params = request->params();
      for (int i = 0; i < params; i++) {
        AsyncWebParameter *p = request->getParam(i);
        if (p->isPost()) {
          if (!strcmp(p->name().c_str(), "ssid")) {
            p->value().toCharArray(ssid, SSID_SIZE);
          }
          if (!strcmp(p->name().c_str(), "pass")) {
            p->value().toCharArray(pass, PASS_SIZE);
          }
        }
      }

      if (strlen(ssid) == 0 || strlen(pass) == 0) {
        request->send(400, "text/plain", "Missed SSID and/or password.");
        return;
      }

      for (int i = 0; i < (SSID_SIZE + PASS_SIZE); i++) {
        EEPROM.write(i, 0);
      }
      EEPROM.end();

      EEPROM.begin(EEPROM_SIZE);

      int idx = 0;
      for (int i = 0; i < SSID_SIZE; i++, idx++) {
        EEPROM.write(idx, ssid[i]);
      }
      for (int i = 0; i < PASS_SIZE; i++, idx++) {
        EEPROM.write(idx, pass[i]);
      }

      if (EEPROM.commit()) {
        request->send(200, "text/plain", "Success. Device is going to restart in a few seconds.");
        needRestart = true;        
      } else {
        request->send(500, "text/plain", "Could not save credentials.");
      }
            
    });
    
  } else {
    Serial.print("Local  IP ");
    Serial.println(WiFi.localIP());

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html");      
    });

    webServer.on("/readings", HTTP_GET, [](AsyncWebServerRequest *request) {
      JSONVar json;
      json["temperature"] = String(::temperature);
      json["humidity"] = String(::humidity);
      request->send(200, "application/json", JSON.stringify(json));
    });

  }

  webServer.serveStatic("/", SPIFFS, "/");
  webServer.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (needRestart) {
    delay(5000);
    ESP.restart();
  }

  if (millis() - lastUpdateReadings > UPDATE_READINGS_INTERVAL_MS) {
    ::temperature = random(10, 30);
    ::humidity = random(20, 50);
    lastUpdateReadings = millis();
  }
}
