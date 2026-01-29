#include <Arduino.h>
// #include <WiFi.h>
// #include <HTTPClient.h>

// --- Configuration Structure ---
struct LoggerConfig {
  String protocol;          // "HTTP" or "MQTT"
  String sensorName;        // The key name for the sensor value (e.g., "temp", "humidity")
  String username;          // MQTT or API Username
  String password;          // MQTT or API Password
  String endpoint;          // Server URL or Broker IP
  unsigned long interval;   // Transmission interval in ms
};

// --- Global Settings ---
// Update these with your actual network details

// Initialize Logger Configuration
LoggerConfig logConfig = {
  "HTTP",                   // Protocol: Change to "MQTT" to test MQTT logic
  "TEST-BLOW-004-TEMP001",                   // Sensor Name (used as JSON key)
  "Medion",             // Username
  "iot@med1on",         // Password
  "https://api-logger-dev2.medionindonesia.com/api/v1/UpdateLoggingRealtime", // Endpoint
  1000                    // Send Interval: 1 second (1000 ms)
};

// Global Variables
float currentTemperature = 0.0;
unsigned long lastTransmissionTime = 0;

// --- Function Declarations ---
void handleSerialInput();
void sendData();
void sendViaHTTP();
void sendViaMQTT();

void setup() {
  Serial.begin(9600);
  
  Serial.println("\n--- ESP32 Data Logger CLI ---");
  Serial.println("Commands:");
  Serial.println("  url:<endpoint>    Set Server URL / Broker IP");
  Serial.println("  user:<name>       Set Username");
  Serial.println("  proto:<HTTP/MQTT> Set Protocol");
  Serial.println("  <number>          Set Temperature (e.g., 25.5)");
  Serial.println("-----------------------------");
}

void loop() {
  // 1. Check for User Input (Simulated Sensor)
  handleSerialInput();

  // 2. Check Transmission Trigger (Time Interval)
  unsigned long now = millis();
  if (now - lastTransmissionTime >= logConfig.interval) {
    sendData();
    lastTransmissionTime = now;
  }
}

// --- Helper Functions ---

void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    if (input.startsWith("url:")) {
      logConfig.endpoint = input.substring(4);
      Serial.println("Endpoint set to: " + logConfig.endpoint);
    } else if (input.startsWith("user:")) {
      logConfig.username = input.substring(5);
      Serial.println("User set to: " + logConfig.username);
    } else if (input.startsWith("proto:")) {
      logConfig.protocol = input.substring(6);
      Serial.println("Protocol set to: " + logConfig.protocol);
    } else {
      float val = input.toFloat();
      if (val != 0.0 || input.equals("0") || input.equals("0.0")) {
        currentTemperature = val;
        Serial.printf("[SENSOR] Temp updated: %.2f\n", currentTemperature);
      }
    }
  }
}

void sendData() {
  Serial.println("\n[TRIGGER] Transmission Interval Reached.");
  
  if (logConfig.protocol == "HTTP") {
    sendViaHTTP();
  } else if (logConfig.protocol == "MQTT") {
    sendViaMQTT();
  } else {
    Serial.println("[ERROR] Unknown Protocol");
  }
}

void sendViaHTTP() {
  Serial.printf("[HTTP] (SIMULATION) Sending to %s...\n", logConfig.endpoint.c_str());
  
  // Prepare JSON payload
  String payload = "{\"" + logConfig.sensorName + "\": " + String(currentTemperature) + ", \"user\": \"" + logConfig.username + "\"}";
  
  Serial.println("[HTTP] Payload: " + payload);
  Serial.println("[HTTP] POST Request Sent (Simulated Success 200 OK)");
}

void sendViaMQTT() {
  // NOTE: Actual MQTT library (PubSubClient) is not in the provided includes.
  // This function simulates the logic flow based on your requirements.
  
  Serial.printf("[MQTT] Connecting to Broker: %s\n", logConfig.endpoint.c_str());
  Serial.printf("[MQTT] Authenticating as User: %s\n", logConfig.username.c_str());
  
  // Simulation of publishing
  String topic = "sensors/" + logConfig.sensorName;
  String payload = String(currentTemperature);
  
  Serial.printf("[MQTT] (SIMULATION) Publishing to topic '%s': %s\n", topic.c_str(), payload.c_str());
  Serial.println("[MQTT] Data sent successfully (Simulated).");
}