/// Flopster101 @ https://github.com/Flopster101
///
/// This project implements an Arduino-based FM Radio around the RDA5807 integrated circuit.
/// It also makes use of the TM1637 4-digit display module for displaying frequency, volume and mute information.
/// Control is provided through 4 push buttons and an IR Receiver.
///
/// Open the Serial console with 9600 baud to see debug information.
///
/// Wiring
/// ------ 
/// Arduino port | RDA5807 signal
/// ------------ | ---------------
///         3.3V | VCC
///          GND | GND
///           A5 | SCLK
///           A4 | SDIO
///           D2 | IR Receiver
///           D5 | TM1637 DIO
///           D6 | TM1637 CLK
///           D8 | Button 4
///           D9 | Button 3
///          D10 | Button 2
///          D11 | Button 1
///
/// * Power for certain components is provided through pin headers and external power supplies.
///

#include <Arduino.h>
#include <Wire.h>
#include <radio.h>
#include <RDA5807.h>

// TM1637 stuff
#define CLK 6
#define DIO 5
#include <TM1637TinyDisplay.h>
TM1637TinyDisplay display(CLK, DIO);
uint8_t dots = 0b01000000; // Add dots or colons (depends on display module)

// IR Remote library stuff
#define DECODE_NEC
#define SEND_PWM_BY_TIMER
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 2

/// The default station that will be tuned by this sketch is 103.70 MHz.
#define DEFAULT_STATION 10370
#define DEFAULT_BAND 00   ///< The band that will be tuned by this sketch is US/Europe.

RDA5807 rx; ///< Create an instance of a RDA5807 chip radio

int default_digvol = 7;
char buf [64];
int freq_buffer_int;
String freq_buffer;
bool mute_status = false;

// Buttons
#include <DailyStruggleButton.h>
const int buttonCHUp = 9;
const int buttonCHDown = 8;
const int buttonVolUp = 10;
const int buttonVolDown = 11;
unsigned int longPressTime = 300;
unsigned int longPressTime_mute = 600;
// Initialize buttons
DailyStruggleButton buttonchup;
DailyStruggleButton buttonchdown;
DailyStruggleButton buttonvolup;
DailyStruggleButton buttonvoldown;

// Command queue
String cmdbuf = ""; // Decoded command received
int cmdbuf_int = 0; // Store command converted to integer

/// Setup a FM only radio configuration
/// with some debugging on the Serial port
void setup() {
  // open the Serial port
  Serial.begin(9600);
  Serial.print("\nFM Radio Project by @Flopster101...");
  delay(200);

  // Start IRReceiver on the pin defined by IR_RECEIVE_PIN
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  // Initialize display
  display.setBrightness(BRIGHT_HIGH);
  display.clear();
  display.showString("Radio FM");

  // Initialize buttons
  buttonchup.set(buttonCHUp, CHUPBUTTON, INT_PULL_UP);
  buttonchdown.set(buttonCHDown, CHDOWNBUTTON, INT_PULL_UP);
  buttonvolup.set(buttonVolUp, VOLUPBUTTON, INT_PULL_UP);
  buttonvoldown.set(buttonVolDown, VOLDOWNBUTTON, INT_PULL_UP);
  buttonchup.enableLongPress(longPressTime);
  buttonchdown.enableLongPress(longPressTime);
  buttonvolup.enableLongPress(longPressTime_mute);
  buttonvoldown.enableLongPress(longPressTime_mute);

  // Initialize the Radio 
  rx.setup();
  delay(300);
  //rx.debugEnable();
  rx.setMono(false);
  rx.setMute(false);
  mute_status=0;
  rx.setFrequency(DEFAULT_STATION);
  rx.setBand(DEFAULT_BAND);
  rx.setVolume(default_digvol);  
  delay(300);

  // Show frequency on display when initialization is done.
  showFrequency_display();
} // setup

void translateIR()                  // takes action based on IR code received
{
  switch(IrReceiver.decodedIRData.decodedRawData)
  {
  case 0xF6097708: // Vol down button on remote
  {
    Serial.println("Received! (-)");
    global_VolDown();
  }
    break;

  case 0xFD027708: // Vol up button on remote
  {
    Serial.println("Received! (+)");
    global_VolUp();
  }
    break;
  case 0xDC237708: // Mute button on remote
  {
    global_MuteRadio();
  }
    break;
  case 0xE51A7708: // Exit button on remote
  {
  }
    break;
  case 0xFC037708: // CH UP button on remote
  {
    Serial.println("Received! (CH UP)");
    global_CHUp();
  }
    break;
  case 0xF50A7708: // CH DOWN button on remote
  {
    Serial.println("Received! (CH DOWN)");
    global_CHDown();
  }
    break;
  case 0xFE017708: // Info button on remote.
  {
    print_info();
  }
    break;
  case 0xFA057708: // UP button on remote.
  {
    if (rx.getRealFrequency() != 10800) {
      Serial.print("\n\nAdvancing 10 Hz...\n");
      rx.setFrequency(rx.getRealFrequency() + 10);
      print_info();
      showFrequency_display();
    }
    else {
      rx.setFrequency(8700);
    }
  }
    break;
  case 0xF9067708: // DOWN button on remote.
  {
    if (rx.getRealFrequency() != 8700) {
      Serial.print("\n\nReceding 10 Hz...\n");
      rx.setFrequency(rx.getRealFrequency() - 10);
      print_info();
      showFrequency_display();
    }
    else {
      rx.setFrequency(10800);
      print_info();
      showFrequency_display();
    }
    break;
  }
  case 0xEE117708: // 1
  {
    cmdbuf = cmdbuf + "1";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xED127708: // 2
  {
    cmdbuf = cmdbuf + "2";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xEC137708: // 3
  {
    cmdbuf = cmdbuf + "3";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xEB147708: // 4
  {
    cmdbuf = cmdbuf + "4";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xEA157708: // 5
  {
    cmdbuf = cmdbuf + "5";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xE9167708: // 6
  {
    cmdbuf = cmdbuf + "6";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xE8177708: // 7
  {
    cmdbuf = cmdbuf + "7";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xE7187708: // 8
  {
    cmdbuf = cmdbuf + "8";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xE6197708: // 9
  {
    cmdbuf = cmdbuf + "9";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xEF107708: // 0
  {
    cmdbuf = cmdbuf + "0";
    delay(200);
    showFrequency_counter();
    break;
  }
  case 0xFB047708: // OK
  {
    if (cmdbuf.length() < 3) {
    display.showString("Err");
    delay(500);
    showFrequency_display();
    cmdbuf = "";
    }

    if (cmdbuf.length() >= 3) {
    cmdbuf = cmdbuf + "0";
    Serial.println(cmdbuf);
    cmdbuf_int = cmdbuf.toInt();
    Serial.println(cmdbuf_int);
    rx.setFrequency(cmdbuf_int);
    showFrequency_display();
    delay(200);
    cmdbuf = "";
    }
  }
 }
}

void print_info() {
  Serial.print("\nCurrent Channel: ");
  Serial.print(rx.getRealChannel());
  delay(500);

  Serial.print("\nReal Frequency.: ");
  Serial.print(rx.getRealFrequency());
  
  Serial.print("\nRSSI: ");
  Serial.print(rx.getRssi());
}

void check_mute() {
  if (mute_status == true) {
    Serial.println("Mute: ON");
  }
  else if (mute_status == false) {
    Serial.println("Mute: OFF");
  }
}

void printVol() {
  sprintf(buf,"Current volume: %3d", rx.getVolume());
  Serial.println(buf);
}

void showFrequency_display() {
  freq_buffer = String(rx.getRealFrequency());
  int lastIndex = freq_buffer.length() - 1;
  freq_buffer.remove(lastIndex);
  Serial.println(freq_buffer);
  freq_buffer_int = freq_buffer.toInt();
  display.clear();
  display.showNumber(freq_buffer_int, false, 4, 0);
  delay(100);
}

void showFrequency_counter() {
  cmdbuf_int = cmdbuf.toInt();
  if (cmdbuf.length() > 4) {
    display.showString("Err");
    delay(500);
    showFrequency_display();
    cmdbuf = "";
  }
  else {
    display.clear();
    display.showNumber(cmdbuf_int, false, 4, 0);
    delay(100);
  }
}

// Shared functions for buttons and IR Remote
void global_VolUp() {
  if (rx.getVolume()<15) {
    rx.setVolume(rx.getVolume() + 1);
    display.clear();
    display.showNumberDec(rx.getVolume());
    delay(200);
    showFrequency_display();
  }
  else {
    rx.setVolume(15);
    Serial.println("Volume already at maximum! (15)");
    display.clear();
    display.showNumberDec(rx.getVolume());
    delay(200);
    showFrequency_display();
  }
  if (mute_status == true) {
    rx.setMute(0);
    mute_status=0;
    check_mute();
  }
    printVol();
}

void global_VolDown() {
  if (rx.getVolume() != 0) {
    rx.setVolume(rx.getVolume() - 1);
    display.clear();
    display.showNumberDec(rx.getVolume());
    delay(200);
    showFrequency_display();
  }
  else {
    rx.setVolume(0);
    Serial.println("Volume already at minimum! (0)");
    display.clear();
    display.showNumberDec(rx.getVolume());
    delay(200);
    showFrequency_display();
  }
 if (mute_status == true) {
   rx.setMute(0);
   mute_status=0;
   check_mute();
 }
    printVol();
}

void global_MuteRadio() {
   if (mute_status == false) {
     rx.setMute(1);
     mute_status=1;
     display.clear();
     display.showString("MUTE ON");
   }
   else if (mute_status == true) {
     rx.setMute(0);
     mute_status=0;
     display.clear();
     display.showString("MUTE OFF");
   }

   Serial.println("Current volume:");
   Serial.println(rx.getVolume());
   check_mute();
   delay(300);
   showFrequency_display();
}

void global_CHUp() {
  Serial.print("\nSeeking next station...");
  rx.seek(0, 1, NULL);
  delay(300);
  print_info();
  showFrequency_display();
}

void global_CHDown() {
  Serial.print("\nSeeking previous station...");
  rx.seek(0, 0, NULL);
  delay(300);
  print_info();
  showFrequency_display();
}

void CHUPBUTTON(byte btnStatus) {
  switch (btnStatus) {
    case onRelease:
    Serial.print("\nChannel up Button pressed!");
    global_CHUp();
    delay(100);
    break;
    
    case onLongPress:
    if (rx.getRealFrequency() != 10800) {
      Serial.print("\n\nAdvancing 10 Hz...\n");
      rx.setFrequency(rx.getRealFrequency() + 10);
      print_info();
      showFrequency_display();
    }
    else {
      rx.setFrequency(8700);
      print_info();
      showFrequency_display();
    }
    break;
  }
}

void CHDOWNBUTTON(byte btnStatus) {
  switch (btnStatus) {
    case onRelease:
    Serial.print("\nChannel down Button pressed!");
    global_CHDown();
    delay(100);
    break;

    case onLongPress:
    if (rx.getRealFrequency() != 8700) {
      Serial.print("\n\nReceding 10 Hz...\n");
      rx.setFrequency(rx.getRealFrequency() - 10);
      print_info();
      showFrequency_display();
    }
    else {
      rx.setFrequency(10800);
      print_info();
      showFrequency_display();
    }
    break;
  }
}

void VOLUPBUTTON(byte btnStatus) {
  switch (btnStatus) {
    case onRelease:
    Serial.print("\nVolume up Button pressed!");
    global_VolUp();
    delay(100);
    break;
  }
}

void VOLDOWNBUTTON(byte btnStatus) {
  switch (btnStatus) {
    case onRelease:
    Serial.print("\nVolume down Button pressed!");
    global_VolDown();
    delay(100);
    break;

    case onLongPress:
    Serial.print("\nMute Button pressed!");
    global_MuteRadio();
    delay(200);
  }
}

// Main loop. Checks for IR input and polls the buttons.
void loop() {
 if (IrReceiver.decode()) { // have we received an IR signal?
   translateIR();
   delay(50);
   IrReceiver.resume();    // Enable receiving of the next value
 }
 buttonchup.poll();
 buttonchdown.poll();
 buttonvolup.poll();
 buttonvoldown.poll();
} // loop

// End.
