#include <DS1307RTC.h>

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

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  while (!Serial) ;
  delay(200);

  tmElements_t tm;
  uint8_t attempt = 10;
  while (!RTC.read(tm)) {
    ++attempt;
    if (RTC.chipPresent()) {
      Serial.println("The DS1307 is stopped. Going to setup it ...");      
      if (!parseDateTime(__DATE__, __TIME__, tm)) {
        Serial.println("Could not parse date/time");
        break;
      }

      if (!RTC.write(tm)) {
        Serial.println("Could not write timestruct into RTC");
      }
    } else {
      Serial.println("DS1307 hardware error.");
      break;      
    }
    if (attempt == 10) {
      break;
    }
  }
}


void loop() {
  // put your main code here, to run repeatedly:

  tmElements_t tm;
  if (RTC.read(tm)) {
    char buf[24] = { 0 };
    sprintf(buf, "%02d/%02d/%04d %02d:%02d:%02d",
      tm.Day, tm.Month, tm.Year + 1970, tm.Hour, tm.Minute, tm.Second);
    Serial.println(buf);
  } else {
    Serial.println("RTC read failed.");
    delay(9000);
  }
  delay(1000);
}
