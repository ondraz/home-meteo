#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include "Adafruit_NeoPixel.h"
#include "SparkFun_SCD4x_Arduino_Library.h"
#include <NTPClient.h>
#include <WiFiUdp.h>

#include <PM1006.h>

#define PIN_FAN 12
#define RXD2 16
#define TXD2 17
static PM1006 pm1006(&Serial2);

const char *ssid = "YOUR_WIFI";
const char *password = "PASSWORD";

// This sends data to tmep.cz service, setup your own :)
// Endpoint for PM values
// String serverNamePM = "http://dolany-pm.tmep.cz/index.php?";
String serverNamePM = "http://";
String GUID_PM = "pm";

// This sends data to tmep.cz service, setup your own :)
// Endpoint for CO2 values
// String serverNameCO2 = "http://dolany.tmep.cz/index.php?";
String serverNameCO2 = "http://";
String GUID_CO2 = "scd41";

SCD4x SCD41;

#define BRIGHTNESS 5
#define PIN_LED 25
#define PM_LED 1
#define TEMP_LED 2
#define CO2_LED 3
Adafruit_NeoPixel leds = Adafruit_NeoPixel(3, PIN_LED, NEO_GRB + NEO_KHZ800);

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void sendHttpGet(String httpGet)
{
  HTTPClient http;

  String serverPath = httpGet;

  http.begin(serverPath.c_str());

  int httpResponseCode = http.GET();

  if (httpResponseCode > 0)
  {
    Serial.print("HTTP response: ");
    Serial.println(httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  }
  else
  {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}

void setColorWS(byte r, byte g, byte b, int led)
{
  uint32_t rgb;
  rgb = leds.Color(r, g, b);
  leds.setPixelColor(led - 1, rgb);
  leds.show();
}

void ledAlert(int led)
{
  Serial.print("Alert on: ");
  Serial.println(led);

  for (int i = 0; i < 10; i++)
  {
    leds.setBrightness(255);
    setColorWS(255, 0, 0, led);
    delay(200);
    leds.setBrightness(BRIGHTNESS);
    setColorWS(0, 0, 0, led);
    delay(200);
  }
}

void setup()
{
  pinMode(PIN_FAN, OUTPUT);
  leds.begin(); // WS2718
  leds.setBrightness(BRIGHTNESS);

  Serial.begin(115200);
  Wire.begin();

  delay(10);

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  if (SCD41.begin(false, true) == false)
  {
    Serial.println("SCD41 not found.");
    Serial.println("Check wiring.");
    while (1)
      ledAlert(CO2_LED);
  }

  if (SCD41.startLowPowerPeriodicMeasurement() == true)
  {
    Serial.println("Low power mode enabled.");
  }

  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  timeClient.begin();
}

void loop()
{
  timeClient.update();
  const int hours = timeClient.getHours();
  const bool day = hours >= 6 && hours < 20;
  Serial.print("Hours: ");
  Serial.println(hours);

  while (!SCD41.readMeasurement())
  {
    delay(1);
  }

  float temperature = SCD41.getTemperature();
  float humidity = SCD41.getHumidity();
  uint16_t co2 = SCD41.getCO2();

  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.println(" degC");
  Serial.print("Humidity: ");
  Serial.print(humidity);
  Serial.println("% rH");
  Serial.print("CO2: ");
  Serial.print(co2);
  Serial.println(" ppm");

  uint16_t pm2_5;
  if (day) {
    digitalWrite(PIN_FAN, HIGH);
    Serial.println("Fan ON");
    delay(30000);

    while (!pm1006.read_pm25(&pm2_5))
    {
      delay(1);
    }

    delay(100);
    digitalWrite(PIN_FAN, LOW);
    Serial.println("Fan OFF");

    Serial.print("PM2.5: ");
    Serial.print(pm2_5);
    Serial.println(" ppm");
  }

  if (WiFi.status() == WL_CONNECTED)
  {
    String serverPathCO2 = serverNameCO2 + "" + GUID_CO2 + "=" + temperature + "&humV=" + humidity + "&CO2=" + co2;
    sendHttpGet(serverPathCO2);

    delay(100);

    if (day) {
      String serverPathPM = serverNamePM + "" + GUID_PM + "=" + pm2_5;
      sendHttpGet(serverPathPM);
    }
  }
  else
  {
    Serial.println("Wi-Fi disconnected");
  }

  if (day) {
    if (co2 < 1000)
    {
      setColorWS(0, 255, 0, CO2_LED);
    }

    if ((co2 >= 1000) && (co2 < 1200))
    {
      setColorWS(128, 255, 0, CO2_LED);
    }

    if ((co2 >= 1200) && (co2 < 1500))
    {
      setColorWS(255, 255, 0, CO2_LED);
    }

    if ((co2 >= 1500) && (co2 < 2000))
    {
      setColorWS(255, 128, 0, CO2_LED);
    }

    if (co2 >= 2000)
    {
      setColorWS(255, 0, 0, CO2_LED);
    }

    if (temperature < 23.0)
    {
      setColorWS(0, 0, 255, TEMP_LED);
    }

    if ((temperature >= 23.0) && (temperature < 28.0))
    {
      setColorWS(0, 255, 0, TEMP_LED);
    }

    if (temperature >= 28.0)
    {
      setColorWS(255, 0, 0, TEMP_LED);
    }

    if (pm2_5 < 30)
    {
      setColorWS(0, 255, 0, PM_LED);
    }

    if ((pm2_5 >= 30) && (pm2_5 < 40))
    {
      setColorWS(128, 255, 0, PM_LED);
    }

    if ((pm2_5 >= 40) && (pm2_5 < 80))
    {
      setColorWS(255, 255, 0, PM_LED);
    }

    if ((pm2_5 >= 80) && (pm2_5 < 90))
    {
      setColorWS(255, 128, 0, PM_LED);
    }

    if (pm2_5 >= 90)
    {
      setColorWS(255, 0, 0, PM_LED);
    }
  } else {
    setColorWS(0, 0, 0, CO2_LED);
    setColorWS(0, 0, 0, TEMP_LED);
    setColorWS(0, 0, 0, PM_LED);
  }

  esp_sleep_enable_timer_wakeup(10 * 60 * 1000000);
  Serial2.flush();
  Serial.flush();
  delay(100);
  esp_deep_sleep_start();
}
