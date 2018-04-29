#include <SPI.h>
#include "config.h"

#if defined(ARDUINO_AVR_ETHERNET)
#include <Ethernet.h>
#if READERS_COUNT > 1
#error Max 1 reader supported
#endif

#elif defined(ARDUINO_AVR_LEONARDO_ETH)
#include <Ethernet2.h>
#if READERS_COUNT > 3
#error Max 3 readers supported
#endif
#endif

#include <PubSubClient.h>
#include <Wiegand.h>
#include <Timer.h>

/*
 * Ethernet
*/

byte mac[] = {  BOARD_MAC_ADDRESS };
IPAddress server(MQTT_SERVER_IP);
EthernetClient ethClient;
PubSubClient client(ethClient);

/*
* Awesome Asynchronous Shit
*/
Timer t;

/*
 * WIEGAND readers
 * The object that handles the wiegand protocol
*/

Wiegand wiegand1;
#if READERS_COUNT > 1
Wiegand wiegand2;
#endif
# if READERS_COUNT > 2
Wiegand wiegand3;
# endif


void setupWiegand(){
  //Install listeners and initialize Wiegand reader
  pinMode(READER1_PIN_D0, INPUT);
  pinMode(READER1_PIN_D1, INPUT);
  wiegand1.onReceive(receivedData, WIEGAND1_TOPIC);
  wiegand1.onStateChange(stateChanged, "State changed: ");
  attachInterrupt(digitalPinToInterrupt(READER1_PIN_D0), wiegand1PinChangeInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(READER1_PIN_D1), wiegand1PinChangeInterrupt, CHANGE);
  wiegand1.begin(WIEGAND_LENGTH);
  wiegand1PinChangeInterrupt();

  #if READERS_COUNT > 1
  pinMode(READER2_PIN_D0, INPUT);
  pinMode(READER2_PIN_D1, INPUT);
  wiegand2.onReceive(receivedData, WIEGAND2_TOPIC);
  wiegand2.onStateChange(stateChanged, "State changed: ");
  attachInterrupt(digitalPinToInterrupt(READER2_PIN_D0), wiegand2PinChangeInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(READER2_PIN_D1), wiegand2PinChangeInterrupt, CHANGE);
  wiegand2.begin(WIEGAND_LENGTH);
  wiegand2PinChangeInterrupt();
  #endif

  # if READERS_COUNT > 2
  pinMode(READER3_PIN_D0, INPUT);
  pinMode(READER3_PIN_D1, INPUT);
  pinMode(READER3_PIN_INTERRUPT, INPUT);
  wiegand3.onReceive(receivedData, WIEGAND3_TOPIC);
  wiegand3.onStateChange(stateChanged, "State changed: ");
  attachInterrupt(digitalPinToInterrupt(READER3_PIN_INTERRUPT), wiegand3PinChangeInterrupt, CHANGE);
  wiegand3.begin(WIEGAND_LENGTH);
  wiegand3PinChangeInterrupt();
  #endif
}

// Notifies when a reader has been connected or disconnected.
// Instead of a message, the seconds parameter can be anything you want -- Whatever you specify on `wiegand.onStateChange()`
void stateChanged(bool plugged, const char* message) {
    Serial.print(message);
    Serial.println(plugged ? "CONNECTED" : "DISCONNECTED");
}

void wiegand1PinChangeInterrupt(){
    wiegand1.setPin0State(digitalRead(READER1_PIN_D0));
    wiegand1.setPin1State(digitalRead(READER1_PIN_D1));
}

#if READERS_COUNT > 1
void wiegand2PinChangeInterrupt(){
    wiegand2.setPin0State(digitalRead(READER2_PIN_D0));
    wiegand2.setPin1State(digitalRead(READER2_PIN_D1));
}
#endif

#if READERS_COUNT > 2
void wiegand3PinChangeInterrupt(){
    wiegand3.setPin0State(digitalRead(READER3_PIN_D0));
    wiegand3.setPin1State(digitalRead(READER3_PIN_D1));
}
#endif

// card read
void receivedData(uint8_t* data, uint8_t bits, const char* message) {
    Serial.print(message);
    Serial.print(" ");

    char card[16];
    //Print value in HEX
    uint8_t bytes = (bits+7)/8;
    for (int i=0; i<bytes; i++) {
        String(data[i] >> 4, HEX).toCharArray(&card[i*2], 2);
        String(data[i] & 0xF, HEX).toCharArray(&card[i*2+1], 2);
    }
    Serial.print(card);
    Serial.println();
    char buffer[50];
    sprintf(buffer, "enterprised/reader/%s/cardread", message);
    Serial.println("publishing message");
    client.publish(buffer, card);
    Serial.println("message published");
}

void printIPAddress() {
  Serial.print("Enterprise Arduino What The Fuck's IP address: ");
  for (byte thisByte = 0; thisByte < 4; thisByte++) {
    // print the value of each byte of the IP address:
    Serial.print(Ethernet.localIP()[thisByte], DEC);
    Serial.print(".");
  }
  Serial.println();
}

void sleep_and_blink(int noop){
  for(int i = 0; i < noop; i++){
    digitalWrite(BLINKENLIGHTS_PIN, LOW);
    delay(50);
    digitalWrite(BLINKENLIGHTS_PIN, HIGH);
    delay(50);
  }
}

void setup() {
  pinMode(BLINKENLIGHTS_PIN, OUTPUT);
  Serial.begin(57600);

  sleep_and_blink(50);
  Serial.println("Enterprise Arduino What The Fuck is Initializing...");

  Serial.println("Configuring MQTT client");
  client.setServer(server, 1883);
  sleep_and_blink(50);

  Serial.println("Configuring Ethernet coprocessor");
  Ethernet.begin(mac);
  sleep_and_blink(30);

  Serial.println("Reading IP address");
  printIPAddress();

   Serial.println("Setting up Wiegand readers");
  setupWiegand();

  Serial.println("Preparing outputs");
  setupOutputs();

  Serial.println("Configuring MQTT callback");
  client.setCallback(callback);

  Serial.println("Connecting to MQTT server");
  reconnect();

  Serial.println("Enterprise Arduino What The Fuck is Initialized.");
}

volatile int counter = 0;
volatile int state = 0;

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  // Every few milliseconds, check for pending messages on the wiegand reader
  // This executes with interruptions disabled, since the Wiegand library is not thread-safe
  noInterrupts();
  wiegand1.flush();
  #if READERS_COUNT > 1
  wiegand2.flush();
  #endif
  #if READERS_COUNT > 2
  wiegand3.flush();
  #endif
  interrupts();
  // Pool timer actions
  t.update();

  client.loop();

  if(!(counter++ % 10)){
    if(state == 0){
      digitalWrite(BLINKENLIGHTS_PIN, HIGH);
      state = 1;
    }else{
      digitalWrite(BLINKENLIGHTS_PIN, LOW);
      state = 0;
    }
  }
  delay(100);
}

/*
 Outputs
*/

void setupOutputs(){
  setup1();
  #if READERS_COUNT > 1
  setup2();
  #endif
  #if READERS_COUNT > 2
  setup3();
  #endif
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  char message[10];

  for (unsigned int i=0;i<length;i++) {
    Serial.print((char)payload[i]);
    if(i < 10) {
      message[i]=payload[i];
    }
  }
  Serial.println();
  message[min(length, 9)]=0;

  if(strncmp(WIEGAND1_TOPIC, &topic[strlen("enterprised/reader/")], strlen(WIEGAND1_TOPIC)) == 0){
    if(strncmp(message,  "accept", strlen("accept")) == 0){
      Serial.print("accept ");
      Serial.println(WIEGAND1_TOPIC);
      open1();
    }else{
      reject_start1();
    }
  }
  #if READERS_COUNT > 1
  else if(strncmp(WIEGAND2_TOPIC, &topic[strlen("enterprised/reader/")], strlen(WIEGAND2_TOPIC)) == 0){
    if(strncmp(message,  "accept", strlen("accept")) == 0){
      Serial.print("accept ");
      Serial.println(WIEGAND2_TOPIC);
      open2();
    }else{
      reject_start2();
    }
  }
  #endif
  #if READERS_COUNT > 2
  else if(strncmp(WIEGAND3_TOPIC, &topic[strlen("enterprised/reader/")], strlen(WIEGAND3_TOPIC)) == 0){
    if(strncmp(message,  "accept", strlen("accept")) == 0){
      Serial.print("accept ");
      Serial.println(WIEGAND3_TOPIC);
      open3();
    }else{
      reject_start3();
    }
  }
  #endif
}


#if READERS_COUNT == 1
void setup1(){
  pinMode(DOOR1_RELAY_PIN, INPUT);
  pinMode(DOOR1_BUZZER_PIN, INPUT);
}
void open1(){
  pinMode(DOOR1_RELAY_PIN, OUTPUT);
  t.after(3000, close1);
}
void close1(){
  pinMode(DOOR1_RELAY_PIN, INPUT);
}
void reject_start1(){
  pinMode(DOOR1_BUZZER_PIN, OUTPUT);
  t.after(1000, reject_end1);
}
void reject_end1(){
  pinMode(DOOR1_BUZZER_PIN, INPUT);
}
#else
void setup1(){
  pinMode(DOOR1_RELAY_PIN, OUTPUT);
  pinMode(DOOR1_BUZZER_PIN, OUTPUT);
}
void open1(){
  digitalWrite(DOOR1_RELAY_PIN, HIGH);
  t.after(5000, close1);
  digitalWrite(DOOR2_RELAY_PIN, HIGH);
  t.after(3000, close2);
}
void close1(){
  digitalWrite(DOOR1_RELAY_PIN, LOW);
}
void reject_start1(){
  digitalWrite(DOOR1_BUZZER_PIN, HIGH);
  t.after(1000, reject_end1);
}
void reject_end1(){
  digitalWrite(DOOR1_BUZZER_PIN, LOW);
}

void setup2(){
  pinMode(DOOR2_RELAY_PIN, OUTPUT);
  pinMode(DOOR2_BUZZER_PIN, OUTPUT);
}
void open2(){
  digitalWrite(DOOR2_RELAY_PIN, HIGH);
  t.after(3000, close2);
}
void close2(){
  digitalWrite(DOOR2_RELAY_PIN, LOW);
}
void reject_start2(){
  digitalWrite(DOOR2_BUZZER_PIN, HIGH);
  t.after(1000, reject_end2);
}
void reject_end2(){
  digitalWrite(DOOR2_BUZZER_PIN, LOW);
}
#endif

#if READERS_COUNT > 2
void setup3(){
  pinMode(DOOR3_RELAY_PIN, OUTPUT);
  pinMode(DOOR3_BUZZER_PIN, OUTPUT);
}
void open3(){
  digitalWrite(DOOR3_RELAY_PIN, HIGH);
  t.after(3000, close3);
}
void close3(){
  digitalWrite(DOOR3_RELAY_PIN, LOW);
}
void reject_start3(){
  digitalWrite(DOOR3_BUZZER_PIN, HIGH);
  t.after(1000, reject_end3);
}
void reject_end3(){
  digitalWrite(DOOR3_BUZZER_PIN, LOW);
}
#endif

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("intercom")) {
      Serial.println("connected");
      // Once connected, resubscribe
      client.subscribe("enterprised/reader/+/action");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
