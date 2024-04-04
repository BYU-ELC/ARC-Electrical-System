/*********
  Rui Santos
  Complete project details at https://RandomNerdTutorials.com/esp-now-one-to-many-esp32-esp8266/
  
  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files.
  
  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.
*********/

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include <FastLED.h>

#define NUM_LEDS 150
#define LEDS_CONTROL_PIN 5

CRGB leds[NUM_LEDS];

bool newData = false;

//Structure example to receive data
//Must match the sender structure
typedef struct struct_message {
  int systemState;
  int systemTime;
} struct_message;

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

void setColorAll(CRGB color) {
	for (int i = 0; i <= NUM_LEDS; i++) {
		leds[i] = color;
	}
	FastLED.show();
}

void fadeInAll(int brightness) {
	for (int i = 0; i <= brightness; i++) {
		FastLED.setBrightness(i);
    FastLED.show();
    if(newData){
      i = brightness + 1; //exits function immediately on system state transition
    }
	}
}

void fadeOutAll(int brightness) {
  for (int i = brightness; i >= 0; i--) {
		FastLED.setBrightness(i);
    FastLED.show();
    if(newData){
      i = -1; //exits function immediately on system state transition
    }
	}
}

//brightness from 0-255 and pulseLength in seconds
void pulseAll(int brightness) {
  fadeInAll(brightness);
  fadeOutAll(brightness);
}

void blinkAll(int brightness, int period) {
  FastLED.setBrightness(brightness);
  FastLED.show();
  delay(250);
  FastLED.setBrightness(0);
  FastLED.show();
  delay(period);
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

	// LED SETUP

	FastLED.addLeds<WS2812,LEDS_CONTROL_PIN, GRB>(leds, NUM_LEDS);
}

void loop() {
  //sound node state machine
  switch(state) {

  case 0: //idle state
    
    newData = false; //lowers the state change flag to avoid interference with fade

    //state actions (white pulse)
    setColorAll(CRGB(255, 255, 255));
    pulseAll(255);

    break;

  case 1: //load-in state

    newData = false; //lowers the state change flag to avoid interference with fade

    //state actions (yellow pulse)
    setColorAll(CRGB(255, 255, 0));
    pulseAll(255);
    
    break;

  case 2: //ready for battle state

    //state actions (all green)
    setColorAll(CRGB(0, 255, 0));
    if(newData){
      newData = false;
      fadeInAll(255);
    }

    break;

  case 3: //countdown state: 1hz white pulse, 3-2-1 countdown text on timer

    //state actions (all blinking yellow)
    setColorAll(CRGB(255, 255, 0));
    if(newData){
      for(int i = 0; i < 3; i++){
        blinkAll(255, 625);
      }
    }

    // Automatically progress to match
    // Catches bug where the remote broadcast is a little slow
    state = 4;
    
    break;

  case 4: //match state

    //state actions (all max white)
    setColorAll(CRGB(255, 255, 255));
    FastLED.setBrightness(255);
    FastLED.show();

    break;

  case 5: // one minute left

    //state actions (all steady yellow)
    if(newData){
      newData = false;
      setColorAll(CRGB(255, 255, 0));
      FastLED.setBrightness(255);
      FastLED.show();
    }

  case 6: // 10 seconds left

    //state actions (all steady orange)
    if(newData){
      newData = false;
      setColorAll(CRGB(255, 100, 0));
      FastLED.setBrightness(255);
      FastLED.show();
    }

  case 7: //match end state
    
    //state actions (all steady red)
    if(newData){
      newData = false;
      setColorAll(CRGB(255, 0, 0));
      FastLED.setBrightness(255);
      FastLED.show();
    }
    
    break;

  case 8: //ko confirm state

    //state actions (no visible action)
    //setColorAll(CRGB(100, 0, 0));
    
    break;
  
  case 9: //ko+tapout state
  
    //state actions (all blinking red then steady red)
    setColorAll(CRGB(255, 0, 0));
    if(newData){
      delay(125);
      for(int i = 0; i<2; i++){
        blinkAll(255, 125);
      }
      newData = false;
    }
    FastLED.setBrightness(255);
    FastLED.show();
    
    break;

  case 10: //estop state

    newData = false; //lowers the state change flag to avoid interference with fade
    
    //state actions (pulse red)
    setColorAll(CRGB(255, 0, 0));
    pulseAll(255);

    break;
  
  case 11: //pause state
    
    //state actions (steady blue)
    setColorAll(CRGB(0, 0, 255));

    break;
  }
	delay(100);
}
