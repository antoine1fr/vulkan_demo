#pragma once

#include <map>
#include <unordered_map>
#include <vector>

// Third party headers
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "base.hpp"
#include "render/RenderSystem.hpp"

// Fat, messy god object. Yeaaah.
class App {
 private:
  render::RenderSystem render_system_;
 public:
  App();

  App(const App&) = delete;
  App(App&&) = delete;
  const App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;

  void cleanup();
  void init();
  void run();
};