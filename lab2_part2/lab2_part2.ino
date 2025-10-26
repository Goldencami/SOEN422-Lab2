#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <NimBLEDevice.h>
#include <NimBLEServer.h>
#include <NimBLEUtils.h>
#include <vector>
#define BUZZER_PIN 21
#define PLAYLIST_MAX 5

const char *ssid = "iPhoneCamila"; // my hotspot
const char *password = "soen422_lab";

// Nordic UART Service UUIDs
static NimBLEUUID NUS_SERVICE_UUID("6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
static NimBLEUUID NUS_RX_UUID     ("6E400002-B5A3-F393-E0A9-E50E24DCCA9E");
static NimBLEUUID NUS_TX_UUID     ("6E400003-B5A3-F393-E0A9-E50E24DCCA9E");
NimBLECharacteristic* g_rxChar = nullptr;

// Song Struct
struct Song {
  String name = "undefined";
  int tempo = 120;
  int melody[50] = {0};
  int length = 0;
};

std::vector<Song> playList;
Song* currentSong = nullptr;
int currentSongIndex = 0;
int noteIndex = 0;
unsigned long noteStartTime = 0;
unsigned long noteElapsed = 0;
bool notePlaying = false;

// Inform the compiler that a variable's value can change unexpectedly
volatile bool skipNext = false;
volatile bool skipPrev = false;
volatile bool paused = false;
volatile bool isPlaying = false;
volatile bool songFinished = false;

// BLE receive callback
class RxCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    std::string rxValue = c->getValue();
    if (rxValue.empty()) return;

    String val;
    for (char ch : rxValue) {
      val += (char)ch;
    } 

    if(val == "!B813") {
      skipNext = true;
      Serial.println("Playing next song");
    }
    else if(val == "!B714") {
      skipPrev = true;
      Serial.println("Playing previous song");
    }
    else if(val == "!B219") {
      paused = !paused;
      Serial.println(paused ? "Paused song" : "Resumed song");
    }
  }
};

void setupWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // disable wifi power saving, always on in radio

  Serial.println("Connecting to wifi...");
  WiFi.begin(ssid, password);

  unsigned long timeNow = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - timeNow < 15000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("Connected. IP: "); Serial.println(WiFi.localIP());
  } else {
    Serial.println("ERROR: Failed to connect.");
  }
}

void setupBLE() {
  // Initialize BLE
  NimBLEDevice::init("ESP32-Jukebox");
  NimBLEDevice::setPower(ESP_PWR_LVL_P7); // max TX power

  // Create server
  NimBLEServer* server = NimBLEDevice::createServer();
  // Create NUS service
  NimBLEService* nus = server->createService(NUS_SERVICE_UUID);
  // TX characteristic 
  nus->createCharacteristic(NUS_TX_UUID, NIMBLE_PROPERTY::NOTIFY);
  // RX characteristic -> receives commands from phone
  g_rxChar = nus->createCharacteristic(NUS_RX_UUID, NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR);
  g_rxChar->setCallbacks(new RxCallbacks());

  // Start the service
  nus->start();
  // Advertising
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(NUS_SERVICE_UUID);

  NimBLEAdvertisementData scanData;
  scanData.setName("ESP32-Jukebox");
  adv->setScanResponseData(scanData);

  adv->setMinInterval(0x30); // ~37.5ms
  adv->setMaxInterval(0x60); // ~75ms
  NimBLEDevice::startAdvertising();
  Serial.println("Advertising ESP32-Jukebox...");
}

Song fetchSong() {
  Song newSong;
  if (WiFi.status() != WL_CONNECTED) return newSong;

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

  Serial.println("Fetched song " + newSong.name);
  return newSong;
}

void prevSong() {
  if(currentSongIndex > 0) {
    noTone(BUZZER_PIN);
    notePlaying = false;
    noteElapsed = 0;
    isPlaying = false;
    currentSongIndex--;
    noteIndex = 0;
    currentSong = &playList[currentSongIndex];
    isPlaying = true;
  }
  else {
    Serial.println("Already at start of playlist");
  }
  skipPrev = false;
}

void nextSong() {
  delay(200);
  noTone(BUZZER_PIN);
  notePlaying = false;
  isPlaying = false;
  noteIndex = 0;
  songFinished = false;

  if(currentSongIndex < playList.size() - 1) {
    // Move to next song in playlist
    currentSongIndex++;
    } 
  else {
    // At the end of playlist: try fetching a new song
    Song newSong = fetchSong();
    if(newSong.name != "undefined") {
      playList.push_back(newSong);
      currentSongIndex = playList.size() - 1;
    } 
    else {
      // No new song fetched, loop to first
      currentSongIndex = 0;
    }
  }

  currentSong = &playList[currentSongIndex];
  isPlaying = true;
  notePlaying = false;
}

void playSong() {
  if(!currentSong) return;

  // Handle skip commands immediately
  if(skipNext) { 
    skipNext = false;
    nextSong(); 
    return;
  }
  if(skipPrev) { 
    skipPrev = false;
    prevSong(); 
    return;
  }

  // Paused state
  if(paused) {
    noTone(BUZZER_PIN);
    return;
  }

  // Check if song finished
  if(noteIndex >= currentSong->length) {
    if(!songFinished) {
      songFinished = true;  // prevent repeated nextSong calls
      nextSong();
    }
    return;
  }

  static bool inPause = false;
  static unsigned long pauseStart = 0;

  int pitch = currentSong->melody[noteIndex];
  int divider = currentSong->melody[noteIndex + 1];
  int wholenote = (60000 * 4) / currentSong->tempo;
  unsigned long noteDuration = (divider > 0 ? wholenote / divider : (wholenote / abs(divider)) * 1.5);
  unsigned long playDuration = noteDuration * 0.9; // 90% for note, 10% pause

  if(!notePlaying) {
    if(pitch != 0) tone(BUZZER_PIN, pitch); // play the note
    noteStartTime = millis();
    notePlaying = true;
    inPause = false;
  }

  if(!inPause && millis() - noteStartTime >= playDuration) {
    noTone(BUZZER_PIN);
    inPause = true;
    pauseStart = millis();
  }

  if(inPause && millis() - pauseStart >= (noteDuration - playDuration)) {
    noteIndex += 2;   // next note
    notePlaying = false;
    inPause = false;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(BUZZER_PIN, OUTPUT);
  setupBLE();
  setupWifi();
  delay(1000);

  for(int i=0; i<PLAYLIST_MAX; i++) {
    Song newSong = fetchSong();
    if(newSong.name != "undefined") {
      playList.push_back(newSong);
    }
  }

  if(!playList.empty()) {
    currentSong = &playList[currentSongIndex];
    noteIndex = 0;
    noteElapsed = 0;
    isPlaying = true;
    songFinished = false;
    notePlaying = false;
  }
}

void loop() {
  playSong();
  delay(1); // tiny yield
}