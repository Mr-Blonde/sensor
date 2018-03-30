/*
 * sensor
 * version 0.9.9 (03/29/2018)
 * 
 */


/*
 * CHANGE SENSOR_ID
 *  1 - muc01sensor
 *  2 - muc02sensor
 *  3 - muc03sensor
 */
#define SENSOR_ID 2

// enable features
#define OLED_DISPLAY
#define SHT30
// #define DHT22

/*
 * END CHANGE
 */


#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <ArduinoOTA.h>
#include <FS.h>
#include <ArduinoJson.h>
#include <detail/RequestHandlersImpl.h>
#include "credentials.h"

#ifdef OLED_DISPLAY
  #include <SPI.h>
  #include <Wire.h>
  #include <Adafruit_GFX.h>
  #include <Adafruit_SSD1306.h>
  #if (SSD1306_LCDHEIGHT != 48)
    #error("Height incorrect, please fix Adafruit_SSD1306.h!");
  #endif
#endif

#ifdef SHT30
  #include <WEMOS_SHT3X.h>
#elif defined(DHT22)
  #include <DHT.h>
#endif

#define DEVICE_VER "0.9.9"
#define MQTT_VERSION MQTT_VERSION_3_1_1

// Depends on SENSOR_ID
#if SENSOR_ID == 1                  // outside sht30 sensor
  #define DEVICE_FQN "muc01sensor"
  #define SENSOR_TYPE "outdoor"
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/outside_1";
#elif SENSOR_ID == 2                // network cabinet sht30 sensor
  #define DEVICE_FQN "muc02sensor"
    #define DEVICE_FQN_SHORT "muc02sens."
  #define SENSOR_TYPE "indoor"
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/cabinet_1";
#elif SENSOR_ID == 3                // network cabinet dht22 sensor
  #define DEVICE_FQN "muc03sensor"
  #define SENSOR_TYPE "indoor"
  // MQTT: topic
  const char* MQTT_SENSOR_TOPIC = "home/sensor/cabinet_2";
#endif

#define SENSOR_READ_INTERVAL 5 * 60 * 1000
long lastMsg = 0;

#ifdef OLED_DISPLAY
  #define OLED_RESET 0  // GPIO0
  Adafruit_SSD1306 display(OLED_RESET);
#endif


#ifdef SHT30
  SHT3X sht30(0x45);
#elif defined(DHT22)
  // DHT - D1/GPIO5
  #define DHTPIN D4
  #define DHTTYPE DHT22
  DHT dht(DHTPIN, DHTTYPE, 11);
#endif

// MQTT: ID, server IP, port, username and password
const char* MQTT_CLIENT_ID = DEVICE_FQN;
const char* MQTT_SERVER_IP = "10.0.1.200";
const uint16_t MQTT_SERVER_PORT = 1883;

WiFiClient wifiClient;
PubSubClient client(wifiClient); 
ESP8266WebServer server(80);
 
float humidity, temp;  // Values read from sensor
char str_humidity[10], str_temperature[10];  // Rounded sensor values and as strings
String webString="";     // String to display
// Generally, you should use "unsigned long" for variables that hold time
unsigned long previousMillis = 0;        // will store last temp was read
const long interval = 2000;              // interval at which to read sensor


/*
 * Read all sensors
 */
void readSensors() {
  
  // Wait at least 2 seconds seconds between measurements.
  // if the difference between the current time and last time you read
  // the sensor is bigger than the interval you set, read the sensor
  // Works better than delay for things happening elsewhere also
  unsigned long currentMillis = millis();
 
  if(currentMillis - previousMillis >= interval) {
    // save the last time you read the sensor 
    previousMillis = currentMillis;   

    #ifdef SHT30
      sht30.get();
      humidity = sht30.humidity;            // SHT30 - Read humidity (percent)
      temp = sht30.cTemp;                   // SHT30 - Read temperature as Celcius
    #elif defined(DHT20)
      humidity = dht.readHumidity();        // DHT22 - Read humidity (percent)
      temp = dht.readTemperature();         // DHT22 - Read temperature as Celcius
    #endif
    
    // Check if any reads failed and exit early (to try again).
    if (isnan(humidity) || isnan(temp)) {
      Serial.println("Failed to read from SHT sensor!");
      return;
    } else {
      dtostrf(humidity, 1, 2, str_humidity);
      dtostrf(temp, 1, 2, str_temperature);
    }
  }

  #ifdef OLED_DISPLAY
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(0,0);
 
    display.println(DEVICE_FQN_SHORT);
    display.print("\nT ");
    display.print(temp);
    display.println(" C");
 
    display.print("H ");
    display.print(humidity);
    display.println(" %");
    display.display();
  #endif
  
} // End readSensors


// function called to publish the temperature and the humidity
void publishData() {
  // create a JSON object
  // doc : https://github.com/bblanchon/ArduinoJson/wiki/API%20Reference
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  // INFO: the data must be converted into a string; a problem occurs when using floats...
  root["temperature"] = str_temperature;
  root["humidity"] = str_humidity;
//  root["waterlevel"] = str_distance;
  root.prettyPrintTo(Serial);
  Serial.println("");
  /*
     {
        "temperature": "23.20" ,
        "humidity": "43.70"
     }
  */
  char data[200];
  root.printTo(data, root.measureLength() + 1);
  client.publish(MQTT_SENSOR_TOPIC, data, true);
}

// function called when a MQTT message arrived
void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("INFO: Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("INFO: connected");
    } else {
      Serial.print("ERROR: failed, rc=");
      Serial.print(client.state());
      Serial.println("DEBUG: try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void handle_root() {

  readSensors();

  String response;
  response += "<html>\n";
  response += "<head>\n";
  response += "<meta charset=\"utf-8\">\n";
  response += "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n";
  response += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n";
  response += "<title>";
  response += DEVICE_FQN;
  response += "</title>\n";
  response += "<link rel=\"manifest\" href=\"manifest.json\">";
  response += "<link rel=\"stylesheet\" href=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/css/bootstrap.min.css\"\n";
  response += "integrity=\"sha384-1q8mTJOASx8j1Au+a5WDVnPi2lkFfwwEAa8hDDdjZlpLegxhjVME1fgjWPGmkzs7\" crossorigin=\"anonymous\">\n";
  response += "<script src=\"https://maxcdn.bootstrapcdn.com/bootstrap/3.3.6/js/bootstrap.min.js\" integrity=\"sha384-0mSbJDEHialfmuBBQP6A4Qrprq5OVfW37PRR3j5ELqxss1yVqOtnepnHVP9aJ7xS\" crossorigin=\"anonymous\"></script>\n";
  response += "</head>\n";

  response += "<body>\n";
  response += "<div class=\"container\">\n";
  response += "<div class=\"page-header\">\n";
  response += "<h1>";
  response += DEVICE_FQN;
  response += "</h1>\n";
  response += "<p class=\"lead\">Current readings.</p>\n";
  response += "</div>\n";
  response += "<div class=\"row\">\n";

  response += "<div class=\"col-md-6\"><div class=\"row\">\n";
  response += "<div class=\"col-md-3\"> <img src=\"temp.png\" /> </div>\n";
  response += "<div class=\"col-md-9\"> Temperature<br/><p class=\"lead\">";
  response += str_temperature;
  response += "&deg;C</p></div>\n";
  response += "</div></div>\n";

  response += "<div class=\"col-md-6\"><div class=\"row\">\n";
  response += "<div class=\"col-md-3\"> <img src=\"humi.png\" /> </div>\n";
  response += "<div class=\"col-md-9\"> Humidity<br/><p class=\"lead\">";
  response += str_humidity;
  response += "%</p></div>\n";
  response += "</div></div>\n";

  response += "</div>\n";
  response += "<div class=\"page-footer\">\n";
  response += "<hr>\n";
  response += DEVICE_FQN;
  response += SENSOR_DOMAIN;
  response += " <span class=\"label label-info\">ver. ";
  response += DEVICE_VER;
  response += "</span>\n";
  response += "</div>\n";
  response += "</div>\n";
  response += "</body>\n";
  response += "</html>\n";
  
  server.send(200, "text/html", response);
}

void handleGetReadings() {
  readSensors();

  String response;

  response += "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>";
  response += "<readings>";
  response += "<temperature>";
  response += str_temperature;
  response += "</temperature>";
  response += "<humidity>";
  response += str_humidity;
  response += "</humidity>";
  response += "</readings>";

  server.send(200, "text/xml",response);
}


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

  #ifdef DHT22
    dht.begin();
  #endif
  
  // Connect to WiFi network
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.println(DEVICE_FQN);
  Serial.print("\n\rWorking to connect");
 
  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  /*
     Over the air firmware update
  */
  ArduinoOTA.setPassword(OTA_PASS);
  //Send OTA events to the browser
  ArduinoOTA.onStart([]() {
    Serial.println("Update Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("Update End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    char p[32];
    sprintf(p, "Progress: %u%%\n", (progress / (total / 100)));
    Serial.println(p);
  });
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Recieve Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.setHostname(DEVICE_FQN);
  ArduinoOTA.begin();

  /* END OTA Update */

  SPIFFS.begin();
  
  Serial.println("");
  Serial.printf("Hello, this is %s\n",DEVICE_FQN);
  Serial.printf("Connected to: %s\n", WIFI_SSID);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
   
  server.on("/", handle_root);
  server.on("/index.html", handle_root);
  server.on("/readings.xml",handleGetReadings);

  server.serveStatic("/", SPIFFS, "/");
  server.onNotFound(handleUnknown);
  
  server.begin();
  Serial.println("HTTP server started");
  
  // init the MQTT connection
  client.setServer(MQTT_SERVER_IP, MQTT_SERVER_PORT);
  client.setCallback(callback);
} // End SETUP

/*
 * MAIN LOOP
 */
 
void loop(void)
{
  server.handleClient();
  ArduinoOTA.handle();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();  

  long now = millis();
  if (now - lastMsg > SENSOR_READ_INTERVAL) {
    lastMsg = now;
    
    readSensors();
    publishData();
  }
} // End MAIN LOOP


// Sendet "Not Found"-Seite
void notFound()
{ String HTML = F("<html><head><title>404 Not Found</title></head><body>"
                  "<h1>Not Found</h1>"
                  "<p>The requested URL was not found on this webserver.</p>"
                  "</body></html>");
  server.send(404, "text/html", HTML);
} // End notFound

// Trying to load the file from SPIFFS
void handleUnknown()
{ String filename = server.uri();

  File pageFile = SPIFFS.open(filename, "r");
  if (pageFile) {
    String contentTyp = StaticRequestHandler::getContentType(filename);
    size_t sent = server.streamFile(pageFile, contentTyp);
    pageFile.close();
  }
  else
    notFound();
} // End handleUnknown
