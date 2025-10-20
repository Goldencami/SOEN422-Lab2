#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#define BUZZER_PIN 21
#define QUEUE_LENGTH 10

NimBLEServer* pServer = nullptr;
NimBLECharacteristic* pRxCharacteristic;
NimBLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;

#define SERVICE_UUID          "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

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

// ====== Song Variables ======
Song queue[QUEUE_LENGTH];
Song pendingSong;
int currentSongIndex = 0;
int currentNoteIndex = 0;
int noteDuration = 0;
bool isPlaying = false;
bool isPaused = false;
bool songReady = false;
unsigned long noteStartTime = 0;

// ====== BLE Commands ======
void getCommand(const char* rxValue) { // return string
  if(rxValue == "!B813") { // -> button
    Serial.println("Next song");
    if(currentSongIndex < (QUEUE_LENGTH-1)){
      ++currentNoteIndex;
      currentNoteIndex = 0;
      isPlaying = true;
      isPaused = false;
    }
    else {
      Song randSong = getRandomSong();
      play(randSong);
    }
  }
  else if(rxValue == "!B714") { // <- button
    Serial.println("Previous song");
    if(currentSongIndex > 0) {
      --currentSongIndex;
      currentNoteIndex = 0;
      isPlaying = true;
      isPaused = false;
    }
  }
  else if(rxValue == "!B219") { // 2 button
    Serial.println("Play/Pause song");
    if (isPlaying && !isPaused) {
      Serial.println("Pausing song");
      isPaused = true;
      noTone(BUZZER_PIN); // stop sound
    } 
    else {
      Serial.println("Resuming song");
      isPlaying = true;
      isPaused = false;
      noteStartTime = millis(); // resume timing from now
    }
  }
}

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
    deviceConnected = true;
  }

  void onDisconnect(NimBLEServer *pServer) {
    Serial.println("Phone disconnected");
    deviceConnected = false;
    NimBLEDevice::startAdvertising();
  }
};

// ====== Plays song ======
void play(Song object) {
  if (!isPlaying || isPaused) return; // only play when active

  int notes = object.length / 2;
  int wholenote = (60000 * 4) / object.tempo;

  // Only handle a note if time passed
  if (millis() - noteStartTime >= noteDuration) {
    // Stop any previous tone
    noTone(BUZZER_PIN);

    // Check if song ended
    if (currentNoteIndex >= notes * 2) {
      Serial.println("Song finished");
      isPlaying = false;
      isPaused = false;
      currentNoteIndex = 0;
      return;
    }

    // Calculate next note duration
    int divider = object.melody[currentNoteIndex + 1];
    if (divider > 0) {
      noteDuration = wholenote / divider;
    } 
    else if (divider < 0) {
      noteDuration = (wholenote / abs(divider)) * 1.5;
    }

    // Play next note
    int frequency = object.melody[currentNoteIndex];
    tone(BUZZER_PIN, frequency, noteDuration * 0.9);

    noteStartTime = millis(); // reset timer for next note
    currentNoteIndex += 2; // move to next note
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

Song getRandomSong() {
  Song randSong;

  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin("https://iotjukebox.onrender.com/song");
    int httpResponseCode = http.GET();

    if(httpResponseCode > 0) {
      Serial.println("HTTP GET Response: " + String(httpResponseCode));
      String payload = http.getString();
      Serial.println(payload);

      randSong = httpGET(payload);
    }
    else {
      Serial.println("HTTP GET Error: " + String(httpResponseCode));
    }
    http.end(); // Free resources
  }

  return randSong;
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
  connectWiFi(); // in case wifi gets disconnected
  
  // Wait until phone is connected
  // if (!deviceConnected) {
  //   noTone(BUZZER_PIN); // make sure buzzer is silent
  //   isPlaying = false; // ensure we’re not playing
  //   isPaused = false;
  //   return; // do nothing until BLE connected
  // }

  if(WiFi.status() == WL_CONNECTED) {
    if(!isPlaying && !isPaused) {
      HTTPClient http;
      http.begin("https://iotjukebox.onrender.com/song");
      int httpResponseCode = http.GET();

      if(httpResponseCode > 0) {
        Serial.println("HTTP GET Response: " + String(httpResponseCode));
        String payload = http.getString();
        Serial.println(payload);

        pendingSong = httpGET(payload);
        songReady = true;
        isPlaying = true;  // start playback after loading
        currentNoteIndex = 0;
      }
      else {
        Serial.println("HTTP GET Error: " + String(httpResponseCode));
      }
      http.end(); // Free resources

      if (songReady) {
        play(pendingSong);
      }
    }
  }

  delay(500); // short delay to avoid continuous HTTP
}