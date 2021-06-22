#include <SDL.h>
#include <SDL_vulkan.h>
#include <MoltenVK/mvk_vulkan.h>
#include <vector>

int main() {
  SDL_Window* window;
  unsigned int extension_count;

  assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
  window = SDL_CreateWindow("Vulkan demo", 0, 0, 800, 600, SDL_WINDOW_VULKAN);
  assert(window != nullptr);

  SDL_Vulkan_GetInstanceExtensions(window, &extension_count, nullptr);
  std::vector<const char *> extension_names {
    "VK_MVK_macos_surface",
    "VK_MVK_moltenvk"
  };
  std::vector<const char *> layer_names;

  VkApplicationInfo app_info;
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "Vulkan demo";
  app_info.applicationVersion = 0;
  app_info.pEngineName = "Custom stuff";
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo instance_info;
  instance_info.pNext = nullptr;
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledLayerCount = layer_names.size();
  instance_info.ppEnabledLayerNames = layer_names.data();
  instance_info.enabledExtensionCount = extension_names.size();
  instance_info.ppEnabledExtensionNames = extension_names.data();

  VkInstance instance;
  assert(vkCreateInstance(&instance_info, nullptr, &instance) == VK_SUCCESS);

  VkSurfaceKHR surface;
  SDL_bool result = SDL_Vulkan_CreateSurface(window, instance, &surface);
  assert(result == SDL_TRUE);

  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
