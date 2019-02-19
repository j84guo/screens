#include "menu.h"
#include "utils.h"
#include "window.h"

#include <string>
#include <utility>
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

/* Global window state */
int nextWindowID = 0;
int currentWindow = 0;
const int SCROLLBACK_CAPACITY = 1024;
std::vector<Window> windows;

/* Forward declarations */
void runChild(int fdm);

std::vector<std::string> getWindowLabels()
{
  return std::vector<std::string>();
}

void forkWindow(Window &window)
{
  pid_t pid = fork();
  if (pid == -1) {
    sysError("fork");
  } else if (pid > 0) {
    window.PID = pid;
  } else {
    runChild(window.fdm);
  }
}

Window &addNewWindow()
{
  Window window(nextWindowID++, SCROLLBACK_CAPACITY);
  /* Since Window wraps a potentially large RingBuffer, we move construct it
     into the vector, which attempts to move all members recursively by default
     or uses any user-supplied move constructor */
  windows.push_back(std::move(window));
  currentWindow = windows.size() - 1;
  return windows.at(currentWindow);
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

  return len;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleScreenCommand()
{
  int res = fgetc(stdin);
  if (res == EOF) {
    return false;
  }

  switch (res) {
  case KEY_DQUOTE: {
    printf("[List screens]\r\n");
    break;
  } 
  case KEY_LOWER_C: {
    printf("[Create screen]\r\n");
    Window &window = addNewWindow();
    forkWindow(window);
    break;
  }
  case KEY_LOWER_N: {
    printf("[Next screen]\r\n");
    currentWindow = (currentWindow + 1) % windows.size();
    break;
  }
  case KEY_UPPER_N: {
    printf("[Previous screen]\r\n");
    currentWindow = !currentWindow ? windows.size() - 1 : currentWindow - 1; 
    break;
  }}

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
void runParent()
{
  if (!setTerminalRawio()) {
    sysError("set_rawio");
  }

  fd_set rset;
  bool cont = true;

  while (cont) {
    int fdm = windows.at(currentWindow).fdm;
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

  Window &window = addNewWindow();

  /* Note the parent closing causes the child to receive SIGHUP while the child
     exiting causes the parent to read EOF from fdm */
  pid_t pid = fork();
  if (pid == -1) {
    sysError("fork");
  } else if (pid) {
    window.PID = pid;
    runParent();
  } else {
    runChild(window.fdm);
  }
}

/* Todo:
   0. [DONE] Implement arrow menu and screen clear/restore
   1. [DONE] Intercept screens commands, e.g. list windows, switch window, create
      window. Commands are two bytes: 3 (ctrl-a) and some identifier
   2. - Function for creating a new window
      - In runChild() accept a window object
      - Track windows in a global vector
      - Create new
      - Switch to next/previous window
      - Detect when a window closes and either update view or terminate when
        last one closes

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
