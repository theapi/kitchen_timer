/*
  Kitchen timer

 Hardware:
 * Monochrome 0.96" 128x64 OLED graphic display https://www.adafruit.com/products/326
 * ADXL345 - Triple-Axis Accelerometer  https://www.adafruit.com/products/1231
 * UM66T-05L: Home Sweet Home
 */

#define DEBUG false

#include <Wire.h>
#include "U8glib.h"
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <avr/wdt.h>
#include <avr/sleep.h>    // Sleep Modes
#include <avr/power.h>    // Power management
#include "SimpleTimer.h"
#include "TimerDisplay.h"



#define START_TIME 90 // Default start at 30 minutes

#define ALARM_SOUND_SECONDS 14 * 1000L // How long for the sound alarm
#define ALARM_LIGHT_SECONDS 60 * 1000L // How long for the light alarm

#define SETTING_WAIT       15 * 1000L // How long to wait for a setting confirmation

#define PIN_PNP      13  // Keep low to stay on
#define PIN_RED      9   // PWM red led
#define PIN_GREEN    10   // PWM green led
#define PIN_BLUE     11   // PWM blue led
#define PIN_SOUND    12  // The sound chip, LOW to play - UM66T-05L: Home Sweet Home :)



byte breath_r = 0;
byte breath_g = 0;
byte breath_b = 200;
float breath_speed = 6000.0;

// The timer states.
enum timer_states {
  T_COUNTDOWN,
  T_ALARM,
  T_SETTING,
  T_OFF,
  T_WOKE,
  T_ERROR
};
timer_states timer_state = T_OFF;

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
int timer_countdown;
int timer_breath;
int timer_battery;

unsigned long setting_none_time = 0;
unsigned long alarm_start; // When the alarm started
unsigned long setting_update_last; // When the display number was last changed

byte interrupt_flag;
int interruptSource;
sensors_event_t accelerometer_event; // the last monitored event
// Assign a unique ID to this sensor at the same time
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);

U8GLIB_SSD1306_128X64 u8g(U8G_I2C_OPT_NONE|U8G_I2C_OPT_DEV_0);

TimerDisplay display = TimerDisplay();

int display_volts = 0;
byte running_flag = 0; // Whether running ok (for watchdog reset power off)

void ISR_activity()
{
  interrupt_flag = 1;
}


void setup()
{
  wdt_reset(); // Ensure watchdog is reset

  pinMode(PIN_PNP, OUTPUT);
  // Set to high so a watchdog reset will power off the device.
  digitalWrite(PIN_PNP, HIGH);
  running_flag = 0;
  // Let the watchdog monitor for hangups
  //wdt_enable(WDTO_8S);

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


  // Sound
  pinMode(PIN_SOUND, OUTPUT);
  digitalWrite(PIN_SOUND, HIGH); // OFF

  //displaySetup();
  accelerometerSetup();

  // React to the accelerometer
  attachInterrupt(0, ISR_activity, HIGH);

  // Check the accelerometer.
  timer.setInterval(200, accelerometerMonitor);

  // Countdown with a minute resolution.
  timer_countdown = timer.setInterval(1000, countdownUpdate);

  timer_breath = timer.setInterval(1011, breathColourSet);

  // Keep an eye on the battery
  timer_battery = timer.setInterval(10333, batteryMonitor);

  batteryMonitor();
  displayUpdate();

  display.setMinutes(START_TIME);
  settingStart();

}

void loop()
{
  // Tell watchdog all is ok.
  wdt_reset();
  if (!running_flag) {
    // Take control of the pnp here
    // so the watchdog can remove power.
    digitalWrite(PIN_PNP, LOW);
    running_flag = 1;
  }

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

  // Set to the default start time if needed.
  if (display.finished()) {
    display.setMinutes(START_TIME);
    display.setMode(0);
  }
}

void timersDisable()
{
  timer.disable(timer_countdown);
}

void goToSleep()
{
  timer_state = T_OFF;

  // Reset the alarm
  alarm_start = 0;

  batteryEnsureAdcOff();
  timersDisable();

  // Turn off the display
  u8g.sleepOn();
  //oled.ssd1306_command(SSD1306_DISPLAYOFF);

  // Ensure sound is off
  digitalWrite(PIN_SOUND, HIGH);

  // Ensure the led is OFF (common anode)
  digitalWrite(PIN_RED, HIGH);
  digitalWrite(PIN_GREEN, HIGH);
  digitalWrite(PIN_BLUE, HIGH);


  noInterrupts();
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  power_all_disable();

  sleep_enable();

  // turn off brown-out enable in software
  //MCUCR = bit (BODS) | bit (BODSE);  // turn on brown-out enable select
  //MCUCR = bit (BODS);        // this must be done within 4 clock cycles of above

  // Turn interrupt back on so the wake up signal can happen
  interrupts();
  sleep_cpu();              // sleep within 3 clock cycles of brown out

  sleep_disable();
  MCUSR = 0; // clear the reset register


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

  // Turn the display back on
  u8g.sleepOff();
}

/**
 * @todo: figure out how to get this into the library
 */
void displaySetup()
{

}

/**
 * @todo: figure out how to get this into the library
 */
void displayUpdate()
{
  // picture loop
  u8g.firstPage();
  do {
    draw();
  } while( u8g.nextPage() );
}


void draw(void) {
  // graphic commands to redraw the complete screen should be placed here
  //u8g.setFont(u8g_font_unifont);
  //u8g.setFont(u8g_font_profont22);
  u8g.setFont(u8g_font_fub42n);
  u8g.setFontPosTop();
  u8g.setPrintPos(0, 0);
  u8g.print(display.getMinutes());


  //u8g.setFont(u8g_font_profont17);
  u8g.setFont(u8g_font_fub14n);
  u8g.setFontPosTop();
  u8g.setPrintPos(105, 0);
  u8g.print(display.getSeconds());

  u8g.setFont(u8g_font_fub11n);
  u8g.setFontPosTop();
  u8g.setPrintPos(0, 52);
  u8g.print((int) accelerometer_event.acceleration.x);

  u8g.setPrintPos(23, 52);
  u8g.print((int) accelerometer_event.acceleration.y);

  u8g.setPrintPos(90, 52);
  u8g.print((int) display.getVcc());

}
