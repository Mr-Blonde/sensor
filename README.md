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

#define WIFI_SSID  *- SSID of your network*

#define WIFI_PASS  *- WPA2 key*

#define OTA_PASS  *- over the air update password (required for OTA firmware updates)*

#define MQTT_USER   *- MQTT username*

#define MQTT_PASSWORD   *- MQTT password*

#define MQTT_SERVER_IP_ADDR   *- IP address of the MQTT server*

#define SENSOR_DOMAIN   *- domain name for the sensor (or .local)*


