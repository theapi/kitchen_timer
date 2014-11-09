/**
 * Handles the led display
 */
 
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
  } else if (display_volts > 0) {
    
    int i = 10;
    switch (current_digit) {
      case 0:
        if (display_volts < 1000) {
          // Show a blank rather than a leading zero.
          i = 10;
        } else {
          i = display_volts % 10000 / 1000;
        }
        break;
      case 1:
        if (display_volts < 100) {
          // Show a blank rather than a leading zero.
          i = 10;
        } else {
          i = display_volts % 1000 / 100;
        }
        break;
      case 2:
        if (display_volts < 10) {
          // Show a blank rather than a leading zero.
          i = 10;
        } else {
          i = display_volts % 100 / 10;
        }
        break;
      case 3:
        i = display_volts % 10;
        break;
    }
    data = digit_map[i];
    
  } else {
 
    int i = 10;
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
