#include <string>
#include <stdexcept>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/resource.h>

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

/* Stdin from /dev/null, stdout and stderr append to a file */
bool redirect_stddes(std::string path="")
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
      !redirect_stddes(path)) {
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
  }
}
