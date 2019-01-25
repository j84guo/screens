#include <stdio.h>

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/types.h>

void sys_error(char *name, int code)
{
  perror(name);
  exit(code);
}

int command(char *path, char **args)
{
  pid_t pid = fork();

  if (pid == -1) {
    return -1;
  } else if (!pid) {
    execvp(path, args);
    sys_error("execvp", 1);
  }

  // the only remaining reason for failure (considering we've hardcoded
  // arguments) is EINTR
  if (waitpid(pid, NULL, 0) == -1) {
    return -1;
  }

  return 0;
}

void demo_command()
{
  char *path = "false";
  char *args[] = {"ls", "-la", NULL};

  if (command(path, args) == -1) {
    perror("fork_exec_wait");
  }
}

int main()
{
  int fdm;
  int res;

  // initial directory contents
  if (system("ls -l /dev/ttys*") == -1) {
    sys_error("system", 1);
  }

  fdm = posix_openpt(O_RDWR);
  if (fdm == -1) {
    sys_error("posix_openpt", 1);
  }

  // device file path associated with pseudo terminal
  printf("\nslave: %s\n", ptsname(fdm));

  res = grantpt(fdm);
  if (res == -1) {
    sys_error("grantpt", 1);
  }

  res = unlockpt(fdm);
  if (res == -1) {
    sys_error("unlockpt", 1);
  }

  // show creation of slave side
  printf("\n");
  if (system("ls -l /dev/ttys*") == -1) {
    sys_error("system", 1);
  }
}
