#include "utils.h"

#include <string>

#include <errno.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>


/* Return as orphan. Note parent calls exit() which does NOT stack unwind */
int fork_orphan()
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
  if (!fork_orphan() ||
      setsid() == -1 ||
      !fork_orphan() ||
      chdir("/") == -1 ||
      !daemonize_stddes(path)) {
    return false;
  }
  umask(022);
  return true;
}

/* Create a file with mode, optionally failing if it already exists (EEXIST).
   Note that with must_exist=false, the file may exist with different mode! */
bool create_file(const std::string &path, int mode, bool must_exist=true)
{
  int fd = open(path.c_str(), O_WRONLY | O_CREAT | O_EXCL, mode);
  if (fd == -1 && (must_exist || errno != EEXIST)) {
    return false;
  }

  close(fd);
  return true;
}

/* Demonstrate a daemon */
void demo()
{
  std::string path = "/Users/jacguo/Projects/screens/out.txt";
  if (!create_file(path, 0644, false)) {
    sys_error("create_file");
  }

  if (!daemonize(path)) {
    sys_error("daemonize");
  }
  printf("I'm a daemon!\n");
}

/* Note uncaught exceptions may not unwind the stack */
int main()
{
  try {
    demo();
  } catch (const std::exception &ex) {
    fprintf(stderr, "%s\n", ex.what());
    return EXIT_FAILURE;
  }
}
