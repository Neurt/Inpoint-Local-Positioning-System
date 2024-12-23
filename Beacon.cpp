#include <esp_now.h>
#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEScan.h>

const int scanTime = 2; 
const uint8_t broadcastAddress[] = {0xA0, 0xDD, 0x6C, 0xAF, 0x76, 0x14};
BLEScan* pBLEScan;


struct struct_message {
  char deviceName[32];
  int rssi;
  int roomID; 
} myData;

esp_now_peer_info_t peerInfo;

void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  Serial.begin(9600);
  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);


  memcpy(peerInfo.peer_addr, broadcastAddress, 6);
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;

  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(500);
  pBLEScan->setWindow(100);
  Serial.println("Peer added successfully");
  Serial.println("Scanning...");


  myData.roomID = 1; 
}

void loop() {
  BLEScanResults foundDevices = pBLEScan->start(scanTime, false);

  for (int i = 0; i < foundDevices.getCount(); i++) {
    BLEAdvertisedDevice device = foundDevices.getDevice(i);
    if (device.haveName() && strcmp(device.getName().c_str(), "Amazfit Bip U") == 0) {
      myData.rssi = device.getRSSI();
      strncpy(myData.deviceName, device.getName().c_str(), sizeof(myData.deviceName) - 1);
      myData.deviceName[sizeof(myData.deviceName) - 1] = '\0';

      // Ensure peer exists before sending
      if (!esp_now_is_peer_exist(broadcastAddress)) {
        Serial.println("Peer not registered. Adding peer.");
        if (esp_now_add_peer(&peerInfo) != ESP_OK) {
          Serial.println("Failed to add peer");
          continue;
        }
      }

      esp_err_t result = esp_now_send(broadcastAddress, (uint8_t *)&myData, sizeof(myData));
      Serial.println(result == ESP_OK ? "Sending confirmed" : "Sending error");
    }
  }

  pBLEScan->clearResults();
}