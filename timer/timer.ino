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

#define ALARM_SOUND_SECONDS 30 * 1000L // How long for the sound alarm
#define ALARM_LIGHT_SECONDS 60 * 1000L // How long for the light alarm

#define SETTING_WAIT       60 * 1000L // How long to wait for a setting confirmation
 
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
#define PIN_DIGIT_0  A0  // Multiplex pin for the digit
#define PIN_DIGIT_1  A1  // Multiplex pin for the digit
#define PIN_DIGIT_2  A2  // Multiplex pin for the digit
#define PIN_DIGIT_3  A3  // Multiplex pin for the digit
#define PIN_BLUE     5   // PWM blue led
#define PIN_RED      6   // PWM red led

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
#define NUM_DASH   B00000001 // -
#define NUM_ERROR  B10010010

#define COMPARE_REG 64 // OCR2A when to interupt (datasheet: 18.11.4)
 
//Pin connected to ST_CP of 74HC595
int latchPin = PIN_LATCH;
//Pin connected to SH_CP of 74HC595
int clockPin = PIN_CLOCK;
//Pin connected to DS of 74HC595
int dataPin = PIN_DATA;

const byte digit_pins[DIGIT_COUNT] = {PIN_DIGIT_0, PIN_DIGIT_1, PIN_DIGIT_2, PIN_DIGIT_3};

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

unsigned long setting_none_time = 0;
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

  timer_dot_blink = timer.setInterval(500, dotBlink);
  // Move the dot every 15 seconds
  timer_dot_move = timer.setInterval(15000, dotMove);
  // Countdown with a minute resolution.
  timer_countdown = timer.setInterval(60000, countdownUpdate);
  
 
  settingStart();
  
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

    int interruptSource = accel.readRegister(ADXL345_REG_INT_SOURCE);
    Serial.print("### ");
    Serial.println(interruptSource, BIN);
    
    
    if (interruptSource & B00000100) {
      Serial.println("### FREE_FALL");
    }
    
    // Inactivity gets sent to the unused INT2 pin so we can ignore it.
    // It still needs to be fired by the ADXL345 so it can set itselt to low power mode
    /*
    if (interruptSource & B00001000) {
      Serial.println("### Inactivity");
    }
    */
    
    if (interruptSource & B00010000) {
      Serial.println("### Activity");
    }
    
    if (interruptSource & B00100000) {
      Serial.println("### DOUBLE_TAP");
      
      if (timer_state == T_SETTING) {
        countdownStart();
      }
      
    }
    else if (interruptSource & B01000000) { // when a double tap is detected also a signle tap is deteced. we use an else here so that we only print the double tap
      Serial.println("### SINGLE_TAP");
    }
    
    
    interrupt_flag = 0; 
  }
  
  
  if (setting_state != S_NONE) {
    setting_none_time = 0;
  }
  
  now = millis();
  
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
      
      if (timer_state == T_SETTING) {
        if (setting_none_time == 0) {
          setting_none_time = now;
        } else if (now - setting_none_time > SETTING_WAIT) {
          setting_none_time = 0;
          setting_update_last = 0;
          // Turn off
          Serial.println("Sleep now");
          Serial.flush();
          goToSleep(); 
          
          Serial.println("Wake now");
          Serial.flush();
          // Wake up, and start a new count down.
          settingStart();
        }
      }
      
      
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
      breath();
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
          timer_state = T_OFF;
          alarm_start = 0;
          goToSleep();
          timer_state = T_WOKE;
        }
        
      }
      break;
      
      
    case T_SETTING:
      breath(); // TEMP
      break;
      
    case T_OFF:
      break;
      
    case T_WOKE:
      settingStart();
      break;
      
    case T_ERROR:
      break;
  } 
}

void settingStart()
{
  if (timer_state == T_ERROR) return;
  
  timer_state = T_SETTING;
}

void countdownStart()
{
  if (timer_state == T_ERROR) return;

  if (display_number == 0) {
    display_number = START_TIME;
  }
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

void countdownResume()
{
  timer_state = T_COUNTDOWN;
  restartCountDownTimers();
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

/**
 * Indicate gently that everything is ok.
 */
void breath()
{
  // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  
  float val = (exp(sin(millis()/6000.0*PI)) - 0.36787944)*108.0;
  analogWrite(PIN_BLUE, val);
  analogWrite(PIN_RED, val);
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
    
    
    Serial.print("ADXL345_REG_POWER_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_POWER_CTL), BIN);
    Serial.print("ADXL345_REG_BW_RATE = "); Serial.println(accel.readRegister(ADXL345_REG_BW_RATE), BIN);   
    Serial.print("ADXL345_REG_ACT_TAP_STATUS = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_TAP_STATUS), BIN);
    Serial.print("ADXL345_REG_INT_ENABLE = "); Serial.println(accel.readRegister(ADXL345_REG_INT_ENABLE), BIN);
    Serial.print("ADXL345_REG_INT_MAP = "); Serial.println(accel.readRegister(ADXL345_REG_INT_MAP), BIN);
    Serial.print("ADXL345_REG_ACT_INACT_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_INACT_CTL), BIN);
    Serial.print("ADXL345_REG_THRESH_ACT = "); Serial.println(accel.readRegister(ADXL345_REG_THRESH_ACT), BIN);
    
    // Turn off interrupts
    accel.writeRegister(ADXL345_REG_INT_ENABLE, 0);
    
    // read the register to clear it
    accel.readRegister(ADXL345_REG_INT_SOURCE);
    
    /*
    // Configure which pins interrupt on what 
    accel.writeRegister(ADXL345_REG_INT_MAP, 0); // all interrupts on INT1
    Serial.print("ADXL345_REG_INT_MAP = "); Serial.println(accel.readRegister(ADXL345_REG_INT_MAP), BIN);
    
    accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, B01110111); // activity on any axis (dc)
    Serial.print("ADXL345_REG_ACT_INACT_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_INACT_CTL), BIN);
    
    accel.writeRegister(ADXL345_REG_THRESH_ACT, 50); // what counts as activity 0 - 255
    Serial.print("ADXL345_REG_THRESH_ACT = "); Serial.println(accel.readRegister(ADXL345_REG_THRESH_ACT), BIN);
    
    // Turn on interrupts
    accel.writeRegister(ADXL345_REG_INT_ENABLE, B00110000); // double tap and activity
    Serial.print("ADXL345_REG_INT_ENABLE = "); Serial.println(accel.readRegister(ADXL345_REG_INT_ENABLE), BIN);
    */
    
    
    accel.writeRegister(ADXL345_REG_INT_MAP, B00001000); // all interrupts on INT1, except inactivity
    
      
    // free fall configuration
    accel.writeRegister(ADXL345_REG_TIME_FF, 0x14); // set free fall time
    accel.writeRegister(ADXL345_REG_THRESH_FF, 0x05); // set free fall threshold
    
    // single tap configuration
    accel.writeRegister(ADXL345_REG_DUR, 0x1F); // 625us/LSB
    accel.writeRegister(ADXL345_REG_THRESH_TAP, 48); // 62.5mg/LSB  <==> 3000mg/62.5mg = 48 LSB as datasheet suggestion
    accel.writeRegister(ADXL345_REG_TAP_AXES, B00000111); // enable tap detection on x,y,z axes
  
    // double tap configuration
    accel.writeRegister(ADXL345_REG_LATENT, 0x10);
    accel.writeRegister(ADXL345_REG_WINDOW, 0xFF);
    
    // inactivity configuration - 0 for inactive as soon as no movement
    accel.writeRegister(ADXL345_REG_TIME_INACT, 0); // 1s / LSB
    accel.writeRegister(ADXL345_REG_THRESH_INACT, 1); // 62.5mg / LSB
    // also working good with high movements: R_TIME_INACT=5, R_THRESH_INACT=16, R_ACT_INACT_CTL=B8(00000111)
    // but unusable for a quite slow movements
    
    // activity configuration
    accel.writeRegister(ADXL345_REG_THRESH_ACT, 4); // 62.5mg / LSB
    
    // activity and inctivity control
    accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, B01110111); // enable activity and inactivity detection on x,y,z using dc
    
   
    // set the ADXL345 in measurement and sleep Mode: this will save power while while we will still be able to detect activity
    // set the Link bit to 1 so that the activity and inactivity functions aren't concurrent but alternatively activated
    // set the AUTO_SLEEP bit to 1 so that the device automatically goes to sleep when it detects inactivity
    //accel.writeRegister(ADXL345_REG_POWER_CTL, B00111100);
    
    accel.writeRegister(ADXL345_REG_POWER_CTL, B00111000);
    
    // set the LOW_POWER bit to 0 in R_BW_RATE: get back to full accuracy measurement (we will consume more power)
    int bwRate = accel.readRegister(ADXL345_REG_BW_RATE);
    accel.writeRegister(ADXL345_REG_BW_RATE, bwRate & B00001010); // 100 Hz 50uA
    
    accel.writeRegister(ADXL345_REG_INT_ENABLE, B1111100); // enable single and double tap, activity, inactivity and free fall detection
  

    
    //accelerometerStartMeasuring();

  }
}

void accelerometerStartMeasuring()
{return;
  // get current power mode
  int powerCTL = accel.readRegister(ADXL345_REG_POWER_CTL);
  // set the device back in measurement mode
  // as suggested on the datasheet, we put it in standby then in measurement mode
  // we do this using a bitwise and (&) so that we keep the current R_POWER_CTL configuration
  accel.writeRegister(ADXL345_REG_POWER_CTL, powerCTL & B11110011);
  accel.writeRegister(ADXL345_REG_POWER_CTL, powerCTL & B11111011);
  
  // set the LOW_POWER bit to 0 in R_BW_RATE: get back to full accuracy measurement (we will consume more power)
  int bwRate = accel.readRegister(ADXL345_REG_BW_RATE);
  accel.writeRegister(ADXL345_REG_BW_RATE, bwRate & B00001111);
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

