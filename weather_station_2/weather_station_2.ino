#include <EEPROM.h>
#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebSrv.h>

AsyncWebServer webServer(80);

#define EEPROM_SIZE 512
#define SSID_SIZE 32
#define PASS_SIZE 32

bool needRestart = false;

float temperature = 0.f;
float humidity = 0.f;

unsigned long lastUpdateReadings = 0;
#define UPDATE_READINGS_INTERVAL_MS 10000

String wifiNetworks;

void scanWiFiNetworks() {
  int n = WiFi.scanNetworks();
  String ssid("\"SSID\":["), rssi("\"RSSI\":["), encrypted("\"encrypted\":[");
  for (int i = 0; i < n; i++) {
    if (i != 0) {
      ssid += ", ";
      rssi += ", ";
      encrypted += ", ";
    }
    rssi += (WiFi.RSSI(i));
    ssid += "\"" + WiFi.SSID(i) + "\"";
    encrypted += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? "0" : "1";
  }
  rssi += "]";
  ssid += "]";
  encrypted += "]";

  wifiNetworks = "{ " + rssi + ", " + ssid + ", " + encrypted + " }";
}

bool checkWiFiConnection() {
  int attempt = 0;
  while (attempt++ < 10) {
    if (WiFi.status() == WL_CONNECTED) {
      return true;
      break;
    }
    delay(1000);
  }
  return false;
}

void clearMemory() {
  for (int i = 0; i < (SSID_SIZE + PASS_SIZE); i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
}

bool storeWiFiCredentials(const char* ssid, const char* pass) {
  for (int i = 0; i < (SSID_SIZE + PASS_SIZE); i++) {
    EEPROM.write(i, 0);
  }
  
  int idx = 0;
  for (int i = 0; i < SSID_SIZE; i++, idx++) {
    EEPROM.write(idx, ssid[i]);
  }
  for (int i = 0; i < PASS_SIZE; i++, idx++) {
    EEPROM.write(idx, pass[i]);
  }

  const bool success = EEPROM.commit();
  return success;
}

void setupRequestHandlersAP() {

    webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/setup.html", "text/html");
    });

    webServer.on("/wifissids", HTTP_GET, [](AsyncWebServerRequest *request) {
      Serial.println("WiFi networks: ");
      Serial.println(wifiNetworks);
      request->send(200, "text/json", wifiNetworks.c_str());
    });

    webServer.on("/direct", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/index.html", "text/html");
    });

    webServer.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/reset.html", "text/html");
    });

    webServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/reboot.html", "text/html");
      needRestart = true;
    });

    webServer.on("/clearmem", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/clear.html", "text/html");
      clearMemory();
      needRestart = true;
    });

    webServer.on("/setting", HTTP_POST, [](AsyncWebServerRequest *request) {
      char ssid[SSID_SIZE] = { 0 };
      char pass[PASS_SIZE] = { 0 };
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

      const bool success = storeWiFiCredentials(ssid, pass);

      if (success) {
        request->send(SPIFFS, "/setting.html", "text/html");
        needRestart = true;
      } else {
        request->send(500, "text/plain", "Couldn't store settings in EEPROM.");
      }

    });

    webServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
      char data[64] = { 0 };
      sprintf(data, "{ \"temperature\": \"%0.2f\", \"rh\": \"%0.2f\" }", ::temperature, ::humidity);
      request->send(200, "text/json", data);
    });
}

void setupRequestHandlers() {

/*
    webServer.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send(SPIFFS, "/setup.html", "text/html");
    });

    webServer.on("/wifissids", HTTP_GET, [](AsyncWebServerRequest *request) {

      wifiNetworks = "{ \"RSSI\":[-69, -91, -74, -65, -70, -48, -90, -82, -83, -45, -64], " \
        " \"SSID\":[\"TP-LINK_A322\", \"TP-Link_FD38\", \"TP-Link_5DB9\", \"HUAWEI-ebqn\", \"HUAWEI-2.4G-Fe3p\", \"linksys\", \"HUAWEI-4huh\", \"HUAWEI-2sMg\", \"byfly WIFI\", \"TP-Link_5497\", \"85-23 2.4\"], " \
        " \"encrypted\":[1, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1] } ";
      Serial.println("WiFi networks: ");
      Serial.println(wifiNetworks);
      request->send(200, "text/json", wifiNetworks.c_str());
    });

    */  
  
  webServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/index.html", "text/html");
  });

  webServer.on("/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/reset.html", "text/html");      
  });

  webServer.on("/clearmem", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/clear.html", "text/html");
    clearMemory();
    needRestart = true;
  });

  webServer.on("/reboot", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/reboot.html", "text/html");
    needRestart = true;
  });

  webServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    char data[64] = { 0 };
    sprintf(data, "{ \"temperature\": \"%0.2f\", \"rh\": \"%0.2f\" }", ::temperature, ::humidity);
    request->send(200, "text/json", data);
  });
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  ::temperature = random(10, 30);
  ::humidity = random(20, 50);

  EEPROM.begin(EEPROM_SIZE);
  delay(10);

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

  WiFi.mode(WIFI_STA);
  delay(200);
  WiFi.begin(ssid, pass);
  bool connected = checkWiFiConnection();

  if (!connected) {
    Serial.println("Start AP");
    WiFi.disconnect();
    delay(100);

    scanWiFiNetworks();
    setupRequestHandlersAP();

    WiFi.softAP("ESP8266");
    delay(100);
    Serial.print("SoftAP IP ");
    Serial.println(WiFi.softAPIP());
    
  } else {
    Serial.print("Local  IP ");
    Serial.println(WiFi.localIP());

    setupRequestHandlers();
  }

  webServer.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (needRestart) {
    delay(1000);
    ESP.restart();
  }

  if (millis() - lastUpdateReadings > UPDATE_READINGS_INTERVAL_MS) {
    ::temperature = random(10, 30);
    ::humidity = random(20, 50);
    lastUpdateReadings = millis();
  }
}