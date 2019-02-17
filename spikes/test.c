#include <stdio.h>

int main()
{
  char a = '[';
  unsigned char b = ']';
  
  // bit patterns mmay be the same, but equality is determined logically
  printf("%d\n", a == b);
  printf("a=%d, b=%d\n", a, b);
}
