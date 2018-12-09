//BlynkEspFrame

/* !!TODO NOW *******************
Get / Set Timestamp from internet
Clean up serial prints - setup for certain serial prints if offline
get the online blynker going again 

TODO Later **********************
Minimal Offline operation 
??? what is char* vrs char[]
BLYNK based wateringTimeout step
Neopix colors for different offline errors 


/* Recent comments
12/9/18 Forgot how much strings suck in C
changed all status and state to Ints
*/





boardState - State Machine
//OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5

boardStatus State informed by Status
  Error conditions / Status
  ----------------
  //WaterGone=1 BatteryLow=2 Offline=3 Online=4

  wateringStatus
  ---------------
  Pumping=1 Pausing=2 Complete=3
  Pausing=2 - only for slow soaking planters
  


STATE   |  LED  |   Because            Can Change To     Change Condition  Notes
--------------------------------------------------------------------------
OK      | Green | Watered &          |  Error              Water Gone or              
        |       | No Error           |                     Battery Low or
--------------------------------------------------------------------------
Water   | Yellow| Timer start on V2  |
--------------------------------------------------------------------------
Watering| Blue  | Pump Cycle Started |  Pump Cycle Complete|  Completes or Error
        |       |                    |  Error              |
--------------------------------------------------------------------------
Watered | Purple| Pump Cycle Complete|  OK                 | Complete notification timeout or Error                       
        |       |                    |  Error               
--------------------------------------------------------------------------        
Error   | Red   | Water Gone or      | OK                  Error resolved
        |       | Battery Low or     |
        |       | Offline            |
--------------------------------------------------------------------------


Timer / BLYNK Eventor
  At 11 am on Sat - send notification
    set V2 to 1 - Pump Cycle start flag - Email Pump Cycle Started
        
  For bottom up tray fill / overflow type planters
    On ESP - when V2 goes to 1 begin pump cycle
    Pump for a set period of time or until Watered sensor fires
    Monitor pump cycle via timer monitor V3 - how does that work? Might not need another Vpin
    I think I want when V2 goes high. For the blynk app ? another timer to check back in on 
      the status of V2 to confirm things?
  
  For slow soak type planters

- ideal operation
  Timer ? Eventor Time event (manageable on phone)
    Time event sets Pump Cycle Start flag / Vpin #?
    ESP starts pump cycle (is always watching for Pump Cycle Start Flag)
    ESP does pump cycle / however that works for this device / and tracks status
    ESP sends status back on another Vpin #?
    ESP Completes pump cylcle and updates status with timestamp - completed
  Pump Cycle for bottom up - on power AC (always on)
    Check status / float - watered
      If OK
        Turn on watervcc pin
        Set onboard led mode
        Set blynk led color
        Send status
        Water till watered pin up or timeout
        Send status
        Reset


On board pull wateredPin low (A0)
Pump off if offline
Get offline code working
Watered sensor power on for watering cycle
  Pump cycle timing
ON board use builtInLED for offline notifications
Put color key on BLYNK
VLed color feedback 
  Blue - watering
  Yellow ? watered
  Red - out of water
  Green - ok
  Black - water off per watered sensor
Last Complete Watered date
Time server is not being used? but would be good for timestamps? 
// around line 128
If the password is wrong the serial output just stops or actually like 
    Platformio crashed? No  
WiFiconnected blinker - needs to blink V2 - but not if on battery power
V2 jumper for battery powered mode
Add the debug on code for Blynk Terminal

TODO Future

END of TODO's
*/

 /* 
 *LOG
 * Board NodeMCU 1.0 ESP-12E
 * 11/13/18 Installed PlatformIO on Surface
 * 11/9/18 Added to Git repository BlynkEspFrame
 * Ported from Arduino IDE MinimalPlanterV8-8266.ino To PlatformIO 11/9/18
 * Use with BLYNK App MinimalPlanter - ad948bfe5868470a9dffde4c3c747332
 * Historical reference Google Drive\Projects\GreensGrower\V4\Electronics\Software\Arduino\GreensGrower4.8
 * 10/9/18 Removed lots of comments and unrelated code from V6
 * 10/12/18 V7 working with protoboard tests
 * 12/1/18 Refinements for PlantFrame[?] 1st 4 up prototoype
 */

/*
    Downloads, docs, tutorials: http://www.blynk.cc
    Sketch generator:           http://examples.blynk.cc
    Blynk community:            http://community.blynk.cc
    Follow us:                  http://www.fb.com/blynkapp
                                http://twitter.com/blynk_app
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
 *                         
 *                         
 *                         ESP 8266 12E   Pin       
 *   From Watered sensor |--A0     D0--GPIO16-- |X Needed for deep sleep
 *                       |--GND    D1--GPIO5--  |
 *                       |--VV     D2--GPIO4----|
 *                       |--SD3    D3--GPIO0----| -- Led
 *                       |--SD2    D4--GPIO2----|X BuiltIn LED pin 2 and Bootloader 
 *                       |--SD1    3V3--3.3V----|V+ 
 *                       |--CMD    GND--GPIO----|
 *                       |--SD0    D5--GPIO14---| -- Mosfet to Pump
 *                       |--CLK    D6--GPIO12---| -- Power to Watered sensor 
 *                       |--GND    D7--GPIO13---| -- Float in
 *                       |--3V3    D8--GPIO15---|xx ? Wants 10k pulldown ? Conflicts with serial output
 *                       |--EN     RX--
 *                       |--RST    TX--
 *                Ground |--GND    GND----------|
 *                Vin 5v |--Vin    3.3V-- 
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
//Timing when offline !!TODO Can still use the Blynk Timer when offline??
unsigned long lastConnectionAttempt = millis();
unsigned long connectionDelay = 5000; // try to reconnect every 5 seconds

//Timer5Sec
int lastRun5 = millis();

//Timestamp
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = 3600;
const int   daylightOffset_sec = 3600;

// Widget Colors
#define BLYNK_BLUE      "#04C0F8"
#define BLYNK_YELLOW    "#ED9D00"
#define BLYNK_RED       "#D3435C"
#define BLYNK_GREEN     "#23C48E"
#define BLYNK_DARK_BLUE "#5F7CD8"
#define BLYNK_PURPLE    "#FF33FB"
#define BLYNK_BLACK     "#000000"

//Blynk Widgets
WidgetTerminal terminal(V0);
bool debug = false;

WidgetLED led1(V1);
bool ledStatus = false;

//Physical Pins
const int pumpPin = 14; // D5
const int floatPin = 13; // D7
const int wateredPin = A0; // Read
const int wateredVCCPin = 12; // D6 Supply 3.3v to wateredPin Read
const int ledPin = 0; // D3 Hardware LED
const int builtInLed = 2; // D4 Hardware LED on Dev Board

//Other Globals
char firmwareVersion[] = "BlynkEspFrame-main.cpp";
int boardStatus = 0; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
int boardState = 0; // OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
bool wifiConnected = false;
bool waterLow = false; //Water reseviour low
int watered = 0; //Water overflow from watering sensor - analog
int wateredThreshold = 250; //Water sensor sensitivity
int wateringStatus = 0; //Pumping=1 Pausing=2 Complete=3
int wateringTimeout = 2400; // In seconds? !!TODO How does this timer work? A counter in per second?
//TODO How to retain values through a restart - like lastWatered
char lastWatered[]= "NaN"; //Last pump cycle complete timestamp
int ledLastState = 0; //?Helps with state management?

/* **********************************************
 FUNCTIONS **************************************
*************************************************
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
 */

/*
State()
If state = OK
  - Confirm state
    if(!wifiConnected){
x  call checkOnline()
  call checkFloat()
  call checkBattery()
  If state = OK - still
    If status = watering
      call pumpCycle()
    else
      call checkWaterNowFlag()

Implimentation
Function State() called every second encapsulates State Machine logic
  State() calls helper functions like bool checkFloat()

Error Status
  WaterGone
  BatteryLow
  Offline

State Machine
OK > Water (pushed by blynk) > Watering > Watered > OK || Error
Encapsulates a state machine logic, called every second 
   Calls helper functions and sets the state
      Helper functions 
        return values and set Status to resolve back in function State
        set hardware changes
        set interface / BLYNK values

State Machine
OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
Runs every second - perSecond() */

bool checkFloat(){
  waterLow  = digitalRead(floatPin);
    if(waterLow){
      led1.setColor(BLYNK_RED);
      digitalWrite(pumpPin, LOW);
      boardStatus = 1; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
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
    digitalWrite(pumpPin, LOW); //Just to make sure pump is off
    boardState = 3; //OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
    led1.setColor(BLYNK_PURPLE); 
    //!!TODO - Is this where we push last watered timestamp - call for the time here?
    return true;
  }
    else{
      return false;
    }
  } 

// Device --> Server --> Interface
void V5Uptime(){ // Push
  Blynk.virtualWrite(V5, millis() / 1000);
}

// V6 - Watering Status
void pushTimeStamp(){
  //TODO get Timestamp
  Blynk.virtualWrite(V6, lastWatered);
}

//TODO Then enable or disable the pump off on watered via virtual pin

void debugPrint()
{
  if (debug) {
    terminal.print("Status = ");
    terminal.println(boardStatus); 
    terminal.print("Watered sensor threshold = ");
    terminal.println(wateredThreshold); 
    terminal.print("wateredPin = ");
    terminal.println(watered);
    //terminal.print("lowTempThreshold = ");
    //terminal.println(lowTempThreshold); 
  }
}

/* Blynk Virtual Pins
 * vPin - Widget/var - Feature - Hardware
 * ----------------------------------
 *  V0  - terminal 
 *  V1  - led1 -LED Water Level Float - 
 *  V2  - Eventor Timer - Start Watering - 
 *  V3  - wateringStatus 
 *  V4  - PushButton - Pump ON  
 *  V5  - Uptime
 *  V6  - lastWatered - Last Watered TimeStamp
 *  V7  - status - Current board status / details beyond LED color
 *  V8  - step / WateredSensitivity -- drip sensor 
 */

//V0 Terminal Widget
BLYNK_WRITE(V0) 
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

/*
V1
WidgetLED led1(V1);- Already instantiated
Use like 
led1.setColor(BLYNK_BLUE);
*/

// Timer Flag to water - V2 - Eventor
BLYNK_WRITE(V2) // Write value from Blynk app event - called by Blynk Library
{
  int vpinValue = param.asInt(); // assigning incoming value from pin V2 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  // Serial.print("V4 Pump Button value is: ");
  // Serial.println(pinValue);
  if(vpinValue == 1){
    wateringStatus = 1; //Pumping=1 Pausing=2 Complete=3
    digitalWrite(pumpPin, HIGH);
    led1.setColor(BLYNK_BLUE);
  }
  else{
    digitalWrite(pumpPin, LOW);
    led1.setColor(BLYNK_BLACK); // TODO ?? Black ??
  }
}

// V3 - Watering Status
void pushWateringStatus(){
  // TODO translate back to string for interface
  //Pumping=1 Pausing=2 Complete=3
  Blynk.virtualWrite(V3, wateringStatus);
}

// Widget button Pump On
BLYNK_WRITE(V4)
{
  int pinValue = param.asInt(); // assigning incoming value from pin V4 to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  // Serial.print("V4 Pump Button value is: ");
  // Serial.println(pinValue);
  if(pinValue == 1){
    digitalWrite(pumpPin, HIGH);
    led1.setColor(BLYNK_BLUE);
  }
  else{
    digitalWrite(pumpPin, LOW);
    led1.setColor(BLYNK_BLACK);
  }
}


//Device <-- Server <-- Interface
BLYNK_WRITE(V8)
// Adapted from Blynk example GetData
{
  wateredThreshold = param.asInt(); // assigning incoming value from pin Vx to a variable
  // You can also use:
  // String i = param.asStr();
  // double d = param.asDouble();
  Serial.print("V8 Step value is: ");
  Serial.println(wateredThreshold);
}

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


/*******************************************************
* Primary runtime control functions ********************
********************************************************/

char stateMachine(){ //Called every second by perSecond()
  //First check status for any errors
  if(!wifiConnected){
    boardStatus = 3; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
    boardState = 5; //OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
  }

  if(!checkFloat()){ 
    boardStatus = 1; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
    boardState = 5; //OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
  }

  /*
  if(!checkBattery(){
    boardStatus = 2; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
    boardState = 5; //OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
  }    
  */ 

//If no errors then check for status changes that change state
  if((boardState != 5)){ //Error
    //Watering?
    if((wateringStatus = 1)){ //Pumping=1 Pausing=2 Complete=3
    boardState = 3;//OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
    }
    //Watering Complete?
    if((wateringStatus = 3)){ //Pumping=1 Pausing=2 Complete=3
    //Last Watered TimeStamp sent from checkWatered
    boardState = 1; ////OK=1 > Water=2 > Watering=3 > Watered=4 > OK || Error=5
    }
  }
}


void perSecond(){// Do every second //!!TODO Only checkWatered if pump is on
  stateMachine();
  // TODO put in V5 as a parmeter
  V5Uptime();
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

 // timer.setInterval(10000L, V5Push); //Every 10 seconds

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
  // if (Blynk.connected())
  if (WiFi.status() != WL_CONNECTED)
  {
//* * BEGIN OFFLINE CODE *******************************
wifiConnected = false; 
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
  { 
    wifiConnected = true; 
    boardStatus = 4; //WaterGone=1 BatteryLow=2 Offline=3 Online=4
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
  
/* How this works
In setup a timer calls perSecond()
perSecond() calls stateMachine()
stateMachine changes state via helper functions that set status flags
  also there are certain injections from the BLYNK app interface
*/

    Blynk.run();
    timer.run(); 

  }
}
