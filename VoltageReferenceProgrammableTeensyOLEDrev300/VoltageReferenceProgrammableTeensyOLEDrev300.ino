/* Programmable Voltage Reference V0.8
For use with Barbouri - Programmable Voltage Reference V2.12 - V3.05 circuit boards
2014 uChip (C. Schnarel) Original software for RevD boards
2017 Barbouri (G. Christenson) Modified for Teensy 3.2 based board with I2C LED display
http://www.barbouri.com/2016/07/21/programmable-voltage-reference-v2-12-assembly/
CC BY-SA 4.0 US
12 September 2017 added +/-50mv buttons on pins 21, 22
   added Mode reset switch on pin 20 for CORRECTED mode and 1.000V
   added Power switch on pin 23 to disable LTC1152 and place it in tri-state mode,
   clear LED display (blank), and set LEDcolor to black. Teensy and references
   remain powered. After switching power switch on, Mode reset must be pressed.
   added averaging for Teensy A0 input voltage monitoring, and additional calibration
   variables to compensate for Teensy A-D errors

Modified for V2.25 board - LTC1152 enable, onboard -5 supply, serial & button headers
Include libraries this sketch will use
*/
#include <SPI.h>                  // part of Arduino - Teensyduino distribution
#include <EEPROM.h>               // part of Arduino - Teensyduino distribution
#include <JC_EEPROM.h>            // https://github.com/JChristensen/JC_EEPROM
#include <Rotary.h>               // https://github.com/brianlow/Rotary
#include <Bounce2.h>              // http://playground.arduino.cc/Code/Bounce
#include <Wire.h>                 // part of Arduino - Teensyduino distribution
#include <avr/io.h>
#include <avr/interrupt.h>        // AVR interrupt header
#include "I2cCharDisplay.h"       // https://github.com/dcityorg/i2c-char-display-library-arduino
#include <Adafruit_MCP23017.h>    // Adafruit IO expander wothout display
// #include <Adafruit_RGBLCDShield.h>   // include MODIFIED Adafruit RGB I2C Display to include 6th MENU button
// #include "Adafruit_LEDBackpack.h" // https://github.com/adafruit/Adafruit_LED_Backpack
// #include "Adafruit_GFX.h"         // https://github.com/adafruit/Adafruit-GFX-Library

// Defines
#define OLEDADDRESS    0x3c       // i2c address for the oled display
I2cCharDisplay oled(OLED_TYPE, OLEDADDRESS, 2); // create an oled object for a 2 line display

// Create a set of new symbols that can be displayed on the LCD
byte batt_full[8] = {B01110,B11111,B11111,B11111,B11111,B11111,B11111,B11111};      // full battery, 9.0V
byte batt_8_7[8] = {B00000,B01110,B11111,B11111,B11111,B11111,B11111,B11111};       // 8.7V level
byte batt_8_3[8] = {B00000,B00000,B01110,B11111,B11111,B11111,B11111,B11111};       // 8.3V level
byte batt_8_0[8] = {B00000,B00000,B00000,B01110,B11111,B11111,B11111,B11111};       // 8.0V level
byte batt_7_7[8] = {B00000,B00000,B00000,B00000,B01110,B11111,B11111,B11111};       // 7.7V level
byte batt_7_5[8] = {B00000,B00000,B00000,B00000,B00000,B01110,B11111,B11111};       // 7.5V level
byte batt_empty[8] = {B00100,B00100,B00100,B00000,B00100,B00000,B01110,B11111};     // <7.3V Empty
byte batt_charging[8] = {B00010,B00100,B01000,B11111,B00010,B10100,B11000,B11100};  // >10V Charging symbol

// DAC Slave Select
#define DACSS 10

// Mode values
#define CORRECTED true  // send refVal to DAC after adding correction factor
#define RAW false       // send raw refVal numbers to the DAC w/o correcting

// RGB Rotary encoder LED's
#define redled 4
#define greenled 5
#define blueled 6

// LTC1152 enable output to MAX14930 isolator pin 6 input, pin 11 output to LTC1152
#define outputena 9

// Instantiate Rotary encoder object
Rotary r = Rotary(2, 3);
// Instantiate Bounce objects
Bounce bp = Bounce();
Bounce bu = Bounce();
Bounce bd = Bounce();
Bounce bm = Bounce();

// Setup JC_EEPROM values for 24LC512T IC1
JC_EEPROM eep(JC_EEPROM::kbits_512, 2, 64);      //one 24LC512 EEPROM on the bus - device size, number of devices, page size
  
// Instantiate objects used in this project
uint32_t MAXVAL = 5000;    // DAC Maximum value
uint32_t MINVAL = 1;        // DAC Minimum value

// LED colors used in LEDcolor()
uint8_t red = 0x1;
uint8_t yellow = 0x3;
uint8_t green = 0x2;
uint8_t teal = 0x6;
uint8_t blue = 0x4;
uint8_t violet = 0x5;
uint8_t white = 0x7;
uint8_t black = 0x0;

int DispBCD = 0;
int analog0 = A0;           // Analog input pin on Teensy 3.2
int code = 0;
int color = 0;
uint32_t i = 0;
int8_t offset = 0;
int16_t refVal = 2500;      // Initial voltage output setting 2.500 volt
float num = refVal;
uint32_t Addr = 0;
uint8_t CmdArray[7];
uint8_t CmdArrayIdx = 0;
uint8_t sdata;
uint16_t DACreg;
boolean newVal = true;
boolean mode = CORRECTED;   // when mode = CORRECTED then offset value is used
boolean useFlag = false;    // when true uses downloaded value of offset
// boolean PWR = true;         // Power switch
float errorvoltage = 5.0;   // Maximum difference between set and output voltage
float average = 0.0000;     // the average for A0 read


//-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|
void setup() {
  // Initialize USB interface
  Serial1.begin(115200);
  r.begin(false);
  delay(250);
  Serial1.println("Programmable Voltage Reference V3.00");

  oled.begin();             // initialize the oled

  // Load battery symbols to OLED memory
    oled.createCharacter(0, batt_full);
    oled.createCharacter(1, batt_8_7);
    oled.createCharacter(2, batt_8_3);
    oled.createCharacter(3, batt_8_0);
    oled.createCharacter(4, batt_7_7);
    oled.createCharacter(5, batt_7_5);
    oled.createCharacter(6, batt_empty);
    oled.createCharacter(7, batt_charging);

  // Setup initial OLED display screen
    oled.setBrightness(50);
    oled.clear();
    oled.cursorMove(1, 1);
    oled.print("0.000 0.000 COR");
    oled.cursorMove(1, 16);
    oled.write(7);
    oled.cursorMove(2, 16);
    oled.write(1);
    oled.cursorMove(2, 1);
    oled.print("Rev 3.0 Delay30");
  delay(30000);
    oled.cursorMove(2, 1);
    oled.print("   Rev 3.00    ");
    
  //---- initialize the i2c/MCP23017 library
  Adafruit_MCP23017 mcp;
 
  pinMode(redled, OUTPUT);
  pinMode(greenled, OUTPUT);
  pinMode(blueled, OUTPUT);
  pinMode(outputena, OUTPUT);
  digitalWrite(outputena, HIGH);
  
  LEDcolor(blue);           // Set initial LED color
  mcp.begin();      // use default address 0
  // lcd.setBacklight(BLUE);   // set I2C RGB LED color to Blue

  byte i2cStat = eep.begin(JC_EEPROM::twiClock400kHz);
    if ( i2cStat != 0 ) {
      Serial1.print ("EEPROM setup problem");  //there was a problem
    }

  // Initialize EEPROM to 0's *****************************
  // Run once to set all MAXVAL memory values to 0 by uncommenting
  // the next three lines, and running once on the Teensy 3.2, then recomment.
  // for(i=0;i<MAXVAL;i++){
  //     eep.write(i, 0);  
  //   }

  // Calibrate Teensy ADC to Output value
  // for(i=6000;i<11000;i++){
  //     eep.write(i, 0);  
  //   }

  // Attach buttons with de-bounce using "Bounce2"
  bp.attach(8, INPUT);
  bp.interval(5);
  bu.attach(22,INPUT_PULLUP);
  bu.interval(5);
  bd.attach(21, INPUT_PULLUP);
  bd.interval(5);
  bm.attach(23, INPUT_PULLUP);
  bm.interval(5);
  
  bp.update();          // Check if Rotary encoder button is held On, on power-up
  if(bp.read() == HIGH) {
    mode = RAW;             // If button is held-On set RAW mode
    LEDcolor(red);
    }
    else {
    mode = CORRECTED;       // If button is Off set normal CORRECTED mode
    LEDcolor(blue);
    }
 
  // Setup A-D converter on Teensy 3.2 to use external 2.048 reference and 14 bits
  analogReference(EXTERNAL);
  analogReadResolution(14);
  analogReadAveraging(32);
  
  // set up the select control pin for the DAC
  // DAC select is active low, data is shifted in MSB first
  // on the falling edge of the clock
  pinMode(DACSS, OUTPUT);
  digitalWrite(DACSS, HIGH);
  SPI.begin();
  SPI.setDataMode(SPI_MODE1);  // set to falling edge clock
  SPI.setBitOrder(MSBFIRST);  // set to MSB first
}

// =-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|
void loop() {
  
  // Increment by 500 if the rotary encoder button was pressed
  // If the increment result is more than MAX just set result to zero
  if(bp.update()) {
    if(bp.read() == HIGH) {
      refVal+=250;
      if(refVal>MAXVAL)refVal=1;
      newVal = true;
    }
  }

  // Increment by 50 if the panel 50mv + button was pressed
  // If the increment result is more than MAX just set result to zero
  if(bu.update()) {
    if(bu.read() == HIGH) {
      refVal+=25;
      if(refVal>MAXVAL)refVal=1;
      newVal = true;
    }
  }
  
  // Decrement by 50 if the panel 50mv - button was pressed
  // If the decrement result is less than MIN just set result to zero
  if(bd.update()) {
    if(bd.read() == HIGH) {
      refVal-=25;
      if(refVal<MINVAL)refVal=1;
      newVal = true;
    }
  }

  // Reset to CORRECTED mode if the panel Mode - button was pressed
  // Set Output to 1.000 V and enable LTC1152 output
  
  if(bm.update()) {
    if(bm.read() == HIGH) {
    mode = CORRECTED;
    LEDcolor(blue);
    refVal = 2500;
    newVal = true;
    }
  }

  // Check the rotary encoder and increment or decrement if it was turned
  // If the increment result is more than MAX roll over to zero
  // If the decrement goes below zero value will roll over to 65535 (unsigned word)
  // In that case set to result to MAX value
  unsigned char result = r.process();
  if (result) {
    result == DIR_CW ? refVal-- : refVal++;
    if(refVal<1)refVal=MAXVAL;
    if(refVal>MAXVAL)refVal=1;
    newVal = true;
  }

  while (Serial1.available())  // Read command from Teensy USB connection
                              // requires mode character, 4 numeric digits, then
                              // LineFeed or CarrigeReturn or both
                              // valid mode charactors: # ! W R U N O C
  {
    sdata = Serial1.read();
    switch (sdata){
    case 35: // '#'  set DAC with correction factor
    case 33: // '!'  set DAC with no correction factor
    case 'W': // 'W' write offset values to EEPROM
    case 'R': // 'R' dump the offset table (mostly for debug)
    case 'U': // 'U' use a temporary offset (must be in CORRECTED mode to use)
    case 'N': // 'N' clear temporary offset and use correction factor
    case 'O': // 'O' turn Off OLED and set DAC with correction factor
    case 'C': // 'C' set calibration mode with no correction factor
      CmdArrayIdx = 0;
      CmdArray[CmdArrayIdx++] = sdata;
      break;
    case 10: // LF
    case 13: // CR
      CmdArray[CmdArrayIdx++] = sdata;
      if(ParseCommand())
        newVal = true;
      break;
    default:
      CmdArray[CmdArrayIdx++] = sdata;
      if(CmdArrayIdx>6) CmdArrayIdx = 6;
    }
  }  

  // If the value has changed, update the display and send 
  // the new value to the DAC and the serial port
  if(newVal) {
    Serial1.print("Set voltage = ");
    Serial1.print((float)refVal/1000, 3);
        
    // Convert the 12-bit DAC value into a 16-bit value plus
    // an optional signed 8-bit correction factor.
    
    // First look up the correction factor
    if(!useFlag){
      offset = eep.read(refVal);
    }
    
    // Next shift the 12-bit value up 4 bits and add in the correction factor if desired.
    if(mode == CORRECTED) {
     Serial1.print(" Offset = ");
     Serial1.print(offset);
     oled.cursorMove(2, 1);
     oled.print("   ");        // clear offset on line 2
     oled.cursorMove(2, 1);
     oled.print(offset);       // Display offset on line 2
     DACreg = ((refVal<<4) * 0.8192) + offset;
    }
    else {
      DACreg = ((refVal<<4) * 0.8192);
    }

    // Now write the 16-bit value out to the DAC
    digitalWrite(DACSS, LOW);
    SPI.transfer((uint8_t) 0);
    SPI.transfer((uint8_t)((DACreg>>8) & 0xFF));
    SPI.transfer((uint8_t)(DACreg & 0xFF));
    digitalWrite(DACSS, HIGH);

    
    
  // *****-----*****-----*****-----*****
    // Print calculated output voltage from Teensy analog input A0
    // Check if output voltage is out of tolerance based on
    // errorvoltage variable and set status LED violet if out
    // of tolerance. The Teensy ADC is not as accurate as precision
    // DAC so this is only a check (~ +/- 0.5%)errorvoltage setting. 
    // Error status must be reset by switching to a mode! or using
    // the front pannel Mode reset switch
    
    delay(75);          // Let the output settle before reading
    average = 0.0000;   // A0 input averaging
      for (int i=0; i < 64; i++) {
        average = average + analogRead(analog0);
      }
    code = average/64;
    
    // include Teensy A-D error correction variables, Set for each Teensy
    Serial1.print(" Output voltage = ");
    Serial1.print((code * 0.0003057)-0.0026, 4);  //0.0003051 0.000 base value
    Serial1.print(" +/- ");
    Serial1.print((errorvoltage-1), 0);
    Serial1.print(" mV ");
    Serial1.println(refVal);

    oled.cursorMove(1, 7);
    oled.print((code * 0.0003057)-0.0026, 3);
    
    if ((refVal - ((code * 0.3057)-2.6)) > errorvoltage) {
      LEDcolor(violet);               //0.3051 base value
    }
    if ((((code * 0.3057)-2.6) - refVal) > errorvoltage) {
      LEDcolor(violet);               //0.3051 base value
    }
  // *****-----*****-----*****-----*****
    
    newVal = false;  // set the flag to false again after updating the DAC
  }
  
  // Convert the binary reference value into decimal digits for display

  num = refVal;
  oled.cursorMove(1, 1);
    oled.print((num / 1000), 3);       // Set decimal point
}

//-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|=-=|
// Takes the data from the array and generates a new value for the Reference
boolean ParseCommand() {

  uint8_t i = 0;
  uint16_t j = 0;
  boolean retVal = false;

  if(CmdArray[5] != 10 && CmdArray[5] != 13) {
    // Error in the command array.  Clear it and start over.
    for(i=0;i<7;i++)CmdArray[i] = 0;
    CmdArrayIdx = 0;
    return false;
  }
  
  for(i=1;i<5;i++){
    j = j*10 + (CmdArray[i]-48); // '0'==48
  }
  if(j>MAXVAL) j = MAXVAL;
   Serial1.println(j);

  switch (CmdArray[0]) {
    case 33: // '!'
      mode = RAW;
      refVal = j;
      retVal = true;
      LEDcolor(red);
      oled.displayOn();
      oled.cursorMove(1, 13);
      oled.print("RAW");
      break;
    case 35: // '#'
      mode = CORRECTED;
      refVal = j;
      retVal = true;
      LEDcolor(blue);
      oled.displayOn();
      oled.cursorMove(1, 13);
      oled.print("COR");
      break;
    case 'W':
      if(mode == RAW) {
        eep.write(refVal, (byte)j);
        Serial1.print(refVal);  // Serial.print(refVal);
        Serial1.print(":");  // Serial.print(":");
        Serial1.println((byte)j);  // Serial.println((byte)j);
      }
      retVal = false;
      break;
    case 'U':
      offset = (byte)j;
      useFlag = true;
      retVal = true;
      LEDcolor(green);
      oled.cursorMove(1, 13);
      oled.print("UOS");
      break;
    case 'N':
      mode = CORRECTED;
      useFlag = false;
      retVal = true;
      LEDcolor(blue);
      oled.cursorMove(1, 13);
      oled.print("COR");
      break;
    case 'C':
      mode = RAW;
      retVal = true;
      LEDcolor(teal);
      oled.cursorMove(1, 13);
      oled.print("CAL");
      break;

    case 'O':
      mode = CORRECTED;
      refVal = j;
      retVal = true;
      LEDcolor(blue);
      oled.displayOff();
      break;
      
    case 'R':
      for(i=0;i<(MAXVAL+1);i++){
        Addr = i;
        Serial1.print((Addr), DEC);  // Serial.print memory address
        Serial1.print(":");  // Serial.print : seperator
        Serial1.println(eep.read(Addr), DEC);
        // delay(10);
      }
    default:
      retVal = false;
  }

  for(i=0;i<7;i++)CmdArray[i] = 0;
  CmdArrayIdx = 0;
  return retVal;
}

void LEDcolor(uint8_t color){     // Set the RGB Rotary LED color
  digitalWrite(blueled, ~(color >> 2) & 0x1);
  digitalWrite(greenled, ~(color >> 1) & 0x1);
  digitalWrite(redled, ~color & 0x1);
}
