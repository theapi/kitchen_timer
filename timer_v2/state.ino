/**
 * The state machine
 */

#define FORCE_SLOW_THRESHOLD 12 // Slower than this settings is forced to be slow.

void stateRun()
{
  unsigned long now;

  if (interrupt_flag) {

    interruptSource = accel.readRegister(ADXL345_REG_INT_SOURCE);

    if (DEBUG) {
      Serial.print("### ");
      Serial.println(interruptSource, BIN);

      if (interruptSource & ACCEL_FREEFALL) {
        Serial.println("### FREE_FALL");
      }

      // Inactivity gets sent to the unused INT2 pin so we can ignore it.
      // It still needs to be fired by the ADXL345 so it can set itselt to low power mode
      // It will alos be in the interruptSource even though it was not interrupted on this pin.
      /*
      if (interruptSource & ACCEL_INACTIVITY) {
        Serial.println("### Inactivity");
      }
      */

      if (interruptSource & ACCEL_ACTIVITY) {
        Serial.println("### Activity");
      }

      if (interruptSource & ACCEL_DOUBLE_TAP) {
        Serial.println("### DOUBLE_TAP");
      } else if (interruptSource & ACCEL_SINGLE_TAP) {
        // when a double tap is detected also a single tap is deteced. we use an else here so that we only print the double tap
        Serial.println("### SINGLE_TAP");
      }
    }

    interrupt_flag = 0;
  }

  now = millis();

  if (timer_state == T_SETTING) {

    if (setting_state != S_NONE) {
      setting_none_time = 0;
    }

    switch(setting_state) {
      case S_REDUCE_FAST:
        setting_update_last = now;
        if (display.getMinutes() < FORCE_SLOW_THRESHOLD) {
          // Force it to be slow
          setting_state = S_REDUCE_SLOW;
        } else {
          display.decrementMinutes(10);
        }
        break;

      case S_REDUCE_MED:
        if (display.getMinutes() < FORCE_SLOW_THRESHOLD) {
          // Force it to be slow
          setting_state = S_REDUCE_SLOW;
        } else {
          setting_update_last = now;
          display.decrementMinutes(5);
        }
        break;

      case S_REDUCE_SLOW:
        if (now - 500 > setting_update_last) {
          setting_update_last = now;
          display.decrementMinutes();
        }
        break;

      case S_NONE:

        //if (timer_state == T_SETTING) {
          if (setting_none_time == 0) {
            setting_none_time = now;
          } else if (now - setting_none_time > SETTING_WAIT) {
            timer_state = T_OFF;
          }
        //}


        break;

      case S_INCREASE_SLOW:
        if (now - 500 > setting_update_last) {
          setting_update_last = now;
          display.incrementMinutes();
        }
        break;

      case S_INCREASE_MED:
        //if (now - 150 > setting_update_last) {
          setting_update_last = now;
          display.incrementMinutes(5);
        //}
        break;

      case S_INCREASE_FAST:
        //if (now - 100 > setting_update_last) {
          setting_update_last = now;
          display.incrementMinutes(10);
        //}
        break;
    }
  }

  switch(timer_state) {
    case T_COUNTDOWN:
      //breath(breath_speed, breath_r, breath_g, breath_b);
      breathWave(breath_speed, breath_steps, breath_r, breath_g, breath_b);
      break;

    case T_ALARM:
      breath(250.0, breath_r, breath_g, breath_b); // Funky flashing red
      if (alarm_start == 0) {
        // Start the alarm
        alarm_start = millis();
        digitalWrite(PIN_SOUND, HIGH); // ON
      } else {
        // Handle the running alarm
        byte finished_sound = 0;
        byte finished_light = 0;

        if (now - alarm_start > ALARM_SOUND_SECONDS) {
          // stop the sound
          finished_sound = 1;
          digitalWrite(PIN_SOUND, LOW); // OFF
        }
        if (now - alarm_start > ALARM_LIGHT_SECONDS) {
          // stop the light
          finished_light = 1;

        }

        if (finished_sound && finished_light) {
          timer_state = T_OFF;
          alarm_start = 0;
        }

      }
      break;


    case T_SETTING:
      // Show the changed values now.
      displayUpdate();
      break;

    case T_OFF:
      // Turn off
      if (DEBUG) {
        Serial.println("Sleep now");
        Serial.flush();
      }
      goToSleep();
      // Wake up
      timer_state = T_WOKE;
      if (DEBUG) {
        Serial.println("Wake now");
      }

      break;

    case T_WOKE:
      settingStart();
      break;

    case T_ERROR:
      break;
  }

  interruptSource = 0;
}
