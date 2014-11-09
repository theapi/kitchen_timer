/**
 * The state machine
 */

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
      breath(breath_speed, breath_r, breath_g, breath_b);
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
      //breath(); // TEMP
      analogWrite(PIN_RED, 22);
      analogWrite(PIN_GREEN, 255);
      analogWrite(PIN_BLUE, 22);
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
