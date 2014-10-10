/*
  Kitchen timer
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register 
 * 4 digit 7 segment display, anodes attached to shift register

 */
 
#include "SimpleTimer.h"

#define PIN_LATCH    8  // ST_CP of 74HC595
#define PIN_CLOCK    12 // SH_CP of 74HC595
#define PIN_DATA     11 // DS of 74HC595
#define PIN_DIGIT_0  7  // Multiplex pin for the digit
#define PIN_DIGIT_1  6  // Multiplex pin for the digit
#define PIN_DIGIT_2  5  // Multiplex pin for the digit
#define PIN_DIGIT_3  4  // Multiplex pin for the digit

#define DIGIT_COUNT 4 // 4 digit display
 
#define NUM_0      B11111010 // 0
#define NUM_1      B00100010 // 1
#define NUM_2      B10111001 // 2
#define NUM_3      B10101011 // 3
#define NUM_4      B01100011 // 4
#define NUM_5      B11001011 // 5
#define NUM_6      B11011011 // 6
#define NUM_7      B10100010 // 7
#define NUM_8      B11111011 // 8
#define NUM_9      B11101011 // 9
#define NUM_BLANK  B00000000 // ' '
#define NUM_DOT    B00000100 // .

#define COMPARE_REG 64 // OCR2A when to interupt (datasheet: 18.11.4)
 
//Pin connected to ST_CP of 74HC595
int latchPin = PIN_LATCH;
//Pin connected to SH_CP of 74HC595
int clockPin = PIN_CLOCK;
//Pin connected to DS of 74HC595
int dataPin = PIN_DATA;

const byte digit_pins[DIGIT_COUNT] = {PIN_DIGIT_3, PIN_DIGIT_2, PIN_DIGIT_1, PIN_DIGIT_0};

volatile int display_number; // the number currently being displayed.
volatile byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

byte start_time; // MINUTES 
volatile byte dot_state; // Position & visibiliy of dot (left most bit indicates visibility - 11000, 10100, 10010, 10001)

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

SimpleTimer timer;

//timer 2 compare ISR
ISR (TIMER2_COMPA_vect)
{
  updateDisplay();
}

void setup() 
{
  Serial.begin(9600);
  
  
  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  for (int i = 0; i < DIGIT_COUNT; i++) {
    pinMode(digit_pins[i], OUTPUT);
    // Set high to be OFF (common cathode)
    digitalWrite(digit_pins[i], HIGH);
  }

  // Initialize the digits to '0000'
  //sprintf(digits, "%04d", 0);
  
  multiplexInit();
  
  
  timer.setInterval(500, dotBlink);
  // Move the dot every 15 seconds
  timer.setInterval(15000, dotMove);
  // Countdown with a minute resolution.
  timer.setInterval(60000, countdownUpdate);
  
  countdownStart(); // @todo: start the countdown on input
}

void loop() 
{
  timer.run();  
}

void countdownStart()
{
  // MINUTES
  start_time = 10; // @todo: get this from an input
  display_number = start_time;
}

void countdownUpdate()
{
  // Coutdown 1 minute per update.
  --display_number;
  //Serial.println(display_number);
}

/**
 * Blink dot to show activity.
 */
void dotBlink()
{
  if (bitRead(dot_state, 4)) {
    // Currently on, so turn it off
    bitWrite(dot_state, 4, 0);
  } else {
    // Currently off, so turn it on
    bitWrite(dot_state, 4, 1);
  }
}

/**
 * Move the dot to the right
 */
void dotMove()
{
  // tmp
  bitWrite(dot_state, 3, 1);
}

void updateDisplay()
{
  // Turn off the previous digit.
  digitalWrite(digit_pins[current_digit], HIGH);
  
  // Multiplexing, so get the next digit.
  current_digit++;
  if (current_digit == DIGIT_COUNT) {
    current_digit = 0;
  }

  int i;
  switch (current_digit) {
    case 0:
      if (display_number < 999) {
        // Show a blank rather than a leading zero.
        i = 10;
      } else {
        i = display_number % 10000 / 1000;
      }
      if (bitRead(dot_state, 3)) {
        
      }
      break;
    case 1:
      if (display_number < 99) {
        // Show a blank rather than a leading zero.
        i = 10;
      } else {
        i = display_number % 1000 / 100;
      }
      break;
    case 2:
      if (display_number < 9) {
        // Show a blank rather than a leading zero.
        i = 10;
      } else {
        i = display_number % 100 / 10;
      }
      break;
    case 3:
      i = display_number % 10;
      break;
  }
  
  byte data = digit_map[i];
  
  // Handle the dot
  switch (current_digit) {
    case 0:
      if (bitWrite(dot_state, 4, 0) && bitWrite(dot_state, 3, 0)) {
        // shift in the dot
        data |= (1 << NUM_DOT);
      }
      break;
    case 1:
      if (bitWrite(dot_state, 4, 0) && bitWrite(dot_state, 2, 0)) {
        // shift in the dot
        data |= (1 << NUM_DOT);
      }
      break;
    case 2:
      if (bitWrite(dot_state, 4, 0) && bitWrite(dot_state, 1, 0)) {
        // shift in the dot
        data |= (1 << NUM_DOT);
      }
      break;
    case 3:
      if (bitWrite(dot_state, 4, 0) && bitWrite(dot_state, 0, 0)) {
        // shift in the dot
        data |= (1 << NUM_DOT);
      }
      break;
  }

  // Shift the byte out to the display.
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, digit_map[i]);
  digitalWrite(latchPin, HIGH);
  
  // Turn on the current digit
  digitalWrite(digit_pins[current_digit], LOW);
}

void multiplexInit(void)
{
  cli();//stop interrupts
  
  //set timer2 interrupt at 8kHz
  TCCR2A = 0;// set entire TCCR2A register to 0
  TCCR2B = 0;// same for TCCR2B
  TCNT2  = 0;//initialize counter value to 0
  // set compare match register 
  OCR2A = COMPARE_REG;// = (16*10^6) / (8000*8) - 1 (must be <256)
  // turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // Set CS21 bit for 8 prescaler
  //TCCR2B |= (1 << CS21);   
  
  // Datatasheet 18.11.2
  // Set  prescaler
  //TCCR2B = 0b00000110;
  TCCR2B = 0b00000111; // 1024
  
  
  // enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
  
  sei();//allow interrupts
  
}

