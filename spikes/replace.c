#include <stdio.h>
#include <unistd.h>

int main()
{
  // clear
  printf("\033[?1049h\033[H");
  printf("hello\n");
  sleep(1);
  printf("bye\n");
  sleep(1);
  // replace
  printf("\033[?1049l");
}
