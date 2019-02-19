#include "menu.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


const char *DIR_CODES[] = {
  "\033[%dA",
  "\033[%dB",
  "\033[%dD",
  "\033[%dC"
};

Menu::Menu(const std::vector<std::string> &options, bool altBuf, bool rawIO):
    options(options),
    current(0),
    altBuf(altBuf),
    rawIO(rawIO),
    active(true)
{
  if (rawIO) {
    toTerminalRawIO();
  }

  if (altBuf) {
    printf("%s", TO_ALT_BUF);
    fflush(stdout);
  }

  printf("Use UP/DOWN to navigate, ENTER to select an option, SPACE to exit.\r\n");
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
  scroll(CursorDir::UP, options.size());
  printMenu();
}

void Menu::printMenu()
{
  for (int i=0; i<options.size(); ++i) {
    if (i == current) {
      printf("%s%s%s\r\n", BG_BLUE, options.at(i).c_str(), RESET);
    } else {
      printf("%s\r\n", options.at(i).c_str());
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
        updateMenu(CursorDir::UP);
        break;
      case KEY_DOWN:
        updateMenu(CursorDir::DOWN);
        break;
      }
    }

    if (res == EOF) {
      if (ferror(stdin)) {
        sysError("fgetc");
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

void Menu::updateMenu(CursorDir dir)
{
  int numOptions = options.size();
  scroll(CursorDir::UP, numOptions);

  if (dir == CursorDir::UP) {
    current = !current ? numOptions - 1 : (current - 1) % numOptions;
  } else if (dir == CursorDir::DOWN) {
    current = (current + 1) % numOptions;
  }

  printMenu();
}

void Menu::scroll(CursorDir dir, int n)
{
  printf(DIR_CODES[(int) dir], n);
}

void Menu::close()
{
  if (!active) {
    return;
  }
  active = false;

  if (rawIO) {
    fromTerminalRawIO();
  }

  if (altBuf) {
    printf("%s", FROM_ALT_BUF);
    fflush(stdout);
  }
}

void Menu::toTerminalRawIO()
{
  struct termios newSettings;

  if (tcgetattr(STDIN_FILENO, &savedSettings) != -1) {
    newSettings = savedSettings;
    cfmakeraw(&newSettings);

    if (tcsetattr(STDIN_FILENO, TCSANOW, &newSettings) != -1) {
      return;
    }
  }

  sysError("toRawTerminalIO");
}

void Menu::fromTerminalRawIO()
{
  if (tcsetattr(STDIN_FILENO, TCSANOW, &savedSettings) == -1) {
    sysError("fromTerminalRawIO");
  }
}

Menu::~Menu()
{
  close();
}

void demoMenu()
{
  /* Raw mode: no echoing, no terminal driver processing, no line-buffering
     Go to alternate buffer (note stdout is line-buffered by default)
     Print newlines
     Move cursor up
     Print menu line by line and choose first to highlight */
  std::vector<std::string> options = {"bash", "java", "python3"};
  Menu menu(options, true);

  /* In cursor mode, up/down arrow press sends 3 bytes: esc, ] and A/B
     Come back from alternate buffer on close */
  int choice = menu.run();
  menu.close();

  /* Determine which choice the user made */
  if (choice != Menu::NOCHOICE && choice != Menu::STDINEOF) {
    printf("You chose: %s\r\n", options.at(choice).c_str());
  }
}
