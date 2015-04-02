#include "TimerDisplay.h"

TimerDisplay::TimerDisplay()
{
  _data.vcc = 0;
  _data.minutes = 0;
  _data.seconds = 0;
  _data.mode = 1; // Finished
}

void TimerDisplay::decrementTime()
{
  if (_data.minutes == 0) {
    // No time left
    _data.mode = 1;
  } else {
    --_data.seconds;
    if (_data.seconds < 1) {
      --_data.minutes;
      if (_data.minutes > 0) {
        _data.seconds = 59;
      }
    }
  }
}

void TimerDisplay::decrementMinutes()
{
  if (_data.minutes > 0) {
    --_data.minutes;
  }
  if (_data.minutes == 0) {
    _data.mode = 1; // Finished
  }
}

void TimerDisplay::incrementMinutes()
{
  ++_data.minutes;
}

uint8_t TimerDisplay::finished()
{
  if (_data.mode == 1) {
    return 1;
  }

  return 0;
}

// The millivolts reported by the device in the message
uint16_t TimerDisplay::getVcc()
{
  return _data.vcc;
}

void TimerDisplay::setVcc(uint16_t vcc)
{
  _data.vcc = vcc;
}

uint16_t TimerDisplay::getMinutes()
{
  return _data.minutes;
}

void TimerDisplay::setMinutes(uint16_t val)
{
  _data.minutes = val;
}

uint8_t TimerDisplay::getSeconds()
{
  return _data.seconds;
}

void TimerDisplay::setSeconds(uint8_t val)
{
  _data.seconds = val;
}

uint8_t TimerDisplay::getMode()
{
  return _data.mode;
}

void TimerDisplay::setMode(uint8_t val)
{
  _data.mode = val;
}
