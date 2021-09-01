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

  assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
  App app;
  app.init_vulkan();
  app.run();
  app.cleanup();
  SDL_Quit();
  return 0;
}
