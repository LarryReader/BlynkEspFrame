//BlynkEspFrame

/* TODO *******************
Pump off if offline - update pump features tested below
Pump off if offline - update pump features tested below
PUMPCYCLE - Modes? Slow Soak / Bottom Up The mode works with the watered sensor or not
Works with Timer - Pump cycle strategy - Low Water and over water sensors

Perhaps for not there is somthing of a watered sensor over ride in case that sensor fails
note multiple PumpCycles probably not required for a bottom soak up setup 
  PumpCycles - like one of them does want to handle where a pumplcycle
  where the watering complete sensor is not triggered does want to work
Virtual Pins
  VP PumpCyclesStart
  VP PumpCycles
  VP PumpCycleLength
  VP PumpCycleCurrentDuration
This verion assumes being online
  Blynk timer sets PumpC?yclesStart
  Local intCycleCount tracks completed pump cycles
  if it goes offline picks cycles back up when back online
Blynk timer > Sets a Virtual Pin 

Pump cycle resume when online
Pump cycle resume when water refilled
Get offline code working
Watered sensor power on for watering cycle
Pump cycle timing
Put color key on BLYNK
VLed color feedback 
  Blue - watering
  Red - out of water
  Green - ok
  Black - water off per watered sensor
Last Complete Watered date
If the password is wrong the serial output just stops or actually like 
    Platformio crashed? No  
WiFiconnected blinker - needs to blink V2 - but not if on battery power
V2 jumper for battery powered mode
Add the debug on code for Blynk Terminal

*/
 /* Board NodeMCU 1.0 ESP-12E
 * 11/13/18 Installed PlatformIO on Surface
 * 11/9/18 Added to Git repository BlynkEspFrame
 * Ported from Arduino IDE MinimalPlanterV8-8266.ino To PlatformIO 11/9/18
 * Use with BLYNK App MinimalPlanter - ad948bfe5868470a9dffde4c3c747332
 * Historical reference Google Drive\Projects\GreensGrower\V4\Electronics\Software\Arduino\GreensGrower4.8
 * 10/9/18 Removed lots of comments and unrelated code from V6
 * 10/12/18 V7 working with protoboard tests
 */

/* FEATURES *****************
 * Water Level notification / pump stop
 * Pump stop on watered sensor
 * Pump cycle timing
 */

/*
  Pump features tested
  With pump on via Blynk button
    When float goes low pump turns off
      When float comes back high pump does not go back on till Blynk button is cycled
    When watered gripped with wet fingers pump turns off
      Stays off till Blynk button is cycled
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

/* * Fundamental Blynk communications
 * Blynk App > Device - Blynk Example - Get Data 
 * BLYNK_WRITE(V1){ //Called every time Widget on V1 in Blynk app writes values to V1
 *  int pinValue = param.asInt(); // assigning incoming value from Interface pin V1 to a variable
 *  }
 *  
 * Device > Blynk App - Blynk Example - Push Data
 *  Blynk.virtualWrite(V7, value); //Set V7 for Eventor to watch for notifications
 *  
 * Blynk App Request < From Device - Blynk Example - Push Data On Request
 * BLYNK_READ(V8){
 *  Blynk.virtualWrite(PIN_UPTIME, millis() / 1000);
 *{
 */

/* Comment this out to disable prints and save space
    I think this wants to be the first line in the code ?
 */
#define BLYNK_PRINT Serial

/* AUTHORIZATIONS ********************/
#include "auth.h"

// Libraries  ***********************

// Platform ***
#include <Arduino.h>

// Boards   ***

// ESP32
// #include <WiFi.h>
// #include <WiFiClient.h>
// #include <BlynkSimpleEsp32.h>

// ESP8266 
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>

// Objects and Global vars

// Timer 
BlynkTimer timer;
//Timing when offline !!Can still use the Blynk Timer when offline??
unsigned long lastConnectionAttempt = millis();
unsigned long connectionDelay = 5000; // try to reconnect every 5 seconds

//Timer5Sec
int lastRun5 = millis();

//Time
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

/* Blynk Virtual Pins - Feature - Objects
 *  V0  - Terminal - terminal
 *  V1  - LED Water Level Float - led1
 *  V4  - Timer - 
 *  V7  - WateredTrigger - drip sensor - Slider
 *  V12 - Pump
 */

//Blynk Widgets
WidgetTerminal terminal(V0);
bool debug = false;

WidgetLED led1(V1);
bool ledStatus = false;
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_GREEN     "#23C48E"
#define BLYNK_DARK_BLUE "#5F7CD8"
#define BLYNK_BLACK     "#000000"


//Physical Pins
const int pumpPin = 14; // D5
const int floatPin = 13; // D7
const int wateredPin = A0; // Read
const int wateredVCCPin = 12; // Supply 3.3v to wateredPin Read
const int ledPin = 0; // D3 Hardware LED
//!!!TODO add builtin LED use builtin LED for connections blink

//Other Globals
char firmwareVersion[] = "BlynkEspFrame-main.cpp";
int wifiConnected = 0;
int waterLow = 0; //Water reseviour low
int watered = 0; //Water overflow from watering sensor
int wateredThreshold = 250; //Water sensor sensitivity
int ledLastState = 0;

// FUNCTIONS **************************************

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
 */

bool checkFloat(){
  waterLow  = digitalRead(floatPin);
    if(waterLow){
      led1.setColor(BLYNK_RED);
      digitalWrite(pumpPin, LOW);
      return  true;
    }
    else{
      led1.setColor(BLYNK_GREEN);
      return false;
    }
}

bool checkWatered(){
  watered = analogRead(wateredPin);
  if(watered > wateredThreshold){ 
    // Watered - turn off pump
    digitalWrite(pumpPin, LOW);
    led1.setColor(BLYNK_BLACK);
    return true;
  }
    else{
      return false;
    }
  } 

//TODO Try to set a variable to the value of a Virtual pin - slider would be cool
//TODO Then enable or disable the pump off on watered via virtual pin

void perSecond() // Do every second
{
  //Check Watered then set pump and led
  if(checkWatered()){
    Serial.print("watered");
  }

  //Check Float then set pump and led
  if(checkFloat()){ //Note function checkFloat must preceed this function or get not is scope error on compile
      Serial.println("Float High");
    }
    else{
      Serial.println("Float Low");
    }
}

void debugPrint()
{
  if (debug) {
    terminal.print("Watered sensor threshold = ");
    terminal.println(wateredThreshold); 
    //terminal.print("lowTempThreshold = ");
    //terminal.println(lowTempThreshold); 
  }
}

BLYNK_WRITE(V0) // Terminal Widget
{
    if (String("debug on") == param.asStr()) {
    debug = true;
    terminal.println("ON") ;
    } 
    
    if (String("debug off") == param.asStr()){
    debug = false;
    terminal.println("OFF") ;
    
    // Send it back
    terminal.print("Debug is ");
    terminal.write(param.getBuffer(), param.getLength());
    terminal.println();
  }

  // Ensure everything is sent
  terminal.flush();
}

void connectionBlink() //!!!Pass the ledPin as parameter so can easily change without editing func
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

// Device --> Server --> Interface
void V5Push()
// Adapted from Blynk example VirtualPinWrite
// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
{
  Blynk.virtualWrite(V5, millis() / 1000);
  Serial.println("V5Push");
  terminal.println(firmwareVersion);
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
    led1.setColor(BLYNK_BLUE);
  }
  else{
    digitalWrite(pumpPin, LOW);
    led1.setColor(BLYNK_BLACK);
  }
}


/*
SETUP *******************************************************************
*/
 
void setup()
{

//Debug 
Serial.begin(9600);
Serial.println(firmwareVersion);

Blynk.begin(auth, ssid, pass);

// Blynk.begin(auth);

  // Clear the terminal content
  terminal.clear();

  // Turn LED on, so colors are visible
  led1.on();
  // Setup periodic color change
  

// TIMER FUNCTION CALLS ***********************
  // Call Function on timed interval
  //               delay - function
timer.setInterval(1000L, perSecond);

// NOTE Can not make multiple calls at same timer interval with timer

  timer.setInterval(10000L, V5Push); //Every 10 seconds

  // WiFi Connected Blinker 
 // timer.setInterval(1000L, connectionBlink);

  // Setup a function to be called every 5 seconds
 // timer.setInterval(500L, checkFloat); //Also stuck the CONNECTED Blink in here

  //timer.setInterval(2000L, blinkLedWidget);
 


//Pump
// !! Had problem
// See https://community.blynk.cc/t/on-boot-up-pins-go-high/10845/49
// Seems with pump on D6 - without float on D7 pin only goes high momentarily now
// related to state of float? D7 mode was not set


//!!TODO Problem
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
// TODO !! TODO just switch following high for watered read to save power and lower corrosion
digitalWrite(wateredVCCPin, HIGH);

}

/* 
MAIN LOOP **************************************
*/ 

void loop()
{
// digitalWrite(ledPin, HIGH); 
  // even this does not prevent the pump from running on boot
  //digitalWrite(pumpPin, LOW);
  // check WiFi connection:
  if (WiFi.status() != WL_CONNECTED)
  {
    
//* * BEGIN OFFLINE CODE *******************************
 
    Serial.println(" Offline ...");
    // check delay:
    if (millis() - lastConnectionAttempt >= connectionDelay)
    {
      lastConnectionAttempt = millis();
      Serial.println(" Connecting ...");
      // attempt to connect to Wifi network:
      //if (pass && strlen(pass))
      //{
        WiFi.begin((char*)ssid, (char*)pass);
      //}
      //else
      //{
      //  WiFi.begin((char*)ssid); // For open networks I guess.
      //}
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

// * BEGIN BLYNK ONLINE CODE ****************************
  
    Blynk.run();
    timer.run(); // BlynkTimer - use the timer.setInterval call in setup
//digitalWrite(ledPin, LOW);
   
    //Serial.print("Status Button");
    //Serial.println(analogRead(34));

  }
}
