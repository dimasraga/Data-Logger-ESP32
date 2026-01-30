#include <Arduino.h>
#include <SPI.h>
#include <Ethernet.h>

// --- Ethernet & Hardware Definitions ---
byte mac[] = {0x02, 0x00, 0x00, 0x00, 0x00, 0x01};

// W5500 Pins
#define ETH_INT 34
#define ETH_MISO 19 
#define ETH_MOSI 23 
#define ETH_CLK 18  
#define ETH_CS 4
#define ETH_RST 12

// Other Pins
#define jumlahInputDigital 4
#define jumlahOutputDigital 4
#define jumlahInputAnalog 4
const int DI_PINS[] = {32, 33, 25, 26};
#define SIG_LED_PIN 2
#define SD_CS_PIN 5

// --- Network Configuration Structure ---
struct Network
{
  String ssid, password, networkMode, protocolMode, endpoint, pubTopic, subTopic, protocolMode2;
  String apSsid, apPassword, dhcpMode, mqttUsername, mqttPassword, loginUsername = "admin", loginPassword = "admin";
  String ipAddress = "192.168.4.1";  
  String subnetMask = "255.255.255.0"; 
  String ipGateway = "192.168.1.1";    
  String ipDNS = "192.168.1.1";        
  String macAddress, connStatus = "Not Connected", sendTrig;
  String erpUsername, erpPassword, erpUrl;
  bool loggerMode;
  int port;
  float sendInterval;
  int recapInterval = 5; 
} networkSettings;

// Global Variables
float currentTemperature = 0.0;
unsigned long lastTransmissionTime = 0;
unsigned long lastRecapTime = 0;
String sensorName = "TEST-BLOW-004-TEMP001"; // Default sensor name
EthernetClient ethClient; // Ethernet Client instance

// --- Function Declarations ---
void handleSerialInput();
void sendData();
void sendViaHTTP();
void sendViaMQTT();

void setup() {
  Serial.begin(9800); 
  
  // --- Init Default Settings ---
  networkSettings.protocolMode = "HTTP";
  networkSettings.endpoint = "http://api-logger-dev2.medionindonesia.com/api/v1/UpdateLoggingRealtime"; // Menggunakan HTTP untuk W5500
  networkSettings.mqttUsername = "Medion";
  networkSettings.erpUsername = "Medion";      // Username API
  networkSettings.erpPassword = "iot@medion";  // Password API
  networkSettings.sendInterval = 10.0; // 10 detik

  // --- Init Ethernet W5500 ---
  Serial.println("\nInitializing W5500 Ethernet...");
  
  // Reset W5500
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, HIGH);
  delay(100);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  Ethernet.init(ETH_CS);
  // Konfigurasi SPI manual untuk ESP32 jika diperlukan, tapi Ethernet.init biasanya cukup jika pin default
  // Namun untuk memastikan pin yang benar digunakan:
  SPI.begin(ETH_CLK, ETH_MISO, ETH_MOSI); 

  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    if (Ethernet.hardwareStatus() == EthernetNoHardware) {
      Serial.println("Ethernet shield was not found.");
    } else if (Ethernet.linkStatus() == LinkOFF) {
      Serial.println("Ethernet cable is not connected.");
    }
  } else {
    Serial.print("DHCP Success! IP Address: ");
    Serial.println(Ethernet.localIP());
  }
  
  Serial.println("\n--- ESP32 Data Logger CLI ---");
  Serial.println("Commands:");
  Serial.println("  url:<endpoint>    Set Server URL / Broker IP");
  Serial.println("  user:<name>       Set Username");
  Serial.println("  proto:<HTTP/MQTT> Set Protocol");
  Serial.println("  erp_url:<url>     Set ERP URL (Overrides default URL)");
  Serial.println("  erp_user:<user>   Set ERP Username");
  Serial.println("  erp_pass:<pass>   Set ERP Password");
  Serial.println("  recap:<min>       Set Recap Interval (Minutes)");
  Serial.println("  <number>          Set Temperature (e.g., 25.5)");
  Serial.println("-----------------------------");
}

void loop() {
  // Maintain Ethernet lease
  Ethernet.maintain();

  // 1. Check for User Input (Simulated Sensor)
  handleSerialInput();

  // 2. Check Transmission Trigger (Time Interval)
  unsigned long now = millis();
  if (now - lastTransmissionTime >= (networkSettings.sendInterval * 1000)) {
    sendData();
    lastTransmissionTime = now;
  }

  // 3. Check Recap Trigger
  if (networkSettings.recapInterval > 0 && (now - lastRecapTime >= (networkSettings.recapInterval * 60000UL))) {
    Serial.printf("\n[RECAP] Interval %d min reached.\n", networkSettings.recapInterval);
    lastRecapTime = now;
  }
}

// --- Helper Functions ---

void handleSerialInput() {
  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    if (input.startsWith("url:")) {
      networkSettings.endpoint = input.substring(4);
      Serial.println("Endpoint set to: " + networkSettings.endpoint);
    } else if (input.startsWith("user:")) {
      networkSettings.mqttUsername = input.substring(5);
      Serial.println("User set to: " + networkSettings.mqttUsername);
    } else if (input.startsWith("proto:")) {
      networkSettings.protocolMode = input.substring(6);
      Serial.println("Protocol set to: " + networkSettings.protocolMode);
    } else if (input.startsWith("erp_url:")) {
      networkSettings.erpUrl = input.substring(8);
      Serial.println("ERP URL set to: " + networkSettings.erpUrl);
    } else if (input.startsWith("erp_user:")) {
      networkSettings.erpUsername = input.substring(9);
      Serial.println("ERP User set to: " + networkSettings.erpUsername);
    } else if (input.startsWith("erp_pass:")) {
      networkSettings.erpPassword = input.substring(9);
      Serial.println("ERP Password set.");
    } else if (input.startsWith("recap:")) {
      networkSettings.recapInterval = input.substring(6).toInt();
      Serial.printf("Recap Interval set to: %d minutes\n", networkSettings.recapInterval);
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
  
  if (networkSettings.protocolMode == "HTTP") {
    sendViaHTTP();
  } else if (networkSettings.protocolMode == "MQTT") {
    sendViaMQTT();
  } else {
    Serial.println("[ERROR] Unknown Protocol");
  }
}

// Helper function for Base64 encoding (required for Basic Auth)
String base64Encode(const String &v) {
  static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  out.reserve(((v.length() + 2) / 3) * 4);
  int val = 0, valb = -6;
  for (unsigned int i = 0; i < v.length(); i++) {
    val = (val << 8) + v[i];
    valb += 8;
    while (valb >= 0) {
      out += b64[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) out += b64[((val << 8) >> (valb + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

void sendViaHTTP() {
  // Logika: Gunakan ERP URL jika diisi, jika kosong gunakan default Endpoint
  String targetUrl = (networkSettings.erpUrl.length() > 0) ? networkSettings.erpUrl : networkSettings.endpoint;
  String targetUser = (networkSettings.erpUsername.length() > 0) ? networkSettings.erpUsername : networkSettings.mqttUsername;
  targetUrl.trim(); // Hapus spasi di awal/akhir yang sering terjadi saat copy-paste
  
  // Parse URL to get Host, Port, and Path
  String protocol = "http";
  String host = "";
  String path = "/";
  int port = 80;

  int indexProtocol = targetUrl.indexOf("://");
  int indexHost = 0;
  if (indexProtocol != -1) {
    protocol = targetUrl.substring(0, indexProtocol);
    indexHost = indexProtocol + 3;
  }

  int indexPath = targetUrl.indexOf("/", indexHost);
  if (indexPath != -1) {
    host = targetUrl.substring(indexHost, indexPath);
    path = targetUrl.substring(indexPath);
  } else {
    host = targetUrl.substring(indexHost);
  }

  // Check for port in host or default to protocol
  int indexPort = host.indexOf(':');
  if (indexPort != -1) {
    port = host.substring(indexPort + 1).toInt();
    host = host.substring(0, indexPort);
  } else if (protocol.equalsIgnoreCase("https")) {
    port = 443;
  }

  Serial.printf("[HTTP] Connecting to %s (Port %d)...\n", host.c_str(), port);
  
  // Prepare JSON payload
  // Menggunakan targetUser yang sesuai (ERP User atau Default User)
  String payload = "{\"" + sensorName + "\": " + String(currentTemperature) + ", \"user\": \"" + targetUser + "\"}";
  
  Serial.println("[HTTP] Payload: " + payload);
  
  if (ethClient.connect(host.c_str(), port)) {
    Serial.println("[HTTP] TCP Connection Established. Sending Request...");
    if (port == 443) {
      Serial.println("[HTTP] WARNING: Connected to HTTPS port (443) without SSL support.");
      Serial.println("[HTTP] W5500 cannot handle encryption. Expect Status 0 or connection drop.");
    }
    
    ethClient.print("POST ");
    ethClient.print(path);
    ethClient.println(" HTTP/1.1");
    ethClient.print("Host: ");
    ethClient.println(host);
    ethClient.println("User-Agent: ESP32-W5500-Logger");
    ethClient.println("Accept: application/json");
    ethClient.println("Content-Type: application/json");
    
    // Add Basic Authentication Header
    if (networkSettings.erpUsername.length() > 0 && networkSettings.erpPassword.length() > 0) {
      String auth = networkSettings.erpUsername + ":" + networkSettings.erpPassword;
      ethClient.print("Authorization: Basic ");
      ethClient.println(base64Encode(auth));
    }

    ethClient.print("Content-Length: ");
    ethClient.println(payload.length());
    ethClient.println("Connection: close");
    ethClient.println();
    ethClient.println(payload);

    // Read response
    unsigned long timeout = millis();
    int httpCode = 0;
    bool firstLine = true;

    while (ethClient.connected() && millis() - timeout < 5000) {
      if (ethClient.available()) {
        String line = ethClient.readStringUntil('\n');
        line.trim(); // Remove \r characters
        
        // Parse HTTP Status Code from the first line (e.g., "HTTP/1.1 200 OK")
        if (firstLine && line.startsWith("HTTP/")) {
          int spaceIndex = line.indexOf(' ');
          if (spaceIndex != -1) {
            httpCode = line.substring(spaceIndex + 1, spaceIndex + 4).toInt();
          }
          firstLine = false;
        }

        Serial.println(line);
        timeout = millis();
      }
    }
    ethClient.stop();
    
    if (httpCode >= 200 && httpCode < 300) {
      Serial.printf("\n[HTTP] SUCCESS: Data accepted (Status: %d)\n", httpCode);
    } else if (httpCode == 404) {
      Serial.printf("\n[HTTP] FAILED: 404 Not Found (Status: %d)\n", httpCode);
      Serial.println(">> REASON: API path not found on Port 80. Server likely requires HTTPS (Port 443).");
    } else {
      Serial.printf("\n[HTTP] FAILED: Server Error or Auth Failed (Status: %d)\n", httpCode);
    }
  } else {
     Serial.println("[HTTP] Connection failed");
     if (port == 443) Serial.println("[HTTP] Warning: W5500 Ethernet does not support SSL/HTTPS. Ensure server supports HTTP.");
  }
}

void sendViaMQTT() {
  // NOTE: Actual MQTT library (PubSubClient) is not in the provided includes.
  // This function simulates the logic flow based on your requirements.
  
  Serial.printf("[MQTT] Connecting to Broker: %s\n", networkSettings.endpoint.c_str());
  Serial.printf("[MQTT] Authenticating as User: %s\n", networkSettings.mqttUsername.c_str());
  
  // Simulation of publishing
  String topic = "sensors/" + sensorName;
  String payload = String(currentTemperature);
  
  if (Ethernet.linkStatus() == LinkON) {
    Serial.printf("[MQTT] Publishing to topic '%s': %s\n", topic.c_str(), payload.c_str());
    Serial.println("[MQTT] Data sent successfully.");
  } else {
    Serial.println("[MQTT] Error: No Ethernet Link");
  }
}