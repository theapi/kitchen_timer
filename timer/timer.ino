/*
  Kitchen timer
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register 
 * 4 digit 7 segment display, anodes attached to shift register

 */
 
#include <avr/wdt.h>
#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management
#include "SimpleTimer.h"
 
#define START_TIME 30 // Default start at 30 minutes

#define ALARM_SOUND_SECONDS 1 * 1000 // How long for the sound alarm
#define ALARM_LIGHT_SECONDS 1 * 1000 // How long for the sound alarm
 
// Inputs from the potentiometer for setting the time
#define INPUT_LEFT_FAST_MIN   0
#define INPUT_LEFT_FAST_MAX   149
#define INPUT_LEFT_MED_MIN    150
#define INPUT_LEFT_MED_MAX    299
#define INPUT_LEFT_SMALL_MIN  300
#define INPUT_LEFT_SMALL_MAX  399
#define INPUT_NONE_MIN        400
#define INPUT_NONE_MAX        600 
#define INPUT_RIGHT_SMALL_MIN 601
#define INPUT_RIGHT_SMALL_MAX 700
#define INPUT_RIGHT_MED_MIN   701
#define INPUT_RIGHT_MED_MAX   850
#define INPUT_RIGHT_FAST_MIN  851
#define INPUT_RIGHT_FAST_MAX  1025

#define PIN_LATCH    8  // ST_CP of 74HC595
#define PIN_CLOCK    12 // SH_CP of 74HC595
#define PIN_DATA     11 // DS of 74HC595
#define PIN_DIGIT_0  7  // Multiplex pin for the digit
#define PIN_DIGIT_1  6  // Multiplex pin for the digit
#define PIN_DIGIT_2  5  // Multiplex pin for the digit
#define PIN_DIGIT_3  4  // Multiplex pin for the digit
#define PIN_TIME_INPUT A0 

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
#define NUM_DASH   B00000001 // -

#define COMPARE_REG 64 // OCR2A when to interupt (datasheet: 18.11.4)
 
//Pin connected to ST_CP of 74HC595
int latchPin = PIN_LATCH;
//Pin connected to SH_CP of 74HC595
int clockPin = PIN_CLOCK;
//Pin connected to DS of 74HC595
int dataPin = PIN_DATA;

const byte digit_pins[DIGIT_COUNT] = {PIN_DIGIT_3, PIN_DIGIT_2, PIN_DIGIT_1, PIN_DIGIT_0};

int last_time_set = START_TIME;
volatile int display_number; // the number currently being displayed.
volatile byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

//int start_time; // MINUTES 
volatile byte dot_state = 0b00011000; // Position & visibiliy of dot (left most bit indicates visibility - 11000, 10100, 10010, 10001)

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

// The timer states.
enum timer_states {
  T_COUNTDOWN, 
  T_ALARM, 
  T_SETTING,
  T_OFF
};
timer_states timer_state = T_COUNTDOWN; // Just start counting


SimpleTimer timer;
int timer_dot_blink;
int timer_dot_move;
int timer_countdown;


unsigned long alarm_start; // When the alarm started
unsigned long input_time_last; // When the countdown time was last updated by user input


//timer 2 compare ISR
ISR (TIMER2_COMPA_vect)
{
  updateDisplay();
}

void setup() 
{
  Serial.begin(9600);

  pinMode(2, INPUT); // External interrupt

  pinMode(latchPin, OUTPUT);
  pinMode(clockPin, OUTPUT);
  pinMode(dataPin, OUTPUT);
  
  pinMode(13, OUTPUT); // TMP
  
  for (int i = 0; i < DIGIT_COUNT; i++) {
    pinMode(digit_pins[i], OUTPUT);
    // Set high to be OFF (common cathode)
    digitalWrite(digit_pins[i], HIGH);
  }
  
  multiplexInit();
  
  // Read the knob
  timer.setInterval(200, inputTime);
  // Blink the dot
  timer_dot_blink = timer.setInterval(500, dotBlink);
  // Move the dot every 15 seconds
  timer_dot_move = timer.setInterval(15000, dotMove);
  // Countdown with a minute resolution.
  timer_countdown = timer.setInterval(60000, countdownUpdate);
  
  countdownStart();
  
  
  //timer.setInterval(200, experiment);
}

void loop() 
{
  timer.run();  
  
  stateRun();
  
  /*
  Serial.println("sleep");
  Serial.flush();
  delay(2000);
  goToSleep();
  Serial.println("WAKE UP");
  Serial.flush();
  */
}

void experiment()
{
  display_number = map(analogRead(PIN_TIME_INPUT), 0, 1023, 0, 99);
}

void stateRun()
{
  unsigned long now;
  
  switch(timer_state) {
    case T_COUNTDOWN:
      break;
    
    case T_ALARM:
      if (alarm_start == 0) {
        // Start the alarm
        alarm_start = millis();
      } else {
        // Handle the running alarm
        byte finished_sound = 0;
        byte finished_light = 0;
        now = millis();
        if (now - alarm_start > ALARM_SOUND_SECONDS) {
          // stop the sound
          finished_sound = 1;
          
        }
        if (now - alarm_start > ALARM_LIGHT_SECONDS) {
          // stop the light
          finished_light = 1;
          
        }
        
        if (finished_sound && finished_light) {
          goToSleep();
          // Wake up, and start a new count down.
          alarm_start = 0;
          countdownStart();
        }
        
      }
      break;
      
      
    case T_SETTING:
      break;
      
    case T_OFF:
      break;
  } 
}

void countdownStart()
{
  timer_state = T_COUNTDOWN;
  // MINUTES 
  display_number = START_TIME; 
  Serial.println(display_number);
  restartCountDownTimers();
}

void countdownUpdate()
{
  if (timer_state == T_COUNTDOWN) {
    if (display_number > 0) {
      // Coutdown 1 minute per update.
      --display_number;
    }
    
    if (display_number == 0) {
      timer_state = T_ALARM; 
      // Countdown finished, no dot now.
      bitWrite(dot_state, 4, 0);
    }
  }
}

void restartCountDownTimers()
{
  timer.restartTimer(timer_dot_blink);
  timer.restartTimer(timer_dot_move);
  timer.restartTimer(timer_countdown);
  dot_state = 0b00011000;
  
  timer.enable(timer_dot_blink);
  timer.enable(timer_dot_move);
  timer.enable(timer_countdown);
}

void timersDisable()
{
  timer.disable(timer_dot_blink);
  timer.disable(timer_dot_move);
  timer.disable(timer_countdown);
}

void inputTime()
{
  unsigned long now = millis();
  int val = map(analogRead(PIN_TIME_INPUT), 0, 1023, 0, 99);
  if (val != last_time_set) {
    display_number = val;
    last_time_set = val;
    input_time_last = now;
    // Setting the time
    timer_state = T_SETTING;
  }
  
  if (now - input_time_last > 500) {
    // No knob change for long enough
    // Carry on counting
    timer_state = T_COUNTDOWN;
    
    if (display_number == 0) {
      // Turn off
      goToSleep(); 
      // Wake up, and start a new count down.
      countdownStart();
      
    } 
  }

/*
  Serial.print(now);  Serial.print(" ");
  Serial.print(input_time_last); Serial.print(" ");
  Serial.print(now - input_time_last); Serial.print(" ");
  Serial.print(timer_state); Serial.print(" ");
  Serial.println(display_number);
  //Serial.flush();
 */ 
}

/**
 * Blink dot to show activity.
 */
void dotBlink()
{
  if (timer_state == T_COUNTDOWN) {
    if (bitRead(dot_state, 4)) {
      // Currently on, so turn it off
      bitWrite(dot_state, 4, 0);
    } else {
      // Currently off, so turn it on
      bitWrite(dot_state, 4, 1);
    }
  } else {
    // dot off
    bitWrite(dot_state, 4, 0);
  }
  
  /*
  Serial.print(timer_state);
  Serial.print(" ");
  Serial.print(dot_state, BIN);
  Serial.print(" ");
  Serial.println(display_number);
  */
}

/**
 * Move the dot to the right
 */
void dotMove()
{
  if (timer_state == T_COUNTDOWN) {
    byte on = 0;
    if (bitRead(dot_state, 4)) {
      on = 1;
      bitWrite(dot_state, 4, 0);
    }
    
    // shift right
    dot_state >>= 1;
    // roll back to start
    if (dot_state == 0) {
      dot_state = 0b0001000;
    }
    
    if (on) {
      // put it back on
      dot_state |= 0b0010000;
    }
  }
}

void updateDisplay()
{
  byte previous_digit = current_digit;
  
  // Multiplexing, so get the next digit.
  current_digit++;
  if (current_digit == DIGIT_COUNT) {
    current_digit = 0;
  }

  byte data = 0;

  if (timer_state == T_ALARM) {
    // All digits as dashes
    data = NUM_DASH;
  } else {
 
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
    
    data = digit_map[i];
    
    // Handle the dot
    switch (current_digit) {
      case 0:
        if (bitRead(dot_state, 4) && bitRead(dot_state, 3)) {
          // Add the dot to the byte
          data |= NUM_DOT;
        }
        break;
      case 1:
        if (bitRead(dot_state, 4) && bitRead(dot_state, 2)) {
          // Add the dot to the byte
          data |= NUM_DOT;
        }
        break;
      case 2:
        if (bitRead(dot_state, 4) && bitRead(dot_state, 1)) {
          // Add the dot to the byte
          data |= NUM_DOT;
        }
        break;
      case 3:
        if (bitRead(dot_state, 4) && bitRead(dot_state, 0)) {
          // Add the dot to the byte
          data |= NUM_DOT;
        }
        break;
    }
  }
  
  // Turn off the previous digit.
  digitalWrite(digit_pins[previous_digit], HIGH);
  
  // Shift the byte out to the display.
  digitalWrite(latchPin, LOW);
  shiftOut(dataPin, clockPin, MSBFIRST, data);
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

void goToSleep()
{
  timer_state == T_OFF;
  timersDisable();

  // Turn off the display - @todo: output enable on shift register to turn off display
  for (int i = 0; i < DIGIT_COUNT; i++) {
    //pinMode(digit_pins[i], OUTPUT);
    // Set high to be OFF (common cathode)
    digitalWrite(digit_pins[i], HIGH); // @todo is high using power here?
  }
  
  digitalWrite(13, LOW);
  // will be called when pin D2 goes high
  attachInterrupt(0, wake, HIGH);
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
  //interrupts();
  sleep_cpu();              // sleep within 3 clock cycles of brown out
  
  sleep_disable(); 
  detachInterrupt(0); 
  //MCUSR = 0; // clear the reset register 

  digitalWrite(13, HIGH);
 
 /*
  power_adc_enable();
  power_timer0_enable();
  power_timer1_enable();
  power_timer2_enable();
  power_usart0_enable();
  //power_twi_enable();
  */
  power_all_enable();
}

void wake()
{
  // Just wake up. Do nothing else here as it is an ISR.
}

