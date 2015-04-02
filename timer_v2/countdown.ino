
void countdownStart()
{
  if (timer_state == T_ERROR) return;

  // If no valid time has been set, then just turn off.
  if (display.finished()) {
    timer_state = T_OFF;
  } else {
    timer_state = T_COUNTDOWN;
    countdownTimersRestart();
  }
}

void countdownUpdate()
{
  if (timer_state == T_COUNTDOWN) {

    display.decrementTime();

    if (display.finished()) {
      timer_state = T_ALARM;
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
