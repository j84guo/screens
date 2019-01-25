#include <stdio.h>
#include <string.h>

int get_line(char *msg, char *buf, int cap)
{
  fprintf(stdout, "%s", msg);
  if (!fgets(buf, cap, stdin)) {
    return -1;
  }

  buf[strlen(buf) - 1] = '\0';
  return 0;
}

// Pseudo-terminals are a solution to the problems outlined in the code below
// See also https://unix.stackexchange.com/questions/21147/what-are-pseudo-terminals-pty-tty
int main()
{
  char login_name[150];
  char password[150];

  if (get_line("Login: ", login_name, sizeof(login_name)) == -1) {
    fprintf(stderr, "Error getting login name\n");
    return 1;
  }

  // Clear stdin to remove characters entered between login_name and password
  // Now when re-directing a file to stdin, the programing is de-synchronized
  // with its input...
  fseek(stdin, 0, SEEK_END);

  if (get_line("Password: ", password, sizeof(password)) == -1) {
    fprintf(stderr, "Error getting password\n");
    return 1;
  }

  fprintf(stdout, "%s\n%s\n", login_name, password);
}
