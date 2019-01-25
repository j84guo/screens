/* Disable echo for the controlling TTY. The slave does this since the master
   will be forwarding its output down the original TTY (GUI), which already has
   echo enabled */
void disable_echo()
{
  struct termios settings;
  if (tcgetattr(STDIN_FILENO, &settings) == -1) {
    sys_error("tcgetattr", 1);
  }

  settings.c_lflag &= ~ECHO;
  if (tcsetattr(STDIN_FILENO, TCSANOW, &settings) == -1) {
    sys_error("tcsetattr", 1);
  }
}

void to_foreground()
{
  /* Expect success since 0 is hardcoded */
  pid_t pgid = getpgid(0);

  if (tcsetpgrp(STDIN_FILENO, pgid) == -1) {
    sys_error("tcsetpgrp", 1);
  }
}
