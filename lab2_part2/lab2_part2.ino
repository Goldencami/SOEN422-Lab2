#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include "esp_bt.h"
#include "esp_coexist.h"
#define BUZZER_PIN 21

// ====== WiFi ======
const char *ssid = "BELL892";
const char *password = "1E7C373CF727";
// const char *ssid = "iPhoneCamila"; // my hotspot
// const char *password = "Nicolas19";


// ====== BLE UART ======
BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
BLEAdvertising *bleAdvertising;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

// ====== Radio Mode ======
enum RadioMode { MODE_BLE, MODE_HTTP };
RadioMode currentMode = MODE_BLE;
unsigned long lastSwitch = 0;
const unsigned long switchInterval = 10000; // 10 seconds per mode

// ====== Song Struct ======
struct Song {
  String name = "undefined";
  int tempo;
  int melody[50];
  int length = 0;
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer *pServer) {
    deviceConnected = true;
    Serial.println("Device connected");
  };

  void onDisconnect(BLEServer *pServer) {
    deviceConnected = false;
    Serial.println("Device disconnected");
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  private:
    String rxValue;

  public:
    void onWrite(BLECharacteristic *pCharacteristic) {
      rxValue = pCharacteristic->getValue();

      if (rxValue.length() > 0) {
        Serial.println("*********");
        Serial.print("Received Value: ");
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }

        Serial.println();
        Serial.println("*********");
      }
    }

    String getValue() {
      return rxValue;
    }

    void clearValue() {
      rxValue = "";
    }
};

bool isPlaying = false;
Song pendingSong;
bool songReady = false;
MyCallbacks myCallbacks;

// ====== Plays song ======
void play(Song object) {
  int notes = object.length / 2;
  int wholenote = (60000 * 4) / object.tempo;
  int divider = 0, noteDuration = 0;
  isPlaying = true;

  // iterate over the notes of the melody.
  // Remember, the array is twice the number of notes (notes + durations)
  for (int thisNote = 0; thisNote < notes * 2; thisNote = thisNote + 2) {
    // calculates the duration of each note
    divider = object.melody[thisNote + 1];
    if (divider > 0) {
      // regular note, just proceed
      noteDuration = (wholenote) / divider;
    } else if (divider < 0) {
      // dotted notes are represented with negative durations!!
      noteDuration = (wholenote) / abs(divider);
      noteDuration *= 1.5; // increases the duration in half for dotted notes
    }

    // we only play the note for 90% of the duration, leaving 10% as a pause
    tone(BUZZER_PIN, object.melody[thisNote], noteDuration * 0.9);

    // Wait for the specief duration before playing the next note.
    delay(noteDuration);

    // stop the waveform generation before the next note.
    noTone(BUZZER_PIN);
  }

  isPlaying = false;
}

// ====== HTTP Request to API ======
Song httpGET(String payload) {
  Song song;
  DynamicJsonDocument doc(1024);

  // Read the JSON document received from the API
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return song;
  }
  // Verify the fields are present
  if (!doc.containsKey("name") || !doc.containsKey("tempo") || !doc.containsKey("melody")) {
    Serial.println("JSON missing required fields!");
    return song;
  }

  song.name = doc["name"].as<String>();
  song.tempo = doc["tempo"].as<int>();
  JsonArray melodyArr = doc["melody"].as<JsonArray>();
  song.length = melodyArr.size();

  // Limit to avoid overflow (since melody[] has 50 elements)
  song.length = min(song.length, 50);

  for (int i = 0; i < song.length; i++) {
    song.melody[i] = melodyArr[i].as<int>();
  }
  
  return song;
}

void getCommand(String rxValue) { // return string
  if(rxValue == "!B813") { // -> button
    Serial.println("Next song");
  }
  else if(rxValue == "!B714") { // <- button
    Serial.println("Previous song");
  }
  else if(rxValue == "!B219") { // 2 button
    Serial.println("Play/Pause song");
  }
}

// ====== Wi-Fi connect ======
void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(ssid, password);
  Serial.printf("[WiFi] Connecting to %s\n", ssid);

  unsigned long start = millis();

  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[WiFi] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
  }
  else {
    Serial.println("\n[WiFi] Failed, retry later");
  }
}

// ====== BLE Setup ======
void setupBLE() {
  // Create the BLE Device
  BLEDevice::init("TTGO Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  // pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

  // // Descriptor 2902 is not required when using NimBLE as it is automatically added based on the characteristic properties
  // pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(&myCallbacks);

  // Start the service
  pService->start();

  // Start advertising
  bleAdvertising = pServer->getAdvertising();
  bleAdvertising->addServiceUUID(SERVICE_UUID);
  bleAdvertising->start();
  Serial.println("Waiting a client connection to notify...");
}

// completely stops BLE
void stopBLE() {
  if (pServer) {
    bleAdvertising->stop(); // stop advertising
    BLEDevice::deinit(true); // fully release BLE stack & memory
    pServer = nullptr;
    bleAdvertising = nullptr;
    pTxCharacteristic = nullptr;
    Serial.println("[BLE] Fully stopped for HTTP mode");
  }
}

// starts BLE
void startBLE() {
  setupBLE();  // reinitialize BLE stack and start advertising
  Serial.println("[BLE] Restarted BLE mode");
}


void setup() {
  Serial.begin(115200);

  // Connect BLE first
  setupBLE();
  connectWiFi();

  esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT); // free Classic BT memory
  esp_coex_preference_set(ESP_COEX_PREFER_WIFI); // Wi-Fi gets priority 
}

void loop() {
  unsigned long timeNow = millis();

  // Issue: http GET requests and BLE share the same radio
  // leads to interferance, http is not able to connect with API and returns error -1
  // Solution: switch between http and BLE in who is on the radio while the other in idle
  if (timeNow - lastSwitch >= switchInterval) {
    lastSwitch = timeNow;

    if (currentMode == MODE_BLE) {
      Serial.println("[MODE] Switching to HTTP mode");
      currentMode = MODE_HTTP;

      // Pause BLE advertising to free the radio completely
      stopBLE();
      Serial.println("[BLE] Advertising stopped for HTTP mode");

    } else {
      Serial.println("[MODE] Switching to BLE mode");
      currentMode = MODE_BLE;

      // Resume BLE advertising
      startBLE();
      Serial.println("[BLE] Advertising resumed for BLE mode");
    }
  }

  // specific behaviour depending on the mode
  if (currentMode == MODE_BLE) {
    // ensure notifications reach
    // Play any song received during HTTP mode
    if(songReady) {
        play(pendingSong);
        songReady = false;
    }
    delay(50); // keep loop light
  }
  else if(currentMode == MODE_HTTP) {
    if (WiFi.status() != WL_CONNECTED) {
      connectWiFi();
    }
    else if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin("https://iotjukebox.onrender.com/song");
      int httpResponseCode = http.GET();

      if(httpResponseCode > 0) {
        Serial.println("HTTP GET Response: " + String(httpResponseCode));
        String payload = http.getString();
        Serial.println(payload);

        pendingSong = httpGET(payload);
        songReady = true;
      }
      else {
        Serial.println("HTTP GET Error: " + String(httpResponseCode));
      }
      http.end(); // Free resources
    }

    delay(500); // short delay to avoid continuous HTTP
  }
}