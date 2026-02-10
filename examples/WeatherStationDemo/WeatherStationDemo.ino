/**The MIT License (MIT)

Copyright (c) 2018 by Daniel Eichhorn - ThingPulse

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at https://thingpulse.com
*/

#include <Arduino.h>

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <coredecls.h>                  // settimeofday_cb()
#else
#include <WiFi.h>
#endif
#include <ESP8266HTTPClient.h>
#include <JsonListener.h>

#include <ArduinoOTA.h>
#include <WiFiManager.h>
#include <Adafruit_Sensor.h>
#include <simpleDSTadjust.h>
#include <DHT.h>
#include <DHT_U.h>
#include <Ticker.h>

// time
#include <time.h>                       // time() ctime()
#include <sys/time.h>                   // struct timeval

#include "SSD1306Wire.h"
#include "OLEDDisplayUi.h"
#include "Wire.h"
#include "OpenWeatherMapCurrent.h"
#include "OpenWeatherMapForecast.h"
#include "WeatherStationFonts.h"
#include "WeatherStationImages.h"
#include "DSEG7Classic-BoldFont.h"

/***************************
 * Begin Settings
 **************************/

// WIFI
// const char* WIFI_SSID = "yourssid";
// const char* WIFI_PWD = "yourpassw0rd";

// Initialize the temperature/ humidity sensor

// DHT Settings
#define DHTPIN D1 // NodeMCU
#define DHTTYPE DHT22   // DHT 22  (AM2302), AM2321
#define DHTTEXT "DHT22"

DHT dht(DHTPIN, DHTTYPE);
float humidity = 0.0;
float temperature = 0.0;
// flag changed in the ticker function every 1 minute
bool readyForDHTUpdate = false;

char FormattedTemperature[10];
char FormattedHumidity[10];

#define HOSTNAME "ESP8266-OTA-"
#define NTP_SERVERS "us.pool.ntp.org", "time.nist.gov", "pool.ntp.org"
#define STYLE_24HR
#define TZ              0       // (utc+) TZ in hours
#define DST_MN          60      // use 60mn for summer time in some countries

// Setup
const int UPDATE_INTERVAL_SECS = 20 * 60; // Update every 20 minutes

// Display Settings
const int I2C_DISPLAY_ADDRESS = 0x3c;
#if defined(ESP8266)
const int SDA_PIN = D5;
const int SDC_PIN = D6;
#else
const int SDA_PIN = 5; //D3;
const int SDC_PIN = 4; //D4;
#endif


// OpenWeatherMap Settings
// Sign up here to get an API key:
// https://docs.thingpulse.com/how-tos/openweathermap-key/
String OPEN_WEATHER_MAP_APP_ID = "xxxxx";
/*
Use the OWM GeoCoder API to find lat/lon for your city: https://openweathermap.org/api/geocoding-api
Or use any other geocoding service.
Or go to https://openweathermap.org, search for your city and monitor the calls in the browser dev console :)
 */
// Example: Leganés, Switzerland
// https://api.openweathermap.org/geo/1.0/direct?q=Legan%C3%A9s,28918&limit=5&appid=3bec5c63928fb399617fc58fa2b5c81a
float OPEN_WEATHER_MAP_LOCATION_LAT = 40.3281942;
float OPEN_WEATHER_MAP_LOCATION_LON = -3.76527;

// Pick a language code from this list:
// Arabic - ar, Bulgarian - bg, Catalan - ca, Czech - cz, German - de, Greek - el,
// English - en, Persian (Farsi) - fa, Finnish - fi, French - fr, Galician - gl,
// Croatian - hr, Hungarian - hu, Italian - it, Japanese - ja, Korean - kr,
// Latvian - la, Lithuanian - lt, Macedonian - mk, Dutch - nl, Polish - pl,
// Portuguese - pt, Romanian - ro, Russian - ru, Swedish - se, Slovak - sk,
// Slovenian - sl, Spanish - es, Turkish - tr, Ukrainian - ua, Vietnamese - vi,
// Chinese Simplified - zh_cn, Chinese Traditional - zh_tw.
String OPEN_WEATHER_MAP_LANGUAGE = "es";
const uint8_t MAX_FORECASTS = 4;

const boolean IS_METRIC = true;

// Adjust according to your language
const String WDAY_NAMES[] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"};
const String MONTH_NAMES[] = {"JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC"};

/***************************
 * End Settings
 **************************/
 // Initialize the oled display for address 0x3c
 // sda-pin=14 and sdc-pin=12
 SSD1306Wire     display(I2C_DISPLAY_ADDRESS, SDA_PIN, SDC_PIN);
 OLEDDisplayUi   ui( &display );

OpenWeatherMapCurrentData currentWeather;
OpenWeatherMapCurrent currentWeatherClient;

OpenWeatherMapForecastData forecasts[MAX_FORECASTS];
OpenWeatherMapForecast forecastClient;

#define TZ_MN           ((TZ)*60)
#define TZ_SEC          ((TZ)*3600)
#define DST_SEC         ((DST_MN)*60)
time_t now;
struct dstRule StartRule = {"CEST", Last, Sun, Mar, 2, 3600}; // Central European Summer Time = UTC/GMT +2 hours
struct dstRule EndRule = {"CET", Last, Sun, Oct, 2, 0};   

simpleDSTadjust dstAdjusted(StartRule, EndRule);

// flag changed in the ticker function every 10 minutes
bool readyForWeatherUpdate = false;

String lastUpdate = "--";

Ticker ticker;

long timeSinceLastWUpdate = 0;

//declaring prototypes
void configModeCallback (WiFiManager *myWiFiManager);
void drawProgress(OLEDDisplay *display, int percentage, String label);
void drawOtaProgress(unsigned int, unsigned int);
void updateData(OLEDDisplay *display);
void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex);
void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y);
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state);
void setReadyForWeatherUpdate();


// Add frames
// this array keeps function pointers to all frames
// frames are the single views that slide from right to left
FrameCallback frames[] = { drawDateTime, drawCurrentWeather, drawIndoor, drawForecast };
int numberOfFrames = 4;

OverlayCallback overlays[] = { drawHeaderOverlay };
int numberOfOverlays = 1;

String ESPChipID(void) {
#if defined(ESP8266)
  return String(ESP.getChipId(), HEX);
#else
  uint64_t EspChipID = ESP.getEfuseMac();
  char chipID[14];
  sprintf(chipID, "%04X%08X", (uint16_t)(EspChipID >> 32), (uint32_t)EspChipID);
  return chipID;

#endif
}
void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println();

  // initialize dispaly
  display.init();
  display.clear();
  display.display();

  // display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setContrast(255);

   //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;
  // Uncomment for testing wifi manager
  //wifiManager.resetSettings();
  wifiManager.setAPCallback(configModeCallback);

  //or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();

  //Manual Wifi
  //WiFi.begin(WIFI_SSID, WIFI_PWD);
  String hostname(HOSTNAME);
  hostname += ESPChipID();
#if defined(ESP8266)
  WiFi.hostname(hostname);
#else
  MDNS.begin((char *)&hostname);
#endif

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    display.clear();
    display.drawString(64, 10, "Connecting to WiFi");
    display.drawXbm(46, 30, 8, 8, counter % 3 == 0 ? activeSymbole : inactiveSymbole);
    display.drawXbm(60, 30, 8, 8, counter % 3 == 1 ? activeSymbole : inactiveSymbole);
    display.drawXbm(74, 30, 8, 8, counter % 3 == 2 ? activeSymbole : inactiveSymbole);
    display.display();

    counter++;
  }
  // Get time from network time service
  configTime(TZ_SEC, DST_SEC, NTP_SERVERS);

  ui.setTargetFPS(30);

  ui.setActiveSymbol(activeSymbole);
  ui.setInactiveSymbol(inactiveSymbole);

  // You can change this to
  // TOP, LEFT, BOTTOM, RIGHT
  ui.setIndicatorPosition(BOTTOM);
//  ui.disableIndicator();
  // Defines where the first frame is located in the bar.
  ui.setIndicatorDirection(LEFT_RIGHT);

  // You can change the transition that is used
  // SLIDE_LEFT, SLIDE_RIGHT, SLIDE_TOP, SLIDE_DOWN
  ui.setFrameAnimation(SLIDE_LEFT);

  ui.setFrames(frames, numberOfFrames);

  ui.setOverlays(overlays, numberOfOverlays);

  // Inital UI takes care of initalising the display too.
  ui.init();
  // Ensure orientation after ui.init()
  display.flipScreenVertically();

  // Initialize DHT sensor
  Serial.println("Initializing DHT22 sensor on pin D1...");
  dht.begin();
  delay(500); // Give sensor time to stabilize
  Serial.println("DHT22 sensor initialized");

    // Setup OTA
  Serial.println("Hostname: " + hostname);
  ArduinoOTA.setHostname((const char *)hostname.c_str());
  ArduinoOTA.onProgress(drawOtaProgress);
  ArduinoOTA.begin();

  Serial.println("");

  updateData(&display);
  
  ticker.attach(UPDATE_INTERVAL_SECS, setReadyForWeatherUpdate);
  ticker.attach(60, setReadyForDHTUpdate);
}

void loop() {

  if (millis() - timeSinceLastWUpdate > (1000L*UPDATE_INTERVAL_SECS)) {
    setReadyForWeatherUpdate();
    timeSinceLastWUpdate = millis();
  }

  if (readyForWeatherUpdate && ui.getUiState()->frameState == FIXED) {
    updateData(&display);
  }
  
  if (readyForDHTUpdate && ui.getUiState()->frameState == FIXED)
    updateDHT();

  int remainingTimeBudget = ui.update();

  if (remainingTimeBudget > 0) {
    // You can do some work here
    // Don't do stuff if you are below your
    // time budget.
    ArduinoOTA.handle();
    delay(remainingTimeBudget);
  }


}

void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "Wifi Manager");
  display.drawString(64, 20, "Please connect to AP");
  display.drawString(64, 30, myWiFiManager->getConfigPortalSSID());
  display.drawString(64, 40, "To setup Wifi Configuration");
  display.display();
}

void drawProgress(OLEDDisplay *display, int percentage, String label) {
  display->clear();
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64, 10, label);
  display->drawProgressBar(2, 28, 124, 10, percentage);
  display->display();
}

void drawOtaProgress(unsigned int progress, unsigned int total) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.setFont(ArialMT_Plain_10);
  display.drawString(64, 10, "OTA Update");
  display.drawProgressBar(2, 28, 124, 12, progress / (total / 100));
  display.display();
}

void updateData(OLEDDisplay *display) {
  drawProgress(display, 10, "Updating time...");
  drawProgress(display, 30, "Updating weather...");
  currentWeatherClient.setMetric(IS_METRIC);
  currentWeatherClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  currentWeatherClient.updateCurrent(&currentWeather, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_LAT, OPEN_WEATHER_MAP_LOCATION_LON);
  drawProgress(display, 50, "Updating forecasts...");
  forecastClient.setMetric(IS_METRIC);
  forecastClient.setLanguage(OPEN_WEATHER_MAP_LANGUAGE);
  uint8_t allowedHours[] = {12};
  forecastClient.setAllowedHours(allowedHours, sizeof(allowedHours));
  forecastClient.updateForecasts(forecasts, OPEN_WEATHER_MAP_APP_ID, OPEN_WEATHER_MAP_LOCATION_LAT, OPEN_WEATHER_MAP_LOCATION_LON, MAX_FORECASTS);

  // Teleplot publish: OpenWeatherMap readings as floats with one decimal
  Serial.print(">External_temp:"); Serial.println(String(currentWeather.temp, 1));
  

  readyForWeatherUpdate = false;
  drawProgress(display, 100, "Done...");
  delay(1000);
}

// Called every 1 minute
void updateDHT() {
  Serial.println("\n=== DHT22 Update Started ===");
  Serial.print("Pin: ");
  Serial.println(DHTPIN);
  Serial.print("Sensor Type: ");
  Serial.println(DHTTEXT);
  
  // Read humidity first (seems to be more stable)
  Serial.println("Reading humidity...");
  humidity = dht.readHumidity();
  Serial.print("Raw Humidity: ");
  Serial.println(humidity);
  
  // Read temperature
  Serial.println("Reading temperature...");
  temperature = dht.readTemperature(!IS_METRIC);
  Serial.print("Raw Temperature: ");
  Serial.println(temperature);
  
  // Check if readings are valid (NaN check)
  if (isnan(humidity)) {
    Serial.println("ERROR: Humidity reading is NaN!");
    Serial.println("Possible causes:");
    Serial.println("  - Sensor not connected or loose contact");
    Serial.println("  - Pin D1 (GPIO5) not correctly configured");
    Serial.println("  - Sensor defective");
    Serial.println("  - Insufficient power to sensor");
    Serial.println("  - Read too frequently (DHT22 needs ~2 seconds between reads)");
  } else {
    Serial.println("✓ Humidity reading is valid");
  }
  
  if (isnan(temperature)) {
    Serial.println("ERROR: Temperature reading is NaN!");
    Serial.println("Possible causes:");
    Serial.println("  - Sensor not connected or loose contact");
    Serial.println("  - Pin D1 (GPIO5) not correctly configured");
    Serial.println("  - Sensor defective");
    Serial.println("  - Insufficient power to sensor");
  } else {
    Serial.println("✓ Temperature reading is valid");
  }
  
  // Only format if values are valid
  if (!isnan(temperature) && !isnan(humidity)) {
    dtostrf(temperature, 4, 1, FormattedTemperature);
    Serial.print("DHT22 Temperature: ");
    Serial.print(FormattedTemperature);
    Serial.println(IS_METRIC ? "°C": "°F");
    
    dtostrf(humidity, 4, 1, FormattedHumidity);
    Serial.print("DHT22 Humidity: ");
    Serial.print(FormattedHumidity);
    Serial.println("%");
    
    // Teleplot format: >varName:1234 (values scaled by 10 to preserve one decimal)
    // Use formatted strings to keep consistent display formatting
    float ft = atof(FormattedTemperature);
    float fh = atof(FormattedHumidity);
    // Teleplot: send as float with one decimal (e.g. 23.4)
    Serial.print(">temperature:"); Serial.println(String(ft, 1));
    Serial.print(">humidity:");  Serial.println(String(fh, 1));
  } else {
    Serial.println("⚠ Skipping display formatting due to NaN values");
    strcpy(FormattedTemperature, "N/A");
    strcpy(FormattedHumidity, "N/A");
  }
  
  Serial.println("=== DHT22 Update Completed ===");
  readyForDHTUpdate = false;
}


void drawDateTime(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  char *dstAbbrev;
  char time_str[11];
  time_t now = dstAdjusted.time(&dstAbbrev);
  struct tm * timeinfo = localtime (&now);

  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  String date = ctime(&now);
  date = date.substring(0,11) + String(1900+timeinfo->tm_year);
  // int textWidth = display->getStringWidth(date);
  // display->drawString(64 + x, 5 + y, date);
  display->setFont(DSEG7_Classic_Bold_21);
  display->setTextAlignment(TEXT_ALIGN_RIGHT);

#ifdef STYLE_24HR
  sprintf(time_str, "%02d:%02d:%02d\n",timeinfo->tm_hour, timeinfo->tm_min, timeinfo->tm_sec);
  display->drawString(108 + x, 19 + y, time_str);
#else
  int hour = (timeinfo->tm_hour+11)%12+1;  // take care of noon and midnight
  sprintf(time_str, "%2d:%02d:%02d\n",hour, timeinfo->tm_min, timeinfo->tm_sec);
  display->drawString(101 + x, 19 + y, time_str);
#endif

  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->setFont(ArialMT_Plain_10);
#ifdef STYLE_24HR
  sprintf(time_str, "%s", dstAbbrev);
  display->drawString(108 + x, 27 + y, time_str);  // Known bug: Cuts off 4th character of timezone abbreviation
#else
  sprintf(time_str, "%s\n%s", dstAbbrev, timeinfo->tm_hour>=12?"pm":"am");
  display->drawString(102 + x, 18 + y, time_str);
#endif

}

void drawCurrentWeather(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(64 + x, 38 + y, currentWeather.description);

  display->setFont(ArialMT_Plain_24);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(60 + x, 5 + y, temp);

  display->setFont(Meteocons_Plain_36);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->drawString(32 + x, 0 + y, currentWeather.iconMeteoCon);
}


void drawForecast(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  drawForecastDetails(display, x, y, 0);
  drawForecastDetails(display, x + 44, y, 1);
  drawForecastDetails(display, x + 88, y, 2);
}

void drawForecastDetails(OLEDDisplay *display, int x, int y, int dayIndex) {
  time_t observationTimestamp = forecasts[dayIndex].observationTime;
  struct tm* timeInfo;
  timeInfo = localtime(&observationTimestamp);
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y, WDAY_NAMES[timeInfo->tm_wday]);

  display->setFont(Meteocons_Plain_21);
  display->drawString(x + 20, y + 12, forecasts[dayIndex].iconMeteoCon);
  String temp = String(forecasts[dayIndex].temp, 0) + (IS_METRIC ? "°C" : "°F");
  display->setFont(ArialMT_Plain_10);
  display->drawString(x + 20, y + 34, temp);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
}

void drawIndoor(OLEDDisplay *display, OLEDDisplayUiState* state, int16_t x, int16_t y) {
  display->setTextAlignment(TEXT_ALIGN_CENTER);
  display->setFont(ArialMT_Plain_10);
  display->drawString(64 + x, 0, DHTTEXT " Indoor Sensor" );
  display->setFont(ArialMT_Plain_16);
  dtostrf(temperature,4, 1, FormattedTemperature);
  display->drawString(64+x, 12, "Temp: " + String(FormattedTemperature) + (IS_METRIC ? "°C": "°F"));
  dtostrf(humidity,4, 1, FormattedHumidity);
  display->drawString(64+x, 30, "Humidity: " + String(FormattedHumidity) + "%");

}
void drawHeaderOverlay(OLEDDisplay *display, OLEDDisplayUiState* state) {
  now = time(nullptr);
  struct tm* timeInfo;
  timeInfo = localtime(&now);
  
  char buff[14];
  sprintf_P(buff, PSTR("%02d:%02d"), timeInfo->tm_hour, timeInfo->tm_min);

  display->setColor(WHITE);
  display->setFont(ArialMT_Plain_10);
  display->setTextAlignment(TEXT_ALIGN_LEFT);
  display->drawString(0, 54, String(buff));
  display->setTextAlignment(TEXT_ALIGN_RIGHT);
  String temp = String(currentWeather.temp, 1) + (IS_METRIC ? "°C" : "°F");
  display->drawString(128, 54, temp);
  display->drawHorizontalLine(0, 52, 128);
}

void setReadyForWeatherUpdate() {
  Serial.println("Setting readyForUpdate to true");
  readyForWeatherUpdate = true;
}

void setReadyForDHTUpdate() {
  Serial.println("Setting readyForDHTUpdate to true");
  readyForDHTUpdate = true;
}
