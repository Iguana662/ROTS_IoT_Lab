#define LED_PIN 2 // internal LED on GPIO 2
#define SDA_PIN 21
#define SCL_PIN 22

#include <WiFi.h>
#include <Arduino_MQTT_Client.h>
#include <ThingsBoard.h>
#include "DHT20.h"
#include "Wire.h"
#include <ArduinoOTA.h>

// --- WIFI ---
constexpr char WIFI_SSID[] = "abcd";
constexpr char WIFI_PASSWORD[] = "123456789";

// --- THINGSBOARD ---
constexpr char TOKEN[] = "7s5pokn2se622pzn1jxu";
constexpr char THINGSBOARD_SERVER[] = "app.coreiot.io";
constexpr uint16_t THINGSBOARD_PORT = 1883U;

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr uint32_t SERIAL_DEBUG_BAUD = 115200U;

// --- ATTRIBUTES ---
constexpr char BLINKING_INTERVAL_ATTR[] = "blinkingInterval";
constexpr char LED_MODE_ATTR[] = "ledMode";
constexpr char LED_STATE_ATTR[] = "ledState";

volatile bool attributesChanged = false;
volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

uint32_t previousStateChange;

constexpr int16_t telemetrySendInterval = 10000U;
uint32_t previousDataSend;

// OLD STYLE: Use standard C-Array for attribute list
const char *SHARED_ATTRIBUTES_LIST[] = {
  LED_STATE_ATTR,
  BLINKING_INTERVAL_ATTR
};

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

DHT20 dht20;

// ---------------------------------------------------------------------------
// OLD STYLE RPC CALLBACK
// 1. Returns 'void' instead of 'RPC_Response'
// 2. Argument is 'const JsonVariant' instead of 'RPC_Data'
// ---------------------------------------------------------------------------
void setLedSwitchState(const JsonVariant &data) {
    Serial.println("Received Switch state");
    
    // In old versions, 'data' holds the parameter directly
    bool newState = data.as<bool>();
    
    Serial.print("Switch state change: ");
    Serial.println(newState);
    
    digitalWrite(LED_PIN, newState);
    attributesChanged = true;
    
    // In old style, we do not return an RPC_Response object.
    // Use tb.sendTelemetryData if you need to confirm state back.
}

// OLD STYLE: Standard Array for callbacks
const RPC_Callback callbacks[] = {
  { "setLedSwitchValue", setLedSwitchState }
};

// ---------------------------------------------------------------------------
// OLD STYLE ATTRIBUTE CALLBACK
// Argument is typically const JsonObject& in very old versions, 
// or JsonVariant in intermediate versions.
// ---------------------------------------------------------------------------
void processSharedAttributes(const JsonObject &data) {
  if (data.containsKey(BLINKING_INTERVAL_ATTR)) {
      uint16_t new_interval = data[BLINKING_INTERVAL_ATTR].as<uint16_t>();
      if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX) {
        blinkingInterval = new_interval;
        Serial.print("Blinking interval is set to: ");
        Serial.println(new_interval);
      }
  }
  
  if (data.containsKey(LED_STATE_ATTR)) {
      ledState = data[LED_STATE_ATTR].as<bool>();
      digitalWrite(LED_PIN, ledState);
      Serial.print("LED state is set to: ");
      Serial.println(ledState);
  }
  
  attributesChanged = true;
}

// OLD STYLE: Constructor takes (function, array, size_of_array)
const Shared_Attribute_Callback attributes_callback(processSharedAttributes, SHARED_ATTRIBUTES_LIST, 2);
const Attribute_Request_Callback attribute_shared_request_callback(processSharedAttributes, SHARED_ATTRIBUTES_LIST, 2);

void InitWiFi() {
  Serial.println("Connecting to AP ...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to AP");
}

const bool reconnect() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }
  InitWiFi();
  return true;
}

void setup() {
  Serial.begin(SERIAL_DEBUG_BAUD);
  pinMode(LED_PIN, OUTPUT);
  delay(1000);
  InitWiFi();

  Wire.begin(SDA_PIN, SCL_PIN);
  dht20.begin();
}

void loop() {
  delay(10);

  if (!reconnect()) {
    return;
  }

  if (!tb.connected()) {
    Serial.print("Connecting to: ");
    Serial.print(THINGSBOARD_SERVER);
    Serial.print(" with token ");
    Serial.println(TOKEN);
    
    if (!tb.connect(THINGSBOARD_SERVER, TOKEN, THINGSBOARD_PORT)) {
      Serial.println("Failed to connect");
      return;
    }

    tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

    Serial.println("Subscribing for RPC...");
    
    // OLD STYLE SUBSCRIPTION:
    // Pass the array and the NUMBER OF ITEMS (1)
    if (!tb.RPC_Subscribe(callbacks, 1)) {
      Serial.println("Failed to subscribe for RPC");
      return;
    }

    if (!tb.Shared_Attributes_Subscribe(attributes_callback)) {
      Serial.println("Failed to subscribe for shared attribute updates");
      return;
    }

    Serial.println("Subscribe done");

    if (!tb.Shared_Attributes_Request(attribute_shared_request_callback)) {
      Serial.println("Failed to request for shared attributes");
      return;
    }
  }

  if (attributesChanged) {
    attributesChanged = false;
    tb.sendAttributeData(LED_STATE_ATTR, digitalRead(LED_PIN));
  }

  if (millis() - previousDataSend > telemetrySendInterval) {
    previousDataSend = millis();

    dht20.read();
    
    float temperature = dht20.getTemperature();
    float humidity = dht20.getHumidity();

    if (isnan(temperature) || isnan(humidity)) {
      Serial.println("Failed to read from DHT20 sensor!");
    } else {
      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" °C, Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      tb.sendTelemetryData("temperature", temperature);
      tb.sendTelemetryData("humidity", humidity);
    }

    tb.sendAttributeData("rssi", WiFi.RSSI());
    tb.sendAttributeData("channel", WiFi.channel());
    tb.sendAttributeData("bssid", WiFi.BSSIDstr().c_str());
    tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    tb.sendAttributeData("ssid", WiFi.SSID().c_str());
  }

  tb.loop();
}