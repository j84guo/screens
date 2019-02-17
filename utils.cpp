#include "utils.h"

#include <stdexcept>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/resource.h>


/* A termios structure which holds the terminal settings on startup, and should
   be restored on exit */
struct termios saved_settings;

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
std::string str_error(int err)
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
   program, while logic_error reports programming mistakes. sys_error is
   intended to be called when system call failures occur and we can't recover,
   e.g. out of of memory or unable to change session, so runtime_error makes
   more sense.

   Note that the exception/error_code/error_condition/error_category system in
   C++ is rather unfortunately designed, so we stick with the two basic
   exceptions (logic and runtime) */
void sys_error(const std::string &name)
{
  throw std::runtime_error(name + ": " + str_error(errno));
}

/* Soft limit, returned here, is the kernel-enforced limit for a resource while
   hard limit is its ceiling. An unprivileged process may set its soft limit up
   to its hard one, but not over. A privileged process may change either! */
int max_fds()
{
  struct rlimit buf;
  getrlimit(RLIMIT_NOFILE, &buf);
  return buf.rlim_cur;
}

/* Redirect stdin from /dev/null; stdout and stderr append to a filesystem path,
   which defaults to /dev/null if not specified */
bool daemonize_stddes(std::string path)
{
  for (int i=0; i<max_fds(); ++i) {
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
bool reset_stddes(int fd)
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
void unset_terminal_rawio()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &saved_settings);
}

/* Set raw I/O on controlling terminal, restoring original settings on exit */
int set_terminal_rawio()
{
  struct termios new_settings;

  if (tcgetattr(STDIN_FILENO, &saved_settings) != -1) {
    new_settings = saved_settings;
    cfmakeraw(&new_settings);

    if (atexit(unset_terminal_rawio) != -1 &&
        tcsetattr(STDIN_FILENO, TCSANOW, &new_settings) != -1) {
      return 0;
    }
  }

  return -1;
}
