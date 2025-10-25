#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include <vector>
#define BUZZER_PIN 21
#define BATCH_SIZE 5

// Wifi credentials
const char* ssid = "BELL892";
const char* password = "1E7C373CF727";

// BLE UART UUIDs
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

BLEAdvertising* pAdvertising = nullptr;
BLECharacteristic* pTxCharacteristic;
bool deviceConnected = false;
SemaphoreHandle_t playlistMutex;

// ====== Functions signature ======
void startSong(int index);
void fetchSongsBatch();

// ====== Song Struct ======
struct Song {
  String name = "undefined";
  int tempo = 120;
  int melody[50] = {0};
  int length = 0;
};

// ====== Playlist Variables ======
std::vector<Song> PlayList;
Song* currentSong = nullptr;
int currentSongIndex = 0;  // index for playlist
int currentNoteIndex = 0;
unsigned long nextNoteTime = 0;
int noteDuration = 0;
bool isPlaying = true;
bool fetchNextSongFlag = false; // to avoid fetching new batch more than once

class MyServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) { 
    deviceConnected = true; 
    Serial.println("BLE connected");
  }
  void onDisconnect(BLEServer* pServer) { 
    deviceConnected = false; 
    Serial.println("BLE disconnected"); 
  }
};

class MyCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic* pCharacteristic) {
    String val = pCharacteristic->getValue().c_str();

    if (val.length() == 0) return;

    char cmd = val[0];
    switch(cmd) {
      case 'P': // Pause/Play song
        isPlaying = !isPlaying; 
        Serial.printf("Play toggle -> %d\n", isPlaying); 
        break;
      case 'N': // Next song
        if(currentSongIndex < PlayList.size() - 1) {
          startSong(currentSongIndex + 1);
        } 
        else {
          fetchNextSongFlag = true; // fetch new batch
        }
        break;
      case 'B': // Previous song
        if (PlayList.size() > 0 && currentSongIndex > 0) {
          startSong(currentSongIndex - 1);
          break;
        }
    }
  }
};
// ====== Wifi Setup ======
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println("Connecting to Wifi...");
  WiFi.disconnect(true); // fresh connection, avoid connecting to a previous network
  WiFi.begin(ssid, password);

  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(250); 
    Serial.print(".");
    retries++;
  }
  Serial.println();

  // In case wifi ever gets disconnected
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }

  Serial.println("Wifi connected");
  delay(200); // stabilization

  return true;
}

// ====== BLE Setup ======
void startBLE() {
  BLEDevice::init("TTGO_Jukebox");
  BLEServer* pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService* pService = pServer->createService(SERVICE_UUID);

  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());
  BLECharacteristic* pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);
  pRxCharacteristic->setCallbacks(new MyCallbacks());
  pService->start();

  pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setMinInterval(3200);
  pAdvertising->setMaxInterval(4000);
  pAdvertising->start();
  Serial.println("BLE resumed and advertising...");
}

void stopBLE() {
  if (pAdvertising) pAdvertising->stop();
  BLEDevice::deinit(true);
  Serial.println("BLE stopped");
}

// ====== Songs Functions ======
void play() {
  if (!isPlaying) return;

  if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
    if (!currentSong) {
      xSemaphoreGive(playlistMutex);
      return;
    }

    unsigned long now = millis();
    if (now >= nextNoteTime) {
      if (currentNoteIndex >= currentSong->length) {
        currentNoteIndex = 0;
        currentSongIndex++;

        if (currentSongIndex >= PlayList.size()) {
          fetchNextSongFlag = true;
        } 
        else {
          startSong(currentSongIndex);
        }

        xSemaphoreGive(playlistMutex);
        return;
      }

      int pitch = currentSong->melody[currentNoteIndex];
      int divider = currentSong->melody[currentNoteIndex + 1];
      int wholenote = (60000 * 4) / currentSong->tempo;

      if (divider > 0) {
        noteDuration = wholenote / divider;
      }
      else if (divider < 0) {
        noteDuration = (wholenote / abs(divider)) * 1.5;
      }
      else {
        noteDuration = wholenote / 4;
      }

      if (pitch > 0) {
        tone(BUZZER_PIN, constrain(pitch, 100, 5000), noteDuration * 0.9);
      }
      else {
        noTone(BUZZER_PIN);
      }

      nextNoteTime = now + noteDuration;
      currentNoteIndex += 2;
    }
    xSemaphoreGive(playlistMutex);
  }
}

Song fetchSong() {
  Song newSong;

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, "https://iotjukebox.onrender.com/song");
  http.setTimeout(7000);
  int httpCode = http.GET();
  String payload = "";

  if(httpCode > 0) {
    payload = http.getString();
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpCode));
    return newSong;
  }
  http.end(); // Free resources

  DynamicJsonDocument doc(1024);
  // Read the JSON document received from the API
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return newSong;
  }
  // Verify the fields are present
  if (!doc.containsKey("name") || !doc.containsKey("tempo") || !doc.containsKey("melody")) {
    Serial.println("JSON missing required fields!");
    return newSong;
  }

  newSong.name = doc["name"].as<String>();
  newSong.tempo = doc["tempo"].as<int>();
  JsonArray melodyArr = doc["melody"].as<JsonArray>();
  newSong.length = melodyArr.size();

  // Limit to avoid overflow (since melody[] has 50 elements)
  newSong.length = min(newSong.length, 50);
  for (int i = 0; i < newSong.length; i++) {
    newSong.melody[i] = melodyArr[i].as<int>();
  }

  WiFi.disconnect(true); // turn off Wifi after fetch
  Serial.println("Wifi stopped");
  return newSong;
}

// Loads song from queue
// Reset playlist variables (currentNoteIndex, nextNoteTime, ...)
void startSong(int index) {
  if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
    if (PlayList.empty()) {
      Serial.println("No songs in playlist!");
      xSemaphoreGive(playlistMutex);
      return;
    }
    // handle error
    if (index < 0 || index >= PlayList.size()) index = 0;

    currentSongIndex = index;
    currentSong = &PlayList[currentSongIndex]; // don't make a copy, get direct address 
    currentNoteIndex = 0;
    nextNoteTime = millis();
    isPlaying = true;

    Serial.println("Now playing: " + currentSong->name + " (index " + String(currentSongIndex) + ")");
    xSemaphoreGive(playlistMutex);
  }
}

// have a limit of playlist having 20 songs
void addSongToPlaylist(Song s) {
  if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
    if (PlayList.size() >= 20) {
      PlayList.clear(); // remove all songs
      currentSong = nullptr;
      currentSongIndex = 0;
      currentNoteIndex = 0;
      nextNoteTime = 0;
      fetchNextSongFlag = true; // trigger fetch of new batch
    }

    PlayList.push_back(s);
    xSemaphoreGive(playlistMutex);
  }
}

// Wifi and BLE run in the same radio, but BLE has more priority.
// This causes wifi's connection to server to be interrupted by BLE
// SOLUTION: completely stop BLE when wifi needs to run, then restart it
void fetchSongsBatch(void* param) {
  // Stop BLE completely, stops advertising and deinitializes BLE
  for(;;) {
    if(fetchNextSongFlag){
      Serial.println("Stopping BLE for Wi-Fi batch fetch...");
      stopBLE();

      if(connectWiFi()) {  // connect once per batch
        Serial.println("Fetching new batch...");
        for(int i=0; i<BATCH_SIZE; i++) {
          Song s = fetchSong();  // use the updated function
          if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
            addSongToPlaylist(s);
            Serial.println(String(s.name) + " added to playlist.");
            xSemaphoreGive(playlistMutex);
          }
        }

        WiFi.disconnect(true); // disconnect once after batch
        Serial.println("Wifi stopped");
      }

      Serial.println("Restarting BLE...");
      startBLE();
      // safely start the first song of the new batch
      if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
        fetchNextSongFlag = false;
        startSong(PlayList.size() - BATCH_SIZE);
        xSemaphoreGive(playlistMutex);
      }
    }
    vTaskDelay(50/portTICK_PERIOD_MS);
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  playlistMutex = xSemaphoreCreateMutex();
  if (!playlistMutex) {
    Serial.println("Failed to create mutex!");
  }

  if (xSemaphoreTake(playlistMutex, (TickType_t)50) == pdTRUE) {
    Serial.println("Fetching initial songs...");
    for(int i=0; i < BATCH_SIZE; i++) {
      Song s = fetchSong();
      addSongToPlaylist(s);
      Serial.println(String(s.name) + " added to playlist.");
    }
    xSemaphoreGive(playlistMutex);
  }

  if (!PlayList.empty()) {
    startSong(0);
  }

  // Start BLE
  startBLE();
  // Fetches songs in the background
  // Allows BLE and playback to keep running while fetching new songs
  xTaskCreate(fetchSongsBatch, "FetchTask", 16384, NULL, 1, NULL);
}

void loop() {
  play();
}