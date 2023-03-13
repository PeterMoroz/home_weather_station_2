#include <EEPROM.h>
#include <FS.h>

#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <AsyncElegantOTA.h>

#include <AHTxx.h>

AsyncWebServer webServer(80);
AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR);


#define EEPROM_SIZE 512
#define SSID_SIZE 32
#define PASS_SIZE 32

bool needRestart = false;

float temperature = 0.f;
float humidity = 0.f;

int8_t minTemperature = 0;
int8_t maxTemperature = 40;
int8_t minHumidity = 40;
int8_t maxHumidity = 60;

unsigned long lastReadAHT10 = 0;
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

  webServer.on("/ranges", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/ranges.html", "text/html");
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


  webServer.serveStatic("/", SPIFFS, "/");

  webServer.on("/charts", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/charts.html", "text/html");
  });

  webServer.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[8] = { 0 };
    sprintf(buf, "%0.2f", ::temperature);
    request->send(200, "text/plain", buf);
  });  

  webServer.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[8] = { 0 };
    sprintf(buf, "%0.2f", ::humidity);
    request->send(200, "text/plain", buf);
  });
      
}

void setupRequestHandlers() {
  
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

  webServer.on("/ranges", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/ranges.html", "text/html");
  });  

  webServer.on("/getranges", HTTP_GET, [](AsyncWebServerRequest *request) {
    char data[96] = { 0 };
    sprintf(data, "{\"minTemperature\": %d, \"maxTemperature\": %d, \"minHumidity\": %d, \"maxHumidity\": %d}", 
            minTemperature, maxTemperature, minHumidity, maxHumidity);
    request->send(200, "text/json", data);
  });

  webServer.on("/setranges", HTTP_POST, [](AsyncWebServerRequest *request) {
    bool saveIntoFile = false;
    const int params = request->params();
    for (int i = 0; i < params; i++) {
      AsyncWebParameter *p = request->getParam(i);
      if (p->isPost()) {
        if (!strcmp(p->name().c_str(), "minTemperature")) {
          minTemperature = atoi(p->value().c_str());
        }
        if (!strcmp(p->name().c_str(), "maxTemperature")) {
          maxTemperature = atoi(p->value().c_str());
        }
        if (!strcmp(p->name().c_str(), "minHumidity")) {
          minHumidity = atoi(p->value().c_str());
        }
        if (!strcmp(p->name().c_str(), "maxHumidity")) {
          maxHumidity = atoi(p->value().c_str());
        }
        if (!strcmp(p->name().c_str(), "save")) {
          saveIntoFile = strcmp(p->value().c_str(), "on") == 0;
        }
      }
    }

    if (saveIntoFile) {
      File f = SPIFFS.open("/ranges.dat", "w");
      if (!f) {
        request->send(500, "text/plain", "Couldn't store ranges.");
        return;
      }
  
      int8_t buf[4] = { 0 };
      buf[0] = minTemperature;
      buf[1] = maxTemperature;
      buf[2] = minHumidity;
      buf[3] = maxHumidity;
      
      f.write((const char *)&buf[0], sizeof(buf));
      f.close();      
    }

    request->send(SPIFFS, "/ranges.html", "text/html");
  });

  webServer.on("/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    char data[64] = { 0 };
    sprintf(data, "{ \"temperature\": \"%0.2f\", \"rh\": \"%0.2f\" }", ::temperature, ::humidity);
    request->send(200, "text/json", data);
  });


  webServer.serveStatic("/", SPIFFS, "/");

  webServer.on("/charts", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/charts.html", "text/html");
  });

  webServer.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[8] = { 0 };
    sprintf(buf, "%0.2f", ::temperature);
    request->send(200, "text/plain", buf);
  });  

  webServer.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request) {
    char buf[8] = { 0 };
    sprintf(buf, "%0.2f", ::humidity);
    request->send(200, "text/plain", buf);
  });

}


void printStatusAHT10() {
  switch (aht10.getStatus()) {
    case AHTXX_NO_ERROR:
      Serial.println("AHT10: no error");
      break;
    case AHTXX_BUSY_ERROR:
      Serial.println("AHT10: sensor busy, increase polling time");
      break;
    case AHTXX_ACK_ERROR:
      Serial.println("AHT10: sensor didn't return ACK. not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)");
      break;
    case AHTXX_DATA_ERROR:
      Serial.println("AHT10: received data smaller than expected. not connected, broken, long wires (reduce speed), bus locked by slave (increase stretch limit)");
      break;
    default:
      Serial.println("AHT10: unknwon status");
  }
}

bool readAHT10() {

  float value = aht10.readTemperature();
  if (value != AHTXX_ERROR) {
    ::temperature = value;    
  } else {
    printStatusAHT10();
    return false;
  }

  value = aht10.readHumidity(AHTXX_USE_READ_DATA);
  if (value != AHTXX_ERROR) {
    ::humidity = value;
  } else {
    printStatusAHT10();
    return false;
  }

  lastReadAHT10 = millis();
  return true;
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  Wire.begin();
  delay(100);

  int attempt = 0;
  while (!aht10.begin() && attempt++ < 10) {
    delay(1000);
  }
  if (attempt == 10) {
    Serial.println("AHT10 begin failed.");
    while (1) ;
  }

  Serial.println("AHT10 - OK");

  if (!readAHT10()) {
    Serial.println("Could not get AHT10 readings.");
  }

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

  if (SPIFFS.exists("/ranges.dat")) {
    File f = SPIFFS.open("/ranges.dat", "r");
    int8_t buf[4] = { 0 };
    f.read((uint8_t*)buf, sizeof(buf));
    f.close();

    minTemperature = buf[0];
    maxTemperature = buf[1];
    minHumidity = buf[2];
    maxHumidity = buf[3];
  }  

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

  AsyncElegantOTA.begin(&webServer);
  webServer.begin();
}

void loop() {
  // put your main code here, to run repeatedly:
  if (needRestart) {
    delay(1000);
    ESP.restart();
  }

  if (millis() - lastReadAHT10 > UPDATE_READINGS_INTERVAL_MS) {
    if (!readAHT10()) {
      Serial.println("Could not get AHT10 readings.");
    }
  }
}
