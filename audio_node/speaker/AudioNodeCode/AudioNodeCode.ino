/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-one-to-many-esp32-esp8266/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include <esp_now.h>
#include <WiFi.h>
#include "DFRobotDFPlayerMini.h"

DFRobotDFPlayerMini player;

bool newData = false;

//Structure example to receive data
//Must match the sender structure
typedef struct struct_message {
  int systemState;
  int systemTime;
}struct_message;

volatile int state = 0;

//Create a struct_message called myData
struct_message myData;

//callback function that will be executed when data is received
void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  Serial.println("Received data");
  memcpy(&myData, incomingData, sizeof(myData));
  state = myData.systemState;
  Serial.println(state);
  newData = true;
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
    player.volume(30);
  } else {
    Serial.println("Connecting to DFPlayer Mini failed!");
  }
}
 
void loop() {

  if (newData == true) {
    //sound node state machine
    switch(state) {

      case 0: //idle state
        
        //state actions (system start sound)
        player.play(1);

        break;

      case 1: //load-in state

        //state actions (announcer)
        player.play(13);
        
        break;

      case 2: //ready for battle state

        //state actions (Announcer)
        player.play(14);
        
        break;

      case 3: //countdown state: 1hz white pulse, 3-2-1 countdown text on timer

        //state actions (3 second countdown on display)
        player.play(4);
        
        break;

      case 4: //match state

        //state actions (no sound)
        //player.play(5);

        break;

      case 5: //one minute remaining state

        //state actions (Announcer)
        player.play(15);

        break;

      case 6: //ten seconds remaining state

        //state actions (Announcer)
        player.play(16);

        break;

      case 7: //match end state
        
        //state action (buzzer)
        player.play(7);
        delay(2000);
        player.play(19);
        
        break;

      case 8: //ko confirm state

        //state actions (no sound)
        //player.play(9);
        
        break;
      
      case 9: //ko+tapout state
      
        //state actions (KO Announcement)
        player.play(10);
        delay(2000);
        player.play(17);
          
        break;

      case 10: //estop state
        
        //state actions (Announcer)
        //player.play(11);
        player.play(18);

        break;
      
      case 11: //pause state
        
        //state actions (pause siren)
        player.play(12);

        break;
    }
    newData = false;
  }
  delay(100);
}