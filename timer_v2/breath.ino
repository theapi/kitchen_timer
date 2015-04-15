
void breathColourSet()
{
  if (timer_state == T_COUNTDOWN) {
    if (display.getMinutes() > 29) {
      // Greater than 30 minutes; blue, calm
      breath_speed = 100;
      breath_steps = 1;
      breath_r = 0;
      breath_g = 0;
      breath_b = 255;
    } else if (display.getMinutes() > 14) {
      // Greater than 15 minutes; green, slow
      breath_speed = 70;
      breath_steps = 1;
      breath_r = 0;
      breath_g = 255;
      breath_b = 0;
    } else if (display.getMinutes() > 4) {
      // The last 5 minutes; yellow, slow
      breath_speed = 50;
      breath_steps = 1;
      breath_r = 255;
      breath_g = 255;
      breath_b = 0;
    } else if (display.getMinutes() > 0) {
      // last minute; orange
      breath_speed = 50;
      breath_steps = 1;
      breath_r = 250;
      breath_g = 69;
      breath_b = 10;
    } else {
      // One minute left; red, fast
      breath_speed = 50;
      breath_steps = 4;
      breath_r = 255;
      breath_g = 0;
      breath_b = 0;
    }
  }
}

/**
 * Breath with a stumpy sine wave
 */
void breathWave(unsigned long interval, byte steps, byte red, byte green, byte blue)
{
  static byte wave[] = {
    109,110,110,111,112,
    115,118,121,124,127,130,133,136,139,143,
    146,149,152,155,158,161,164,167,170,173,
    176,178,181,184,187,190,192,195,198,200,
    203,205,208,210,212,215,217,219,221,223,
    225,227,229,231,233,234,236,238,239,240,
    242,243,244,245,247,248,249,249,250,251,
    252,252,253,253,253,254,254,254,254,254,
    254,254,253,253,253,252,252,251,250,249,
    249,248,247,245,244,243,242,240,239,238,
    236,234,233,231,229,227,225,223,221,219,
    217,215,212,210,208,205,203,200,198,195,
    192,190,187,184,181,178,176,173,170,167,
    164,161,158,155,152,149,146,143,139,136,
    133,130,127,124,121,118,115,112,111,110,
    110,109
  };
  static byte wave_index = 0;  
  static unsigned long last = 0;
  
  unsigned long now = millis();
  if (now - last > interval) {
    last = now;
    
    // inverse because common anode. HIGH is off
    byte val_r = 255 - (map(wave[wave_index], 0, 255, 0, red));
    byte val_g = 255 - (map(wave[wave_index], 0, 255, 0, green));
    byte val_b = 255 - (map(wave[wave_index], 0, 255, 0, blue));
  
    wave_index += steps;
  
    if (wave_index > 146) {
      wave_index = 0;
    }
    
    analogWrite(PIN_RED,   val_r);
    analogWrite(PIN_GREEN, val_g);
    analogWrite(PIN_BLUE,  val_b);
  }
}

/**
 * Gently change the led
 */
void breath(float breath_speed, byte red, byte green, byte blue)
{
  // http://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/

  float val = (exp(sin(millis()/ breath_speed *PI)) - 0.36787944)*108.0;
  val = map(val, 0, 255, 50, 255);
  // inverse because common anode. HIGH is off
  byte val_r = 255 - (map(val, 0, 255, 0, red));
  byte val_g = 255 - (map(val, 0, 255, 0, green));
  byte val_b = 255 - (map(val, 0, 255, 0, blue));

  analogWrite(PIN_RED,   val_r);
  analogWrite(PIN_GREEN, val_g);
  analogWrite(PIN_BLUE,  val_b);
}

