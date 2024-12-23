#include <esp_now.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define MQTT_BROKER "broker.espx.io"
#define MQTT_TOPIC_PUBLISH "binus/CompEng/IoT/esp32/inpoint"

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32

int rssi1 = 0; // HD3
int rssi2 = 0; // HD4
int receivedCount = 0;

WiFiManager wm;
WiFiClient espClient;
PubSubClient mqtt(espClient);

// Define a data structure with a room identifier
typedef struct struct_message {
  char deviceName[32];
  int rssi;
  int roomID; // Added room identifier
} struct_message;

struct_message myData;

// OLED display object
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Function declarations
void mqttCallback(char* topic, byte* payload, unsigned int len);
boolean mqttConnect();

void OnDataRecv(const esp_now_recv_info_t *recv_info, const uint8_t *incomingData, int len) {
  Serial.print("Packet received from: ");
  for (int i = 0; i < 6; i++) {
    Serial.printf("%02X", recv_info->src_addr[i]);
    if (i < 5) Serial.print(":");
  }
  Serial.println();

  Serial.printf("Packet size: %d bytes\n", len);
  memcpy(&myData, incomingData, sizeof(myData));
  Serial.printf("Device: %s, RSSI: %d, Room ID: %d\n", myData.deviceName, myData.rssi, myData.roomID);

  if (myData.roomID == 1) {
    rssi1 = myData.rssi;
  } else if (myData.roomID == 2) {
    rssi2 = myData.rssi;
  }

  receivedCount++;

  if (receivedCount >= 2) {
    char payload[128];

    if (rssi1 > rssi2) {
      Serial.print("HD3 RSSI: ");
      Serial.println(rssi1);
      snprintf(payload, sizeof(payload), "{\"deviceName\":\"Amazfit\",\"roomID\":\"HD3\",\"rssi\":%d}", rssi1);
    } else if (rssi2 > rssi1) {
      Serial.print("HD4 RSSI: ");
      Serial.println(rssi2);
      snprintf(payload, sizeof(payload), "{\"deviceName\":\"Amazfit\",\"roomID\":\"HD4\",\"rssi\":%d}", rssi2);
    } else {
      Serial.println("Both rooms have equal signal strength");
      snprintf(payload, sizeof(payload), "{\"deviceName\":\"Amazfit\",\"roomID\":\"equal\",\"rssi1\":%d}", rssi1);
    }

    // Publish to MQTT
    if (mqtt.publish(MQTT_TOPIC_PUBLISH, payload)) {
      Serial.println("Publish successful");
    } else {
      Serial.println("Publish failed");
    }

    // Update OLED display with RSSI and room info
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    if (rssi1 > rssi2) {
      display.setCursor(0, 0);
      display.println("HD3 RSSI:");
      display.setCursor(0, 10);
      display.print(rssi1);
    } else if (rssi2 > rssi1) {
      display.setCursor(0, 0);
      display.println("HD4 RSSI:");
      display.setCursor(0, 10);
      display.print(rssi2);
    } else {
      display.setCursor(0, 0);
      display.println("Equal RSSI:");
      display.setCursor(0, 10);
      display.print(rssi1);
    }

    display.display();

    // Reset receivedCount after publishing
    receivedCount = 0;
  }
}

void setup() {
  Serial.begin(9600);

  WiFi.mode(WIFI_STA);

  wm.setConfigPortalTimeout(180);
  if(!wm.autoConnect("ESP32-BLE-AP")) {
    Serial.println("Failed to connect and hit timeout");
    delay(3000);
    ESP.restart();
  }

  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());

  mqtt.setServer(MQTT_BROKER, 1883);
  mqttConnect();

  Wire.begin(4, 5); // SDA to pin 4, SCK to pin 5
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_recv_cb(OnDataRecv);
}

void loop() {
  mqtt.loop();
}

boolean mqttConnect() {
  char g_szDeviceId[30];
  sprintf(g_szDeviceId, "esp32_%08X", (uint32_t)ESP.getEfuseMac());
  mqtt.setCallback(mqttCallback);
  Serial.printf("Connecting to %s clientId: %s\n", MQTT_BROKER, g_szDeviceId);

  boolean fMqttConnected = false;
  for (int i = 0; i < 3 && !fMqttConnected; i++) {
    Serial.print("Connecting to mqtt broker...");
    fMqttConnected = mqtt.connect(g_szDeviceId);
    if (!fMqttConnected) {
      Serial.print(" fail, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
  if (fMqttConnected) {
    Serial.println(" success");
  }
  return mqtt.connected();
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.write(payload, len);
  Serial.println();