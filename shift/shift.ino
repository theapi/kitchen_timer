/*
  Shift Register Example
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register 
 * 4 digit 7 segment display, anodes attached to shift register

 */
 
// The SimpleTimer library lets us do things when we want them to happen
// without stopping everything for a delay.
#include "SimpleTimer.h"

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

#define COMPARE_REG 32 // OCR2A when to interupt (datasheet: 18.11.4)

#define PIN_RED      5   // PWM red led
#define PIN_GREEN    6   // PWM green led
#define PIN_BLUE     9   // PWM blue led


 
//Pin connected to DS of 74HC595
int dataPin = 13;
//Pin connected to ST_CP of 74HC595
int latchPin = 12;
//Pin connected to SH_CP of 74HC595
int clockPin = 11;


const byte digit_pins[DIGIT_COUNT] = {A3,A2,A1,A0};

volatile int display_number = 8; // the number currently being displayed.
volatile byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

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
  
  pinMode(PIN_RED, OUTPUT);
  pinMode(PIN_GREEN, OUTPUT);
  pinMode(PIN_BLUE, OUTPUT);
  
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
  
  timer.setInterval(1000, updateTime);


}

void loop() 
{
  timer.run();  
  breath(6000.0, 0, 255, 80);
}

void updateTime()
{
  display_number = millis() / 1000;
  Serial.println(display_number);
  
  //sprintf(digits, "%04d", now);
  //Serial.println(digits);
  
  //display_buffer = digits;
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
      if (display_number < 1000) {
        // Show a blank rather than a leading zero.
        i = 10;
      } else {
        i = display_number % 10000 / 1000;
      }
      break;
    case 1:
      if (display_number < 100) {
        // Show a blank rather than a leading zero.
        i = 10;
      } else {
        i = display_number % 1000 / 100;
      }
      break;
    case 2:
      if (display_number < 10) {
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

/**
 * Gently change the led
 */
void breath(float breath_speed, byte red, byte green, byte blue)
{
  // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  
  float val = (exp(sin(millis()/ breath_speed *PI)) - 0.36787944)*108.0;
  analogWrite(PIN_RED, map(val, 0, 255, 0, red));
  analogWrite(PIN_GREEN, map(val, 0, 255, 0, green));
  analogWrite(PIN_BLUE, map(val, 0, 255, 0, blue));
  
}

