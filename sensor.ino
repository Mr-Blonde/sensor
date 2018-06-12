/*
 * sensor
 * version 1.1.0 (06/02/2018)
 *
 * 
 * Please note: If you use a brand new device or have previously erased the flash, uncomment the SPIFFS.format() line withing void setup() below.
 *              The first boot/startup of the device will take quite a wile (depending on the devices flash size - 16MByte will take a couple of minutes for example)!
 *              Make sure to comment the line out, once SPIFFS has been formated and reflash the device. Otherwise you will losse your flash content every time it boots.
 */


/*
 * CHANGE SENSOR_ID
 *  1 - muc01sensor
 *  2 - muc02sensor
 *  3 - muc03sensor
 *  
 *  99 - mucXXsensor / Development sensor
 */
 
#define SENSOR_ID 3
#define DEVICE_VER "1.2.0"

/*
 * END CHANGE
 */

#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#elif ESP8266
  #include <ESP8266WiFi.h>
  #include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <ArduinoJson.h>
#include "credentials.h"

// Depends on SENSOR_ID
#if SENSOR_ID == 1                  // outside bme280 sensor
  #define DEVICE_FQN "muc01sensor"
  #define DEVICE_FQN_SHORT "muc01sens."
  #define SENSOR_TYPE "outdoor"
  // feature
  #define BME280
  #define BATTERY_MONITOR
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/outside_1";
  
#elif SENSOR_ID == 2                // network cabinet sht30 sensor
  #define DEVICE_FQN "muc02sensor"
  #define DEVICE_FQN_SHORT "muc02sens."
  #define SENSOR_TYPE "indoor"
  // feature
  #define OLED_DISPLAY
  #define SHT30
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/cabinet_1";
  
#elif SENSOR_ID == 3                // network cabinet sht30 sensor
  #define DEVICE_FQN "muc03sensor"
  #define DEVICE_FQN_SHORT "muc03sens."
  #define SENSOR_TYPE "indoor"
  // feature
  #define OLED_DISPLAY
  #define SHT30
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/cabinet_2";
  
#elif SENSOR_ID == 99                // DEVELOPMENT SENSOR
  #define DEVICE_FQN "mucXXsensor"
  #define DEVICE_FQN_SHORT "mucXXsens."
  #define SENSOR_TYPE "DEVELOPMENT"
  // feature
  #define SERIAL_DEBUG
//  #define OLED_DISPLAY
//  #define SHT30
  #define BME280
  #define BATTERY_MONITOR
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/DEVELOPMENT";  
#endif

//////////////////////////////
// modules configuration
//////////////////////////////

#ifdef SHT30
  #include <Wire.h>
  #include <SHTSensor.h>
  SHTSensor sht30;
#endif

#ifdef BME280
  #include <Adafruit_Sensor.h>
  #include <Adafruit_BME280.h>
  #define BME_SENSOR_ADDR 0x76      // I2C address of the sensor
  Adafruit_BME280 bme;
#endif

#ifdef OLED_DISPLAY
  #include <SPI.h>
  #ifndef SHT30         // SHT30 already includes Wire.h
    #include <Wire.h>
  #endif
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #if (SSD1306_LCDHEIGHT != 48)     // The wemos OLED shield is not supported in the original lib
    #error("Height incorrect, please fix Adafruit_SSD1306.h!");
  #endif
#endif

#ifdef BATTERY_MONITOR
  unsigned int raw_adc_value = 0;
#endif

//////////////////////////////
// END modules configuration 
//////////////////////////////

#define MQTT_VERSION MQTT_VERSION_3_1_1

#define SENSOR_READ_INTERVAL 5 * 60 * 1000
long lastMsg = 0;

#ifdef OLED_DISPLAY
  #define OLED_RESET 0  // GPIO0
  Adafruit_SSD1306 display(OLED_RESET);
#endif


// MQTT: ID, server IP, port, username and password
const char* MQTT_CLIENT_ID = DEVICE_FQN;
const char* MQTT_SERVER_IP = MQTT_SERVER_IP_ADDR;
const uint16_t MQTT_SERVER_PORT = 1883;

WiFiClient wifiClient;
PubSubClient client(wifiClient); 
AsyncWebServer server(80);

struct readings {                   // Holds the current sensor readings
  char str_temperature[10] = "0";   // trying to avoid String due to high memory fragmentation
  char str_humidity[10] = "0";
  char str_pressure[10] = "0";
  char str_battery[10] = "0";
  float temperature = 0;
  float humidity = 0;
  float pressure = 0;
  float battery = 0;
} currentReadings;
 
unsigned long previousMillis = 0;        // will store last sensor was read
const long interval = 2000;              // interval at which to read sensor


/*
 * Read all sensors
 */
void readSensors() {

  #ifdef SHT30
    sht30.readSample();               
    currentReadings.temperature = sht30.getTemperature();     // SHT30 - Read temperature as Celcius
    currentReadings.humidity = sht30.getHumidity();           // SHT30 - Read humidity (percent)
  #elif defined(BME280)
    currentReadings.temperature = bme.readTemperature();      // BME280 - Read temperature as Celcius
    currentReadings.humidity = bme.readHumidity();            // BME280 - Read humidity (percent)
    currentReadings.pressure = bme.readPressure() / 100;      // BME280 - Read pressure in Pascal (Pa) and convert it to hPa
  #endif

  #ifdef BATTERY_MONITOR
    raw_adc_value = analogRead(A0);                           // Get ADC value (connected via 100k with vbat+)
    currentReadings.battery=raw_adc_value/1023.0*4.2;         // Max reading equals 1V ADC meassurement
                                                              // and normalize to 4.2V (max. battery voltage - li-ion single cell)
  #endif
  
  // Convert float to string
  dtostrf(currentReadings.temperature, 1, 2, currentReadings.str_temperature);
  dtostrf(currentReadings.humidity, 1, 2, currentReadings.str_humidity);
  
  #ifdef BME280
    dtostrf(currentReadings.pressure, 3, 2, currentReadings.str_pressure);
  #endif

  #ifdef BATTERY_MONITOR
    dtostrf(currentReadings.battery, 1, 3, currentReadings.str_battery);
  #endif
  
  #ifdef SERIAL_DEBUG
    Serial.print("Sensor reading: T=");
    Serial.print(currentReadings.temperature);
    Serial.print("C H=");
    Serial.print(currentReadings.humidity);
    Serial.print("% P=");
    Serial.print(currentReadings.pressure);
    Serial.print("hPa V=");   
    Serial.print(currentReadings.battery);
    Serial.println("V");
  #endif 

  #ifdef OLED_DISPLAY                       // Show values on OLED display
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
 
    display.println(DEVICE_FQN_SHORT);
    display.print("\nT ");
    display.print(currentReadings.temperature);
    display.println(" C");
    display.print("H ");
    display.print(currentReadings.humidity);
    display.println(" %");

    #ifdef BME280
      display.print("P ");
      display.print(currentReadings.pressure);
      display.println(" Pa");
    #endif
    
    display.display();
  #endif
  
} // End readSensors


/*
 * Publish sensor readings via MQTT
 */
void publishData() {
  // create a JSON object
  // doc : https://github.com/bblanchon/ArduinoJson/wiki/API%20Reference
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  // INFO: the data must be converted into a string; a problem occurs when using floats...
  root["temperature"] = currentReadings.str_temperature;
  root["humidity"] = currentReadings.str_humidity;
  #ifdef BME280
    root["pressure"] = currentReadings.str_pressure;
  #endif
  #ifdef BATTERY_MONITOR
    root["battery"] = currentReadings.str_battery;
  #endif
  
  #ifdef SERIAL_DEBUG  
    root.prettyPrintTo(Serial);
    Serial.println("");
  #endif
  
  char data[200];
  root.printTo(data, root.measureLength() + 1);
  client.publish(MQTT_SENSOR_TOPIC, data, true);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  // nothing to do here
  // we are not expecting any messages...
  #ifdef SERIAL_DEBUG
    Serial.printf("MQTT msg received. Topic: %s", p_topic);
  #endif
}

/*
 * MQTT reconnect
 */
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    #ifdef SERIAL_DEBUG
      Serial.print(F("INFO: Attempting MQTT connection..."));
    #endif
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      #ifdef SERIAL_DEBUG
        Serial.println(F("INFO: connected"));
      #endif
    } else {
      #ifdef SERIAL_DEBUG
        Serial.print(F("ERROR: failed, rc="));
        Serial.print(client.state());
        Serial.println(F("DEBUG: try again in 5 seconds"));
      #endif
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

/*
 * HTML_processor
 * Evaluates the tags within the HTML page and replaces them with the correct values
 */
String HTML_processor(const String& var) {
  if(var == "DEVICE_FQN")
    return F(DEVICE_FQN);
  if(var == "TEMPERATURE_VALUE")
    return currentReadings.str_temperature;
  if(var == "HUMIDITY_VALUE")
    return currentReadings.str_humidity;
  if(var == "PRESSURE_VALUE")
    #ifdef BME280
      return currentReadings.str_pressure;
    #else
      return "-";
    #endif
  if(var == "BATTERY_VALUE")
    #ifdef BATTERY_MONITOR
      return currentReadings.str_battery;
    #else
      return "-";
    #endif
    
  if(var == "DEVICE_DOMAIN")
    return F(SENSOR_DOMAIN); 
  if(var == "FIRMWARE_VERSION")
    return F(DEVICE_VER);           
  return String();
}
/*
 * Serve main HTML page
 */
void handleRoot(AsyncWebServerRequest *request) {
  readSensors();                                                           // get sensor readings
  request->send(SPIFFS, "/index.html", String(), false, HTML_processor);   // serve index.html after handled by HTML_procesor
}

/*
 * Return sensor readings in XML format
 */
void handleGetReadings(AsyncWebServerRequest *request) {
  readSensors();                                                                          // read sensor values

  AsyncResponseStream *response = request->beginResponseStream("text/xml");               // construct the response as XML

  response->print("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>");
  response->print("<readings>");
  response->printf("<temperature>%s</temperature>", currentReadings.str_temperature);
  response->printf("<humidity>%s</humidity>", currentReadings.str_humidity);
  #ifdef BME280                                                                           // we only have pressure values in case a BME280 sensor is present
    response->printf("<pressure>%s</pressure>", currentReadings.str_pressure);
  #endif
  #ifdef BATTERY_MONITOR                                                                  // we only have the battery level if battery monitoring is present
    response->printf("<battery>%s</battery>", currentReadings.str_battery);
  #endif
  response->print("</readings>");

  request->send(response);                                                                // send response to client
}

/*
 * Handle HTTP file upload (POST)
 * Write file to SPIFFS
 */
void onUpload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
  File f;
  String fname = "/" + filename;                  // Prepend '/' to filename
  if(!index){
    #ifdef SERIAL_DEBUG
      Serial.printf("UploadStart: %s\n", fname.c_str());
    #endif
    f = SPIFFS.open(fname, "w");                  // create file
  } else {
    f = SPIFFS.open(fname, "a");                  // only append
  }
  for(size_t i=0; i<len; i++){                    // write all bytes to file
    // Serial.write(data[i]);
    f.write(data[i]);
  }
  if(final){
    #ifdef SERIAL_DEBUG
      Serial.printf("UploadEnd: %s, %u B\n", fname.c_str(), index+len);
      Serial.printf("Filsize in flash: %u B \n", f.size());
    #endif
  }
  f.flush();                                      // close and flush
  f.close();
}



/*
 * ////////////////////////////////////////////////////////////////////////////////////////////////////
 * ////////////////////////////////////////////////////////////////////////////////////////////////////
 * ////////////////////////////////////////////////////////////////////////////////////////////////////
 */


/*
 * SETUP
 */
 
void setup(void)
{
  Serial.begin(250000);

  
  #ifdef OLED_DISPLAY
    display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 64x48)
    display.display();
  #endif

  #ifdef BME280
    if (!bme.begin(BME_SENSOR_ADDR)) Serial.println("Could not find a valid BME280 sensor, check wiring!");
  #endif


  #ifdef BATTERY_MONITOR
    pinMode(A0, INPUT);
  #endif

  #ifdef SHT30
    Wire.begin();
    sht30.init();
  #endif
  
  // Connect to WiFi network
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  #ifdef SERIAL_DEBUG
    Serial.println(DEVICE_FQN);
    Serial.print(F("\n\rWorking to connect"));
  #endif
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    #ifdef SERIAL_DEBUG
      Serial.print(".");
    #endif
  }

  #ifdef SERIAL_DEBUG
    Serial.println("  done.");
  #endif


  /*
     Over the air firmware update
  */
  ArduinoOTA.setPassword(OTA_PASS);
  //Send OTA events to the browser
  ArduinoOTA.onStart([]() {
    #ifdef SERIAL_DEBUG
      Serial.println(F("Update Start"));
    #endif
  });
  ArduinoOTA.onEnd([]() {
    #ifdef SERIAL_DEBUG
      Serial.println(F("Update End"));
    #endif
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    #ifdef SERIAL_DEBUG
      char p[32];
      sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
      Serial.println(p);
    #endif
  });
  ArduinoOTA.onError([](ota_error_t error) {
    #ifdef SERIAL_DEBUG
      if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
      else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
      else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
      else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Recieve Failed"));
      else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
    #endif
  });
  ArduinoOTA.setHostname(DEVICE_FQN);
  ArduinoOTA.begin();

  /* END OTA Update */

  #ifdef SERIAL_DEBUG
    Serial.printf("\nHello, this is %s\n",DEVICE_FQN);
    Serial.printf("Connected to: %s\n", WIFI_SSID);
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
  #endif

  /*
   * Opening SPIFFS
   */

  bool spiffs_open = SPIFFS.begin();
  //Serial.printf("Formatting done: %d\n", SPIFFS.format());    // WARNING: ONLY USE WHEN THE FLASH MEMORY HAS BEEN ERASED VIA IDE/ESPTOOL.PY OR YOU ARE USING A BRAND NEW DEVICE!
  
  #ifdef SERIAL_DEBUG
    if (spiffs_open) {
      Serial.println(F("\nOpening SPIFFS\t\tdone."));
    } else {
       Serial.println(F("FAILED TO OPEN SPIFFS!"));
    }
  #endif

  // register HTTP server callbacks
  server.on("/", handleRoot);
  server.on("/index.htm", handleRoot);                                    // redirect to index.html
  server.on("/readings.xml",handleGetReadings);                           // serve readings via XML
  server.serveStatic("/", SPIFFS, "/").setCacheControl("max-age=600");    // serve static files
  server.onNotFound([](AsyncWebServerRequest *request){                   // serve 404
    request->send(404,"text/html",F("<html><head><title>404 Not Found</title></head><body><h1>Not Found</h1><p>The requested URL was not found on this webserver.</p></body></html>"));
  });

  // accept file uploading
  server.on("/upload", HTTP_POST, [](AsyncWebServerRequest *request){
    request->send(200);
  }, onUpload);

  #ifdef SERIAL_DEBUG             // although not a serial debug function, it can be used to check which files are currently with the flash
    server.on("/files", HTTP_GET, [](AsyncWebServerRequest *request) {
      AsyncResponseStream *response = request->beginResponseStream("text/plain");
      response->print("Files in SPIFFS:\r\n");
      Dir dir = SPIFFS.openDir("/");
      while (dir.next()) {
        response->printf(" - %s (%i)\r\n",dir.fileName().c_str(),dir.fileSize());
      }
      request->send(response);
    });
  #endif
  
  server.begin();
  #ifdef SERIAL_DEBUG
    Serial.println(F("HTTP server started"));
  #endif
  
  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
} // End SETUP

/*
 * MAIN LOOP
 */
 
void loop(void) {
  ArduinoOTA.handle();        // handle OTA clients

  if (!client.connected()) {  // keep MQTT connection alive
    reconnect();
  }
  client.loop();  

  long now = millis();
  if (now - lastMsg > SENSOR_READ_INTERVAL) {   // periodically send measurements via MQTT
    lastMsg = now;
    readSensors();
    publishData();
  }
} // End MAIN LOOP
