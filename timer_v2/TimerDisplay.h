/*
  TimerDisplay.h - Library for sending data to the oled.

  Created by Peter Clarke.
*/
#ifndef TIMERDISPLAY_H
#define TIMERDISPLAY_H

#include <stdint.h>
#include <Arduino.h>

class TimerDisplay{
  public:
    typedef struct{
      uint16_t vcc;
      uint16_t minutes;
      uint8_t seconds;
      uint8_t mode;
    }
    data_t;

    // Constructor
    TimerDisplay();

    // Count down 1 second
    void decrementTime();
    uint8_t finished();

    // How many minutes to display
    uint16_t getMinutes();
    void setMinutes(uint16_t val);
    void decrementMinutes();
    void incrementMinutes();
    void decrementMinutes(uint16_t val);
    void incrementMinutes(uint16_t val);

    // How many seconds to display
    uint8_t getSeconds();
    void setSeconds(uint8_t val);

    // The millivolts reported by the device in the message
    uint16_t getVcc();
    void setVcc(uint16_t vcc);

    // The mode
    uint8_t getMode();
    void setMode(uint8_t val);

  private:
    data_t _data;

};

#endif

