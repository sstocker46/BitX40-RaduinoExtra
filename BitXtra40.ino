#include <Wire.h>
#include <si5351.h>
#include "UpsideDown.h"

// Comment this line out if you want 'traditional' tuning and no scan option
#define SHUTTLE_TUNE

/**
 * This source file is under General Public License version 3.
 * 
 * Most source code are meant to be understood by the compilers and the computers. 
 * Code that has to be hackable needs to be well understood and properly documented. 
 * Donald Knuth coined the term Literate Programming to indicate code that is written be 
 * easily read and understood.
 * 
 * The Raduino is a small board that includes the Arduin Nano, a 16x2 LCD display and
 * an Si5351a frequency synthesizer. This board is manufactured by Paradigm Ecomm Pvt Ltd
 * 
 * To learn more about Arduino you may visit www.arduino.cc. 
 * 
 * The Arduino works by firt executing the code in a function called setup() and then it 
 * repeatedly keeps calling loop() forever. All the initialization code is kept in setup()
 * and code to continuously sense the tuning knob, the function button, transmit/receive,
 * etc is all in the loop() function. If you wish to study the code top down, then scroll
 * to the bottom of this file and read your way up.
 * 
 * Below are the libraries to be included for building the Raduino 
 * 
 * The EEPROM library is used to store settings like the frequency memory, caliberation data, 
 * callsign etc .
 */

#include <EEPROM.h>

/** 
 *  The main chip which generates upto three oscillators of various frequencies in the
 *  Raduino is the Si5351a. To learn more about Si5351a you can download the datasheet 
 *  from www.silabs.com although, strictly speaking it is not a requirment to understand this code. 
 *  Instead, you can look up the Si5351 library written by Jason Mildrum, NT7S. You can download and 
 *  install it from https://github.com/etherkit/Si5351Arduino to complile this file.
 *  The Wire.h library is used to talk to the Si5351 and we also declare an instance of 
 *  Si5351 object to control the clocks.
 */

Si5351 si5351;

/** 
 * The Raduino board is the size of a standard 16x2 LCD panel. It has three connectors:
 * 
 * First, is an 8 pin connector that provides +5v, GND and six analog input pins that can also be 
 * configured to be used as digital input or output pins. These are referred to as A0,A1,A2,
 * A3,A6 and A7 pins. The A4 and A5 pins are missing from this connector as they are used to 
 * talk to the Si5351 over I2C protocol. 
 * 
 *  A0     A1   A2   A3    +5v    GND   A6   A7
 * BLACK BROWN RED ORANGE YELLOW GREEN BLUE VIOLET
 * 
 * Second is a 16 pin LCD connector. This connector is meant specifically for the standard 16x2
 * LCD display in 4 bit mode. The 4 bit mode requires 4 data lines and two control lines to work:
 * Lines used are : RESET, ENABLE, D4, D5, D6, D7 
 * We include the library and declare the configuration of the LCD panel too
 */

#include <LiquidCrystal.h>
LiquidCrystal lcd(8,9,10,11,12,13);

/**
 * The Arduino, unlike C/C++ on a regular computer with gigabytes of RAM, has very little memory.
 * We have to be very careful with variables that are declared inside the functions as they are 
 * created in a memory region called the stack. The stack has just a few bytes of space on the Arduino
 * if you declare large strings inside functions, they can easily exceed the capacity of the stack
 * and mess up your programs. 
 * We circumvent this by declaring a few global buffers as  kitchen counters where we can 
 * slice and dice our strings. These strings are mostly used to control the display or handle
 * the input and output from the USB port. We must keep a count of the bytes used while reading
 * the serial port as we can easily run out of buffer space. This is done in the serial_in_count variable.
 */
char serial_in[32], c[30], b[30], printBuff[32];
int count = 0;
int sm;  // Signal meter
int s_level;
volatile int peak_sm = 0;
int last_sm;
int wpm = 18; // Read from a pot on Don's setup
int p ;         //Timing period (milliseconds) for keyer function
int offset=750; // CW offset
int started_cw_tone = 0;
volatile int cw_on = 0;
volatile int tuning_lock = 0;
volatile unsigned long last_interrupt_time_digital = 0L;
volatile unsigned int need_update = 0;
volatile int scan = 0;

unsigned char serial_in_count = 0;


/**
 * From circuit diagram http://www.hfsigs.com/bitx40v3_circuit.html
 * 
 */

// P1 = Controls Connector
#define P1_1  (A7)    // Violet    // Tuning pot (INPUT)
#define P1_2  (A6)    // Blue      // CW speed pot (INPUT)
//      P1_3  +5v     // Yellow
//      P1_4  GND     // Green
#define P1_5  (A3)    // Orange    // Function Button 1 (INPUT)
#define P1_6  (A2)    // Red       // S-Meter (INPUT) - originally Calibration button (not present)
#define P1_7  (A1)    // Brown
#define P1_8  (A0)    // Black 

// P3 = Oscillators
#define P3_1   (7)  // New wire    // TX_RX control (OUTPUT) - puts radio into transmit mode
#define P3_2   (6)  // New wire    // CW_TONE (OUTPUT) - 700Hz audio tone for speaker
#define P3_3   (5)  // New wire    // CW_KEY_DOT  (INPUT) - either from iambic or straight key
#define P3_4   (4)  // New wire    // CW_KEY_DASH (INPUT) - from iambic key
#define P3_5   (3)  // New wire    // Function Button 2 (INPUT)
#define P3_6   (2)
//      P3_8  CLK0
//      P3_11 CLK1  // New wire    // CW Carrier
//      P3_12 CLK2  // Existing    // SSB Carrier

/**
 * We need to carefully pick assignment of pin for various purposes.
 * There are two sets of completely programmable pins on the Raduino.
 * First, on the top of the board, in line with the LCD connector is an 8-pin connector
 * that is largely meant for analog inputs and front-panel control. It has a regulated 5v output,
 * ground and six pins. Each of these six pins can be individually programmed 
 * either as an analog input, a digital input or a digital output. 
 * The pins are assigned as follows: 
 *        A0,   A1,  A2,  A3,   +5v,   GND,  A6,  A7 
 *      BLACK BROWN RED ORANGE YELLW  GREEN  BLUEVIOLET
 *      (while holding the board up so that back of the board faces you)
 *      
 * Though, this can be assigned anyway, for this application of the Arduino, we will make the following
 * assignment
 * A2 will connect to the PTT line, which is the usually a part of the mic connector
 * A3 is connected to a push button that can momentarily ground this line. This will be used to switch between different modes, etc.
 * A6 is to implement a keyer, it is reserved and not yet implemented
 * A7 is connected to a center pin of good quality 100K or 10K linear potentiometer with the two other ends connected to
 * ground and +5v lines available on the connector. This implments the tuning mechanism
 */


//#define ANALOG_KEYER (A1)
#define SMETER (A2)
#define FBUTTON_1 (A3)
#define CW_SPEED   (A6)
#define ANALOG_TUNING (A7)
#define SCAN_LEVEL 1

/** 
 *  The second set of 16 pins on the bottom connector are have the three clock outputs and the digital lines to control the rig.
 *  This assignment is as follows :
 *    Pin   1   2    3    4    5    6    7    8    9    10   11   12   13   14   15   16
 *         +5V +5V CLK0  GND  GND  CLK1 GND  GND  CLK2  GND  D2   D3   D4   D5   D6   D7  
 *  These too are flexible with what you may do with them, for the Raduino, we use them to :
 *  - TX_RX line : Switches between Transmit and Receive after sensing the PTT or the morse keyer
 *  - CW_KEY line : turns on the carrier for CW
 *  These are not used at the moment.
 */

#define TX_RX   P3_1
#define CW_TONE P3_2

// Adopted for Iambic keyer, just uses CW_KEY_DOT for straight key contact
#define CW_KEY_DOT  P3_3  
#define CW_KEY_DASH P3_4
#define FBUTTON_2   P3_5

#define CAL_BUTTON FBUTTON_2  // for now


/**
 *  The raduino has a number of timing parameters, all specified in milliseconds 
 *  CW_TIMEOUT : how many milliseconds between consecutive keyup and keydowns before switching back to receive?
 *  The next set of three parameters determine what is a tap, a double tap and a hold time for the funciton button
 *  TAP_DOWN_MILLIS : upper limit of how long a tap can be to be considered as a button_tap
 *  TAP_UP_MILLIS : upper limit of how long a gap can be between two taps of a button_double_tap
 *  TAP_HOLD_MILIS : many milliseconds of the buttonb being down before considering it to be a button_hold
 */
 
#define TAP_UP_MILLIS (500)
#define TAP_DOWN_MILLIS (600)
#define TAP_HOLD_MILLIS (2000)

/**
 * The Raduino supports two VFOs : A and B and receiver incremental tuning (RIT). 
 * we define a variables to hold the frequency of the two VFOs, RITs
 * the rit offset as well as status of the RIT
 * 
 * To use this facility, wire up push button on A3 line of the control connector
 */
#define VFO_A 0
#define VFO_B 1
char ritOn = 0;
char vfoActive = VFO_A;
unsigned long vfoA=7100000L, vfoB=14200000L, ritA, ritB, sideTone=800;

/**
 * Raduino needs to keep track of current state of the transceiver. These are a few variables that do it
 */

char inTx = 0;
char keyDown = 0;
char isUSB = 0;
unsigned char txFilter = 0;

/** Tuning Mechanism of the Raduino
 *  We use a linear pot that has two ends connected to +5 and the ground. the middle wiper
 *  is connected to ANALOG_TUNNING pin. Depending upon the position of the wiper, the
 *  reading can be anywhere from 0 to 1024.
 *  The tuning control works in steps of 50Hz each for every increment between 50 and 950.
 *  Hence the turning the pot fully from one end to the other will cover 50 x 900 = 45 KHz.
 *  At the two ends, that is, the tuning starts slowly stepping up or down in 10 KHz steps 
 *  To stop the scanning the pot is moved back from the edge. 
 *  To rapidly change from one band to another, you press the function button and then
 *  move the tuning pot. Now, instead of 50 Hz, the tuning is in steps of 50 KHz allowing you
 *  rapidly use it like a 'bandset' control.
 *  To implement this, we fix a 'base frequency' to which we add the offset that the pot 
 *  points to. We also store the previous position to know if we need to wake up and change
 *  the frequency.
 */

#define INIT_BFO_FREQ (1199800L)
unsigned long baseTune =  7100000L;
unsigned long bfo_freq = 11998000L;

int  old_knob = 0;

#define CW_TIMEOUT (750l)
#define LOWEST_FREQ  (7000000l)
#define HIGHEST_FREQ (7210000l)

long frequency = 7100000l;
long stepSize=100000;

/**
 * The raduino can be booted into multiple modes:
 * MODE_NORMAL : works the radio  normally
 * MODE_CALIBRATION : used to calibrate Raduino.
 * To enter this mode, hold the function button down and power up. Tune to exactly 10 MHz on clock0 and release the function button
 */
 #define MODE_NORMAL (0)
 #define MODE_CALIBRATE (1)
 char mode = MODE_NORMAL;

/**
 * Display Routines
 * These two display routines print a line of characters to the upper and lower lines of the 16x2 display
 */

void printLine1(char *c){
  if (strcmp(c, printBuff)){
    lcd.setCursor(0, 0);
    lcd.print(c);
    strcpy(printBuff, c);
    count++;
  }
}

void printLine2(char *c){
  lcd.setCursor(0, 1);
  lcd.print(c);
}

// returns a logarithmic scale from the raw signal voltage on the scale 0 - 19
int logscale(int s)
{
   if (s <= 5) return s;     
   if (s <= 12) return 6;   
   if (s <= 17) return 7;
   if (s <= 23) return 8;
   if (s <= 30) return 9;
   if (s <= 37) return 10;
   if (s <= 46) return 11;
   if (s <= 55) return 12;
   if (s <= 65) return 13;
   if (s <= 77) return 14;
   if (s <= 89) return 15;    
   if (s <= 102) return 16;
   if (s <= 116) return 17; 
   if (s <= 132) return 18;  
   return 19;
}

/**
 * Building upon the previous  two functions, 
 * update Display paints the first line as per current state of the radio
 * 
 * At present, we are not using the second line. YOu could add a CW decoder or SWR/Signal strength
 * indicator
 */

void updateFrequency()
{

    printFrequency(c, frequency);     

#ifdef UPSIDE_DOWN
    lcd.setCursor(0,1);
#else
    lcd.setCursor(8,0);
#endif
    lcd.print(c);

#ifdef UPSIDE_DOWN
    lcd.setCursor(13,1);
#else
    lcd.setCursor(0,0);
#endif

    lcd.print("(");
    if (cw_on) lcd.print((char) 0xff);
    else if (scan) lcd.print("S");
    else if (tuning_lock) lcd.print("+");
    else lcd.print(" ");
    lcd.print(")");
}

void updateSMeter()
{
    // Signal meter
    
    int i;
    int smi = sm / 2;
    int smp = peak_sm / 2;  
    printSignal(c, s_level);
    
#ifdef UPSIDE_DOWN
    lcd.setCursor(0,0);
    lcd.print("[");
    for (i=9; i>=smi+1; i--) 
    {
       if (i == smp) lcd.print("|");
       else lcd.print(" ");
    }
    if (sm%2==1) lcd.print((char) 0x6);
    else lcd.print(" ");
    for (i=0; i<smi; i++) lcd.print((char) 0x0);
    lcd.print("]");
    lcd.print(" ");
    lcd.print(c);
#else
    lcd.setCursor(0,1);
    lcd.print(c);
    lcd.print(" ");
    lcd.print("[");
    for (i=0; i<smi; i++) lcd.print((char) 0x0);
    if (sm%2==1) lcd.print((char) 0x6);
    else lcd.print(" ");
    for (i=smi+1; i<10; i++) 
    {
       if (i == smp) lcd.print("|");
       else lcd.print(" ");
    }
    lcd.print("]");
#endif
}

void updateDisplay()
{
   updateFrequency();
   updateSMeter();
}

// Install Pin change interrupt for a pin, can be called multiple times
 
void pciSetup(byte pin)
{
    *digitalPinToPCMSK(pin) |= bit (digitalPinToPCMSKbit(pin));  // enable pin
    PCIFR  |= bit (digitalPinToPCICRbit(pin)); // clear any outstanding interrupt
    PCICR  |= bit (digitalPinToPCICRbit(pin)); // enable interrupt for the group
}


// Interrupt service routine -- Digital Pins
ISR (PCINT2_vect) // handle pin change interrupt for D0 to D7 here
{
    unsigned long now = millis();
    
    // Function Button 2 is tuning lock - toggle
    if ((digitalRead(FBUTTON_2) == LOW) && ((now - last_interrupt_time_digital) > 50L))
    {
#ifdef SHUTTLE_TUNE
       if (scan) scan = 0;
       else scan = 1;
#else
       if (tuning_lock) tuning_lock = 0;
       else tuning_lock = 1;
#endif
       peak_sm = 0;
       last_interrupt_time_digital = now;
       need_update = 1;
    }
}  


// Interrupt service routine -- Analogue Pins
ISR (PCINT1_vect) // handle pin change interrupt for A0 to A5 here
{
    //Serial.println("Interrupt Service Routine - analogue\n");
    need_update = 1;
}  

 
/**
 * To use calibration sets the accurate readout of the tuned frequency
 * To calibrate, follow these steps:
 * 1. Tune in a signal that is at a known frequency.
 * 2. Now, set the display to show the correct frequency, 
 *    the signal will no longer be tuned up properly
 * 3. Press the CAL_BUTTON line to the ground
 * 4. tune in the signal until it sounds proper.
 * 5. Release CAL_BUTTON
 * In step 4, when we say 'sounds proper' then, for a CW signal/carrier it means zero-beat 
 * and for SSB it is the most natural sounding setting.
 * 
 * Calibration is an offset that is added to the VFO frequency by the Si5351 library.
 * We store it in the EEPROM's first four bytes and read it in setup() when the Radiuno is powered up
 */
void calibrate(){
    int32_t cal;

    // The tuning knob gives readings from 0 to 1000
    // Each step is taken as 10 Hz and the mid setting of the knob is taken as zero
    cal = (analogRead(ANALOG_TUNING) * 10)-5000;

    // if the button is released, we save the setting
    // and delay anything else by 5 seconds to debounce the CAL_BUTTON
    // Debounce : it is the rapid on/off signals that happen on a mechanical switch
    // when you change it's state
    if (digitalRead(CAL_BUTTON) == HIGH){
      mode = MODE_NORMAL;
      printLine1("Calibrated      ");

      //scale the caliberation variable to 10 MHz
      cal = (cal * 10000000l) / frequency;
      //Write the 4 bytes into the eeprom memory.
      EEPROM.write(0, (cal & 0xFF));
      EEPROM.write(1, ((cal >> 8) & 0xFF));
      EEPROM.write(2, ((cal >> 16) & 0xFF));
      EEPROM.write(3, ((cal >> 24) & 0xFF));
      printLine2("Saved.    ");
      delay(5000);
    }
    else {
      // while the calibration is in progress (CAL_BUTTON is held down), keep tweaking the
      // frequency as read out by the knob, display the chnage in the second line
      si5351.set_freq((bfo_freq + cal - frequency) * 100LL, SI5351_CLK2); 
      sprintf(c, "offset:%d ", cal);
      printLine2(c);
    }  
}


/**
 * The setFrequency is a little tricky routine, it works differently for USB and LSB
 * 
 * The BITX BFO is permanently set to lower sideband, (that is, the crystal frequency 
 * is on the higher side slope of the crystal filter).
 * 
 * LSB: The VFO frequency is subtracted from the BFO. Suppose the BFO is set to exactly 12 MHz
 * and the VFO is at 5 MHz. The output will be at 12.000 - 5.000  = 7.000 MHz
 * 
 * USB: The BFO is subtracted from the VFO. Makes the LSB signal of the BITX come out as USB!!
 * Here is how it will work:
 * Consider that you want to transmit on 14.000 MHz and you have the BFO at 12.000 MHz. We set
 * the VFO to 26.000 MHz. Hence, 26.000 - 12.000 = 14.000 MHz. Now, consider you are whistling a tone
 * of 1 KHz. As the BITX BFO is set to produce LSB, the output from the crystal filter will be 11.999 MHz.
 * With the VFO still at 26.000, the 14 Mhz output will now be 26.000 - 11.999 = 14.001, hence, as the
 * frequencies of your voice go down at the IF, the RF frequencies will go up!
 * 
 * Thus, setting the VFO on either side of the BFO will flip between the USB and LSB signals.
 */

void setFrequency(unsigned long f){
  uint64_t osc_f;
  
  if (isUSB){
    si5351.set_freq((bfo_freq + f) * 100ULL, SI5351_CLK2);
  }
  else{
    si5351.set_freq((bfo_freq - f) * 100ULL, SI5351_CLK2);
  }
  
  frequency = f;
}


/**
 * A trivial function to wrap around the function button
 */
int btnDown(){
  if (digitalRead(FBUTTON_1) == HIGH)
    return 0;
  else
    return 1;
}

/**
 * A click on the function button toggles the RIT
 * A double click switches between A and B vfos
 * A long press copies both the VFOs to the same frequency
 */
void checkButton(){
  int i, t1, t2, knob, new_knob, duration;

  //rest of this function is interesting only if the button is pressed
  if (!btnDown())
    return;

  // we are at this point because we detected that the button was indeed pressed!
  // we wait for a while and confirm it again so we can be sure it is not some noise
  delay(50);
  if (!btnDown())
    return;
    
  // note the time of the button going down and where the tuning knob was
  t1 = millis();
  knob = analogRead(ANALOG_TUNING);
  duration = 0;
  
  // if you move the tuning knob within 3 seconds (3000 milliseconds) of pushing the button down
  // then consider it to be a coarse tuning where you you can move by 100 Khz in each step
  // This is useful only for multiband operation.
  while (btnDown() && duration < 3000){
  
      new_knob = analogRead(ANALOG_TUNING);
      //has the tuninng knob moved while the button was down from its initial position?
      if (abs(new_knob - knob) > 10){
        int count = 0;
        /* track the tuning and return */
        while (btnDown()){
          frequency = baseTune = ((analogRead(ANALOG_TUNING) * 30000l) + 1000000l);
          setFrequency(frequency);
          updateDisplay();
          count++;
          delay(200);
        }
        delay(1000);
        return;
      } /* end of handling the bandset */
      
     delay(100);
     duration += 100;
  }

  //we reach here only upon the button being released

  // if the button has been down for more thn TAP_HOLD_MILLIS, we consider it a long press
  // set both VFOs to the same frequency, update the display and be done
  if (duration > TAP_HOLD_MILLIS){
    printLine2("VFOs reset!");
    vfoA= vfoB = frequency;
    delay(300);
    updateDisplay();
    return;    
  }

  // t1 holds the duration for the first press
  t1 = duration;
  //now wait for another click
  delay(100);
  // if there a second button press, toggle the VFOs
  if (btnDown()){

    //swap the VFOs on double tap
    if (vfoActive == VFO_B){
      vfoActive = VFO_A;
      vfoB = frequency;
      frequency = vfoA;
    }
    else{
      vfoActive = VFO_B;
      vfoA = frequency;
      frequency = vfoB;
    }
     //printLine2("VFO swap! ");
     delay(600);
     updateDisplay();
    
  }
  // No, there was not more taps
  else {
    //on a single tap, toggle the RIT
    if (ritOn)
      ritOn = 0;
    else
      ritOn = 1;
    updateDisplay();
  }    
}

/**
 * The Tuning mechansim of the Raduino works in a very innovative way. It uses a tuning potentiometer.
 * The tuning potentiometer that a voltage between 0 and 5 volts at ANALOG_TUNING pin of the control connector.
 * This is read as a value between 0 and 1000. Hence, the tuning pot gives you 1000 steps from one end to 
 * the other end of its rotation. Each step is 50 Hz, thus giving approximately 50 Khz of tuning range.
 * When the potentiometer is moved to either end of the range, the frequency starts automatically moving
 * up or down in 10 Khz increments
 */


#ifdef SHUTTLE_TUNE
void doShuttleTuning()
{
    int knob = analogRead(ANALOG_TUNING);
    long new_freq;

    if ((knob > 560) && (frequency < HIGHEST_FREQ)) new_freq = frequency + (pow((knob - 560)/5,3)/100);
    else if ((knob < 464) && ( frequency > LOWEST_FREQ)) new_freq = frequency - (pow((464 - knob)/5,3)/100);
    else return;

    if ((new_freq != frequency) && ((new_freq / 10) != (frequency / 10)))
    {
       setFrequency(new_freq);
       updateDisplay();
    }
}
#else
void doTuning(){
 unsigned long newFreq;
 
 int knob = analogRead(ANALOG_TUNING)-10;
 unsigned long old_freq = frequency;

  // the knob is fully on the low end, move down by 10 Khz and wait for 200 msec
 if (knob < 10 && frequency > LOWEST_FREQ) {
      baseTune = baseTune - 10000l;
      frequency = baseTune;
      updateDisplay();
      setFrequency(frequency);
      delay(200);
  } 
  // the knob is full on the high end, move up by 10 Khz and wait for 200 msec
  else if (knob > 1010 && frequency < HIGHEST_FREQ) {
     baseTune = baseTune + 10000l; 
     frequency = baseTune + 50000l;
     setFrequency(frequency);
     updateDisplay();
     delay(200);
  }
  // the tuning knob is at neither extremities, tune the signals as usual
  else if (knob != old_knob){
     frequency = baseTune + (50l * knob);
     old_knob = knob;
     setFrequency(frequency);
     updateDisplay();
  }
}
#endif


/**
 * setup is called on boot up
 * It setups up the modes for various pins as inputs or outputs
 * initiliaizes the Si5351 and sets various variables to initial state
 * 
 * Just in case the LCD display doesn't work well, the debug log is dumped on the serial monitor
 * Choose Serial Monitor from Arduino IDE's Tools menu to see the Serial.print messages
 */

 
void setup()
{
  int32_t cal;

  setupUpsideDown(&lcd);
  
  lcd.begin(16, 2);
  lcd.clear();
  printBuff[0] = 0;
    
  // Start serial and initialize the Si5351
  Serial.begin(9600);
  analogReference(DEFAULT);
  Serial.println("*Raduino booting up\nv0.01\n");

  //configure the function button to use the external pull-up
  pinMode(FBUTTON_1, INPUT);
  digitalWrite(FBUTTON_1, HIGH);
  pciSetup(FBUTTON_1);
  pciSetup(SMETER);
  pciSetup(ANALOG_TUNING);

  pinMode(CW_KEY_DOT, INPUT_PULLUP);
  pinMode(CW_KEY_DASH, INPUT_PULLUP); 
  pinMode(FBUTTON_2, INPUT_PULLUP);
  
  // Setup interrupt handler for the buttons
  //pciSetup(CW_KEY_DOT);
  //pciSetup(CW_KEY_DASH);
  pciSetup(FBUTTON_2);
      
  pinMode(CW_TONE, OUTPUT);
  pinMode(TX_RX, OUTPUT);
    
  digitalWrite(CW_TONE, 0);
  digitalWrite(TX_RX, LOW);
  delay(500);

  /* Etherkit library */
  
  si5351.init(SI5351_CRYSTAL_LOAD_8PF, 25000000l, 0);
  Serial.println("*Initiliazed Si5351\n");
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLA);
  si5351.set_pll(SI5351_PLL_FIXED, SI5351_PLLB);
  Serial.println("*Fixed PLL\n");  
  si5351.drive_strength(SI5351_CLK2, SI5351_DRIVE_2MA);
  //si5351.drive_strength(SI5351_CLK1, SI5351_DRIVE_2MA);
  si5351.output_enable(SI5351_CLK0, 0);
  si5351.output_enable(SI5351_CLK1, 0);
  si5351.output_enable(SI5351_CLK2, 1);
  Serial.println("*Output enabled PLL\n");
  si5351.set_freq(500000000l, SI5351_CLK2);   
  
  
  Serial.println("*Si5350 ON\n");       
  mode = MODE_NORMAL;
  delay(10);
  updateDisplay();
}

void sk() {  //Straight Key mode

  digitalWrite(TX_RX, HIGH);
  
  while (count < 500)  // Delay time after last action to return to normal SSB
  {
     while(digitalRead(CW_KEY_DOT)==LOW)
     {
        if (!started_cw_tone) 
        {
           si5351.set_freq((frequency-offset) * 100ULL , SI5351_CLK1); //Key down
           tone(CW_TONE,offset); //Sidetone  
           started_cw_tone = 1;
        }
        delay(2);   
        count=0; //Reset counter
    }

    if (started_cw_tone) 
    {
       si5351.output_enable(SI5351_CLK1, 0); // Unkey transmit
       started_cw_tone = 0;
       noTone(CW_TONE); 
    }
    delay(2);
    count++; 
  }
  count=0; //Reset counter
}

void loop()
{
   //unsigned long now = millis();
   
// Disable Calibration for now
/*
   if (digitalRead(CAL_BUTTON) == LOW && mode == MODE_NORMAL){
    mode = MODE_CALIBRATE;    
    si5351.set_correction(0);
    printLine1("Calibrating: Set");
    printLine2("to zerobeat.    ");
    delay(2000);
    return;
  }
  else if (mode == MODE_CALIBRATE){
    calibrate();
    delay(50);
    return;
  }
  */

  
  if (digitalRead(CW_KEY_DOT) == LOW) 
  { 
    cw_on = 1;
    started_cw_tone = 0;
    updateDisplay();
    sk();
  }
  if (cw_on)
  {
     digitalWrite(TX_RX, LOW); // Restore T/R relays from CW mode
     cw_on = 0;
  }
#ifdef SHUTTLE_TUNE
  if (scan)
  {
     if (frequency < HIGHEST_FREQ) setFrequency(frequency + 100);
     else scan = 0;
  }
  else doShuttleTuning();
  
#else
  if (!tuning_lock) doTuning();
#endif

  // Read signal level
  int sma = analogRead(SMETER);
  sm = logscale(sma); 
  if (sma < 10) s_level = sm;
  else if (sma < 80) s_level = 9;
  else if (sma < 110) s_level = 10;
  else s_level = 20;
   
  if ((scan) && (sm >= SCAN_LEVEL)) 
  {
     scan = 0;
     need_update = 1;
  }
  if (sm > peak_sm) peak_sm = sm;
  
  if (need_update)
  {
     updateDisplay();
     need_update = 0;
  }
  else if (sm != last_sm) updateSMeter();

  last_sm = sm;
  
  delay(50); 
}


