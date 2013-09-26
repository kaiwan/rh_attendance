/*
 * RFID 125 TTL <--> Arduino (Uno) : RFID Tag Reader
 *
 The RFID reader has these 5 header pins:
 
 D0 - Data (Wiegand protocol only)
 D1 - Data (Wiegand protocol only)
 TX - TTL RFID serial data out
 GND
 5V
 
 Basic Schematic:: Arduino Uno - pins used:
 
 (on POWER header)    5V        --> RFID reader 5V 
 (on POWER header)    GND       --> RFID reader GND
 (on serial RX ) DIO 0 (RX)     <-- RFID reader TX
 (in DIGITAL (PWM~) upper header)
 
 RFID reader chip module (ext antenna):
 http://www.nskelectronics.com/rfid-_ttl.html
 (purchased from Tenet Technetronics)
 
 Theory of Operation:
 --------------------
 The RFID tag card we're using (as of now) has 2 numbers printed on it's surface. For eg:
 0000354181  005,26501
 
 Apparently (ahem! unsure!) the unique RFID tag is the second (on-the-right) 8-digit number
 (excluding the 'comma' "," character). 
 The chip reader, when it reads a tag, transmits the 8-digit unique serial # to it's TX pin. We route the 
 chip reader TX to the Arduino's serial-in (RX) pin # 0 (on the upper DIGITAL (PWM~) header.
 The sketch below does a serial read (8 times, 1 byte each time) to retrieve the data, then ensures
 that it flushes the Tx/Rx serial buffers on the Arduino. 
 That's it!
 -------------------------------------
 Now, how do we store the tags in a non-volatile manner??
 One option is to use an Arduino SDcard Shield..
 --------------------------------------
 Common issue faced:
 When all 3 wires are connected between the devices and you hit the 'Upload' button, you see the error:
 "avrdude: stk500_recv(): programmer is not responding" 
 -OR-
 "avrdude: stk500_getsync(): not in sync: resp=0x00"
 
 Solution: looks like the Arduino Rx being connected to the RFID reader is causing an issue on the upload; so 
 just disconnect the Rx pin when uploading. Then it's fine; re-connect to test...
 Of course, once programmed and powered-off, the Arduino runs the existing code on the microcontroller flash
 when powered back on; so it just works.
 
 13Jun13:
 Debug facilities: Added a "beeper" and two "debug" LEDs to the project.
 Debug Beep / Lights Logic:
 
 ---------------------------------------------------------------------------------
 Event                |                  Response 
 --------------------------------------------------------------------------------- 
 |    Beeper            |     LEDs
 --------------------------------------------------------------------------------- 
 ----- INIT -----
 1. Power-on and init-in-progress   | 3 times (50ms delay*)| blink both 3 times, 150ms delay
 2. Init passed! (=> SD ok)         | 5 times (50ms delay) | blink both 5 times, 150ms delay
 ----- PROCESSING -----
 3. Successful read of RFID card    |     -NONE-           | blink BLUE LED once, 200ms delay
 ----- FAILURES -----
 4. setup(): 
 a. FATAL: microSD card init fails| 5 times (500ms delay) | blink both LEDs FOREVER, 1000ms delay
 b. loop(): Non-Fatal:            | 5 times (500ms delay) | blink both 5 times, 500ms delay
 c. File append failed (Fatal!)   | 10 times (500ms delay)| blink both LEDs FOREVER, 1000ms delay
 d. (serial TTL) read exceeded    |                       |
     MAXLEN bytes                 |                       |
 *delay: delay in milliseconds between beeps or LEDs On/Off (how long they stay On/Off)
 ---------------------------------------------------------------------------------
 
 ===================================
 Note!
 ===================================
 1. With the Sparkfun MicroSD shield attached, Don't use digital I/O lines 10-13 on the board;
 as it uses them.
 2. SCM: git; any code changes must be fed into the git repo
 3. When reading the SDcard on a computer, first close any stale version of the attendance logfile.
 =================================== 

 TODOs:
 1. Circuit schematic
 
 -------------------------------------- 
 * Author: Kaiwan N Billimoria
 * < kaiwan.billimoria -at- gmail -dot- com >
 * (c) Runner's High, June 2013.
 * License: LGPL / GPL
 */

//----------SDFat 
#include <SdFat.h>

// Test with reduced SPI speed for breadboards.
// Change spiSpeed to SPI_FULL_SPEED for better performance
// Use SPI_QUARTER_SPEED for even slower SPI bus speed
const uint8_t spiSpeed = SPI_HALF_SPEED;

// file system object
SdFat sd;

#define SS_PIN        8  // for the Sparkfun MicroSD shield
const int chipSelect = SS_PIN;

// define a serial output stream
ArduinoOutStream cout(Serial);
//------------------------------

#define RFID_SENTINEL   0xFFFFFFFF
#define MAXREAD  64
#define TAGLEN    8

int DISPLAY_IT=1;
int DONT_DISPLAY_IT=0;

int tcount=0;
int val=0x0;
char tag[MAXREAD];
int pinbeep=2; // D2 has +ve of the beeper

//---------Debug LEDs------------
#define NUMLEDS     2
#define BLUE_LED    0
#define RED_LED     1
#define BOTH_LEDS   3
#define FOREVER    -1

int leds[NUMLEDS], dbgled[NUMLEDS];
char logpath[] = "/RH_ATTND.TXT";


//-----------DEBUG LED--------------------------------
//boolean ledon=true;

void blinkit(int dlyms) {
  int i;
  // take all reqd LEDs HIGH (On)
  for (i=0; i<NUMLEDS; i++) {
    if (leds[i] == 1)  {
      digitalWrite(dbgled[i], HIGH);
    }
  }

  delay(dlyms);

  // take all reqd LEDs LOW (Off)
  for (i=0; i<NUMLEDS; i++) {
    if (leds[i] == 1)  {
      digitalWrite(dbgled[i], LOW);
    }
  }
  delay(dlyms);
}

void blink(int whichled, int dlyms, int reps) {
  int i;

  for (i=0; i<NUMLEDS; i++)
    leds[i]=0;

  if (whichled == BLUE_LED)
    leds[BLUE_LED]=1;
  else if (whichled == RED_LED)
    leds[RED_LED]=1;
  else if (whichled == BOTH_LEDS) {
    leds[BLUE_LED]=1;
    leds[RED_LED]=1; 
  }

  if ((leds[0] == 0) && (leds[1] == 0))
    return;

  if (reps == FOREVER) {
    for (;;)
      blinkit(dlyms);
  } 
  else {
    for (i=0; i<reps; i++)
      blinkit(dlyms);
  }
}


//-----------BEEPER--------------------------------
//boolean beepon=true;

#define SHORT_BEEP    100   // delay in ms
#define LONG_BEEP     500   // delay in ms

void beepit(int delayms) {
  digitalWrite(pinbeep, HIGH);
  delay(delayms);
  digitalWrite(pinbeep, LOW);
  delay(delayms);
}

void beeper(int reps, int delayms) {
  int i;
  //  if (!beepon)
  //    return;
  for (i=0; i<reps; i++) {
    Serial.print("i");
    beepit(delayms);
  }
}
//--------------------------------------------------------

/*
 * Append a line to LOGFILE.TXT
 */
void logEvent(const char *msg) {
  // create or open a file for append
  ofstream sdlog(logpath, ios::out | ios::app);

  // append a line to the file
  sdlog << msg << endl;

  // check for errors
  if (!sdlog) {
    beeper(10, LONG_BEEP);
    blink(BOTH_LEDS, 500, FOREVER);
    sd.errorHalt("append failed");
  }

  Serial.print("  /RH_ATTND.TXT Appended.");
  blink(BLUE_LED, 200, 1);

  // file will be closed when sdlog goes out of scope; still... :-)
  sdlog.close();
  /* from doc:
   "Close a file and force cached data and directory information to be written to the storage device."
   */
}


//-----------------Init------------------------------

void setup() {
  Serial.begin(9600);
  
  /* The uSD shield takes up DIOs 10-13; do NOT use them! */
  pinMode(pinbeep, OUTPUT);
  digitalWrite(pinbeep, LOW);

  // LED1 : BLUE debug LED
  dbgled[BLUE_LED] = 4; // D4 has +ve of the Blue LED
  pinMode(dbgled[BLUE_LED], OUTPUT); 
  digitalWrite(dbgled[BLUE_LED], LOW);

  // LED2 : RED debug LED
  dbgled[RED_LED] = 7; // D7 has +ve of the Red LED
  pinMode(dbgled[RED_LED], OUTPUT); 
  digitalWrite(dbgled[RED_LED], LOW);

  /* 
  //testing
   blink(BLUE_LED, 350, 5);
   blink(RED_LED, 350, 5);
   blink(BOTH_LEDS, 350, 5);
   delay(1000*1000);
  */

  // In Init ...
  beeper(1, SHORT_BEEP);
  blink(BOTH_LEDS, 150, 1);

  // initialize the SD card at SPI_HALF_SPEED to avoid bus errors with
  // breadboards.  use SPI_FULL_SPEED for better performance.
  if (!sd.init(SPI_HALF_SPEED, chipSelect)) {
    delay(500);
    beeper(10, LONG_BEEP);
    Serial.print("FATAL ERROR! SD card not found or not responding.");
    blink(BOTH_LEDS, 1000, FOREVER);
    // code below this does not get called!!
    sd.initErrorHalt();
  }

  // Init passed! READY.
  beeper(2, SHORT_BEEP);
  blink(BOTH_LEDS, 150, 2);

  logEvent("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
}


//-------------------------------------------
// Debug routines
void showint(char *str, int ival) {
  if(DISPLAY_IT) {
    Serial.print(str);
    Serial.print(": ");
    Serial.println(ival);
  }
}
void showcbuf(char *str, char *cbuf) {
  if(DISPLAY_IT) {
    Serial.print(str);
    Serial.print(": ");
    Serial.println(cbuf);
  }
}

int get_disp_byte(int display) {
  int loc=0;
  loc = Serial.read();
  if (display) {
    Serial.print(loc, HEX);    
    Serial.print(':');
    if(loc != RFID_SENTINEL)
      Serial.print(loc-0x30);
    else
      Serial.print('.');
    Serial.print(' ');
  }
  return loc;
}


//-----------------Main Loop--------------------------------
void loop() {
  int i, numread=MAXREAD;

  if (Serial.available() >= TAGLEN) {
    val=0;
    for (i=0; i < TAGLEN; i++) {
      val = get_disp_byte(DONT_DISPLAY_IT);

      if ((val != RFID_SENTINEL) && (val > 0)) {
        tag[tcount] = val;
        tcount ++;
      }
      if (tcount >= MAXREAD) {
        Serial.println("\nerror: exceeded MAXREAD bytes!");
        beeper(5, LONG_BEEP);
        blink(BOTH_LEDS, 1000, 5);
        break;
      }
    } // for()

    Serial.println(tag);

    // Append to SD!
    logEvent(tag);

    // Guarantee that no data remains in the serial Tx/Rx buffers
    Serial.flush(); // Tx flush
    while (Serial.available() > 0) // Rx flush
      Serial.read();
  }

  Serial.flush();
  delay(100);
  tcount=0;
  memset(tag,0,MAXREAD);
}

/* End sketch. */
