#include "utils.h"

#include <stdexcept>

#include <stdio.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>

/* Ctrl-A */
const unsigned char ASCII_1 = 1;

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

/* Multiplex read on stdin and the pseudo-terminal master, expects a non-null
   fd_set */
int fdm_stdin_rselect(fd_set *rset, int fdm)
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

int write_all(int fd, char *buf, size_t len)
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
bool handle_stdin_read(int fdm)
{
  int res = fgetc(stdin);
  if (res != EOF) {
    unsigned char c = res;
    if (c == ASCII_1) {
      printf("(Got ctrl-a!)\r\n");
    } else {
      if (write_all(fdm, (char *) &c, 1) == -1) {
        sys_error("write");
      }
    }
  } else {
    return false;
  }

  return true;
}

/* Return whether the parent loop should continue or not (error or EOF) */
bool handle_fdm_read(int fdm)
{
  char buf[512];

  int res = read(fdm, buf, sizeof(buf));
  if (res > 0) {
    if (write_all(STDIN_FILENO, buf, res) == -1) {
      sys_error("write");
    }
  } else {
    return false;
  }

  return true;
}

/* Forwards raw bytes to slave, print slave output */
void run_parent(int fdm)
{
  fd_set rset;

  if (set_terminal_rawio() == -1) {
    sys_error("set_rawio");
  }

  bool cont = true;

  while (cont) {
    if (fdm_stdin_rselect(&rset, fdm) == -1) {
      sys_error("fdm_stdin_rselect");
    }

    if (FD_ISSET(STDIN_FILENO, &rset)) {
      cont = handle_stdin_read(fdm);
    } else if (FD_ISSET(fdm, &rset)) {
      cont = handle_fdm_read(fdm);
    }
  }

  close(fdm);
}

/* Side effect may be new session id and group id! */
int acquire_ctty(int fdm)
{
  int res = -1;

  char *path = ptsname(fdm);
  if (path) {
    if (setsid() != -1) {
      int fds = open(path, O_RDWR);
      if (fds != -1) {
        if (reset_stddes(fds)) {
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
  if (acquire_ctty(fdm) == -1) {
    sys_error("acquire_ctty");
  }

  execl("/bin/bash", "bash", NULL);
  sys_error("execl");
}

/* Todo:
   0. Implement arrow menu and screen clear/restore
   1. Intercept screens commands, e.g. list windows, switch window. Commands are
      two bytes: 3 (ctrl-a) and some identifier
   2. Track windows, switch to next window when one closes and terminate when
      last one closes. This includes maintaining a circular buffer for each
      window representing the last N bytes outputtted; we display this when a
      use switches terminals!
   3. Daemonize server and communicate with it via Unix socket */
void demo()
{
  if (!isatty(STDIN_FILENO)) {
    throw std::runtime_error("Stdin must be connected to a terminal.");
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
    return EXIT_FAILURE;
  }
}
