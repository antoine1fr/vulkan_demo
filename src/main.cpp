#include "config.hpp"

#if defined(DEMO_BUILD_WINDOWS)
#include <windows.h>
#define SDL_MAIN_HANDLED
#endif

#include <cassert>
#include <iostream>
#include <string>

#include "base.hpp"

#include "App.hpp"

int main() {
#if defined(DEMO_BUILD_WINDOWS)
  DWORD size = GetCurrentDirectory(0, nullptr);
  std::string cwd(static_cast<size_t>(size), '\0');
  GetCurrentDirectory(size, cwd.data());
  std::cout << "CWD: " << cwd << '\n';
#endif

  App app;
  app.Run();
  return 0;
}
