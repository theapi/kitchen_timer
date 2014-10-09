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
 
#define NUM_0    B11111010 //0
#define NUM_1    B00100010 //1
#define NUM_2    B10111001 //2
#define NUM_3    B10101011 //3
#define NUM_4    B01100011 //4
#define NUM_5    B11001011 //5
#define NUM_6    B11011011 //6
#define NUM_7    B10100010 //7
#define NUM_8    B11111011 //8
#define NUM_9    B11101011 //9
#define NUM_DOT  B00000100  //.
 
//Pin connected to ST_CP of 74HC595
int latchPin = 8;
//Pin connected to SH_CP of 74HC595
int clockPin = 12;
//Pin connected to DS of 74HC595
int dataPin = 11;

const byte digit_pins[DIGIT_COUNT] = {4,5,6,7};

//holders for information you're going to pass to shifting function
byte data;

char digits[DIGIT_COUNT];
byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

const byte digit_map[11] =      //seven segment digits in bits
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
  NUM_DOT
};

SimpleTimer timer;

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
  sprintf(digits, "%04d", 0);
  
  
  timer.setInterval(1000, updateTime);
}

void loop() 
{
  timer.run();
  updateDisplay(); // @TODO interupt driven update display
  
}

void updateTime()
{
  int now = millis() / 1000;
  sprintf(digits, "%04d", now);
  Serial.println(digits);
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
  
  int num = digits[current_digit];
  int i = num - 48;

  // Shift the byte out to the display.
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, digit_map[i]);
  digitalWrite(latchPin, HIGH);
  
  // Turn on the current digit
  digitalWrite(digit_pins[current_digit], LOW);
}

