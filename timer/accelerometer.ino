#ifndef ACCELEROMETER_H
#define ACCELEROMETER_H

#define ACCEL_TILT_OFF      -5 // tilt backward passed this value to turn off
#define ACCEL_TILT_COUNTDOWN 8 // tilt forward passed this value to start the countdown

#define ACCEL_FREEFALL   B00000100
#define ACCEL_INACTIVITY B00001000
#define ACCEL_ACTIVITY   B00010000
#define ACCEL_DOUBLE_TAP B00100000
#define ACCEL_SINGLE_TAP B01000000

void accelerometerDisplaySensorDetails(void)
{
  if (DEBUG) {
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
}


void accelerometerSetup(void)
{
  // Initialise the sensor
  if (!accel.begin()) {
    // There was a problem detecting the ADXL345
    if (DEBUG) Serial.println("Damn, no ADXL345 detected");
    timer_state = T_ERROR;
  } else {
    
    accel.setRange(ADXL345_RANGE_2_G);
    // Display some basic information on this sensor
    accelerometerDisplaySensorDetails();
    
    if (DEBUG) {
      Serial.print("ADXL345_REG_POWER_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_POWER_CTL), BIN);
      Serial.print("ADXL345_REG_BW_RATE = "); Serial.println(accel.readRegister(ADXL345_REG_BW_RATE), BIN);   
      Serial.print("ADXL345_REG_ACT_TAP_STATUS = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_TAP_STATUS), BIN);
      Serial.print("ADXL345_REG_INT_ENABLE = "); Serial.println(accel.readRegister(ADXL345_REG_INT_ENABLE), BIN);
      Serial.print("ADXL345_REG_INT_MAP = "); Serial.println(accel.readRegister(ADXL345_REG_INT_MAP), BIN);
      Serial.print("ADXL345_REG_ACT_INACT_CTL = "); Serial.println(accel.readRegister(ADXL345_REG_ACT_INACT_CTL), BIN);
      Serial.print("ADXL345_REG_THRESH_ACT = "); Serial.println(accel.readRegister(ADXL345_REG_THRESH_ACT), BIN);
    }
    
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
    //accel.writeRegister(ADXL345_REG_INT_MAP, B00000000);
      
    // free fall configuration
    accel.writeRegister(ADXL345_REG_TIME_FF, 0x14); // set free fall time
    accel.writeRegister(ADXL345_REG_THRESH_FF, 0x05); // set free fall threshold
    
    // single tap configuration
    //accel.writeRegister(ADXL345_REG_DUR, 0x1F); // 625us/LSB
    accel.writeRegister(ADXL345_REG_DUR, 0x30);
    accel.writeRegister(ADXL345_REG_THRESH_TAP, 0x40); // 62.5mg/LSB  <==> 3000mg/62.5mg = 48 LSB as datasheet suggestion
    accel.writeRegister(ADXL345_REG_TAP_AXES, B00000111); // enable tap detection on x,y,z axes
  
    // double tap configuration
    accel.writeRegister(ADXL345_REG_LATENT, 0x40);
    accel.writeRegister(ADXL345_REG_WINDOW, 0xFF);
    
    // inactivity configuration - 0 for inactive as soon as no movement
    accel.writeRegister(ADXL345_REG_TIME_INACT, 1); // 1s / LSB
    accel.writeRegister(ADXL345_REG_THRESH_INACT, 1); // 62.5mg / LSB
    // also working good with high movements: R_TIME_INACT=5, R_THRESH_INACT=16, R_ACT_INACT_CTL=B8(00000111)
    // but unusable for a quite slow movements
    
    // activity configuration
    accel.writeRegister(ADXL345_REG_THRESH_ACT, 2); // 62.5mg / LSB
    
    // activity and inctivity control
    //accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, B01110111); // enable activity and inactivity detection on x,y,z using dc
    accel.writeRegister(ADXL345_REG_ACT_INACT_CTL, B11111111); // ac
   
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


/**
 * Act on accelerometer readings
 */
void accelerometerMonitor()
{
  // Store the exiasting event so it can be compared with the reading about to be taken
  sensors_event_t previous_event = accelerometer_event;

  accelerometer_event = accelerometerRead(); 
  int val_x = accelerometer_event.acceleration.x; // chop to an int
  int val_y = accelerometer_event.acceleration.y; // chop to an int
  
  if (val_y <= -10) {   
    // Tilt 90 degrees backward, show voltmeter.
    if (!batteryAdcIsOn()) {
      // Turn the ADC on so its ready for next time round to make a reading.
      batteryAdcOn();
      //if (DEBUG) Serial.println("ADC: batteryAdcOn");
    } else {
      if (batteryReadComplete()) {
        long vcc = batteryRead();
        //if (DEBUG) Serial.println("ADC: batteryRead");
        display_volts = vcc;
      } else {
        batteryStartReading();
        //if (DEBUG) Serial.println("ADC: batteryStartReading");
      }
    }
    //if (DEBUG) Serial.println(display_volts);
    
  } else if (val_y <= ACCEL_TILT_OFF) {
    // turn off.
    timer_state = T_OFF; 
    
  } else if (val_y >= ACCEL_TILT_COUNTDOWN) {
    if (timer_state == T_SETTING) {
      countdownStart();
    }
  
  } else {
    
    // Allow normal display to happen.
    display_volts = 0;

    // Ensure the power hungry ADC is off.
    if (batteryAdcIsOn()) {
      batteryEnsureAdcOff(); 
      //if (DEBUG) Serial.println("ADC: batteryEnsureAdcOff");
    }
    
    if (timer_state == T_SETTING) {
      switch (val_x) {
        case 10:
        case 9:
        case 8:
        case 7:
        case 6:
          setting_state = S_REDUCE_FAST; 
          break;
          
        case 5:
          setting_state = S_REDUCE_MED; 
          break;
          
        case 4:
        case 3:
          setting_state = S_REDUCE_SLOW; 
          break;
          
        case -10:
        case -9:
        case -8:
        case -7:
        case -6:
          setting_state = S_INCREASE_FAST; 
          break;
          
        case -5:
          setting_state = S_INCREASE_MED; 
          break;
          
        case -4:
        case -3:
          setting_state = S_INCREASE_SLOW; 
          break;
          
        default: setting_state = S_NONE;  
      }

    }
  }
  
  //if (DEBUG) Serial.println(val_y);
  
/*
  if (DEBUG) {
    Serial.print(val_y);
    Serial.print(" ");
    Serial.println(setting_state);
    Serial.flush();
  }
*/
}

#endif

