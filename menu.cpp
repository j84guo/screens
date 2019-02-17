#include "utils.h"

#include <string>
#include <vector>

#include <stdio.h>
#include <unistd.h>

/* Colours */
const char *BG_BLUE = "\x1b[44m";
const char *RESET = "\033[0m";
const char *CLEAR = "\e[1;1H\e[2J";

/* Alternate buffer */
const char *TO_ALT_BUF = "\033[?1049h\033[H";
const char *FROM_ALT_BUF = "\033[?1049l";

/* Key presses */
const unsigned char KEY_SPACE = 32;
const unsigned char KEY_ESC = 27;
const unsigned char KEY_LSQBR = 91;
const unsigned char KEY_UP = 65;
const unsigned char KEY_DOWN = 66;
const unsigned char KEY_ENTER = 13;

/* Cursor directions */
const char *DIR_CODES[] = {
  "\033[%dA",
  "\033[%dB",
  "\033[%dD",
  "\033[%dC"
};

enum CursorDir {
  DIR_UP,
  DIR_DOWN,
  DIR_LEFT,
  DIR_RIGHT
};

/* constructor: possibly go to alternate buffer, make space, scroll back
                up, print prompt and options
   run: wait for user to choose an option and return
   destructor: possibly switch back to main buffer

   Note: assumes terminal in raw IO mode! */
class Menu {
  public:
    static const int STDINEOF = -1;
    static const int NOCHOICE = -2;

    Menu(const std::vector<std::string> &options, bool altBuf=true);
    ~Menu();

    int run();
    void close();

  private:
    void makeMenuSpace();
    void displayMenu();
    void printMenu();
    void updateMenu(enum CursorDir);
    void scroll(enum CursorDir, int n);

    const std::vector<std::string> options;
    int current;
    bool altBuf;
    bool active;
};

Menu::Menu(const std::vector<std::string> &options, bool altBuf):
  options(options), current(0), altBuf(altBuf), active(false)
{
  if (altBuf) {
    printf("%s", TO_ALT_BUF);
    fflush(stdout);
  }

  printf("Use UP/DOWN to navigate, press SPACE to exit.\r\n");
  displayMenu();
}

void Menu::makeMenuSpace()
{
  for (int i=0; i<options.size(); ++i) {
    printf("\r\n");
  }
}

void Menu::displayMenu()
{
  makeMenuSpace();
  scroll(DIR_UP, options.size());
  printMenu();
}

void Menu::printMenu()
{
  for (int i=0; i<options.size(); ++i) {
    if (i == current) {
      printf("%s%d. %s%s\r\n", BG_BLUE, i + 1, options.at(i).c_str(), RESET);
    } else {
      printf("%d. %s\r\n", i + 1, options.at(i).c_str());
    }
  }
}

/* Returns the user's choice, -1 on user declining or EOF on stdin closing.
   Throws an exception on I/O error */
int Menu::run()
{
  int res;
  while ((res = fgetc(stdin)) != KEY_SPACE) {
    if (res == KEY_ESC && (res = fgetc(stdin)) == KEY_LSQBR) {
      switch ((res = fgetc(stdin))) {
      case KEY_UP:
        updateMenu(DIR_UP);
        break;
      case KEY_DOWN:
        updateMenu(DIR_DOWN);
        break;
      }
    }

    if (res == EOF) {
      if (ferror(stdin)) {
        sys_error("fgetc");
      }
      return STDINEOF;
    } else if (res == KEY_SPACE) {
      break;
    } else if (res == KEY_ENTER) {
      return current;
    }
  }

  return NOCHOICE;
}

void Menu::updateMenu(enum CursorDir dir)
{ 
  int numOptions = options.size();
  scroll(DIR_UP, numOptions);

  if (dir == DIR_UP) {
    current = !current ? numOptions - 1 : (current - 1) % numOptions;
  } else {
    current = (current + 1) % numOptions;
  }

  printMenu();
}

void Menu::scroll(enum CursorDir dir, int n)
{
  printf(DIR_CODES[dir], n);
}

void Menu::close()
{
  if (altBuf) {
    printf("%s", FROM_ALT_BUF);
    fflush(stdout);
  }
  active = false;
}

Menu::~Menu()
{
  if (active) {
    close(); 
  }
}

int main(int argc, char *argv[])
{
  /* Raw mode: no echoing, no terminal driver processing, no line-buffering */
  set_terminal_rawio();

  /* Go to alternate buffer (note stdout is line-buffered by default)
     Print newlines
     Move cursor up
     Print menu line by line and choose first to highlight */
  std::vector<std::string> options = {"Bash", "java", "python3"};
  Menu menu(options, true);

  /* In cursor mode, up/down arrow press sends 3 bytes: esc, ] and A/B
     Come back from alternate buffer */
  int choice = menu.run();
  menu.close();
  printf("You chose: %d\r\n", choice);
}
