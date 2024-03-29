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
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Button.h>

// Improve State Readability
#define STATE_IDLE          0
#define STATE_LOAD_IN       1
#define STATE_READY         2
#define STATE_COUNTDOWN     3
#define STATE_MATCH         4
#define STATE_1_MIN         5
#define STATE_10_SEC        6
#define STATE_END           7
#define STATE_KO_CONFIRM    8
#define STATE_KO            9
#define STATE_E_STOP        10
#define STATE_PAUSE         11

//define peers
#define SOUND_NODE //sound node
#define TIMER_NODE //timer node
#define LIGHT_NODE //light node

//setup OLED screen
#define SCREEN_WIDTH 128 //OLED display width
#define SCREEN_HEIGHT 32 //OLED display height

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

//define the button input pins
#define next 16
#define pause 17
#define ko 5
#define estop 18
#define reset 19

//debounce setup
Button nextButton(next);
Button pauseButton(pause);
Button koButton(ko);
Button estopButton(estop);
Button resetButton(reset);

//initialize state counter, timer, and match length
volatile int state = STATE_IDLE;
int matchLength = 180; //in seconds
bool publishStateFlag = false; // indicates a request to broadcast the system state

// Timing
long currentTime, prevTime, stateTime;
long countdown;

//receiver MAC address
uint8_t broadcastAddress1[] = {0xA8, 0x42, 0xE3, 0x47, 0x98, 0x30}; //sound node
#ifdef TIMER_NODE
uint8_t broadcastAddress2[] = {0x48, 0x27, 0xE2, 0x3D, 0x0E, 0x28}; //timer node
#endif
#ifdef LIGHT_NODE
uint8_t broadcastAddress3[] = {0xA8, 0x42, 0xE3, 0x47, 0x95, 0x30}; //light node
#endif

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int systemState; //broadcast the system state
  long systemTime; //timer for the system
} struct_message;

// Create struct_messages for incoming and outgoing data
struct_message outGoing;
struct_message incomingReadings;

// callback when data is sent
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status == 0){
    Serial.println("GOOD");
    Serial.println(outGoing.systemState);
    Serial.println(outGoing.systemTime);
  }
  else{
    Serial.println("FAIL");
  }
}

void OnDataRecv(const uint8_t * mac, const uint8_t *incomingData, int len) {
  memcpy(&incomingReadings, incomingData, sizeof(incomingReadings));
}

// Set the publish flag on every state change
void UpdateState(uint8_t newState){
    Serial.println("Updating to state " + String(newState));
    state = newState;
    publishStateFlag = true;
    stateTime = currentTime; // Record State Change Time

    // Restart the countdown
    if (state == STATE_IDLE) {
      countdown = matchLength * 1000;
    }
}

//peer info
esp_now_peer_info_t peerInfo;
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);

  //---------------Button Setup----------------
  nextButton.begin();
  pauseButton.begin();
  koButton.begin();
  estopButton.begin();
  resetButton.begin();

  //---------------ESP-NOW Setup----------------

  // Set device as a Wi-Fi Station
  WiFi.mode(WIFI_STA);

  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  //register the send callback
  esp_now_register_send_cb(OnDataSent);
  
  // Register peer
  peerInfo.channel = 0;  
  peerInfo.encrypt = false;
  
  // Add first peer
  memcpy(peerInfo.peer_addr, broadcastAddress1, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }

  // Add second peer
  #ifdef TIMER_NODE
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  #endif

  // Add third peer
  #ifdef LIGHT_NODE
  memcpy(peerInfo.peer_addr, broadcastAddress3, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  #endif


  //---------------- OLED Setup -----------------

  // SSD1306_SWITCHCAPVCC = generate display voltage from 3.3V internally
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); // Don't proceed, loop forever
  }
  // Show initial display buffer contents on the screen --
  // the library initializes this with an Adafruit splash screen.
  delay(2000);
  display.clearDisplay();

  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0, 10);
  // Display static text
  display.println("Loading...");
  display.display();

  currentTime = millis();
  prevTime = currentTime;
  stateTime = currentTime;

  // Init Match time
  countdown = matchLength * 1000;
}

void nextFunction() { //next button
  switch (state) { //state changes from "next" button

    case STATE_IDLE: //change from idle to load-in
      UpdateState(STATE_LOAD_IN);
      break;
    
    case STATE_LOAD_IN: //change from load-in to ready for battle
      UpdateState(STATE_READY);
      break;

    case STATE_READY: //change from ready for battle to start countdown
      UpdateState(STATE_COUNTDOWN);
      break;

    case STATE_KO_CONFIRM: //change from ko confirm to match timer
      UpdateState(STATE_MATCH);
      break;
  }
}

void pauseFunction() { //pause button
  if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC) { //change from match timer to pause
    UpdateState(STATE_PAUSE);
  } else if (state == STATE_PAUSE) { //change from pause to match timer
    UpdateState(STATE_COUNTDOWN);
  }
  return;
}

void koFunction() { //ko button
  Serial.println("KO Pressed, in State " + state); 
  if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC) { //change from match timer to ko confirm
    UpdateState(STATE_KO_CONFIRM);
  } else if (state == STATE_KO_CONFIRM) { //change from ko confirm to knock out
    Serial.println("ko confirm trigger");
    UpdateState(STATE_KO);
  }
  return;
}

void estopFunction() { //estop button
  if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC || state == STATE_10_SEC) { //change from match timer to estop
    UpdateState(STATE_E_STOP);
  } else if (state == STATE_E_STOP) { //change from estop to match timer
    UpdateState(STATE_COUNTDOWN);
  }
  return;
}

void resetFunction() { //reset button
  if (state == STATE_END) { //change from match end to idle
    UpdateState(STATE_IDLE);
  } else if (state == STATE_KO) { //change from knock out to idle
    UpdateState(STATE_IDLE);
  } else if (state == STATE_E_STOP) { //change from knock out to idle
    UpdateState(STATE_IDLE);
  }
}

void displayCountdown() {
  int minute = countdown / 60000;
  int second = (countdown / 1000) % 60;

  display.clearDisplay();
  display.setCursor(10, 10);
  display.println(minute);
  display.setCursor(20, 10);
  display.println(":");
  if (second < 10) {
    display.setCursor(30, 10);
    display.println(0);
    display.setCursor(42, 10);
    display.println(second);
  } else {
    display.setCursor(30, 10);
    display.println(second);
  }
  display.display();
}
 
void loop() {

  // Button Routines (use released() for faster response)
  if (nextButton.released()) nextFunction();
  if (pauseButton.released()) pauseFunction();
  if (koButton.released()) koFunction();
  if (estopButton.released()) estopFunction();
  if (resetButton.released()) resetFunction();

  // Update current_time
  currentTime = millis();
  long timeSinceStateChange = currentTime - stateTime;
  
  //match state machine
  switch(state) {

    case STATE_IDLE: //idle state: steady white leds
      //state actions (display "Idle" text)
      display.clearDisplay();
      display.setCursor(10, 10);
      display.println("Idle");
      display.display();
      break;

    case STATE_LOAD_IN: //load-in state: slow yellow pulse, yellow load-in text on timer
      //state actions (display "Load-In" text)
      display.clearDisplay();
      display.setCursor(10, 10);
      display.println("Load-In");
      display.display();
      break;

    case STATE_READY: //ready for battle state: steady green light, ready for battle text on timer
      //state actions (display "Ready for Battle" text)
      display.clearDisplay();
      display.setCursor(10, 0);
      display.println("Ready for Battle");
      display.display();
      break;

    case STATE_COUNTDOWN: //countdown state: 1hz white pulse, 3-2-1 countdown text on timer
      //state actions (3 second countdown on display)
      
      if (timeSinceStateChange < 1000) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("3...");
        display.display();
      } else if (timeSinceStateChange < 2000) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("2...");
        display.display();
      } else if (timeSinceStateChange < 3000) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("1...");
        display.display();
      } else { //three seconds
        //GO!!!
        UpdateState(STATE_MATCH);
      }
      break;

    case STATE_MATCH: //match state: steady white for t>1min, steady yellow for 1min>t>10sec, 1hz red pulse for t<10sec
      //timer counting from 3 minutes
      //state actions (maintain a 3 minute timer on the controller)
      countdown -= currentTime - prevTime;
      displayCountdown();
      
      if (countdown <= 60000) {
        UpdateState(STATE_1_MIN);
      }
      
      break;

    case STATE_1_MIN: //one minute left state
      //state actions (maintain a 3 minute timer on the controller)
      countdown -= currentTime - prevTime;
      displayCountdown();
      
      if (countdown <= 10000) {
        UpdateState(STATE_10_SEC);
      }
      break;
    
    case STATE_10_SEC: //ten seconds left state
      //state actions (maintain a 3 minute timer on the controller)
      countdown -= currentTime - prevTime;
      displayCountdown();
      
      if (countdown <= 0) {
        UpdateState(STATE_END);
      }
      break;

    case STATE_END: //match end state: steady red light, match end text on timer
      //show solid red
      display.clearDisplay();
      display.setCursor(0, 10);
      display.println("Match End");
      display.display();
      break;

    case STATE_KO_CONFIRM: //ko confirm state
      //ko question displayed
      countdown -= currentTime - prevTime;
      
      if (timeSinceStateChange % 1000 < 500) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("KO?");
        display.display();
      } else {
        display.clearDisplay();
        display.display();
      }

      if (countdown <= 0) {
        UpdateState(STATE_END);
      }
      
      break;
    
    case STATE_KO: //ko+tapout state: 3 red pulses at 2hz, ko on timer
      //state actions (blinking "Knock Out" three times)
      
      if (timeSinceStateChange < 3000)
      {
        if (timeSinceStateChange % 500 < 250) {
          display.clearDisplay();
          display.setCursor(10, 10);
          display.println("Knock Out");
          display.display();
        } else {
          display.clearDisplay();
          display.display();
        }
      }
      else
      {
        //wait until reset button is pressed
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("Knock Out");
        display.display();
      }
      // else do nothing
      break;

    case STATE_E_STOP: //estop state
      if (timeSinceStateChange % 1000 < 500) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("E-STOP");
        display.display();
      } else {
        display.clearDisplay();
        display.display();
      }
      break;
    
    case STATE_PAUSE: //pause state
      if (timeSinceStateChange % 1000 < 500) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("Paused");
        display.display();
      } else {
        displayCountdown();
      }
      break;
  }

  //broadcast sound command if not yet sent after state change
  if (publishStateFlag) {

    //readout new state
    String str = "Change to State ";
    switch (state)
    {
        case STATE_IDLE:
            str += "IDLE";
            break;
        case STATE_LOAD_IN:
            str += "LOAD_IN";
            break;
        case STATE_READY:
            str += "READY";
            break;
        case STATE_COUNTDOWN:
            str += "COUNTDOWN";
            break;
        case STATE_MATCH:
            str += "MATCH";
            break;
        case STATE_1_MIN:
            str += "1_MIN";
            break;
        case STATE_10_SEC:
            str += "10_SEC";
            break;
        case STATE_END:
            str += "END";
            break;
        case STATE_KO_CONFIRM:
            str += "KO_CONFIRM";
            break;
        case STATE_KO:
            str += "KO";
            break;
        case STATE_E_STOP:
            str += "E_STOP";
            break;
        case STATE_PAUSE:
            str += "PAUSE";
            break;
        default:
            str += "INVALID";
            break;
    }
    Serial.println(str);

    //set system state for broadcast
    broadcastSystemState();
  
    // Send message via ESP-NOW
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
    
    #ifdef TIMER_NODE
    esp_err_t result2 = esp_now_send(
      broadcastAddress2, 
      (uint8_t *) &outGoing,
      sizeof(outGoing));

    if (result2 == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    #endif

    #ifdef LIGHT_NODE
    esp_err_t result3 = esp_now_send(
      broadcastAddress3, 
      (uint8_t *) &outGoing,
      sizeof(outGoing));
    
    if (result3 == ESP_OK) {
      Serial.println("Sent with success");
    }
    else {
      Serial.println("Error sending the data");
    }
    #endif

    publishStateFlag = false; //set publishStateFlag to false after broadcast
  }

  // Record prev_time
  prevTime = currentTime;
}

void broadcastSystemState(){ //check the state machine to send the state signal

  outGoing.systemState = state; //set output state to current state
  outGoing.systemTime = countdown; //arbitrary test value

  return;
}
