// STL headers
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "App.hpp"
#include "render/Vertex.hpp"
#include "system.hpp"

App::App() {}

void App::cleanup() {
  render_system_.cleanup();
}

void App::init() {
  render_system_.init();
}

void App::run() {
  bool run = true;

  while (run) {
    SDL_Event event;
    SDL_PollEvent(&event);
    if (event.type == SDL_QUIT)
      run = false;
    render_system_.draw_frame();
    SDL_Delay(16);
  }
  render_system_.wait_idle();
}