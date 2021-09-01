#include <windows.h>
#include "system.hpp"

size_t get_terminal_width() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return static_cast<size_t>(csbi.srWindow.Right) -
         static_cast<size_t>(csbi.srWindow.Left) + 1;
}