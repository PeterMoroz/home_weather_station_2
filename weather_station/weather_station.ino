/* 
 *  Don't change the order of the header files below!
 *  I observed unexpected crashes during startup.
 */
#include <DS1307RTC.h>
#include <AHTxx.h>

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <WiFiManager.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

#include "ccs811.h"

// If sampling rate is too high, the sensor overheats
#define AHT10_SAMPLING_PERIOD 2000

// #define CCS811_SAMPLING_PERIOD 60000
#define CCS811_SAMPLING_PERIOD 1000

//#define SSID ""
//#define PKEY ""

#define MQTT_BROKER "broker.emqx.io"
#define MQTT_PORT 1883

#define TEMPERATURE_TOPIC "dht/temperature"
#define HUMIDITY_TOPIC "dht/humidity"
#define ECO2_TOPIC "ccs811/eco2"
#define TVOC_TOPIC "ccs811/tvoc"

CCS811 ccs811;

AHTxx aht10(AHTXX_ADDRESS_X38, AHT1x_SENSOR);

// LiquidCrystal_I2C lcd(0x27, 16, 2);

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

WiFiManager wifiManager;

#define UTC_OFFSET_SECONDS (3600 * 3)

WiFiUDP udp;
NTPClient ntpClient(udp, "pool.ntp.org", UTC_OFFSET_SECONDS);


long lastTimeReadTH = 0;
long lastTimeReadTVOC = 0;
long lastTimeSync = 0;

///////////////////////////////////////////////////////
// BuzzerController
class BuzzerController {

public:
  explicit BuzzerController(const byte pin);

  void setup();
  void tick();
  void setSoundDuration(unsigned v);
  void setAlarmOn(bool v);

  bool isAlarmOn() const { return alarmOn; }

private:
  void turnOver();
  void setSoundOn(bool v);
  
private:
  bool alarmOn = false;
  bool soundOn = false;
  const byte buzzerPin = 0xFF;
  unsigned halfPeriodOfSound = 2000;
  unsigned long lastTurnTime = 0;  
};

BuzzerController::BuzzerController(const byte pin)
  : buzzerPin(pin)
  {
  }

void BuzzerController::tick() {
  if (alarmOn) {
    if (millis() - lastTurnTime > halfPeriodOfSound) {
      turnOver();
    }
  }
}

void BuzzerController::setup() {  
  pinMode(buzzerPin, OUTPUT);
}

void BuzzerController::setSoundDuration(unsigned v) {
  halfPeriodOfSound = v / 2;
}

void BuzzerController::setAlarmOn(bool v) {
  if (!v) {
    alarmOn = false;
    setSoundOn(false);
  } else {
    alarmOn = true;
    setSoundOn(true);
    lastTurnTime = millis();
  }
}

void BuzzerController::turnOver() {
  if (soundOn)
    setSoundOn(false);
  else
    setSoundOn(true);
  lastTurnTime = millis();
}

void BuzzerController::setSoundOn(bool v) {
  soundOn = v;
  digitalWrite(buzzerPin, v ? HIGH : LOW);
}


BuzzerController buzzerController(D5);


///////////////////////////////////////////////////////
// AlarmController

/* To accumulate CO2 samples during 15 minutes interval,
 * and taking into account that sampling period is 1 s 
 * the required size of buffer is 15 * 60 = 900 
 */
#define CO2_SAMPLES_BUFF_SIZE 900

/* The averaging of CO2 samples is performed each 15 min during 8 hours interval,
 * the required buffer size is (8 * 60) / 15 = 32   
 */
#define CO2_AVERAGES_BUFF_SIZE 32


class AlarmController {
  
public:
  explicit AlarmController(BuzzerController &aBuzzerController);

  void registerMeasurements(uint16_t tvoc, uint16_t co2);

  void setCO2ExposureThreshold(uint16_t v) {
    co2ExposureThreshold = v;
  }

  void setTVOCExposureThreshold(uint16_t v) {
    tvocExposureThreshold = v;
  }

private:
  void processCO2Sample(uint16_t v);

private:
  BuzzerController& buzzerController;
  uint16_t co2ExposureThreshold = 2000;
  uint16_t tvocExposureThreshold = 2500;
  uint16_t co2SustainedThresholdShort = 30000; // threshold to sustained CO2 value during a short term (15 min)
  uint16_t co2SustainedThresholdLong = 5000;  // threshold to sustained CO2 value during a long term (8 hr)
  bool turnOnAlarm = false;
  uint16_t co2SamplesIdx = 0;
  uint16_t co2AveragesIdx = 0;
  uint16_t co2Samples[CO2_SAMPLES_BUFF_SIZE] = { 0 };
  uint16_t co2Averages[CO2_AVERAGES_BUFF_SIZE] = { 0 };
};

AlarmController::AlarmController(BuzzerController &aBuzzerController)
  : buzzerController(aBuzzerController)
  {
    
  }

void AlarmController::registerMeasurements(uint16_t tvoc, uint16_t co2) {

  turnOnAlarm = false;

  processCO2Sample(co2);
  
  if ((tvoc >= tvocExposureThreshold || co2 >= co2ExposureThreshold) && !buzzerController.isAlarmOn()) {
    turnOnAlarm = true;
  }

  if (turnOnAlarm) {
    buzzerController.setAlarmOn(true);    
  } else {
    if ((tvoc < tvocExposureThreshold && co2 < co2ExposureThreshold) && buzzerController.isAlarmOn()) {
      buzzerController.setAlarmOn(false);
    }
  }
}

void AlarmController::processCO2Sample(uint16_t v) {
  co2Samples[co2SamplesIdx] = v;
  ++co2SamplesIdx;
  if (co2SamplesIdx == CO2_SAMPLES_BUFF_SIZE) {
    uint32_t sum = 0;
    do {
      co2SamplesIdx--;
      sum += co2Samples[co2SamplesIdx];
    } while (co2SamplesIdx != 0);
    uint32_t avg = sum / CO2_SAMPLES_BUFF_SIZE;
    if (avg >= co2SustainedThresholdShort) {
      turnOnAlarm = true;
    }

    co2Averages[co2AveragesIdx] = static_cast<uint16_t>(avg);
    ++co2AveragesIdx;
    if (co2AveragesIdx == CO2_AVERAGES_BUFF_SIZE) {
      sum = 0;
      do {
        co2AveragesIdx--;
        sum += co2Averages[co2AveragesIdx];        
      } while (co2AveragesIdx != 0);
      avg = sum / CO2_AVERAGES_BUFF_SIZE;
      if (avg >= co2SustainedThresholdLong) {
        turnOnAlarm = true;
      }
    }
  }
}

AlarmController alarmController(buzzerController);


////////////////////////////////////////////////////////////////////////////////
// AHT10 (temperature/humidity sensor)
//
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

bool readAHT10(float &temperature, float &humidity) {
  float value = aht10.readTemperature();
  if (value != AHTXX_ERROR) {
    temperature = value;
  } else {
    printStatusAHT10();
    return false;
  }

  value = aht10.readHumidity(AHTXX_USE_READ_DATA);
  if (value != AHTXX_ERROR) {
    humidity = value;
  } else {
    printStatusAHT10();
    return false;
  }
    
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// connect MQTT to broker
//
void mqttConnectToBroker() {
  byte mac[6];
  WiFi.macAddress(mac);
  char clientId[32];
  sprintf(clientId, "esp8266-%02d%02d%02d%02d%02d%02d", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.print("Connect to MQTT broker...   ");

  if (mqttClient.connect(clientId)) {
    Serial.println("connected");
  } else {
    Serial.print("Failed with state ");
    Serial.println(mqttClient.state());
  }
}

////////////////////////////////////////////////////////////////////////////////
// RTC
//
const char* monthNames[12] = {
  "Jan", "Feb", "Mar", "Apr", "May", "Jun",
  "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

bool parseDateTime(const char* strDate, const char* strTime, tmElements_t& tm) {
  char monthName[12] = { 0 };
  int day = 0, year = 0;
  uint8_t monthIndex = 0;

  Serial.printf("Parse date: %s\n", strDate);
  if (sscanf(strDate, "%s %d %d", &monthName, &day, &year) != 3) {
    return false;
  }

  for (; monthIndex < 12; monthIndex++) {
    if (!strcmp(monthName, monthNames[monthIndex])) {
      break;
    }
  }

  if (monthIndex >= 12) {
    return false;
  }

  tm.Day = day;
  tm.Month = monthIndex + 1;
  tm.Year = CalendarYrToTm(year);

  int hour = 0, minute = 0, second = 0;
  Serial.printf("Parse time: %s\n", strTime);
  if (sscanf(strTime, "%d:%d:%d", &hour, &minute, &second) != 3) {
    return false;
  }

  tm.Hour = hour;
  tm.Minute = minute;
  tm.Second = second;
  return true;  
}

bool setupRTC() {
  tmElements_t tm;
  uint8_t attempt = 10;
  while (!RTC.read(tm)) {
    ++attempt;
    if (RTC.chipPresent()) {
      Serial.println("The DS1307 is stopped. Going to setup it ...");      
      if (!parseDateTime(__DATE__, __TIME__, tm)) {
        Serial.println("Could not parse date/time");
        return false;
      }

      if (!RTC.write(tm)) {
        Serial.println("Could not write timestruct into RTC");
      }
    } else {
      Serial.println("DS1307 hardware error.");
      return false;
    }
    if (attempt == 10) {
      return false;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DisplayController
//

#define UPDATE_DISPLAY_STATE_INTERVAL_MS 5000

class DisplayController {
  enum State {
    DisplayInitLogo,
    DisplayDT,
    DisplayTH,
    DisplayAIQ,
  };

public:
  void init();
  void showInitLogo();

  void setTH(const float& t, const float& h);
  void setAIQ(uint16_t eco2, uint16_t tvoc);

  void tick();

private:
  void showDT();
  void showTH();
  void showAIQ();
 
private:  
  static LiquidCrystal_I2C lcd;
  unsigned long lastUpdateMs = 0L;
  State state = DisplayInitLogo;
  float temperature = 0.f;
  float humidity = 0.f;
  uint16_t eco2 = 0;
  uint16_t tvoc = 0;
};

LiquidCrystal_I2C DisplayController::lcd(0x27, 16, 2);

void DisplayController::init() {
  lcd.init();
  lcd.backlight();  
}

void DisplayController::showInitLogo() {
  lcd.setCursor(0, 0);
  lcd.print("Weather Station");
  lcd.setCursor(0, 1);  // col, row
  lcd.print("  2nd edition  ");
}

void DisplayController::setTH(const float& t, const float& h) {
  temperature = t;
  humidity = h;
}

void DisplayController::setAIQ(uint16_t eco2, uint16_t tvoc) {
  this->eco2 = eco2;
  this->tvoc = tvoc;
}

void DisplayController::tick() {
  if (millis() - lastUpdateMs > UPDATE_DISPLAY_STATE_INTERVAL_MS) {

    switch (state) {
      case DisplayInitLogo:
        state = DisplayDT;
        break;
      case DisplayDT:
        showDT();
        state = DisplayTH;
        break;
      case DisplayTH:
        showTH();
        state = DisplayAIQ;
        break;
      case DisplayAIQ:
        showAIQ();
        state = DisplayDT;
        break;
      default:
        break;
    }

    lastUpdateMs = millis();
  }
}

void DisplayController::showDT() {
  tmElements_t tm;
  if (RTC.read(tm)) {
//    char buf[24] = { 0 };
//    sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d",
//      tm.Day, tm.Month, tm.Year + 1970, tm.Hour, tm.Minute, tm.Second);
//    Serial.println(buf);

    char buf[18] = { 0 };
    lcd.clear();
    sprintf(buf, "Date %02d/%02d/%04d", tm.Day, tm.Month, tm.Year + 1970);
    lcd.setCursor(0, 0);
    lcd.print(buf);

    sprintf(buf, "Time %02d:%02d:%02d", tm.Hour, tm.Minute, tm.Second);
    lcd.setCursor(0, 1);  // col, row
    lcd.print(buf);
    
  } else {
    Serial.println("RTC read failed.");
  }  
}

void DisplayController::showTH() {
//  char buf[64] = { 0 };
//  sprintf(buf, "temperature: %0.2f degrees, humidity: %0.2f percentage", ::temperature, ::humidity);
//  Serial.println(buf);

  char buf[18] = { 0 };
  lcd.clear();
  sprintf(buf, "T %0.2f degrees", temperature);
  lcd.setCursor(0, 0);
  lcd.print(buf);

  sprintf(buf, "H %0.2f %%", humidity);
  lcd.setCursor(0, 1);  // col, row
  lcd.print(buf);
}

void DisplayController::showAIQ() {
  // Serial.printf("CCS811 - eCO2: %u ppm, eTVOC: %u ppb\n", eco2, etvoc);
  char buf[18] = { 0 };
  lcd.clear();
  sprintf(buf, "eCO2 %u ppm", eco2);
  lcd.setCursor(0, 0);
  lcd.print(buf);

  sprintf(buf, "TVOC %u ppb", tvoc);
  lcd.setCursor(0, 1);  // col, row
  lcd.print(buf);    
}

DisplayController displayController;


////////////////////////////////////////////////////////////////////////////////
// UpdateTime
//

#define SYNC_TIME_INTERVAL_MS (3600 * 1000)

void syncTime() {
  ntpClient.update();
  time_t epochTime = ntpClient.getEpochTime();
  struct tm* ptm = gmtime(&epochTime);

/*
  Serial.printf("NTP\t%04d/%02d/%02d - %02d:%02d:%02d\n",
    ptm->tm_year + 1900, ptm->tm_mon, ptm->tm_mday, 
    ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
    */

  tmElements_t tm;
  tm.Day = ptm->tm_mday;
  tm.Month = ptm->tm_mon + 1;
  tm.Year = ptm->tm_year - 70; // ptm->tm_year + 1900 - 1970

  tm.Hour = ptm->tm_hour;
  tm.Minute = ptm->tm_min;
  tm.Second = ptm->tm_sec;

  if (!RTC.write(tm)) {
    Serial.println("Could not write timestruct into RTC");
  }
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  // buzzerController.setup();  

  // enable I2C
  Wire.begin();
  delay(100);

  Serial.println("Going to setup RTC...");
  if (!setupRTC()) {
    Serial.println("RTC setup failed.");
    while (1) ;
  }  
  Serial.println("RTC setup - OK.");

  byte attempt = 0;
  while (!aht10.begin() && attempt++ < 10) {
    delay(1000);
  }
  if (attempt == 10) {
    Serial.println("AHT10 begin failed.");
    while (1) ;
  }
  Serial.println("AHT10 setup - OK");
  
  Serial.print("CCS811 library version: ");
  Serial.println(CCS811_VERSION);

// TO DO: check - is it really needed ?
  //ccs811.set_i2cdelay(500);  
  
  if (!ccs811.begin()) {
    Serial.println("Could not initialize CCS811 library!");
    while (1) ;
  }

  int v = ccs811.hardware_version();
  if (v != -1)
    Serial.printf("CCS811 hardware version: %02x\n", v);
  else
    Serial.println("CCS811 hardware version - couldn't get due to I2C error");

  v = ccs811.bootloader_version();
  if (v != -1)
    Serial.printf("CCS811 bootloader version: %04x\n", v);
  else
    Serial.println("CCS811 bootloader version - couldn't get due to I2C error");

  v = ccs811.application_version();
  if (v != -1)
    Serial.printf("CCS811 application version: %04x\n", v);
  else
    Serial.println("CCS811 application version - couldn't get due to I2C error");        

//  if (!ccs811.start(CCS811_MODE_60SEC)) {
//    Serial.println("CCS811 start failed");
//  }

  if (!ccs811.start(CCS811_MODE_1SEC)) {
    Serial.println("CCS811 start failed");
  }

//  WiFi.begin(SSID, PKEY);
//  while (WiFi.status() != WL_CONNECTED) {
//    delay(1000);
//    Serial.println("Connecting to wifi ...");
//  }

  displayController.init();
  displayController.showInitLogo();

  wifiManager.autoConnect();
  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);

  syncTime();

  Serial.println("Setup complete.");
}

void publishTH(float temperature, float humidity) {
  if (mqttClient.connected()) {
    char str[16];
    sprintf(str, "%f", temperature);
    mqttClient.publish(TEMPERATURE_TOPIC, str, true);
    sprintf(str, "%f", humidity);
    mqttClient.publish(HUMIDITY_TOPIC, str, true);
  }
}

void publishAIQ(uint16_t eco2, uint16_t etvoc) {
  if (mqttClient.connected()) {
    char str[8];
    sprintf(str, "%u", eco2);
    mqttClient.publish(ECO2_TOPIC, str, true);
    sprintf(str, "%u", etvoc);
    mqttClient.publish(TVOC_TOPIC, str, true);
  }
}

void loop() {

  if (!mqttClient.connected()) {
    if (WiFi.status() == WL_CONNECTED) {
      mqttConnectToBroker();
    }
  }

  unsigned long currTime = millis();

  if ((currTime - lastTimeReadTH) > AHT10_SAMPLING_PERIOD) {
    lastTimeReadTH = currTime;
    float temperature = 0.f, humidity = 0.f;
    if (readAHT10(temperature, humidity)) {
      Serial.printf("temperature: %.2f celsius degrees, humidity: %.2f %%\n", temperature, humidity);
      if (!isnan(humidity) && !isnan(temperature)) {
        ccs811.set_envdata_Celsius_percRH(temperature, humidity);
        displayController.setTH(temperature, humidity);
        publishTH(temperature, humidity);
      }
    }
  }

  if ((currTime - lastTimeReadTVOC) > CCS811_SAMPLING_PERIOD) {    
    uint16_t eco2, etvoc, errstat, raw;
    ccs811.read(&eco2, &etvoc, &errstat, &raw);
    lastTimeReadTVOC = currTime;

    if (errstat == CCS811_ERRSTAT_OK) {
      Serial.printf("CCS811 - eCO2: %u ppm, eTVOC: %u ppb\n", eco2, etvoc);
      publishAIQ(eco2, etvoc);
      displayController.setAIQ(eco2, etvoc);
      alarmController.registerMeasurements(etvoc, eco2);
    } else if (errstat == CCS811_ERRSTAT_OK_NODATA) {
      Serial.println("CCS811 - waiting for new data.");
    } else if (errstat & CCS811_ERRSTAT_I2CFAIL) {
      Serial.println("CCS811 - I2C error.");
    } else {
      Serial.printf("CCS811 - error %04x (%s)\n", errstat, ccs811.errstat_str(errstat));
    }
  }

  // buzzerController.tick(); 

  displayController.tick();

  if (currTime - lastTimeSync > SYNC_TIME_INTERVAL_MS) {
    lastTimeSync = currTime;
    syncTime();
  }
}
