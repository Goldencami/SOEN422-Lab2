#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#include <BluetoothSerial.h>
#define BUZZER_PIN 21
#define BT_DISCOVER_TIME 10000

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Bluetooth not available or not enabled. It is only available for the ESP32 chip.
#endif

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

// const char *ssid = "BELL892";
// const char *password = "1E7C373CF727";
const char *ssid = "iPhoneCamila"; // my hotspot
const char *password = "Nicolas19";

String BASE_URL = "https://iotjukebox.onrender.com";
String STUDENT_ID = "40239038";
String DEVICE1_NAME = "iPhoneCamila";
String DEVICE2_NAME = "Camila_MBP";

BluetoothSerial SerialBT;
static bool btScanSync = true;

struct Song {
  String name = "undefined";
  int tempo;
  int melody[50];
  int length = 0;
};

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

Song httpGET(String endpoint) {
  Song randomSong;
  if (WiFi.status() != WL_CONNECTED) return randomSong; // don't continue if there's no wifi

  HTTPClient http;
  http.begin(BASE_URL + endpoint);

  int httpResponseCode = http.GET();
  String payload = "";

  if (httpResponseCode > 0) {
    Serial.println("HTTP GET Response: " + String(httpResponseCode));
    payload = http.getString();
    Serial.println(payload);
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpResponseCode));
    return randomSong;
  }

  http.end(); // Free resources

  JSONVar songObj = JSON.parse(payload);
  if (JSON.typeof(songObj) == "undefined" || !songObj.hasOwnProperty("melody")) {
    Serial.println("Parsing input failed!");
    return randomSong;
  }

  randomSong.name = (const char*) songObj["name"]; // converts JSON string into a string C++ can accept
  randomSong.tempo = atoi((const char*) songObj["tempo"]); // converts string to an integer

  JSONVar melodyArr = songObj["melody"];
  randomSong.length = songObj["melody"].length();

  for (int i = 0; i < randomSong.length; i++) {
    randomSong.melody[i] = atoi((const char*) melodyArr[i]);
  }

  return randomSong;
}

Song getSong(String song_name) {
  return httpGET("/song?name=" + song_name);
}

Song getPreferedSong(String student_id, String device) {
  Song emptySong;
  if (WiFi.status() != WL_CONNECTED) return emptySong;

  HTTPClient http;
  http.begin(BASE_URL + "/preference?id=" + student_id + "&key=" + device);

  int httpResponseCode = http.GET(); // will only return the name of the prefered song of the linked device
  String payload = "";

  if (httpResponseCode > 0) {
    payload = http.getString();
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpResponseCode));
    return emptySong;
  }
  http.end(); // Free resources

  JSONVar songObj = JSON.parse(payload);
  if (JSON.typeof(songObj) == "undefined") {
    Serial.println("Parsing input failed!");
    return emptySong;
  }

  String song_name = (const char*) songObj["name"]; // converts JSON string into a string C++ can accept
  return httpGET("/song?name=" + song_name);
}

void postDevice(String student_id, String device, String song_name) {
  if (WiFi.status() != WL_CONNECTED) return ;

  String endpoint = "/preference?id=" + student_id + "&key=" + device + "&value=" + song_name;

  HTTPClient http;
  http.begin(BASE_URL + endpoint);
  int httpResponseCode = http.POST(""); // Send HTTP POST request

  if (httpResponseCode > 0) {
    Serial.println("HTTP POST Response: " + String(httpResponseCode));
  }
  else {
    Serial.println("HTTP POST Error: " + String(httpResponseCode));
    return;
  }
  
  http.end(); // Free resources
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  //************************
  //   SETTING BLUETOOTH
  //************************
  SerialBT.begin("ESP32test");  //Bluetooth device name
  Serial.println("The device started, now you can pair it with bluetooth!");
  if (btScanSync) {
    Serial.println("Starting synchronous discovery... ");
    BTScanResults *pResults = SerialBT.discover(BT_DISCOVER_TIME);
    if (pResults) {
      pResults->dump(&Serial);
    } else {
      Serial.println("Error on BT Scan, no result!");
    }
  }

  //************************
  //     SETTING WIFI
  //************************
  WiFi.begin(ssid, password);
  Serial.println("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  Serial.println("GET RANDOM SONG");
  httpGET("/song");
  Serial.println("POST DEVICE");
  postDevice(STUDENT_ID, DEVICE1_NAME, "harrypotter");
  Serial.println("GET DEVICE");
  Song toPlay = getPreferedSong(STUDENT_ID, DEVICE1_NAME);
  play(toPlay);
  // Serial.println("GET SONG");
  // getSong("gameofthrones");
}

void loop() {
}