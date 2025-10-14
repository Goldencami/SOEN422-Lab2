#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define BUZZER_PIN 21

unsigned long timerDelay = 5000;
unsigned long lastTimeDelay = millis();

const char *ssid = "BELL892";
const char *password = "1E7C373CF727";
// const char *ssid = "iPhoneCamila"; // my hotspot
// const char *password = "Nicolas19";

String BASE_URL = "https://iotjukebox.onrender.com";
String STUDENT_ID = "40239038";

BLEServer *pServer = NULL;
BLECharacteristic *pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;
uint8_t txValue = 0;
#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"  // UART service UUID
#define CHARACTERISTIC_UUID_RX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_TX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

struct Song {
  String name = "undefined";
  int tempo;
  int melody[50];
  int length = 0;
};

Song currentSong;
int currentNote = 0;
unsigned long noteStartTime = 0;
int noteDuration = 0;
bool notePlaying = false;
bool isPlaying = false; // bool variable to know a song is playing and not interrupt it

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
  void onWrite(BLECharacteristic *pCharacteristic) {
    String rxValue = pCharacteristic->getValue();

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
};

void wifiConnection() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected, reconnecting...");
    WiFi.disconnect();
    WiFi.reconnect();
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) { // wait max 10s
      delay(500);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("\nReconnected to WiFi!");
    } else {
      Serial.println("\nFailed to reconnect to WiFi.");
    }
  }
}


void setSong(Song object) {
  currentSong = object;
  currentNote = 0;
  noteStartTime = millis();
  noteDuration = 0;
  notePlaying = false;
  isPlaying = true;
}

void playSong() {
  if (!isPlaying || currentNote >= currentSong.length) return;

  unsigned long timeNow = millis();

  if (!notePlaying) {
    // Calculate duration of current note
    int wholenote = (60000 * 4) / currentSong.tempo;
    int divider = currentSong.melody[currentNote + 1];
    if (divider > 0) {
      noteDuration = wholenote / divider;
    } else { 
      noteDuration = (wholenote / abs(divider)) * 1.5; // dotted notes
    }

    // Play the note for 90% of the duration
    tone(BUZZER_PIN, currentSong.melody[currentNote], noteDuration * 0.9);
    noteStartTime = timeNow;
    notePlaying = true;
  }

  // Check if the note duration has passed
  if (notePlaying && timeNow - noteStartTime >= noteDuration) {
    noTone(BUZZER_PIN);
    currentNote += 2; // move to next note
    notePlaying = false;
  }

  // If finished all notes
  if (currentNote >= currentSong.length) {
    isPlaying = false;
  }
}

Song httpGET(String endpoint) {
  Song song;
  if (WiFi.status() != WL_CONNECTED) return song; // don't continue if there's no wifi

  HTTPClient http;
  http.begin(BASE_URL + endpoint, "");

  int httpResponseCode = http.GET();
  String payload = "";

  if (httpResponseCode > 0) {
    Serial.println("HTTP GET Response: " + String(httpResponseCode));
    payload = http.getString(); // fetch the query
    Serial.println(payload);
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpResponseCode));
    return song;
  }

  http.end(); // Free resources

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

Song getSong(String song_name) {
  return httpGET("/song?name=" + song_name);
}

void setup() {
  Serial.begin(115200);

  // Connect WiFi first
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected to WiFi!");

  Song toPlay = httpGET("/song");
  setSong(toPlay);

  // Create the BLE Device
  BLEDevice::init("TTGO Service");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX, BLECharacteristic::PROPERTY_NOTIFY);

  // Descriptor 2902 is not required when using NimBLE as it is automatically added based on the characteristic properties
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic *pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX, BLECharacteristic::PROPERTY_WRITE);

  pRxCharacteristic->setCallbacks(new MyCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();
  Serial.println("Waiting a client connection to notify...");
}

void loop() {
  wifiConnection();

  unsigned long timeNow = millis();
  if(!isPlaying && (timeNow - lastTimeDelay > timerDelay)) {
    if (WiFi.status() == WL_CONNECTED) {
      Song toPlay = httpGET("/song");
      if (toPlay.length > 0) {  // only set song if HTTP GET succeeded
        setSong(toPlay);
        lastTimeDelay = timeNow;
      } else {
        Serial.println("Failed to fetch song, will retry later...");
      }
    } else {
      Serial.println("Wi-Fi not connected, skipping song fetch...");
    }
  }

  playSong();
}
