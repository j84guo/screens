#include "utils.h"

#include <stdexcept>

#include <fcntl.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/resource.h>


/* A termios structure which holds the terminal settings on startup, and should
   be restored on exit */
struct termios savedSettings;

/* strerror_r() comes in two versions

   POSIX:
   Copies an error message into the buffer, failing if err is invalid or the
   buffer doesn't have enough space. It is not specified whether any string is
   copied on failure or if a copied string string, on failure or success, is
   null terminated!

   GNU:
   Returns a pointer to an error message, either static or copied into the
   provided buffer. The string is truncated if needed but is always null
   terminated! Even so, we defend agains bugs and save a null byte...

   The max buffer length is not standardized, but 256 to 1024 is common. */
std::string strError(int err)
{
  char buf[256] = {0};
  const char *ptr = nullptr;
#if defined(__GLIBC__) && defined(_GNU_SOURCE)
  ptr = strerror_r(err, buf, sizeof(buf) - 1);
#else
  if (!strerror_r(err, buf, sizeof(buf) - 1)) {
    ptr = buf;
  } else {
    ptr = "Error, but the message could not be shown via (POSIX) strerror_r!";
  }
#endif
  return ptr;
}

/* A runtime_error reports errors which cannot be easily predicted by the
   program, while logic_error reports programming mistakes. sysError is
   intended to be called when system call failures occur and we can't recover,
   e.g. out of of memory or unable to change session, so runtime_error makes
   more sense.

   Note that the exception/error_code/error_condition/error_category system in
   C++ is rather unfortunately designed, so we stick with the two basic
   exceptions (logic and runtime) */
void sysError(const std::string &name)
{
  throw std::runtime_error(name + ": " + strError(errno));
}

/* Soft limit, returned here, is the kernel-enforced limit for a resource while
   hard limit is its ceiling. An unprivileged process may set its soft limit up
   to its hard one, but not over. A privileged process may change either! */
int maxFds()
{
  struct rlimit buf;
  getrlimit(RLIMIT_NOFILE, &buf);
  return buf.rlim_cur;
}

/* Redirect stdin from /dev/null; stdout and stderr append to a filesystem path,
   which defaults to /dev/null if not specified */
bool daemonizeStddes(std::string path)
{
  for (int i=0; i<maxFds(); ++i) {
    close(i);
  }

  std::string null = "/dev/null";
  if (path == "") {
    path = null;
  }

  if (open(null.c_str(), O_RDONLY) == -1 ||
      open(path.c_str(), O_WRONLY | O_APPEND) == -1 ||
      dup(1) == -1) {
    return false;
  }

  return true;
}

/* Set standard descriptors to fd */
bool resetStddes(int fd)
{
  close(0);
  close(1);
  close(2);

  if (dup(fd) == -1 ||
      dup(fd) == -1 ||
      dup(fd) == -1) {
    return false;
  }
  return true;
}

/* Restore original terminal settings. This is  */
void unsetTerminalRawIO()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &savedSettings);
}

/* Set raw I/O on controlling terminal, restoring original settings on exit
   Should only be called once! */
bool setTerminalRawio()
{
  struct termios new_settings;

  if (tcgetattr(STDIN_FILENO, &savedSettings) != -1) {
    new_settings = savedSettings;
    cfmakeraw(&new_settings);

    if (atexit(unsetTerminalRawIO) != -1 &&
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != -1) {
      return true;
    }
  }

  return false;
}

/* Create a pseudo-terminal pair

   Note: standards define O_NOCTTY for opening a PTY without it becoming the
   controlling terminal of the calling process, but this seems to be included
   for compatibility reasons */
int makePTY()
{
  int fdm = posix_openpt(O_RDWR | O_NOCTTY);
  if (fdm != -1) {
    if (grantpt(fdm) != -1 && unlockpt(fdm) != -1) {
      return fdm;
    }
    close(fdm);
  }

  return -1;
}
