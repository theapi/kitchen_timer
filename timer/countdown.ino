
void countdownStart()
{
  if (timer_state == T_ERROR) return;

  if (display_number == 0) {
    display_number = START_TIME;
  }
  timer_state = T_COUNTDOWN;
  restartCountDownTimers();
  
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
      bitWrite(dot_state, 4, 0);
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
