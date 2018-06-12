# sensor
ESP8266 based sensor

Temperature/Humidity/Pressure sensor node for indoor and outdoor use. 

## Features
- SHT30 supported
- BME280 supported
- OLED display supported
- Battery monitoring supported (for wemos battery shield)
- sensor reading response via HTML (port 80 - /index.html)
- sensor reading response via XML (port 80 - /readings.xml)
- periodically publishing sensor readings via MQTT

## Requires config.h

Config.h must contain the following #defines

`#define WIFI_SSID`  *- SSID of your network*

`#define WIFI_PASS`  *- WPA2 key*

`#define OTA_PASS`  *- over the air update password (required for OTA firmware updates)*

`#define MQTT_USER`   *- MQTT username*

`#define MQTT_PASSWORD`   *- MQTT password*

`#define MQTT_SERVER_IP_ADDR`   *- IP address of the MQTT server*

`#define SENSOR_DOMAIN`   *- domain name for the sensor (or .local)*


## Libraries
- *All versions*
  - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer)
- *For ESP8266*
  - [ESP8266 Arduino core](https://github.com/esp8266/Arduino)
  - [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP)
- *For ESP32*
  - [ESP32 Arduino core](https://github.com/espressif/arduino-esp32)
  - [AsyncTCP](https://github.com/me-no-dev/AsyncTCP)
- *For OLED display*
  - [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)
  - [Adafruit_SSD1306](https://github.com/mcauser/Adafruit_SSD1306/tree/esp8266-64x48)
- *For SHT30*
  - [arduino-sht](https://github.com/Sensirion/arduino-sht)
- *For BME280*
  - [Adafruit_Sensor](https://github.com/adafruit/Adafruit_Sensor)
  - [Adafruit_BME280](https://github.com/adafruit/Adafruit_BME280_Library)


