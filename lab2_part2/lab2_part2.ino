#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#define BUZZER_PIN 21

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pRxCharacteristic;
NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID          "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

class RxCallback : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic *pCharacteristic) {  // pointer version is safe
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() > 0) {
      Serial.print("Received from Bluefruit: ");
      Serial.println(rxValue.c_str());

      // Add song control from app
      getCommand(rxValue.c_str());
    }
  }
};

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer *pServer) {
    Serial.println("Phone connected via BLE");
  }

  void onDisconnect(NimBLEServer *pServer) {
    Serial.println("Phone disconnected");
    NimBLEDevice::startAdvertising();
  }
};

// ====== WiFi ======
// const char *ssid = "BELL892";
// const char *password = "1E7C373CF727";
const char *ssid = "iPhoneCamila"; // my hotspot
const char *password = "Nicolas19";

// ====== Song Struct ======
struct Song {
  String name = "undefined";
  int tempo;
  int melody[50];
  int length = 0;
};

bool isPlaying = false;
Song pendingSong;
bool songReady = false;

// ====== Plays song ======
void play(Song object) {
  int notes = object.length / 2;
  int wholenote = (60000 * 4) / object.tempo;
  int divider = 0, noteDuration = 0;

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

void getCommand(const char* rxValue) { // return string
  if(rxValue == "!B813") { // -> button
    Serial.println("Next song");
    // fetch and play next song from API
    songReady = true;
  }
  else if(rxValue == "!B714") { // <- button
    Serial.println("Previous song");
    // handle previous song logic
    songReady = true;
  }
  else if(rxValue == "!B219") { // 2 button
    Serial.println("Play/Pause song");
    // handle previous song logic
    songReady = true;
    
    if(isPlaying) {
      noTone(BUZZER_PIN);
      isPlaying = false;
    } 
    else {
      play(pendingSong);
      isPlaying = true;
    }
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
  // Start BLE after WiFi is stable
  NimBLEDevice::init("ESP32 BLE UART");
  NimBLEDevice::setPower(ESP_PWR_LVL_N0);
  pServer = NimBLEDevice::createServer();
  pServer->setCallbacks(new ServerCallbacks());

  NimBLEService* pService = pServer->createService(SERVICE_UUID);

  // TX characteristic (ESP32 -> phone)
  pTxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_TX,
    NIMBLE_PROPERTY::NOTIFY
  );

  // RX characteristic (phone -> ESP32)
  pRxCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID_RX,
    NIMBLE_PROPERTY::WRITE
  );
  pRxCharacteristic->setCallbacks(new RxCallback());

  pService->start();

  NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->start();
  Serial.println("BLE advertising started");
}

void setup() {
  Serial.begin(115200);

  // Connect BLE first
  connectWiFi();
  setupBLE();
}

void loop() {
  if(WiFi.status() == WL_CONNECTED) {
    if(!isPlaying) {
      HTTPClient http;
      http.begin("https://iotjukebox.onrender.com/song");
      int httpResponseCode = http.GET();

      if(httpResponseCode > 0) {
        Serial.println("HTTP GET Response: " + String(httpResponseCode));
        String payload = http.getString();
        Serial.println(payload);

        pendingSong = httpGET(payload);
      }
      else {
        Serial.println("HTTP GET Error: " + String(httpResponseCode));
      }
      http.end(); // Free resources

      isPlaying = true;
      play(pendingSong);
      isPlaying = false;
      songReady = false; // reset after playing
    }
  }

  delay(500); // short delay to avoid continuous HTTP
}