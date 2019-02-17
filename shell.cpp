#include "menu.h"
#include "utils.h"

#include <string>
#include <vector>
#include <stdexcept>

#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>


/* Ctrl-A */
const unsigned char ASCII_1 = 1;

// int currentWindow;
// std::vector<Window> windows;
// 
// std::vector<std::string> getWindowNames();

/* Create a pseudo-terminal pair */
int makePTY()
{
  int fdm = posix_openpt(O_RDWR);
  if (fdm != -1) {
    if (grantpt(fdm) != -1 && unlockpt(fdm) != -1) {
      return fdm;
    }
    close(fdm);
  }

  return -1;
}

/* Multiplex read on stdin and the pseudo-terminal master, expects a non-null
   fd_set */
int fdmStdinRselect(fd_set *rset, int fdm)
{
  FD_ZERO(rset);
  FD_SET(fdm, rset);
  FD_SET(STDIN_FILENO, rset);

  return select(
    fdm + 1, /* largest + 1 */
    rset, /* read */
    NULL, /* write */
    NULL, /* error */
    NULL /* timeout */
  );
}

/* Unix write() may only process some of the request bytes, it may also be
   interrupted by a signal. This helper continues writing untill all requested
   bytes are processed, or I/O error */
int writeAll(int fd, char *buf, size_t len)
{
  size_t i = 0;

  while (i < len) {
    int res = write(fd, buf + i, len);
    if (res == -1) {
      if (errno == EINTR) {
        continue;
      }
      return -1;
    }
    i += res;
  }

  return 0;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleScreenCommand()
{
  int res = fgetc(stdin);
  if (res == EOF) {
    return false;
  }

  switch (res) {
  case KEY_DQUOTE:
    printf("List screens\r\n");
    break;
  case KEY_LOWER_C:
    printf("Create screen\r\n");
    break;
  case KEY_LOWER_N:
    printf("Next screen\r\n");
    break;
  case KEY_UPPER_N:
    printf("Previous screen\r\n");
    break;
  }

  return true;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleStdinRead(int fdm)
{
  int res = fgetc(stdin);
  if (res == EOF) {
    return false;
  }

  unsigned char c = res;
  if (c == ASCII_1) {
    return handleScreenCommand();
  } else {
    if (writeAll(fdm, (char *) &c, 1) == -1) {
      sysError("write");
    }
  }

  return true;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleFdmRead(int fdm)
{
  char buf[512];

  int res = read(fdm, buf, sizeof(buf));
  if (res > 0) {
    if (writeAll(STDIN_FILENO, buf, res) == -1) {
      sysError("writeall");
    }
  } else {
    return false;
  }

  return true;
}

/* Forwards raw bytes to slave, print slave output */
void runParent(int fdm)
{
  fd_set rset;

  if (!setTerminalRawio()) {
    sysError("set_rawio");
  }

  bool cont = true;

  while (cont) {
    if (fdmStdinRselect(&rset, fdm) == -1) {
      sysError("fdmStdinRselect");
    }

    /* Act on current Window */
    if (FD_ISSET(STDIN_FILENO, &rset)) {
      cont = handleStdinRead(fdm);
    } else if (FD_ISSET(fdm, &rset)) {
      cont = handleFdmRead(fdm);
    }
  }

  close(fdm);
}

/* Side effect may be new session id and group id! */
bool acquireCTTY(int fdm)
{
  int res = false;

  char *path = ptsname(fdm);
  if (path) {
    if (setsid() != -1) {
      int fds = open(path, O_RDWR);
      if (fds != -1) {
        if (resetStddes(fds)) {
          res = true;
          close(fdm);
        }
        close(fds);
      }
    }
  }

  return res;
}

/* Obtain new controlling tty (slave) from fdm (master) and exec Bash */
void runChild(int fdm)
{
  if (!acquireCTTY(fdm)) {
    sysError("acquireCTTY");
  }

  execl("/bin/bash", "bash", NULL);
  sysError("execl");
}

void demoShell()
{
  if (!isatty(STDIN_FILENO)) {
    throw std::runtime_error("Stdin must be connected to a terminal.");
  }

  int fdm = makePTY();
  if (fdm == -1) {
    sysError("makePTY");
  }

  /* Note the parent closing causes the child to receive SIGHUP while the child
     exiting causes the parent to read EOF from fdm */
  pid_t pid = fork();
  if (pid == -1) {
    sysError("fork");
  } else if (pid) {
    runParent(fdm);
  } else {
    runChild(fdm);
  }
}

/* Todo:
   0. [DONE] Implement arrow menu and screen clear/restore
   1. [DONE] Intercept screens commands, e.g. list windows, switch window, create
      window. Commands are two bytes: 3 (ctrl-a) and some identifier
   2. Track windows in a global vector, switch to next window when one closes
      and terminate when last one closes.

      Note:
      This includes maintaining a circular buffer for each window representing
      the last N bytes outputtted

   3. Daemonize server and make a client to communicate with it via Unix
      socket */
int main()
{
  try {
    demoShell();
  } catch (const std::exception &ex) {
    fprintf(stderr, "%s\r\n", ex.what());
    return EXIT_FAILURE;
  }
}
