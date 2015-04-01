
void countdownStart()
{
  if (timer_state == T_ERROR) return;

  // If no valid time has been set, then just turn off.
  if (display_number == 0) {
    timer_state = T_OFF;
  } else {
    timer_state = T_COUNTDOWN;
    countdownTimersRestart();
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
      // Countdown finished
      // @todo: animated alarm?
    }
  }

  displayUpdate();
}

void countdownResume()
{
  timer_state = T_COUNTDOWN;
  countdownTimersRestart();
}

void countdownTimersRestart()
{
  timer.restartTimer(timer_countdown);
  timer.enable(timer_countdown);
}
