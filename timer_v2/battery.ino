/**
 * Battery power management funtions
 */

/**
 * Monitor the battery level (non blocking).
 */
void batteryMonitor()
{
  static unsigned long last = 0;
  static byte count = 0;
  
  unsigned long now = millis();
  if (now - last > BATTERY_INTERVAL) {
    last = now;
    if (!batteryAdcIsOn()) {
      // Turn the ADC on so its ready for next time round to make a reading.
      batteryAdcOn();
      if (DEBUG) Serial.println("ADC: batteryAdcOn");
    } else {
      batteryStartReading();
      if (DEBUG) Serial.println("ADC: batteryStartReading");
    }
  }
  
  // Check for a completed reading
  if (batteryReadComplete()) {
    
    long vcc = batteryRead();
    // Set on display
    display.setVcc((int) vcc);
    
    if (DEBUG) {
      Serial.print("VCC: "); Serial.println(vcc);
      Serial.flush();
    }
    
    if (vcc < 3500) {
      
      if (count > 1) {
        // Finished double check
        // Battery too, low sleep.
        timer_state = T_OFF; 
      } else {
        // Force another battery check on next stateRun()
        last = 0;
        ++count;
      }
      
    } else {
      // Save power
      batteryEnsureAdcOff();
      count = 0;
    }
    
  }
  
}

void batteryEnsureAdcOff() {
  ADCSRA = 0;
}

/**
 * Checks to see if the ADC is on.
 */
byte batteryAdcIsOn()
{
  return bit_is_set(ADCSRA, ADEN);
}

/**
 * Get the ADC ready.
 * needs to "warm up" (datasheet: 23.5.2) as we are using the bandgap reference.
 * "The first ADC conversion result after switching reference voltage source may be inaccurate"
 */
void batteryAdcOn()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  ADMUX = (1 << REFS0) | (1 << MUX3) | (1 << MUX2) | (1 << MUX1);
  
  // Power up the ADC, default values: no interrupts
  ADCSRA = (1 << ADEN);
}


/**
 * Start a non blocking read of the internal voltage ref.
 */
void batteryStartReading()
{
  // Start conversion
  ADCSRA |= (1 << ADSC); 
}

/**
 * ADSC will read as one as long as a conversion is in progress. (Datasheet: 23.9.2)
 */
byte batteryIsReading()
{
  return bit_is_set(ADCSRA, ADSC);
}

/**
 * One when a conversion has completed.
 */ 
byte batteryReadComplete()
{
  // This bit is set when an ADC conversion completes and the Data Registers are updated
  // (datasheet: 23.9.2)
  return bit_is_set(ADCSRA, ADIF);
}

/**
 * Read the result, if available.
 * Returns 0 if still measuring.
 */
long batteryRead()
{
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  
  // Clear the conversion complete flag.
  // "ADIF is cleared by writing a logical one to the flag" (datasheet: 23.9.2)
  ADCSRA |= (1 << ADIF); // write 1 to flag to clear it
  
  return result; // Vcc in millivolts
}


/**
 **
 ** Deprecated below...
 **
 **
 **/


long readVcc() {
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif  
 
  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

/**
 * Read the internal voltage.
 * 2ms blocking.
 */
long batteryReadVcc()
{
  // Read 1.1V reference against AVcc
  // set the reference to Vcc and the measurement to the internal 1.1V reference
  #if defined(__AVR_ATmega32U4__) || defined(__AVR_ATmega1280__) || defined(__AVR_ATmega2560__)
    ADMUX = _BV(REFS0) | _BV(MUX4) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #elif defined (__AVR_ATtiny24__) || defined(__AVR_ATtiny44__) || defined(__AVR_ATtiny84__)
    ADMUX = _BV(MUX5) | _BV(MUX0);
  #elif defined (__AVR_ATtiny25__) || defined(__AVR_ATtiny45__) || defined(__AVR_ATtiny85__)
    ADMUX = _BV(MUX3) | _BV(MUX2);
  #else
    ADMUX = _BV(REFS0) | _BV(MUX3) | _BV(MUX2) | _BV(MUX1);
  #endif

  // Power up the ADC, default values: no interrupts
  ADCSRA = (1 << ADEN);

  delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring

  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH
  uint8_t high = ADCH; // unlocks both

  long result = (high<<8) | low;

  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
