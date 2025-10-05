#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#define BUZZER_PIN 21

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

const char *ssid = "BELL892";
const char *password = "1E7C373CF727";

String BASE_URL = "https://iotjukebox.onrender.com";
String STUDENT_ID = "40239038";
String DEVICE1_NAME = "Camila_iPhone16";
String DEVICE2_NAME = "Camila_MBP";

JSONVar httpGET(String endpoint) { // return string??
  if (WiFi.status() != WL_CONNECTED) return JSONVar();

  HTTPClient http;
  http.begin(BASE_URL + endpoint);

  int httpResponseCode = http.GET();
  String payload = "";

  if (httpResponseCode > 0) {
    Serial.println("HTTP GET Response: " + String(httpResponseCode));
    Serial.println(http.getString()); // prints payload
    payload = http.getString();
  }
  else {
    Serial.println("HTTP GET Error: " + String(httpResponseCode));
  }
  
  http.end(); // Free resources
  // return payload;

  if (payload == "") return JSONVar();

  JSONVar songObj = JSON.parse(payload);

  if (JSON.typeof(songObj) == "undefined") {
    Serial.println("Parsing input failed!");
    return JSONVar();
  }

  return songObj;
}

JSONVar getSong(String song_name) { // change to string??
  return httpGET("/song?name=" + song_name);
  // if (payload == "") return JSONVar();
 
  // JSONVar songObj = JSON.parse(payload);

  // if (JSON.typeof(songObj) == "undefined") {
  //   Serial.println("Parsing input failed!");
  //   return JSONVar();
  // }

  // return songObj;


  // int tempo = (int)songObj["tempo"];
  // JSONVar melody = songObj["melody"];

  // Serial.println("Tempo: " + String(tempo));
  // Serial.print("Melody: ");

  // for (int i = 0; i < melody.length(); i++) {
  //   Serial.print((int)melody[i]);
  //   Serial.print(" ");
  // }
  // Serial.println();
  
  // return payload;
}

JSONVar getDevice(String student_id, String device) {
  return httpGET("/preference?id=" + student_id + "&key=" + device);
  // if (payload == "") return JSONVar();

  // JSONVar songObj = JSON.parse(payload);

  // if (JSON.typeof(songObj) == "undefined") {
  //   Serial.println("Parsing input failed!");
  //   return JSONVar();
  // }

  // return songObj;
}

void postDevice(String student_id, String device, String song_name) {
  if (WiFi.status() != WL_CONNECTED) return;

  String endpoint = "/preference?id=" + student_id + "&key=" + device + "&value=" + song_name;

  HTTPClient http;
  http.begin(BASE_URL + endpoint);
  int httpResponseCode = http.POST(""); // Send HTTP POST request

  if (httpResponseCode > 0) {
    Serial.println("HTTP POST Response: " + String(httpResponseCode));
    Serial.println(http.getString()); // prints payload
  }
  else {
    Serial.println("HTTP POST Error: " + String(httpResponseCode));
  }
  
  http.end(); // Free resources
}

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");

  Serial.println("GET RANDOM SONG");
  httpGET("/song");
  // Serial.println("POST DEVICE");
  // postDevice(STUDENT_ID, DEVICE1_NAME, "harrypotter");
  Serial.println("GET DEVICE");
  getDevice(STUDENT_ID, DEVICE1_NAME);
  Serial.println("GET SONG");
  getSong("gameofthrones");
}

void loop() {
}