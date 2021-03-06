/*
  Kitchen timer
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register 
 * 4 digit 7 segment display, anodes attached to shift register

 */
 
#define DEBUG false
 
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <avr/wdt.h>
#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management
#include "SimpleTimer.h"

 
#define START_TIME 30 // Default start at 30 minutes

#define ALARM_SOUND_SECONDS 14 * 1000L // How long for the sound alarm
#define ALARM_LIGHT_SECONDS 60 * 1000L // How long for the light alarm

#define SETTING_WAIT       7 * 1000L // How long to wait for a setting confirmation
 
#define PIN_VOLT     4  // A pin on the connector to monitor volts externally
#define PIN_LATCH    12  // ST_CP of 74HC595
#define PIN_CLOCK    11 // SH_CP of 74HC595
#define PIN_DATA     13 // DS of 74HC595
#define PIN_DIGIT_0  A0  // Multiplex pin for the digit
#define PIN_DIGIT_1  A1  // Multiplex pin for the digit
#define PIN_DIGIT_2  A2  // Multiplex pin for the digit
#define PIN_DIGIT_3  A3  // Multiplex pin for the digit
#define PIN_RED      5   // PWM red led
#define PIN_GREEN    6   // PWM green led
#define PIN_BLUE     9   // PWM blue led
#define PIN_SOUND    10  // The sound chip, LOW to play - UM66T-05L: Home Sweet Home :)

#define DIGIT_COUNT 4 // 4 digit display

#define NUM_0      B11110101 // 0
#define NUM_1      B00000101 // 1
#define NUM_2      B10110011 // 2
#define NUM_3      B10010111 // 3
#define NUM_4      B01000111 // 4
#define NUM_5      B11010110 // 5
#define NUM_6      B01110110 // 6
#define NUM_7      B10000101 // 7
#define NUM_8      B11110111 // 8
#define NUM_9      B11000111 // 9
#define NUM_BLANK  B00000000 // ' '
#define NUM_DOT    B00001000 // .
#define NUM_DASH   B00000010 // -
#define NUM_ERROR  B10010010
 
#define COMPARE_REG 32 // OCR2A when to interupt (datasheet: 18.11.4)
 
//Pin connected to ST_CP of 74HC595
int latchPin = PIN_LATCH;
//Pin connected to SH_CP of 74HC595
int clockPin = PIN_CLOCK;
//Pin connected to DS of 74HC595
int dataPin = PIN_DATA;

const byte digit_pins[DIGIT_COUNT] = {PIN_DIGIT_3, PIN_DIGIT_2, PIN_DIGIT_1, PIN_DIGIT_0};
const byte digit_map[12] =      //seven segment digits in bits
{
  NUM_0,
  NUM_1,
  NUM_2,
  NUM_3,
  NUM_4,
  NUM_5,
  NUM_6,
  NUM_7,
  NUM_8,
  NUM_9,
  NUM_BLANK,
  NUM_DOT
};

byte breath_r = 0;
byte breath_g = 0;
byte breath_b = 200;
float breath_speed = 6000.0;

volatile int display_volts; // The volt reading to show when requested.
volatile int display_number = START_TIME; // the number currently being displayed.
volatile byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

volatile byte dot_state = 0b00001000; // Position & visibiliy of dot (left most bit indicates visibility - 11000, 10100, 10010, 10001)



// The timer states.
enum timer_states {
  T_COUNTDOWN, 
  T_ALARM, 
  T_SETTING,
  T_OFF,
  T_WOKE,
  T_ERROR
};
timer_states timer_state = T_SETTING;

// Time setting states
enum setting_states {
  S_NONE,
  S_REDUCE_FAST,
  S_REDUCE_MED,
  S_REDUCE_SLOW,
  S_INCREASE_SLOW,
  S_INCREASE_MED,
  S_INCREASE_FAST,
};
setting_states setting_state = S_NONE;

SimpleTimer timer;
int timer_dot_blink;
int timer_dot_move;
int timer_countdown;
int timer_breath;

unsigned long setting_none_time = 0;
unsigned long alarm_start; // When the alarm started
unsigned long setting_update_last; // When the display number was last changed

byte interrupt_flag;
int interruptSource;
sensors_event_t accelerometer_event; // the last monitored event
// Assign a unique ID to this sensor at the same time
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);


//timer 2 compare ISR
ISR (TIMER2_COMPA_vect)
{
  displayUpdate();
}

void ISR_activity() 
{
  interrupt_flag = 1;
}

void setup() 
{
  // Turn ADC off
  batteryEnsureAdcOff(); 
  
  if (DEBUG) {
    Serial.begin(9600);
  }
  
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  
  // Ensure the led is OFF (common anode)
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, HIGH);

  pinMode(2, INPUT); // External interrupt

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  pinMode(PIN_VOLT, OUTPUT);
  digitalWrite(PIN_VOLT, HIGH);
  
  for (int i = 0; i < DIGIT_COUNT; i++) {
    pinMode(digit_pins[i], OUTPUT);
    // Set high to be OFF (common anode)
    digitalWrite(digit_pins[i], HIGH);
  }
  
  // Sound
  pinMode(PIN_SOUND, OUTPUT);
  digitalWrite(PIN_SOUND, HIGH); // OFF
  
  displaySetup();
  accelerometerSetup();
  
  attachInterrupt(0, ISR_activity, HIGH);
  
  // Check the accelerometer.
  timer.setInterval(100, accelerometerMonitor);

  timer_dot_blink = timer.setInterval(500, dotBlink);
  // Move the dot every 15 seconds
  timer_dot_move = timer.setInterval(15000, dotMove);
  // Countdown with a minute resolution.
  timer_countdown = timer.setInterval(60000, countdownUpdate);
  
  timer_breath = timer.setInterval(1000, breathColourSet);
 
  settingStart();
  
}

void loop() 
{
  if (timer_state != T_ERROR) {
    stateRun();
    timer.run();  
  }
}

void settingStart()
{  
  if (timer_state == T_ERROR) return;
  
  // Purple
  analogWrite(PIN_RED, 22);
  analogWrite(PIN_GREEN, 255);
  analogWrite(PIN_BLUE, 22);
  
  setting_none_time = 0;
  setting_update_last = 0;
  timer_state = T_SETTING;
  // Ensure the dot is off.
  dotOff();

  // Set to the default start time if needed.
  if (display_number == 0) {
    display_number = START_TIME;
  }
}

void timersDisable()
{
  timer.disable(timer_dot_blink);
  timer.disable(timer_dot_move);
  timer.disable(timer_countdown);
}

void goToSleep()
{
  noInterrupts();
  timer_state = T_OFF;
  
  // Ensure sound is off
  digitalWrite(PIN_SOUND, HIGH);
  
  // Reset the alarm
  alarm_start = 0;
  
  batteryEnsureAdcOff();
  timersDisable();

  // Turn off the display 
  for (int i = 0; i < DIGIT_COUNT; i++) {
    // Set high to be OFF (common anode)
    digitalWrite(digit_pins[i], HIGH); 
  }
  
  // Ensure the led is OFF (common anode)
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, HIGH);
  
  //cli();

  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  power_all_disable();

  sleep_enable();
  //sei();
  
  // Do not interrupt before we go to sleep, or the
  // ISR will detach interrupts and we won't wake.
  //noInterrupts();
  
  
  
  // turn off brown-out enable in software
  //MCUCR = bit (BODS) | bit (BODSE);  // turn on brown-out enable select
  //MCUCR = bit (BODS);        // this must be done within 4 clock cycles of above
  
  // Turn interrupt back on so the wake up signal can happen
  interrupts();
  sleep_cpu();              // sleep within 3 clock cycles of brown out
  
  sleep_disable(); 
  //detachInterrupt(0); 
  //MCUSR = 0; // clear the reset register 
 
 
 /*
  //power_adc_enable();
  power_timer0_enable();
  power_timer1_enable();
  power_timer2_enable();
  if (DEBUG) {
    power_usart0_enable();
  }
  //power_twi_enable();
  */
  power_all_enable();
}





