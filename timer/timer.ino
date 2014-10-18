/*
  Kitchen timer
  Turning on the outputs of a 74HC595 using an array

 Hardware:
 * 74HC595 shift register 
 * 4 digit 7 segment display, anodes attached to shift register

 */
 
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <avr/wdt.h>
#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management
#include "SimpleTimer.h"
 
#define START_TIME 30 // Default start at 30 minutes

#define ALARM_SOUND_SECONDS 1 * 1000 // How long for the sound alarm
#define ALARM_LIGHT_SECONDS 1 * 1000 // How long for the sound alarm
 
// Inputs from the accelerometer for setting the time
#define INPUT_UP_FAST     7
#define INPUT_UP_MED      4
#define INPUT_UP_SLOW     1
#define INPUT_NONE        0
#define INPUT_DOWN_SLOW  -1
#define INPUT_DOWN_MED   -4
#define INPUT_DOWN_FAST  -7

#define PIN_LATCH    8  // ST_CP of 74HC595
#define PIN_CLOCK    12 // SH_CP of 74HC595
#define PIN_DATA     11 // DS of 74HC595
#define PIN_DIGIT_0  7  // Multiplex pin for the digit
#define PIN_DIGIT_1  6  // Multiplex pin for the digit
#define PIN_DIGIT_2  5  // Multiplex pin for the digit
#define PIN_DIGIT_3  4  // Multiplex pin for the digit
//#define PIN_TIME_INPUT A0 

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
#define NUM_ERROR  B10001001

#define COMPARE_REG 64 // OCR2A when to interupt (datasheet: 18.11.4)
 
//Pin connected to ST_CP of 74HC595
int latchPin = PIN_LATCH;
//Pin connected to SH_CP of 74HC595
int clockPin = PIN_CLOCK;
//Pin connected to DS of 74HC595
int dataPin = PIN_DATA;

const byte digit_pins[DIGIT_COUNT] = {PIN_DIGIT_3, PIN_DIGIT_2, PIN_DIGIT_1, PIN_DIGIT_0};

volatile int display_number = START_TIME; // the number currently being displayed.
volatile byte current_digit = DIGIT_COUNT - 1; // The digit currently being shown in the multiplexing.

volatile byte dot_state = 0b00001000; // Position & visibiliy of dot (left most bit indicates visibility - 11000, 10100, 10010, 10001)

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
  T_OFF,
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
int zero_delay = 1000; // How long to wait with a setting of zero befor sleeping, allows sweeping past zero.
unsigned long zero_prev = 0;
//int alarm_count = ALARM_SECONDS; // How many calls to everySecond() to sound the alarm
unsigned long alarm_start; // When the alarm started
unsigned long setting_update_last; // When the display number was last changed

byte interrupt_flag;

// Assign a unique ID to this sensor at the same time
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

//timer 2 compare ISR
ISR (TIMER2_COMPA_vect)
{
  updateDisplay();
}

void ISR_activity() 
{
  interrupt_flag = 1;
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
  
  multiplexSetup();
  accelerometerSetup();
  
  attachInterrupt(0, ISR_activity, HIGH);
  
  
  timer.setInterval(200, inputTime);
  
  //
  
  timer_dot_blink = timer.setInterval(500, dotBlink);
  // Move the dot every 15 seconds
  timer_dot_move = timer.setInterval(15000, dotMove);
  // Countdown with a minute resolution.
  timer_countdown = timer.setInterval(60000, countdownUpdate);
  
  countdownStart();
  
  
  //timer.setInterval(100, experiment);
  
}

void experiment()
{
   // Display tilt
   sensors_event_t event = accelerometerRead();
   
   // acceleration is measured in m/s^2 
   int tilt = event.acceleration.x;
   Serial.println(tilt); 
   display_number = tilt;
}

void loop() 
{
  if (timer_state != T_ERROR) {
    timer.run();  
    stateRun();
  }
}

void stateRun()
{
  unsigned long now;
  
  if (interrupt_flag) {
    Serial.print("flag = "); Serial.println(interrupt_flag);
    
    // read the register to clear it
    byte reg_val = accel.readRegister(ADXL345_REG_INT_SOURCE);
    Serial.print("ADXL345_REG_INT_SOURCE = "); Serial.println(reg_val, BIN);

    interrupt_flag = 0; 
  }
  
  if (setting_state == S_NONE) {
    setting_update_last = 0;
  } else {
    now = millis();
  }
  
  switch(setting_state) {
    case S_REDUCE_FAST:
      if (now - 100 > setting_update_last) {
        setting_update_last = now;
        if (display_number > 0) {
          --display_number; 
        }
      }
      break;
      
    case S_REDUCE_MED:
      if (now - 250 > setting_update_last) {
        setting_update_last = now;
        if (display_number > 0) {
          --display_number; 
        }
      }
      break;
      
    case S_REDUCE_SLOW:
      if (now - 500 > setting_update_last) {
        setting_update_last = now;
        if (display_number > 0) {
          --display_number; 
        }
      }
      break;
      
    case S_NONE:
      break;
      
    case S_INCREASE_SLOW:
      if (now - 500 > setting_update_last) {
        setting_update_last = now;
        if (display_number < 9999) {
          ++display_number; 
        }
      }
      break;
      
    case S_INCREASE_MED:
      if (now - 250 > setting_update_last) {
        setting_update_last = now;
        if (display_number < 9999) {
          ++display_number; 
        }
      }
      break;
      
    case S_INCREASE_FAST:
      if (now - 100 > setting_update_last) {
        setting_update_last = now;
        if (display_number < 9999) {
          ++display_number; 
        }
      }
      break;
  }
  
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
      
    case T_ERROR:
      break;
  } 
}

void countdownStart()
{
  if (timer_state == T_ERROR) return;

  timer_state = T_COUNTDOWN;
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
  sensors_event_t event = accelerometerRead(); 
  int val = event.acceleration.x; // chop to an int

  if (val >= INPUT_UP_FAST) {
    timer_state = T_SETTING;
    setting_state = S_INCREASE_FAST;
  } else if (val >= INPUT_UP_MED) {
    timer_state = T_SETTING;
    setting_state = S_INCREASE_MED;
  } else if (val >= INPUT_UP_SLOW) {
    timer_state = T_SETTING;
    setting_state = S_INCREASE_SLOW;
  } else if (val >= INPUT_DOWN_SLOW) {
    
    setting_state = S_NONE;
    timer_state = T_COUNTDOWN; // TEMP
    
  } else if (val >= INPUT_DOWN_MED) {
    timer_state = T_SETTING;
    setting_state = S_REDUCE_SLOW;
  } else if (val >= INPUT_DOWN_FAST) {
    timer_state = T_SETTING;
    setting_state = S_REDUCE_MED;
  } else {
    timer_state = T_SETTING;
    setting_state = S_REDUCE_FAST;
  }
  
/*
  Serial.print(val);
  Serial.print(" ");
  Serial.println(setting_state);
  Serial.flush();
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

  if (timer_state == T_ERROR) {
    // All digits as error
    data = NUM_ERROR;
  } else if (timer_state == T_ALARM) {
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
  //attachInterrupt(0, wake, HIGH);
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
  //detachInterrupt(0); 
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

void multiplexSetup(void)
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

void accelerometerSetup(void)
{
  // Initialise the sensor
  if (!accel.begin()) {
    // There was a problem detecting the ADXL345
    Serial.println("Damn, no ADXL345 detected");
    timer_state == T_ERROR;
  } else {
    
    accel.setRange(ADXL345_RANGE_16_G); //  yep 16 - reduces sensitivity
    // Display some basic information on this sensor
    displaySensorDetails();
    
    Serial.print("ADXL345_REG_INT_ENABLE = "); Serial.println(accel.readRegister(ADXL345_REG_INT_ENABLE), BIN);
    
    Serial.print("ADXL345_REG_INT_MAP = "); Serial.println(accel.readRegister(ADXL345_REG_INT_MAP), BIN);
    
    Serial.print("ADXL345_REG_ACT_INACT_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_INACT_CTL), BIN);

    Serial.print("ADXL345_REG_THRESH_ACT = "); Serial.println(accel.readRegister(ADXL345_REG_THRESH_ACT), BIN);
    
    // read the register to clear it
    accel.readRegister(ADXL345_REG_INT_SOURCE);
    
    // Configure which pins interrupt on what 
    accel.writeRegister(ADXL345_REG_INT_MAP, B00100000); // double tap on int 1 (??comes in on int1 though)
    Serial.print("ADXL345_REG_INT_MAP = "); Serial.println(accel.readRegister(ADXL345_REG_INT_MAP), BIN);
    
    accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, B01110111); // activity on any axis (dc)
    Serial.print("ADXL345_REG_ACT_INACT_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_INACT_CTL), BIN);
    
    accel.writeRegister(ADXL345_REG_THRESH_ACT, 75); // what counts as activity 0 - 255
    Serial.print("ADXL345_REG_THRESH_ACT = "); Serial.println(accel.readRegister(ADXL345_REG_THRESH_ACT), BIN);
    
    // Turn on interrupts
    accel.writeRegister(ADXL345_REG_INT_ENABLE, B00110000); // double tap and activity
    Serial.print("ADXL345_REG_INT_ENABLE = "); Serial.println(accel.readRegister(ADXL345_REG_INT_ENABLE), BIN);
    
  }
}

sensors_event_t accelerometerRead(void)
{
  // Get a new sensor event 
  sensors_event_t event; 
  accel.getEvent(&event);
  return event;
}

void displaySensorDetails(void)
{
  sensor_t sensor;
  accel.getSensor(&sensor);
  Serial.println("------------------------------------");
  Serial.print  ("Sensor:       "); Serial.println(sensor.name);
  Serial.print  ("Driver Ver:   "); Serial.println(sensor.version);
  Serial.print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  Serial.print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" m/s^2");
  Serial.print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" m/s^2");
  Serial.print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" m/s^2");  
  Serial.println("------------------------------------");
  Serial.println("");
}

