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

#define PEER_ONE
//#define PEER_TWO
//#define PEER_THREE


#define SOUND_1_PIN 32
#define SOUND_2_PIN 33
#define SOUND_3_PIN 25
#define SOUND_4_PIN 26
#define SOUND_5_PIN 27
#define SHOULD_BREAK_PIN 14

// REPLACE WITH YOUR ESP RECEIVER'S MAC ADDRESS
uint8_t broadcastAddress1[] = {0xa8, 0x42, 0xe3, 0x47, 0x98, 0x30};
#ifdef PEER_TWO
uint8_t broadcastAddress2[] = {0xFF, , , , , };
#endif
#ifdef PEER_THREE
uint8_t broadcastAddress3[] = {0xFF, , , , , };
#endif

typedef struct struct_message {
  bool sound1;
  bool sound2;
  bool sound3;
  bool sound4;
  bool sound5;
  bool shouldBreak;
}struct_message;

struct_message outGoing;
struct_message incomingReadings;


void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status ==0){
    Serial.println("GOOD");
    Serial.println(outGoing.sound1);
    Serial.println(outGoing.sound2);
    Serial.println(outGoing.sound3);
    Serial.println(outGoing.sound4);
    Serial.println(outGoing.sound5);
  }
  else{
    Serial.println("FAIL");
  }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
}

esp_now_peer_info_t peerInfo;

void setup() {
 
  Serial.begin(115200);
 
  WiFi.mode(WIFI_STA);

  pinMode(SOUND_1_PIN, INPUT_PULLUP);
  pinMode(SOUND_2_PIN, INPUT_PULLUP);
  pinMode(SOUND_3_PIN, INPUT_PULLUP);
  pinMode(SOUND_4_PIN, INPUT_PULLUP);
  pinMode(SOUND_5_PIN, INPUT_PULLUP);
  pinMode(SHOULD_BREAK_PIN, INPUT_PULLUP);
 
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  // register peer
  
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;


  outGoing.sound1 = false;
  outGoing.sound2 = false;
  outGoing.sound3 = false;
  outGoing.sound4 = false;
  outGoing.sound5 = false;
  outGoing.shouldBreak = false;
  
    
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  #ifdef PEER_TWO
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  #endif

  #ifdef PEER_THREE
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  #endif
}
 
void loop() {
  
  
  readButtons();
 
  esp_err_t result1 = esp_now_send(
    broadcastAddress1, 
    (uint8_t *) &outGoing,
    sizeof(outGoing));
   
  if (result1 == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  
#ifdef PEER_TWO
  esp_err_t result2 = esp_now_send(
    broadcastAddress2, 
    (uint8_t *) &test2,
    sizeof(outGoing));

  if (result2 == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
  
  delay(500);
#endif

#ifdef PEER_TWO
  esp_err_t result3 = esp_now_send(
    broadcastAddress3, 
    (uint8_t *) &test3,
    sizeof(outGoing));
   
  if (result3 == ESP_OK) {
    Serial.println("Sent with success");
  }
  else {
    Serial.println("Error sending the data");
  }
#endif
  delay(100);
}




void readButtons(){
  if(!digitalRead(SOUND_1_PIN)){
    delay(5);
    if(!digitalRead(SOUND_1_PIN)){
      Serial.println("Setting sound 1 to true");
      outGoing.sound1 = true;
      return;
    }else{outGoing.sound1 = false;}
  }else{outGoing.sound1 = false;}

  /////////SOUND 2
  if(!digitalRead(SOUND_2_PIN)){
    delay(5);
    if(!digitalRead(SOUND_2_PIN)){
      outGoing.sound2 = true;
      return;
    }else{outGoing.sound2 = false;}
  }else{outGoing.sound2 = false;}


  ///////SOUND 3
  if(!digitalRead(SOUND_3_PIN)){
    delay(5);
    if(!digitalRead(SOUND_3_PIN)){
      outGoing.sound3 = true;
      return;
    }else{outGoing.sound3 = false;}
  }else{outGoing.sound3 = false;}


  //////SOUND 4
  if(!digitalRead(SOUND_4_PIN)){
    delay(5);
    if(!digitalRead(SOUND_4_PIN)){
      outGoing.sound4 = true;
      return;
    }else{outGoing.sound4 = false;}
  }else{outGoing.sound4 = false;}

  ////////SOUND 5
  if(!digitalRead(SOUND_5_PIN)){
    delay(5);
    if(!digitalRead(SOUND_5_PIN)){
      outGoing.sound5 = true;
      return;
    }else{outGoing.sound5 = false;}
  }else{outGoing.sound5 = false;}


  ///////SHOULD_BREAK
  if(!digitalRead(SHOULD_BREAK_PIN)){
    delay(5);
    if(!digitalRead(SHOULD_BREAK_PIN)){
      outGoing.shouldBreak = true;
      return;
    }else{outGoing.shouldBreak = false;}
  }else{outGoing.shouldBreak = false;}
  
  return;
}
