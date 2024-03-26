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
#define PEER_ONE //sound node
#define PEER_TWO //timer node
#define PEER_THREE //light node

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
unsigned long lastDebounceTime = 0; //last time the button was toggled
unsigned long debounceDelay = 50; //debounce time
int lastButtonState = LOW; //previous input reading
int buttonState; //current input reading

//initialize state counter, timer, and match length
volatile int state = STATE_IDLE;
int timer = 0;
int minute = 0; //minute counter for timer display
int second = 0; //second counter for timer display
int countdown = 0; //countdown timer
int matchLength = 180; //in seconds
int timeCount; //counter for one second
int paused = 0; //pause counter
bool soundPlayed = false; //tracks whether state sound has been played

//receiver MAC address
uint8_t broadcastAddress1[] = {0xA8, 0x42, 0xE3, 0x47, 0x98, 0x30}; //sound node
#ifdef PEER_TWO
uint8_t broadcastAddress2[] = {0x48, 0x27, 0xE2, 0x3D, 0x0E, 0x28}; //timer node
#endif
#ifdef PEER_THREE
uint8_t broadcastAddress3[] = {0xA8, 0x42, 0xE3, 0x47, 0x95, 0x30}; //light node
#endif

// Structure example to send data
// Must match the receiver structure
typedef struct struct_message {
  int systemState; //broadcast the system state
  int systemTime; //timer for the system
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

//peer info
esp_now_peer_info_t peerInfo;
 
void setup() {
  // Init Serial Monitor
  Serial.begin(115200);


  //---------------Button Setup----------------

  //set pinModes for inputs
  pinMode(next, INPUT);
  pinMode(pause, INPUT);
  pinMode(ko, INPUT);
  pinMode(estop, INPUT);
  pinMode(reset, INPUT);
 
  //attach ISRs
  attachInterrupt(digitalPinToInterrupt(next), nextISR, RISING);
  attachInterrupt(digitalPinToInterrupt(pause), pauseISR, RISING);
  attachInterrupt(digitalPinToInterrupt(ko), koISR, RISING);
  attachInterrupt(digitalPinToInterrupt(estop), estopISR, RISING);
  attachInterrupt(digitalPinToInterrupt(reset), resetISR, RISING);


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
  #ifdef PEER_TWO
  memcpy(peerInfo.peer_addr, broadcastAddress2, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK){
    Serial.println("Failed to add peer");
    return;
  }
  #endif

  // Add third peer
  #ifdef PEER_THREE
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
}

void nextISR() { //next button ISR
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
  {
    switch (state) { //state changes from "next" button

      case STATE_IDLE: //change from idle to load-in
        state = STATE_LOAD_IN;
        soundPlayed = false; //indicates a state change
        break;
      
      case STATE_LOAD_IN: //change from load-in to ready for battle
        state = STATE_READY;
        soundPlayed = false; //indicates a state change
        break;

      case STATE_READY: //change from ready for battle to start countdown
        state = STATE_COUNTDOWN;
        soundPlayed = false; //indicates a state change
        break;

      case STATE_KO_CONFIRM: //change from ko confirm to match timer
        state = STATE_MATCH;
        soundPlayed = false; //indicates a state change
        break;
    }
  }
  last_interrupt_time = interrupt_time;
  return;
}

void pauseISR() { //pause button ISR
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
  {
    if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC) { //change from match timer to pause
      state = STATE_PAUSE;
    } else if (state == STATE_LOAD_IN1) { //change from pause to match timer
      state = STATE_COUNTDOWN;
    }
  }
  last_interrupt_time = interrupt_time;
  soundPlayed = false; //indicates a state change
  return;
}

void koISR() { //ko button ISR
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
    {
    if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC) { //change from match timer to ko confirm
      state = STATE_KO_CONFIRM;
    } else if (state == STATE_KO_CONFIRM) { //change from ko confirm to knock out
      state = STATE_KO;
    }
  }
  last_interrupt_time = interrupt_time;
  soundPlayed = false; //indicates a state change
  return;
}

void estopISR() { //estop button ISR
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
    {
    if (state == STATE_MATCH || state == STATE_1_MIN || state == STATE_10_SEC) { //change from match timer to estop
      state = STATE_E_STOP;
    } else if (state == STATE_LOAD_IN0) { //change from estop to match timer
      state = STATE_COUNTDOWN;
    }
  }
  last_interrupt_time = interrupt_time;
  soundPlayed = false; //indicates a state change
  return;
}

void resetISR() { //reset button ISR
  static unsigned long last_interrupt_time = 0;
  unsigned long interrupt_time = millis();
  // If interrupts come faster than 200ms, assume it's a bounce and ignore
  if (interrupt_time - last_interrupt_time > 200) 
  {
    if (state == STATE_END) { //change from match end to idle
      state = STATE_IDLE;
    } else if (state == STATE_KO) { //change from knock out to idle
      state = STATE_IDLE;
    }
  }
  last_interrupt_time = interrupt_time;
  soundPlayed = false; //indicates a state change
  return;
}
 
void loop() {
  //match state machine
  switch(state) {

    case STATE_IDLE: //idle state: steady white leds
      //reset timer
      timer = matchLength;

      //state actions (display "Idle" text)
      display.clearDisplay();
      display.setCursor(10, 10);
      display.println("Idle");
      display.display();
      break;

    case STATE_LOAD_IN;: //load-in state: slow yellow pulse, yellow load-in text on timer
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
      if (countdown < 70) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("3...");
        display.display();
      } else if (70 < countdown && countdown < 140) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("2...");
        display.display();
      } else if (140 < countdown) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("1...");
        display.display();
      }
      countdown++;
      
      //automatically progress state machine
      if (countdown == 209) { //three seconds
        //GO!!!
        countdown = 0;
        state = STATE_MATCH;
        soundPlayed = false; //indicates a state change
      }
      break;

    case STATE_MATCH: //match state: steady white for t>1min, steady yellow for 1min>t>10sec, 1hz red pulse for t<10sec
            //timer counting from 3 minutes
      //state actions (maintain a 3 minute timer on the controller)
      minute = timer / 60;
      second = timer - ( minute * 60 );

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

      timeCount += 1;

      if (timeCount == 77) { //77 is just under 1 second slow for 3 minutes
        timeCount = 0;
        timer -= 1;
      }
      
      if (timer <= 60) {
        state = STATE_1_MIN; //automatically progress state machine
        soundPlayed = false; //indicates a state change
      }
      break;

    case STATE_1_MIN: //one minute left state
      //state actions (maintain a 3 minute timer on the controller)
      minute = timer / 60;
      second = timer - ( minute * 60 );

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

      timeCount += 1;

      if (timeCount == 79) { //79 is just under 1 second fast for 3 minutes
        timeCount = 0;
        timer -= 1;
      }
      
      if (timer <= 10) {
        state = STATE_10_SEC; //automatically progress state machine
        soundPlayed = false; //indicates a state change
      }
      break;
    
    case STATE_10_SEC: //ten seconds left state
      //state actions (maintain a 3 minute timer on the controller)
      minute = timer / 60;
      second = timer - ( minute * 60 );

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

      timeCount += 1;

      if (timeCount == 79) { //79 is just under 1 second fast for 3 minutes
        timeCount = 0;
        timer -= 1;
      }
      
      if (timer == 0) {
        state = STATE_END; //automatically progress state machine
        soundPlayed = false; //indicates a state change
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
      if (paused < 40) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("KO?");
        display.display();
        paused++;
      } else if (paused < 80) {
        display.clearDisplay();
        display.display();
        paused++;
      } else {
        paused = 0;
      }
      
      //continue timer while awaiting confirmation
      timeCount += 1;

      if (timeCount == 79) { //79 is just under 1 second fast for 3 minutes
        timeCount = 0;
        timer -= 1;
      }

      if (timer == 0) {
        state = STATE_END; //automatically progress state machine
        soundPlayed = false; //indicates a state change
      }
      
      break;
    
    case STATE_KO: //ko+tapout state: 3 red pulses at 2hz, ko on timer
      //state actions (blinking "Knock Out" three times)
      for (int i = 0; i < 59; i++) {
        if (paused < 10) {
          display.clearDisplay();
          display.setCursor(10, 10);
          display.println("Knock Out");
          display.display();
          paused++;
        } else if (paused < 20) {
          display.clearDisplay();
          display.display();
          paused++;
        } else {
          paused = 0;
        }
        
      }
      //wait until reset button is pressed
      while(state == STATE_KO) {
        //display "Knock Out"
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("Knock Out");
        display.display();
      }
      break;

    case STATE_E_STOP: //estop state
      if (paused < 40) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("E-STOP");
        display.display();
        paused++;
      } else if (paused < 80) {
        display.clearDisplay();
        display.display();
        paused++;
      } else {
        paused = 0;
      }
      break;
    
    case STATE_PAUSE: //pause state
      if (paused < 40) {
        display.clearDisplay();
        display.setCursor(10, 10);
        display.println("Paused");
        display.display();
        paused++;
      } else if (paused < 80) {
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
        paused++;
      } else {
        paused = 0;
      }
      break;
  }

  //broadcast sound command if not yet sent after state change
  if (soundPlayed == false) {

    //readout new state
    Serial.println(state);

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
    
    #ifdef PEER_TWO
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

    #ifdef PEER_THREE
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

    soundPlayed = true; //set soundPlayed to true after broadcast
  }
}

void broadcastSystemState(){ //check the state machine to send the state signal

  outGoing.systemState = state; //set output state to current state
  outGoing.systemTime = 7; //arbitrary test value

  return;
}