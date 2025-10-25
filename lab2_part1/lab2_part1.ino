#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <BluetoothSerial.h>
#define BUZZER_PIN 21
#define BT_DISCOVER_TIME 10000

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

const char *ssid = "BELL892";
const char *password = "1E7C373CF727";
// const char *ssid = "iPhoneCamila"; // my hotspot
// const char *password = "Nicolas19";

String BASE_URL = "https://iotjukebox.onrender.com";
String STUDENT_ID = "40239038";
String DEVICE1_NAME = "iPhoneCamila";
String DEVICE2_NAME = "MBP_Camila";

BluetoothSerial SerialBT;
static bool btScanAsync = true;
String myDevice;
bool deviceFound = false;

struct Song {
  String name = "undefined";
  int tempo;
  int melody[50];
  int length = 0;
};

Song currentSong;
bool isPlaying = false;

void btAdvertisedDeviceFound(BTAdvertisedDevice *pDevice) {
  String discoveredDevice = pDevice->getName().c_str();
  Serial.printf("Devices found: %s\n", discoveredDevice.c_str());
  if(discoveredDevice == DEVICE1_NAME || discoveredDevice == DEVICE2_NAME) {
    myDevice = pDevice->getName().c_str();
    deviceFound = true;
    Serial.println("Saved device: " + myDevice);
    Serial.println("Stopping Bluetooth discovery...");
    SerialBT.discoverAsyncStop();
  }
}

void playSong(Song object) {
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

Song httpGET(String endpoint) {
  Song song;
  if (WiFi.status() != WL_CONNECTED) return song; // don't continue if there's no wifi

  HTTPClient http;
  http.begin(BASE_URL + endpoint);
  int httpCode = http.GET();
  String payload = "";

  if (httpCode > 0) {
    Serial.println("HTTP GET Response: " + String(httpCode));
    payload = http.getString();
    Serial.println(payload);
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpCode));
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
  // Limit to avoid overflow (since melody[] has 50 elements)
  song.length = min((int)melodyArr.size(), 50);

  for(int i = 0; i < song.length; i++) {
    song.melody[i] = melodyArr[i].as<int>();
  }
  
  return song;
}

Song getSong(String song_name) {
  return httpGET("/song?name=" + song_name);
}

Song getPreferedSong(String student_id, String device) {
  Song newSong;
  if (WiFi.status() != WL_CONNECTED) return newSong;

  HTTPClient http;
  http.begin(BASE_URL + "/preference?id=" + student_id + "&key=" + device);
  int httpCode = http.GET(); // will only return the name of the prefered song of the linked device
  String payload = "";

  if (httpCode > 0) {
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
  if (!doc.containsKey("name")) {
    Serial.println("JSON missing required fields!");
    return newSong;
  }
  String song_name = doc["name"].as<String>(); // converts JSON string into a string C++ can accept
  return httpGET("/song?name=" + song_name);
}

void postDevice(String student_id, String device, String song_name) {
  if (WiFi.status() != WL_CONNECTED) return ;

  String endpoint = "/preference?id=" + student_id + "&key=" + device + "&value=" + song_name;

  HTTPClient http;
  http.begin(BASE_URL + endpoint);
  int httpCode = http.POST(""); // Send HTTP POST request

  if (httpCode > 0) {
    Serial.println("HTTP POST Response: " + String(httpCode));
  }
  else {
    Serial.println("HTTP POST Error: " + String(httpCode));
    return;
  }
  
  http.end(); // Free resources
}

void setupWifi() {
  WiFi.begin(ssid, password);
  Serial.println("Connecting to wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to wifi!");
}

void asyncDiscovery() {
  SerialBT.begin("ESP32test");  //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
  if (btScanAsync) {
    Serial.print("Starting asynchronous discovery... ");
    if (SerialBT.discoverAsync(btAdvertisedDeviceFound)) {
      Serial.println("Findings will be reported in \"btAdvertisedDeviceFound\"");
      delay(10000);
      Serial.print("Stopping discoverAsync... ");
      SerialBT.discoverAsyncStop();
      Serial.println("stopped");
    } else {
      Serial.println("Error on discoverAsync f.e. not working after a \"connect\"");
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  SerialBT.begin("ESP32 BT");
  pinMode(BUZZER_PIN, OUTPUT);
  setupWifi();

  postDevice(STUDENT_ID, DEVICE1_NAME, "jigglypuffsong");
  postDevice(STUDENT_ID, DEVICE2_NAME, "zeldaslullaby");
  currentSong = getPreferedSong(STUDENT_ID, DEVICE1_NAME);

  asyncDiscovery();
}

void loop() {
  if (currentSong.length > 0 && myDevice == DEVICE1_NAME && !isPlaying) {
    playSong(currentSong);
  } else {
    Serial.println("No song received, skipping playback.");
    delay(1000);
  }
}