#include "menu.h"
#include "utils.h"
#include "window.h"

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <stdexcept>

#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>


/* Window switch directions */
enum class SwitchDir {
  NEXT,
  PREV
};

/* Ctrl-A */
const unsigned char ASCII_1 = 1;

/* Global window state */
int nextWindowID = 0;
int currentWindow = 0;
const int SCROLLBACK_CAPACITY = 1024;
std::vector<std::unique_ptr<Window>> windows;

/* Forward declarations */
void runChild(int fdm);

std::vector<std::string> getWindowLabels()
{
  std::vector<std::string> res;

  for (auto &ptr : windows) {
    std::string label = std::to_string(ptr->WID) + " bash";
    res.push_back(label);
  }

  return res;
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

Window &getWindow(int i)
{
  return *windows.at(i);
}

Window &addNewWindow()
{
  Window *window = new Window(nextWindowID++, SCROLLBACK_CAPACITY);
  /* Since Window wraps a potentially large RingBuffer, we move construct it
     into the vector, which attempts to move all members recursively by default
     or uses any user-supplied move constructor

     Note that std::vector will try to use the move constructor when resizing
     itself */
  windows.push_back(std::unique_ptr<Window>(window));

  currentWindow = windows.size() - 1;
  return getWindow(currentWindow);
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

void handleSwitchWindow(SwitchDir dir)
{
  printf("%s", CLEAR);

  if (dir == SwitchDir::NEXT) {
    printf("[Next screen]\r\n");
    currentWindow = (currentWindow + 1) % windows.size();
  } else {
    printf("[Previous screen]\r\n");
    currentWindow = !currentWindow ? windows.size() - 1 : currentWindow - 1;
  }

  size_t res;
  char buf[512];

  /* in order to read non-destructively, we make a copy
     todo: avoid unnecessary buffering */
  RingBuffer ringBuf = getWindow(currentWindow).buffer;

  /* Dump the buffer which represents the last N bytes of output */
  while ((res = ringBuf.read(buf, sizeof(buf))) > 0) {
    if (writeAll(STDOUT_FILENO, buf, res) == -1) {
      sysError("writeAll");
    }
  }
}

void handleCreateWindow()
{
  printf("%s", CLEAR);
  printf("[Create screen]\r\n");
  Window &window = addNewWindow();
  forkWindow(window);
}

void handleSelectWindow()
{
  printf("%s", CLEAR);
  printf("[List screens]\r\n");

  std::vector<std::string> options = getWindowLabels();
  Menu menu(options, true, false);

  int choice = menu.run();
  if (choice == Menu::NOCHOICE) {
    return;
  }

  currentWindow = choice;
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
    handleSelectWindow();
    break;
  }
  case KEY_LOWER_C: {
    handleCreateWindow();
    break;
  }
  case KEY_LOWER_N: {
    handleSwitchWindow(SwitchDir::NEXT);
    break;
  }
  case KEY_UPPER_N: {
    handleSwitchWindow(SwitchDir::PREV);
    break;
  }}

  return true;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleStdinRead(Window &window)
{
  int res = fgetc(stdin);
  if (res == EOF) {
    return false;
  }

  unsigned char c = res;
  if (c == ASCII_1) {
    return handleScreenCommand();
  } else {
    if (writeAll(window.fdm, (char *) &c, 1) == -1) {
      sysError("write");
    }
  }

  return true;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handleFdmRead(Window &window)
{
  char buf[512];

  int res = read(window.fdm, buf, sizeof(buf));
  if (res > 0) {
    /* Remember the last N bytes output from the window
       todo: start from first full line */
    window.buffer.write(buf, res);
    /* Write to STDOUT */
    if (writeAll(STDOUT_FILENO, buf, res) == -1) {
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
    Window &window = getWindow(currentWindow);
    if (fdmStdinRselect(&rset, window.fdm) == -1) {
      sysError("fdmStdinRselect");
    }

    /* Act on current Window */
    if (FD_ISSET(STDIN_FILENO, &rset)) {
      cont = handleStdinRead(window);
    } else if (FD_ISSET(window.fdm, &rset)) {
      cont = handleFdmRead(window);
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
   2. - [DONE] Function for creating a new window
      - [DONE] In runChild() accept a window object
      - [DONE] Track windows in a global vector
      - [DONE] Create new
      - [DONE] Switch to next/previous window
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
