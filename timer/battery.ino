/**
 * Battery power management funtions
 */

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
  
  // Power up the ADC, default values: no interrupts etc.
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
 * Read the result, if available.
 * Returns 0 if still measuring.
 */
long batteryRead()
{
  // ADSC will read as one as long as a conversion is in progress. (Datasheet: 23.9.2)
  if (bit_is_set(ADCSRA, ADSC)) {
    return 0; 
  }
  
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}

void batteryEnsureAdcOff() {
  ADCSRA = 0;
}

/**
 * Read the internal voltage.
 * NB this can take too long and cause the display to ficker.
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
 
  //delay(2); // Wait for Vref to settle
  ADCSRA |= _BV(ADSC); // Start conversion
  while (bit_is_set(ADCSRA,ADSC)); // measuring
 
  uint8_t low  = ADCL; // must read ADCL first - it then locks ADCH  
  uint8_t high = ADCH; // unlocks both
 
  long result = (high<<8) | low;
 
  result = 1125300L / result; // Calculate Vcc (in mV); 1125300 = 1.1*1023*1000
  return result; // Vcc in millivolts
}
