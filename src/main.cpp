/*
   RCv0.3
   DEEPSLEEP
   En esta version se despierta desde sleep por medio del bang del attiny85 que lee interrupcion de HW.
   Enviar el json parametrizado (DONE)
   TODO: config the Wifi manager (DONE)
   TOFIX: the mac address leading Zeros.. (DONE)
*/


#define CONFIG 2
#define DEBUG
#define TIMEOUT 60
#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h> // https://github.com/bblanchon/ArduinoJson
#include <WiFiManager.h>

#ifdef ESP32
#include <SPIFFS.h>
#endif

//define your default values here, if there are different values in config.json, they are overwritten.
char server[60] = "http://data.sector7gp.com/save.php";
char port[6]  = "80";
char clientId[5] = "1";


//default custom static IP
char static_ip[16] = "192.168.1.180";
char static_gw[16] = "192.168.1.1";
char static_sn[16] = "255.255.255.0";

//flag for saving data
bool shouldSaveConfig = false;

byte mac[6];
String ID = "";
WiFiClient  client;
WiFiManager wm;


//callback notifying us of the need to save config
void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void setupSpiffs() {
  //clean FS, for testing
  // SPIFFS.format();

  //read configuration from FS json
  Serial.println("mounting FS...");

  if (SPIFFS.begin()) {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject& json = jsonBuffer.parseObject(buf.get());
        json.printTo(Serial);
        if (json.success()) {
          Serial.println("\nparsed json");

          strcpy(server, json["server"]);
          strcpy(port, json["port"]);
          strcpy(clientId, json["clientId"]);

          if (json["ip"]) {
            Serial.println("setting custom ip from config");
            strcpy(static_ip, json["ip"]);
            strcpy(static_gw, json["gateway"]);
            strcpy(static_sn, json["subnet"]);
            Serial.println(static_ip);
          } else {
            Serial.println("no custom ip in config");
          }

        } else {
          Serial.println("failed to load json config");
        }
      }
    }
  } else {
    Serial.println("failed to mount FS");
  }
  //end read
}

void setup() {
  wm.setDebugOutput(false);
  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP
  //WiFi.macAddress(mac);
  Serial.begin(115200);  // Initialize serial
  Serial.println(ESP.getChipId());
  wifi_status_led_uninstall();
  //reset settings - for testing
  //wm.resetSettings();
  wm.setMinimumSignalQuality(40);

  setupSpiffs();

  //set config save notify callback
  wm.setSaveConfigCallback(saveConfigCallback);

  // setup custom parameters
  //
  // The extra parameters to be configured (can be either global or just in the setup)
  // After connecting, parameter.getValue() will get you the configured value
  // id/name placeholder/prompt default length
  WiFiManagerParameter custom_server("server", "URL", server, 40);
  WiFiManagerParameter custom_port("port", "Puerto", port, 6);
  WiFiManagerParameter custom_clientId("id", "ID Cliente", clientId, 32);

  //add all your parameters here
  wm.addParameter(&custom_server);
  wm.addParameter(&custom_port);
  wm.addParameter(&custom_clientId);

  //  //  //set static ip
  //  IPAddress _ip, _gw, _sn;
  //  _ip.fromString(static_ip);
  //  _gw.fromString(static_gw);
  //  _sn.fromString(static_sn);
  //  wm.setSTAStaticIPConfig(_ip, _gw, _sn);

  // set configportal timeout
  wm.setConfigPortalTimeout(TIMEOUT);
  wm.setConnectTimeout(20);

  //if pressed, starts the configPortal
  delay(1000);
  if ( digitalRead(CONFIG) == LOW) {
    wm.startConfigPortal("", "agetech0");
  }
  wm.autoConnect("", "agetech0");

  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //read updated parameters
  strcpy(server, custom_server.getValue());
  strcpy(port, custom_port.getValue());
  strcpy(clientId, custom_clientId.getValue());

  //save the custom parameters to FS
  if (shouldSaveConfig) {
    Serial.println("saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject& json = jsonBuffer.createObject();
    json["server"] = server;
    json["port"]   = port;
    json["clientId"]   = clientId;

    //    json["ip"]          = WiFi.localIP().toString();
    //    json["gateway"]     = WiFi.gatewayIP().toString();
    //    json["subnet"]      = WiFi.subnetMask().toString();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile) {
      Serial.println("failed to open config file for writing");
    }

    json.prettyPrintTo(Serial);
    json.printTo(configFile);
    configFile.close();
    //end save
    shouldSaveConfig = false;
  }


  Serial.println("local ip");
  Serial.println(WiFi.localIP());
  Serial.println(WiFi.gatewayIP());
  Serial.println(WiFi.subnetMask());
}


void loop() {
  byte tries = 0;
  HTTPClient http;
  ID = wm.getDefaultAPName().substring(wm.getDefaultAPName().indexOf("_") + 1, wm.getDefaultAPName().indexOf("_") + 7);
  String toPost = "{\"clientId\":\"" + String(clientId) + "\",\"deviceId\":\"" + ID + "\",\"value\":\"" + "1" + "\"}";

  // Your Domain name with URL path or IP address with path
  Serial.println(server);
  http.begin(client, server);
  http.addHeader("Content-Type", "application/json");
  int httpResponseCode = http.POST(toPost);

#ifdef DEBUG
  Serial.print("Data to Post: ");
  Serial.println(toPost);
#endif

  while ((httpResponseCode != 200) && (tries < 5)) {
    httpResponseCode = http.POST(toPost);
    tries++;
    delay(2000);

#ifdef DEBUG
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
#endif

  }

  // Free resources
  http.end();

#ifdef DEBUG
  Serial.println("Deep Sleep");
#endif

  ESP.deepSleep(0);
}