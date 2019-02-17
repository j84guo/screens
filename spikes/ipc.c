#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>

void sysError(char *name, int code)
{
  perror(name);
  exit(code);
}

int make_pty()
{
  int fdm = posix_openpt(O_RDWR);
  if (fdm == -1) {
    return -1;
  }

  /* On error, cleanup the allocated descriptor; close() always releases the
     descriptor, but may report associated errors, e.g. closing a file on a
     network filesystem. We ignore any such errors as they will be reported
     on subsequent I/O */
  if (grantpt(fdm) == -1 ||
      unlockpt(fdm) == -1) {
    close(fdm);
    return -1;
  }
  return fdm;
}

void run_parent(int fdm)
{
  char input[150];

  while (1) {
    /* write() prompt (since stdout is line-buffered) and read from stdin */
    write(1, "Input: ", 7);

    int res = read(STDIN_FILENO, input, sizeof(input));
    if (res > 0) {
      /* write to child and read response */
      write(fdm, input, res);

      /* Writing ctrl-D (4) into the master fd closes the stdin of the slave
         side. Writing ctrl-C (3) sends SIGINT to the foreground process group
         of the slave side (only if the slave is the controlling terminal for a
         session).

         Also, in non-raw mode, master input is echoed back in addition to the
         child output, sleep(1) is a hack which can wait until the child sends
         something back */
      res = read(fdm, input, sizeof(input) - 1);

      if (res > 0) {
        input[res] = '\0';
        fprintf(stderr, "%s", input);
      } else {
        break;
      }
    } else {
      break;
    }
  }
}

void print_termios(struct termios *tc)
{
  printf("%lu %lu %lu %lu\n",
      tc->c_iflag, tc->c_oflag, tc->c_cflag, tc->c_lflag);
}

/* Print the foregroud process group id for the specified terminal */
void print_ctty_fgp(int fds)
{
  int fgp = tcgetpgrp(fds);
  if (fgp == -1) {
    sysError("tcgetpgrp", 1);
  }
  printf("fgp: %d\n", fgp);
}

/* Replace std descriptors with fd */
void resetStddes(int fd)
{
  close(0);
  close(1);
  close(2);

  if (dup(fd) == -1 ||
      dup(fd) == -1 ||
      dup(fd) == -1) {
    sysError("dup", 1);
  }
}

/* To acquire a new controlling terminal, the process needs new session. It then
   opens a tty file wihtout the O_NOCTTY flag to gain a controlling terminal */
void init_new_ctty(int fdm)
{
  struct termios old_settings;
  struct termios new_settings;

  if (setsid() == -1) {
    sysError("setsid", 1);
  }

  char *path = ptsname(fdm);
  if (!path) {
    sysError("ptsname", 1);
  }

  int fds = open(path, O_RDWR);
  if (fds == -1) {
    sysError("open", 1);
  }

  close(fdm);

  /* New terminal settings are based on the old ones, which we also save */
  if (tcgetattr(fds, &old_settings) == -1) {
    sysError("tcgetattr", 1);
  }
  new_settings = old_settings;

  /* With no-raw I/O, input in the master is echoed back, which is how terminal
     emulators show what you just typed. Raw I/O is like pipes */
  cfmakeraw(&new_settings);
  if (tcsetattr(fds, TCSANOW, &new_settings) == -1) {
    sysError("tcsetattr", 1);
  }

  resetStddes(fds);
  close(fds);
}

void run_child(int fdm)
{
  char input[150];
  init_new_ctty(fdm);

  while (1) {
    int res = read(0, input, sizeof(input) - 1);
    if (res > 0) {
      /* Expecting terminal input (lines), so we just add a null terminator */
      input[res] = '\0';
      printf("[CHILD] %s", input);
    } else {
      break;
    }
  }

  // execl("/bin/bash", "bash", NULL);
  // sysError("execl", 1);
}

int main()
{
  int fdm = make_pty();
  if (fdm == -1) {
    sysError("make_pty", 1);
  }

  pid_t pid = fork();
  if (pid == -1) {
    sysError("fork", 1);
  } else if (pid) {
    run_parent(fdm);
  } else {
    run_child(fdm);
  }
}
