#include <WiFi.h>
#include <HTTPClient.h>
#include <Arduino_JSON.h>
#define BUZZER_PIN 21

// the following variables are unsigned longs because the time, measured in
// milliseconds, will quickly become a bigger number than can be stored in an int.
unsigned long lastTime = 0;
unsigned long timerDelay = 5000;

const char* ssid = "BELL892";
const char* password = "1E7C373CF727";

String BASE_URL = "https://iotjukebox.onrender.com";
String STUDENT_ID = "40239038";
String DEVICE1_NAME = "Camila_iPhone16";
String DEVICE2_NAME = "Camila_MBP";

void getRandomSong() {
  //Check WiFi connection status
  if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      http.begin(BASE_URL + "/song");

      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
  }
}

void getSong(String song_name) {
  //Check WiFi connection status
  if(WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String httpRequestData = "/song?name=" + song_name;
      http.begin(BASE_URL + httpRequestData);

      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
  }  
}

void getDevice(String student_id, String device) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    String httpRequestData = "/preference?id=" + student_id + "&key=" + device;
    http.begin(BASE_URL + httpRequestData);

      // Send HTTP GET request
      int httpResponseCode = http.GET();
      
      if (httpResponseCode > 0) {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
      }
      else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }
      // Free resources
      http.end();
  }
}

void postDevice(String student_id, String device, String song_name) {
  if(WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Data to send with HTTP POST
    String httpPostData = "/preference?id=" + student_id + "&key=" + device + "&value=" + song_name;

    http.begin(BASE_URL + httpPostData);

    // Send HTTP POST request
    int httpResponseCode = http.POST(""); // empty body

    if (httpResponseCode > 0) {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      Serial.println(payload);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    // Free resources
    http.end();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin (115200);
  WiFi.begin(ssid, password);
  Serial.println("Connecting");

  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Timer set to 5 seconds (timerDelay variable), it will take 5 seconds before publishing the first reading.");


  Serial.println("GET RANDOM SONG");
  getRandomSong();
  Serial.println("POST DEVICE");
  // postDevice(STUDENT_ID, DEVICE1_NAME, "harrypotter");
  Serial.println("GET DEVICE");
  getDevice(STUDENT_ID, DEVICE1_NAME);
  Serial.println("GET SONG");
  getSong("gameofthrones");
}

void loop() {
  
}
