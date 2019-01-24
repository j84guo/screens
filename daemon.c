#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>

void sys_error(char *name, int code)
{
  perror(name);
  exit(code);
}

int max_fds()
{
  struct rlimit buf;
  getrlimit(RLIMIT_NOFILE, &buf);
  return buf.rlim_cur;
}

int redirect_stddes(char *path)
{
  for (int i=0; i<max_fds(); ++i) {
    close(i);
  }

  char *null = "/dev/null";
  path = path ? path : null;

  if (open(null, O_RDONLY) == -1 ||
      open(path, O_WRONLY | O_APPEND) == -1 ||
      dup(1) == -1) {
    return -1;
  }

  return 0;
}

int fork_orphan()
{
  pid_t pid = fork();

  if (pid == -1) {
    return -1;
  } else if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  return 0;
}

int daemonize(char *path)
{
  if (fork_orphan() == -1 ||
      setsid() == -1 ||
      fork_orphan() ||
      chdir("/") == -1 ||
      redirect_stddes(path) == -1) {
    return -1;
  }

  return 0;
}

int create_file(char *path, int mode)
{
  int fd = open(path, O_WRONLY | O_CREAT, mode);
  if (fd == -1) {
    return -1;
  }

  close(fd);
  return 0;
}

int main(int argc, char *argv[])
{
  char *path = "/Users/jacguo/Projects/snippets/out.txt";
  if (create_file(path, 0644) == -1) {
    sys_error("create_file", 1);
  }

  if (daemonize(path) == -1) {
    sys_error("daemonize", 1);
  }
  printf("I'm a daemon!\n");
}
