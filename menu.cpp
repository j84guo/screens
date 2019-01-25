#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

const char *BG_BLUE = "\x1b[44m";
const char *RESET = "\033[0m";
const char *CLEAR = "\e[1;1H\e[2J";

const char KEY_SPACE = 32;
const char KEY_ESC = 27;
const char KEY_LSQBR = 91;
const char KEY_UP = 65;
const char KEY_DOWN = 66;

struct termios saved_settings;

const char *dir_codes[] = {
  "\033[%dA",
  "\033[%dB",
  "\033[%dD",
  "\033[%dC"
};

enum cursor_dir {
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

const char *options[] = {
  "1. bash",
  "2. bash",
  "3. bash"
};

int num_options = 3;
int cur_option = 0;

void unset_terminal_rawio()
{
  tcsetattr(STDIN_FILENO, TCSANOW, &saved_settings);
}

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

void scroll(enum cursor_dir dir, int n)
{
  printf(dir_codes[dir], n);
}

void print_menu(int highlight)
{
  for (int i=0; i<num_options; ++i) {
    if (i == highlight) {
      printf("%s%s%s\r\n", BG_BLUE, options[i], RESET);
    } else {
      printf("%s\r\n", options[i]);
    }
  }
}

void update_menu(enum cursor_dir dir)
{
  scroll(DIR_UP, num_options);

  if (dir == DIR_UP) {
    cur_option = !cur_option ? num_options - 1 : (cur_option - 1) % num_options;
  } else {
    cur_option = (cur_option + 1) % num_options;
  }

  print_menu(cur_option);
}

void make_menu_space()
{
  for (int i=0; i<num_options; ++i) {
    printf("\r\n");
  }
}

int main(int argc, char *argv[])
{
  /* Raw mode: no echoing, no terminal driver processing, no line-buffering */
  set_terminal_rawio();

  /* Clear terminal screen
     Print newlines
     Move curor up
     Print menu line by line and choose first to highlight */
  printf("%s", CLEAR);
  printf("Use UP/DOWN to navigage, press SPACE to exit.\r\n");
  make_menu_space();
  scroll(DIR_UP, num_options);
  print_menu(0);

  /* In cursor mode, up/down arrow press sends 3 bytes: esc, ] and A/B  */
  int res;
  while ((res = fgetc(stdin)) != KEY_SPACE) {
    if (res == KEY_ESC && (res = fgetc(stdin)) == KEY_LSQBR) {
      switch ((res = fgetc(stdin))) {
      case KEY_UP:
        update_menu(DIR_UP);
        break;
      case KEY_DOWN:
        update_menu(DIR_DOWN);
        break;
      }
    }

    if (res == EOF) {
      if (ferror(stdin)) {
        exit(1);
      }
      break;
    } else if (res == KEY_SPACE) {
      break;
    }
  }
}
