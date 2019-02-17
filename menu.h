#ifndef MENU_H
#define MENU_H

#include <string>
#include <vector>
#include <termios.h>


/* Colours */
#define BG_BLUE "\x1b[44m"
#define RESET "\033[0m"
#define CLEAR "\e[1;1H\e[2J"

/* Alternate buffer */
#define TO_ALT_BUF "\033[?1049h\033[H"
#define FROM_ALT_BUF "\033[?1049l"

/* Key presses */
#define KEY_SPACE 32
#define KEY_ESC 27
#define KEY_LSQBR 91
#define KEY_UP 65
#define KEY_DOWN 66
#define KEY_ENTER 13
#define KEY_DQUOTE 34
#define KEY_LOWER_C 99
#define KEY_LOWER_N 110
#define KEY_UPPER_N 78

/* Cursor directions */
extern const char *DIR_CODES[4];

enum class CursorDir {
  UP,
  DOWN,
  LEFT,
  RIGHT
};

/* Constructor: possibly go to alternate buffer, make space, scroll back
                up, print prompt and options
   Run: wait for user to choose an option and return
   Destructor: possibly switch back to main buffer

   Note: assumes terminal in raw IO mode!
   Todo: allow option for scoped rawio mode */
class Menu {
public:
  static const int STDINEOF = -1;
  static const int NOCHOICE = -2;

  Menu(const std::vector<std::string> &options, bool altBuf=true, bool rawIO=true);
  ~Menu();

  int run();
  void close();

private:
  void makeMenuSpace();
  void displayMenu();
  void printMenu();
  void updateMenu(CursorDir dir);
  void scroll(CursorDir dir, int n);
  void toTerminalRawIO();
  void fromTerminalRawIO();

  const std::vector<std::string> options;
  int current;
  bool altBuf;
  bool rawIO;
  struct termios savedSettings;
  bool active;
};

#endif
