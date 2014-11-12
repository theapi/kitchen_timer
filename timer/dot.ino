/**
 * Handle the dot
 */ 


/**
 * Blink dot to show activity.
 */
void dotBlink()
{
  if (timer_state == T_COUNTDOWN) {
    if (bitRead(dot_state, 4)) {
      // Currently on, so turn it off
      dotOff();
    } else {
      // Currently off, so turn it on
      dotOn();
    }
  } else {
    dotOff();
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
      // Turn it off, so we can move it
      dotOff();
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

void dotOn()
{
  bitWrite(dot_state, 4, 1);
}

void dotOff()
{
  bitWrite(dot_state, 4, 0);
}

byte dotMergeWithShiftData(byte digit, byte data)
{
  switch (digit) {
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
  
  return data;
}
