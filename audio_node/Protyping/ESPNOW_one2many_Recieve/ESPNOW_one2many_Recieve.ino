

#include <esp_now.h>
#include <WiFi.h>
#include "DFRobotDFPlayerMini.h"

DFRobotDFPlayerMini player;

//Structure example to receive data
//Must match the sender structure
typedef struct struct_message {
  bool sound1;
  bool sound2;
  bool sound3;
  bool sound4;
  bool sound5;
  bool shouldBreak;
}struct_message;

bool shouldSound1 = false;
bool shouldSound2 = false;
bool shouldSound3 = false;
bool shouldSound4 = false;
bool shouldSound5 = false;
bool shouldBreak = false;

//Create a struct_message called myData
struct_message myData;

//callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("Recieved data");
  memcpy(&myData, incomingData, sizeof(myData));
  shouldSound1 = myData.sound1;
  Serial.println(shouldSound1);
  shouldSound2 = myData.sound2;
  Serial.println(shouldSound2);
  shouldSound3 = myData.sound3;
  Serial.println(shouldSound3);
  shouldSound4 = myData.sound4;
  Serial.println(shouldSound4);
  shouldSound5 = myData.sound5;
  Serial.println(shouldSound5);
}
 
void setup() {
  //Initialize Serial Monitor
  Serial.begin(115200);
  
  //Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  //Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  
  // Once ESPNow is successfully Init, we will register for recv CB to
  // get recv packer info
  esp_now_register_recv_cb(OnDataRecv);


  /////////////////////////////DF PLAYER SETUP
  Serial2.begin(9600);

  // Start communication with DFPlayer Mini
  if (player.begin(Serial2)) {
    Serial.println("OK");

    // Set volume to maximum (0 to 30).
    player.volume(20);
  } else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }
}
 
void loop() {
  if (shouldSound1){
    Serial.println("Playing 1");
    shouldSound1 = false;
    player.play(1);
  }
  if (shouldSound2){
    Serial.println("Playing 2");
    shouldSound2 = false;
    player.play(2);
  }
  if (shouldSound3){
    Serial.println("Playing 3");
    shouldSound3 = false;
    player.play(3);
  }
  if (shouldSound4){
    Serial.println("Playing 4");
   shouldSound4 = false;
    player.play(4);
  }
  if (shouldSound5){
    Serial.println("Playing 5");
    shouldSound5 = false;
    player.play(5);
  }
  delay(100);
}
