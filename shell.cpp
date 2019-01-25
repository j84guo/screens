#include <string>
#include <stdexcept>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/select.h>

/* Original terminal settings */
struct termios saved_settings;

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

void sys_error(const std::string &name)
{
  throw std::runtime_error(name + ": " + str_error(errno));
}

/* Create a pseudo-terminal pair */
int make_pty()
{
  int fdm = posix_openpt(O_RDWR);
  if (fdm != -1) {
    if (grantpt(fdm) != -1 &&
        unlockpt(fdm) != -1) {
      return fdm;
    }
    close(fdm);
  }

  return -1;
}

/* Multiplex read on stdin and the pseudo-terminal master */
int fdm_stdin_rselect(fd_set *rset, int fdm)
{
  if (!rset) {
    return -1;
  }

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

/* Restore original terminal settings */
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

int write_all(int fd, char *buf, size_t len)
{
  size_t i = 0;

  while (i < len) {
    int res = write(fd, buf + i, len);
    if (res == -1) {
      return -1;
    }
    i += res;
  }

  return 0;
}

/* Forwards raw bytes to slave, print slave output */
void run_parent(int fdm)
{
  fd_set rset;
  char buf[512];
  int in_fd, out_fd;

  if (set_terminal_rawio() == -1) {
    sys_error("set_rawio");
  }

  while (1) {
    int res = fdm_stdin_rselect(&rset, fdm);
    if (res == -1) {
      sys_error("fdm_stdin_rselect");
    }

    if (FD_ISSET(STDIN_FILENO, &rset)) {
      in_fd = STDIN_FILENO;
      out_fd = fdm;
    } else if (FD_ISSET(fdm, &rset)) {
      in_fd = fdm;
      out_fd = STDOUT_FILENO;
    }

    res = read(in_fd, buf, sizeof(buf));
    if (res > 0) {
      if (write_all(out_fd, buf, res) == -1) {
        sys_error("write");
      }
    } else {
      break;
    }
  }
}

/* Set standard descriptors to fd */
int reset_stddes_to(int fd)
{
  close(0);
  close(1);
  close(2);

  if (dup(fd) == -1 ||
      dup(fd) == -1 ||
      dup(fd) == -1) {
    return -1;
  }
  return 0;
}

/* Side effect may be new session id and group id! */
int get_new_ctty(int fdm)
{
  int res = -1;

  char *path = ptsname(fdm);
  if (path) {
    if (setsid() != -1) {
      int fds = open(path, O_RDWR);
      if (fds != -1) {
        if (reset_stddes_to(fds) != -1) {
          res = 0;
          close(fdm);
        }
        close(fds);
      }
    }
  }

  return res;
}

/* Obtain new controlling tty (slave) from fdm (master) and exec Bash */
void run_child(int fdm)
{
  if (get_new_ctty(fdm) == -1) {
    sys_error("get_new_ctty");
  }

  execl("/bin/bash", "bash", NULL);
  sys_error("execl");
}

/* Todo:
   0. Implement arrow menu and screen clear/restore
   1. Intercept screens commands, e.g. list windows, switch window. Commands are
      two bytes: 3 (ctrl-a) and some identifier
   2. Track windows, switch to next window when one closes and terminate when
      last one closes
   3. Daemonize server and communicate with it via Unix socket */
void demo()
{
  if (!isatty(STDIN_FILENO)) {
    fprintf(stderr, "Stdin must be connected to a terminal.\n");
    return 1;
  }

  int fdm = make_pty();
  if (fdm == -1) {
    sys_error("make_pty");
  }

  /* Note the parent closing causes the child to receive SIGHUP while the child
     exiting causes the parent to read EOF from fdm */
  pid_t pid = fork();
  if (pid == -1) {
    sys_error("fork");
  } else if (pid) {
    run_parent(fdm);
  } else {
    run_child(fdm);
  }
}

int main()
{
  try {
    demo();
  } catch (const std::exception &ex) {
    fprintf(stderr, "%s\r\n", ex.what());
  }
}
