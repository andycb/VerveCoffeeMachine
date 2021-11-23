#include "CoffeeMachine.h"
#include "Config.h"

#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <WiFiManager.h>

#define FIRMWARE_PREFIX "CoffeeMaker"
#define AVAILABILITY_ONLINE "online"
#define AVAILABILITY_OFFLINE "offline"

WiFiManager wifiManager;
WiFiClient wifiClient;
PubSubClient mqttClient;

WiFiManagerParameter custom_mqtt_server("server", "mqtt server", Config::mqtt_server, sizeof(Config::mqtt_server));
WiFiManagerParameter custom_mqtt_user("user", "MQTT username", Config::username, sizeof(Config::username));
WiFiManagerParameter custom_mqtt_pass("pass", "MQTT password", Config::password, sizeof(Config::password));

uint32_t lastMqttConnectionAttempt = 0;
const uint16_t mqttConnectionInterval = 60000; // 1 minute = 60 seconds = 60000 milliseconds

char identifier[24];

char MQTT_TOPIC_AVAILABILITY[128];
char MQTT_TOPIC_STATE[128];
char MQTT_TOPIC_COMMAND[128];
char MQTT_TOPIC_AUTOCONF_STATE_SENSOR[128];
char MQTT_TOPIC_AUTOCONF_COMMAND[128];

void publishState(MachineState state);  

bool shouldSaveConfig = false;
void saveConfigCallback() {
    shouldSaveConfig = true;
}

void OnStateChanged(MachineState state) {
  publishState(state);
  Serial.print("State Changed =");
  Serial.println(state);
}

CoffeeMachine coffeeMachine = CoffeeMachine(&OnStateChanged);

void mqttCallback(char* topic, byte* message, unsigned int length) {
  if (String(topic) == String(MQTT_TOPIC_COMMAND)) {

  String messageStr;
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageStr += (char)message[i];
  }
    
    if(messageStr == "BrewSC"){
      Serial.println("Brewing - Strength = Single Cup");
      coffeeMachine.Brew(SingleCup);
    }
    else if(messageStr == "Brew1"){
      Serial.println("Brewing - Strength = 1");
      coffeeMachine.Brew(One);
    }
    else if(messageStr == "Brew2"){
      Serial.println("Brewing - Strength = 2");
      coffeeMachine.Brew(Two);
    }
    else if(messageStr == "Brew3"){
      Serial.println("Brewing - Strength = 3");
      coffeeMachine.Brew(Three);
    }
  }
}

void setupWifi() {
    wifiManager.setDebugOutput(false);
    wifiManager.setSaveConfigCallback(saveConfigCallback);

    wifiManager.addParameter(&custom_mqtt_server);
    wifiManager.addParameter(&custom_mqtt_user);
    wifiManager.addParameter(&custom_mqtt_pass);

    WiFi.hostname(identifier);
    wifiManager.autoConnect(identifier);
    mqttClient.setClient(wifiClient);

    strcpy(Config::mqtt_server, custom_mqtt_server.getValue());
    strcpy(Config::username, custom_mqtt_user.getValue());
    strcpy(Config::password, custom_mqtt_pass.getValue());

    if (shouldSaveConfig) {
        Config::save();
    } else {
        // For some reason, the read values get overwritten in this function
        // To combat this, we just reload the config
        // This is most likely a logic error which could be fixed otherwise
        Config::load();
    }
}

void resetWifiSettingsAndReboot() {
    wifiManager.resetSettings();
    delay(3000);
    ESP.restart();
}

void publishAutoConfig() {
    char mqttPayload[2048];
    DynamicJsonDocument device(256);
    DynamicJsonDocument autoconfPayload(1024);
    StaticJsonDocument<64> identifiersDoc;
    JsonArray identifiers = identifiersDoc.to<JsonArray>();

    identifiers.add(identifier);

    device["identifiers"] = identifiers;
    device["manufacturer"] = "andycb";
    device["model"] = "Coffee Maker";
    device["name"] = identifier;
    device["sw_version"] = "2021.11.19";

    autoconfPayload["device"] = device.as<JsonObject>();
    autoconfPayload["availability_topic"] = MQTT_TOPIC_AVAILABILITY;
    autoconfPayload["state_topic"] = MQTT_TOPIC_STATE;
    autoconfPayload["name"] = identifier + String(" Brew State");
    autoconfPayload["value_template"] = "{{value_json.brewState}}";
    autoconfPayload["unique_id"] = identifier + String("_brewState");
    autoconfPayload["icon"] = "mdi:coffee-maker";

    serializeJson(autoconfPayload, mqttPayload);
    mqttClient.publish(&MQTT_TOPIC_AUTOCONF_STATE_SENSOR[0], &mqttPayload[0], true);

    autoconfPayload.clear();
}

void mqttReconnect() {
    for (uint8_t attempt = 0; attempt < 3; ++attempt) {
        if (mqttClient.connect(identifier, Config::username, Config::password, MQTT_TOPIC_AVAILABILITY, 1, true, AVAILABILITY_OFFLINE)) {
            mqttClient.publish(MQTT_TOPIC_AVAILABILITY, AVAILABILITY_ONLINE, true);
            publishAutoConfig();

            // Make sure to subscribe after polling the status so that we never execute commands with the default data
            mqttClient.subscribe(MQTT_TOPIC_COMMAND);
            break;
        }
        delay(5000);
    }
}

bool isMqttConnected() {
    return mqttClient.connected();
}

char* StateToString(MachineState state) {
  if (state == MachineState::Unknown) {
      return "Unknown";
  }

  if (state == MachineState::Standby) {
      return "Standby";
  }

  if (state == MachineState::Brewing) {
      return "Brewing";
  }

  if (state == MachineState::KeepWarm) {
      return "KeepWarm";
  }
}

void publishState(MachineState state) {
    DynamicJsonDocument stateJson(604);
    char payload[256];

    stateJson["brewState"] = StateToString(state);

    serializeJson(stateJson, payload);
    mqttClient.publish(&MQTT_TOPIC_STATE[0], &payload[0], true);
}

void setup() {
    Serial.begin(9600);
  
    snprintf(identifier, sizeof(identifier), "COFFEEMAKER-%X", ESP.getChipId());
    snprintf(MQTT_TOPIC_AVAILABILITY, 127, "%s/%s/status", FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_STATE, 127, "%s/%s/state", FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_COMMAND, 127, "%s/%s/command", FIRMWARE_PREFIX, identifier);

    snprintf(MQTT_TOPIC_AUTOCONF_STATE_SENSOR, 127, "homeassistant/sensor/%s/%s_brewState/config", FIRMWARE_PREFIX, identifier);
    snprintf(MQTT_TOPIC_AUTOCONF_COMMAND, 127, "homeassistant/sensor/%s/%s_command/config", FIRMWARE_PREFIX, identifier);

    Config::load();

    setupWifi();
    mqttClient.setServer(Config::mqtt_server, 1883);
    mqttClient.setKeepAlive(10);
    mqttClient.setBufferSize(2048);
    mqttClient.setCallback(mqttCallback);
   
    mqttReconnect();
    
    coffeeMachine.Init();
}

void loop() {
  coffeeMachine.Tick();
  mqttClient.loop();

  const uint32_t currentMillis = millis();
  if (!mqttClient.connected() && currentMillis - lastMqttConnectionAttempt >= mqttConnectionInterval) {
      lastMqttConnectionAttempt = currentMillis;
      mqttReconnect();
  }
}
