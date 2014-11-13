
void countdownStart()
{
  if (timer_state == T_ERROR) return;

  // If no valid time has been set, then just turn off.
  if (display_number == 0) {
    timer_state = T_OFF;
  } else {
    timer_state = T_COUNTDOWN;
    restartCountDownTimers();
  }
}

void countdownUpdate()
{
  if (timer_state == T_COUNTDOWN) {
    if (display_number > 0) {
      // Coutdown 1 minute per update.
      --display_number;
    }
    
    if (display_number == 0) {
      timer_state = T_ALARM; 
      // Countdown finished, no dot now.
      dotOff();
    }
  }
}

void countdownResume()
{
  timer_state = T_COUNTDOWN;
  restartCountDownTimers();
}

void restartCountDownTimers()
{
  timer.restartTimer(timer_dot_blink);
  timer.restartTimer(timer_dot_move);
  timer.restartTimer(timer_countdown);
  dot_state = 0b00011000;
  
  timer.enable(timer_dot_blink);
  timer.enable(timer_dot_move);
  timer.enable(timer_countdown);
}
