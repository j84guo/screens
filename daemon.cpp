#include "utils.h"

#include <string>

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


/* Return as orphan. Note parent calls exit() which does NOT stack unwind */
int forkOrphan()
{
  pid_t pid = fork();

  if (pid == -1) {
    return false;
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  return true;
}

/* 1. fork (cannot setsid as a PRGRP leader, e.g. shell job)
   2. setsid to lose tty
   3. fork (opening a tty as session leader acquires it as controlling tty )
   4. chdir to root (don't keep filesystems mounted)
   5. redirect standard descriptors
   6. reset any inherited umask to a reasonable 022 */
bool daemonize(const std::string &path)
{
  if (!forkOrphan() ||
      setsid() == -1 ||
      !forkOrphan() ||
      chdir("/") == -1 ||
      !daemonizeStddes(path)) {
    return false;
  }
  umask(022);
  return true;
}

/* Create a file with mode, optionally failing if it already exists (EEXIST).
   Note that with exclusive=false, any existing file may be truncated and have
   its permissions reset! */
bool createFile(const std::string &path, mode_t mode, bool exclusive=true)
{
  if (exclusive) {
    mode |= O_EXCL;
  }

  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, mode);
  if (fd == -1) {
    return false;
  }

  close(fd);
  return true;
}

/* Demonstrate a daemon */
void demoDaemon()
{
  std::string path = "/Users/jacguo/Projects/screens/out.txt";
  if (!createFile(path, 0644, false)) {
    sysError("createFile");
  }

  if (!daemonize(path)) {
    sysError("daemonize");
  }
  printf("I'm a daemon!\n");
}

/* Note uncaught exceptions may not unwind the stack */
int main()
{
  try {
    demoDaemon();
  } catch (const std::exception &ex) {
    fprintf(stderr, "%s\n", ex.what());
    return EXIT_FAILURE;
  }
}
