
void colourSet()
{
  if (timer_state == T_COUNTDOWN) {
    if (display_number > 3) {
      breath_speed = 6000.0;
      breath_r = 0;
      breath_g = 0;
      breath_b = 255;
    } else if (display_number > 2) {
      breath_speed = 4000.0;
      breath_r = 0;
      breath_g = 255;
      breath_b = 30;
    } else if (display_number > 1) {
      breath_speed = 2000.0;
      breath_r = 255;
      breath_g = 128;
      breath_b = 0;
    } else {
      // Alarm
      breath_speed = 500.0;
      breath_r = 255;
      breath_g = 0;
      breath_b = 0;
    }
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
  
  //analogWrite(PIN_RED, 255-red);
  //analogWrite(PIN_GREEN, 255-green);
  //analogWrite(PIN_BLUE, 255-blue);
  
  //analogWrite(PIN_RED, 255-val);
  
}
