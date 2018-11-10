/*
 * Board NodeMCU 1.0 ESP-12E
 * Ported from Arduino IDE MinimalPlanterV8-8266.ino To PlatformIO 11/9/18
 * Use with BLYNK App MinimalPlanter - ad948bfe5868470a9dffde4c3c747332
 * Historical reference Google Drive\Projects\GreensGrower\V4\Electronics\Software\Arduino\GreensGrower4.8
 * 10/9/18 Removed lots of comments and unrelated code from V6
 * 10/12/18 V7 working with protoboard tests
 */

/****************************
 * * FEATURES ***************
 * **************************
 * Water Level notification / pump stop
 * Pump stop on watered sensor
 * Pump cycle timing
 * 
 */
/**************************** 
 * * TODO *******************
 * **************************
Pump stop on water low
Pump stop on watered sensor
Watered sensor power on for watering cycle
 * Pump cycle timing


WiFiconnected blinker - needs to blink V2 - but not if on battery power
V2 jumper for battery powered mode

*/



/*                      
 *                    https://arduino.stackexchange.com/questions/25260/basic-question-esp8266-board-pins
 *                    ESP8266 LoLin pinout at - https://kevoster.wordpress.com/2016/07/14/esp-8266-nodemcu-lolin/
 *                    Board num--Arduino IDE 
 *                         
 *                         
 *                         ESP 8266 12E   Pin       
 *                       --A0     D0--GPIO16-- |X Needed for deep sleep
 *                       --GND    D1--GPIO5--  |
 *                       --VV     D2--GPIO4----|
 *                       --SD3    D3--GPIO0----| -- Led
 *                       --SD2    D4--GPIO2----|X BuiltIn LED pin 2 and Bootloader 
 *                       --SD1    3V3--3.3V----|V+ 
 *                       --CMD    GND--GPIO----|
 *                       --SD0    D5--GPIO14---| -- Mosfet to Pump
 *                       --CLK    D6--GPIO12---| -- Power to Watered sensor 
 *                       --GND    D7--GPIO13---| -- Float in
 *                       --3V3    D8--GPIO15---|xx ? Wants 10k pulldown ? Conflicts with serial output
 *                       --EN     RX--
 *                       --RST    TX--
 *                 Ground--GND    GND----------|
 *                 Vin 5v--Vin    3.3V-- 
 *  
 */


/* Comment this out to disable prints and save space */
#define BLYNK_PRINT Serial

/* ********************************
 * * LIBRARIES ********************
 * ********************************
 */

// Platform
#include <Arduino.h>

// ESP32 Libs
// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <BlynkSimpleEsp32.h>

// ESP8266 Libs
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

/* *********************************
 * * INITIALIZATIONS ***************
 * *********************************
 */

// Timer object
BlynkTimer timer;


/* ***********************************
 * * AUTHORIZATIONS *******************
 * ***********************************
 */

#include "auth.h"

// Auth Token
//char auth[] = "ad948bfe5868470a9dffde4c3c747332"; // BLYNK APP - MinimalPlanter

// WiFi credentials.
// Set password to "" for open networks.

// char ssid[] = "MySpectrumWiFi7c-2G";
// char pass[] = "chillyrabbit361";

//char ssid[] = "Fat_Cat_Fablab"; 
//char pass[] = "m4k3rb0t";

//char ssid[] = "LRPixel";
//char pass[] = "LRPixel222";

//char ssid[] = "D1B436";
//char pass[] = "55539777";

/* 
 * **********************************
 * * GLOBAL VARS ********************
 * **********************************
 */

//Timing when offline !!Can still use the Blynk Timer when offline??
int lastConnectionAttempt = millis();
int connectionDelay = 5000; // try to reconnect every 5 seconds

//Timer5Sec
int lastRun5 = millis();

//Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;


int wifiConnected = 0;
int waterLow = 0;
int watered = 0;
int ledLastState = 0;

// Attach virtual serial terminal to Virtual Pin V0
WidgetTerminal terminal(V0);

WidgetLED led1(V1);
bool ledStatus = false;

#define BLYNK_GREEN     "#23C48E"
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_DARK_BLUE "#5F7CD8"

const int pumpPin = 14; // D5
const int floatPin = 13; // D7
const int wateredPin = A0; // Read
const int wateredVCCPin = 12; // Supply 3.3v to wateredPin Read
const int ledPin = 0; // D3 Hardware LED

/* Blynk Virtual Pins
 *  V1  - LED - Water Level Float
 *  V4  - Timer
 *  V12 - Pump
 */


/* 
 * **************************************************
 * * FUNCTIONS **************************************
 * **************************************************
 */

/* 
 *  For Minimal Planter
 *  
 *  Pump time trigger
 *  Server --> Device
 *  
 *  Water Low Float Switch
 *  Device --> Server --> Interface
 *  
 *  Device Offline
 *  Server --> Interface
 *  
 *  Watering Done
 *  Device --> Server --> Interface
 *  Plan to evolve this via open pins and pads
 *  Start with a simple conductivity trigger with copper wires in a way that 
 *  can be maintained / corrosion removed. Push D5 high before watering starts and read on A0
 *  
 *  Pump On - Off
 *  Interface --> Server --> Device
 *   
 */


void perSecond() // Do every second
{
  // Watered - Drip / Overflow sensor
  watered = analogRead(wateredPin);
  Serial.print("wateredPin = ");
  Serial.println(watered);
  // TODO Try to set a variable to the value of a Virtual pin - slider would be cool
  // TODO Then enable or disable the pump off on watered via virtual pin 
  if(watered > 250){ 
  // Watered - turn off pump
  digitalWrite(pumpPin, LOW);
  }
  // Float / Water level
    waterLow  = digitalRead(floatPin);
    if(waterLow){
      Serial.println("Float Low");
      led1.setColor(BLYNK_RED);
      digitalWrite(pumpPin, LOW);
    }
    else{
       Serial.println("Float High");
       led1.setColor(BLYNK_GREEN); 
    }

}


/*
void readWatered()
{
  watered = analogRead(wateredPin);
  Serial.print("wateredPin = ");
  Serial.println(watered);
}
*/

/*
void checkFloat()
  {
  waterLow  = digitalRead(floatPin);
    if(waterLow){
      Serial.println("Float Low");
    }
    else{
       Serial.println("Float High"); 
      
    }
    
  }
*/

// V1 LED Widget is blinking

/*
void blinkLedWidget()
{
  if (ledStatus) {
    led1.setColor(BLYNK_RED);
    Serial.println("LED on V1: red");
    ledStatus = false;
  } else {
    led1.setColor(BLYNK_GREEN);
    Serial.println("LED on V1: green");
    ledStatus = true;
  }
}
*/



void connectionBlink()
  {
    if(wifiConnected){
      // Toggle led
      if(ledLastState == 1){
        digitalWrite(ledPin, LOW);
        ledLastState = 0;
        }
      else{
        digitalWrite(ledPin, HIGH);
        ledLastState = 1;
        }      
    }
    else{ // Not connected
       digitalWrite(ledPin, LOW);
    }
    Serial.print("connected = "); 
    Serial.println(wifiConnected); 
  }


//Device <-- Server <-- Interface
BLYNK_WRITE(V1)
// Adapted from Blynk example GetData
{
  int pinValue = param.asInt(); // assigning incoming value from pin V1 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V1 Slider value is: ");
  Serial.println(pinValue);
}

/* GET EVENTS FROM BLYNK
 *   App project setup:
 *   Timer widget attached to V4 and running project.
 
BLYNK_WRITE(V4)
{
  // You'll get HIGH/1 at startTime and LOW/0 at stopTime.
  // this method will be triggered every day
  // until you remove widget or stop project or
  // clean stop/start fields of widget
  Serial.print("Got a value: ");
  Serial.println(param.asStr());
} 

*/

// Device --> Server --> Interface
void V5Push()
// Adapted from Blynk example VirtualPinWrite
// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
{
  Blynk.virtualWrite(V5, millis() / 1000);
  Serial.println("V5Push");
  terminal.println("MinimalPlanterV8-8266");
}


// This function will be called every time the Widget
// in Blynk app writes values to the Virtual Pin 21
BLYNK_WRITE(V4)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V21 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V4 Button value is: ");
  Serial.println(pinValue);
  if(pinValue == 1){
    digitalWrite(pumpPin, HIGH);
  }
  else{
    digitalWrite(pumpPin, LOW);
  }
}


/*
 *  **********************************************************

  Simple push notification example

  App project setup:
    Push widget

  Connect a button to pin 2 and GND...
  Pressing this button will also push a message! ;)
 **************************************************************
 */
 void notifyOnButtonPress()
{
  // Invert state, since button is "Active LOW"
  int isButtonPressed = !digitalRead(2);
  if (isButtonPressed) {
    Serial.println("Button is pressed.");

    // Note:
    //   We allow 1 notification per 5 seconds for now.
    Blynk.notify("Yaaay... button is pressed!");

    // You can also use {DEVICE_NAME} placeholder for device name,
    // that will be replaced by your device name on the server side.
    //Blynk.notify("Yaaay... {DEVICE_NAME}  button is pressed!");
  }
}


/*
 * ************************************************
 * * SETUP ****************************************
 * ************************************************
 */

void setup()
{

// Debug console
Serial.begin(9600);

Blynk.begin(auth, ssid, pass);

// Blynk.begin(auth);

  // Clear the terminal content
  terminal.clear();

  // Turn LED on, so colors are visible
  led1.on();
  // Setup periodic color change
  

/*
 * **********************************************
 * * TIMER FUNCTION CALLS ***********************
 * **********************************************
 */

 // NOTE Can not make multiple calls at same timer interval with timer.setInterval below.


//TODO - Consolidate into frequency like have a every1second function that then 
         // calls all everything we want to do every second
 
  // Call Function on timed interval
  //               delay - function
  timer.setInterval(10000L, V5Push); //Every 10 seconds

  // WiFi Connected Blinker 
  timer.setInterval(1000L, connectionBlink);

  // Setup a function to be called every 5 seconds
 // timer.setInterval(500L, checkFloat); //Also stuck the CONNECTED Blink in here

  //timer.setInterval(2000L, blinkLedWidget);
 
  // Every second
  timer.setInterval(1000L, perSecond); //Watered overflow or drip sensor

  // Setup notification button on pin 2
  pinMode(2, INPUT_PULLUP);
  // Attach pin 2 interrupt to our handler
  attachInterrupt(digitalPinToInterrupt(2), notifyOnButtonPress, CHANGE);

//Pump
// !! Had problem
// See https://community.blynk.cc/t/on-boot-up-pins-go-high/10845/49
// Seems with pump on D6 - without float on D7 pin only goes high momentarily now
// related to state of float? D7 mode was not set


//!! Problem
// Ok so some pins go high on boot probably depends on dev board
// Pins behave differently if they are powered by VIN or by the USB
// pinMode also impacts this
// ? pins go high during upload
// For now trying pumpPin on D5 - GPIO 14 
// Ok on D5 was getting very short pump runs on boot till that changed.
digitalWrite(pumpPin, LOW);
pinMode(pumpPin, OUTPUT);
digitalWrite(pumpPin, LOW);
pinMode(ledPin, OUTPUT);
pinMode(floatPin, INPUT);
//pinMode(floatPin, INPUT_PULLUP);
//pinMode(LED_BUILTIN, OUTPUT);

pinMode(wateredVCCPin, OUTPUT);
// !! just switch following high for watered read to save power and lower corrosion
digitalWrite(wateredVCCPin, HIGH);

}

/* **************************************************
 * * MAIN LOOP **************************************
 * **************************************************
 */
 
void loop()
{
// digitalWrite(ledPin, HIGH); 
  // even this does not prevent the pump from running on boot
  //digitalWrite(pumpPin, LOW);
  // check WiFi connection:
  if (WiFi.status() != WL_CONNECTED)
  {
    
/*
 * ****************************************************
 * * BEGIN OFFLINE CODE *******************************
 * ****************************************************
 */
    Serial.println(" Offline ...");
    // check delay:
    if (millis() - lastConnectionAttempt >= connectionDelay)
    {
      lastConnectionAttempt = millis();
      Serial.println(" Connecting ...");
      // attempt to connect to Wifi network:
      if (pass && strlen(pass))
      {
        WiFi.begin((char*)ssid, (char*)pass);
      }
      else
      {
        WiFi.begin((char*)ssid); // For open networks I guess.
      }
    }
  }
  else
wifiConnected = 1; 
  
  { 
  // Homespun timer not BLYNK timer 
    if (millis() - lastRun5 >= 5000){
      Serial.println(" CONNECTED");
      //init and get the time
      configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
      // printInternetTime(); !! Check examples for fix 
      Serial.print("millis ");
      Serial.println(millis());
      lastRun5 = millis();
      Serial.print("lastRun5");
      Serial.println(lastRun5);
    }

/* 
 ********************************************************
 * * BEGIN BLYNK ONLINE CODE ****************************
 ********************************************************
 */
  
    Blynk.run();
    timer.run(); // BlynkTimer - use the timer.setInterval call in setup
digitalWrite(ledPin, LOW);
   
    //Serial.print("Status Button");
    //Serial.println(analogRead(34));

  }
}
