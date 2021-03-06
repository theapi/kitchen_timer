#include "TimerDisplay.h"

TimerDisplay::TimerDisplay()
{
  _data.vcc = 0;
  _data.minutes = 90;
  _data.seconds = 0;
  _data.mode = 0;
}

void TimerDisplay::decrementTime()
{
  if (_data.mode == 1) {
    // Do nothing
    return;
  }
  
  if (_data.seconds == 0) {
    _data.seconds = 60;
    this->decrementMinutes();
  }
  
  --_data.seconds;

  if (_data.minutes == 0 && _data.seconds == 0) {
    // No time left
    _data.mode = 1;
  }
}

void TimerDisplay::decrementMinutes()
{
  if (_data.minutes > 0) {
    --_data.minutes;
  }
}

void TimerDisplay::decrementMinutes(uint16_t val)
{
  if (_data.minutes > val) {
    _data.minutes -= val;
  }
}

void TimerDisplay::incrementMinutes()
{
  if (_data.minutes < 999) {
    ++_data.minutes;
  }
}

void TimerDisplay::incrementMinutes(uint16_t val)
{
  if (_data.minutes < 999 - val) {
  _data.minutes += val;
  }
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
