#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>
#include <sys/ioctl.h>
#include <unistd.h>

struct AppState
{
  SDL_Window* window;
  VkInstance instance;
  VkPhysicalDevice physical_device;
  uint32_t queue_family_index;
  VkDevice device;
  VkQueue queue;
  VkCommandPool command_pool;
  VkCommandBuffer command_buffer;
  VkSurfaceKHR surface;
  VkSwapchainKHR swapchain;
  VkShaderModule shader_module;
};

static size_t get_terminal_width()
{
  winsize window_size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
  return window_size.ws_col;
}

static void check_extensions(std::vector<VkExtensionProperties> available_extensions_vec,
                             std::vector<const char*> wanted_extensions)
{
  std::map<std::string, VkExtensionProperties> available_extensions;
  for (const auto& extension: available_extensions_vec)
  {
    available_extensions[extension.extensionName] = extension;
  }

  bool missing_extension = false;
  size_t terminal_width = get_terminal_width();
  std::cout << "Checking device extensions:\n";
  for (auto extension: wanted_extensions)
  {
    std::cout << " - " << extension
              << std::setfill('.')
              << std::setw(terminal_width - 3 - std::string(extension).size())
              << std::right;
    if (available_extensions.find(extension) == available_extensions.end())
    {
      missing_extension = true;
      std::cout << "MISSING\n";
    }
    else
    {
      std::cout << "PRESENT\n";
    }
  }
  std::cout << std::left << std::setfill(' ');
  assert(missing_extension == false);
}

static void check_layers(std::vector<VkLayerProperties> available_layers_vec,
                             std::vector<const char*> wanted_layers)
{
  std::map<std::string, VkLayerProperties> available_layers;
  for (const auto& layer: available_layers_vec)
  {
    available_layers[layer.layerName] = layer;
  }

  bool missing_layer = false;
  size_t terminal_width = get_terminal_width();
  std::cout << "Checking device layers:\n";
  for (auto layer: wanted_layers)
  {
    std::cout << " - " << layer
              << std::setfill('.')
              << std::setw(terminal_width - 3 - std::string(layer).size())
              << std::right;
    if (available_layers.find(layer) == available_layers.end())
    {
      missing_layer = true;
      std::cout << "MISSING\n";
    }
    else
    {
      std::cout << "PRESENT\n";
    }
  }
  std::cout << std::left << std::setfill(' ');
  assert(missing_layer == false);
}

static void create_vulkan_instance(AppState& state)
{
  unsigned int extension_count;
  // VkResult result =
  //   SDL_Vulkan_GetInstanceExtensions(state.window, &extension_count, nullptr);
  VkResult result =
    vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
  assert(result == VK_SUCCESS);
  std::vector<VkExtensionProperties> available_extensions(extension_count);
  result = vkEnumerateInstanceExtensionProperties(nullptr,
                                                  &extension_count,
                                                  available_extensions.data());

  std::vector<const char *> extension_names {
    "VK_KHR_surface",
    "VK_MVK_macos_surface"
    // "VK_EXT_metal_surface"
    // "VK_KHR_display"
  };
  check_extensions(available_extensions, extension_names);

  uint32_t layer_count;
  result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
  assert(result == VK_SUCCESS);
  std::vector<VkLayerProperties> layer_properties(layer_count);
  result = vkEnumerateInstanceLayerProperties(&layer_count, layer_properties.data());
  assert(result == VK_SUCCESS);
  // std::cout << "Instance layer names:\n";
  // for (auto properties: layer_properties)
  // {
  //   std::cout << " - " << properties.layerName << ": " << properties.description << '\n';
  // }
  std::vector<const char *> layer_names {
    "VK_LAYER_KHRONOS_validation"
  };
  check_layers(layer_properties, layer_names);

  VkApplicationInfo app_info {};
  app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  app_info.pNext = nullptr;
  app_info.pApplicationName = "Vulkan demo";
  app_info.applicationVersion = 0;
  app_info.pEngineName = "Custom stuff";
  app_info.apiVersion = VK_API_VERSION_1_0;

  VkInstanceCreateInfo instance_info {};
  instance_info.pNext = nullptr;
  instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  instance_info.pApplicationInfo = &app_info;
  instance_info.enabledLayerCount = layer_names.size();
  instance_info.ppEnabledLayerNames = layer_names.data();
  instance_info.enabledExtensionCount = extension_names.size();
  instance_info.ppEnabledExtensionNames = extension_names.data();

  result = vkCreateInstance(&instance_info, nullptr, &state.instance);
  assert(result == VK_SUCCESS);
}

static void create_vulkan_surface(AppState& state)
{
  SDL_bool result = SDL_Vulkan_CreateSurface(state.window,
                                             state.instance,
                                             &state.surface);
  assert(result == SDL_TRUE);
}

static void select_best_surface_format(const AppState& state, VkSurfaceFormatKHR& surface_format)
{
  uint32_t format_count;
  VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(state.physical_device,
                                                         state.surface,
                                                         &format_count,
                                                         nullptr);
  assert(result == VK_SUCCESS);

  std::vector<VkSurfaceFormatKHR> formats(format_count);
  result = vkGetPhysicalDeviceSurfaceFormatsKHR(state.physical_device,
                                                state.surface,
                                                &format_count,
                                                formats.data());
  assert(result == VK_SUCCESS);

  size_t best_index = 0;
  for (size_t i = 0; i < formats.size(); ++i)
  {
    switch (formats[i].format)
    {
    case VK_FORMAT_R32G32B32A32_SFLOAT:
    case VK_FORMAT_R32G32B32_SFLOAT:
    case VK_FORMAT_R16G16B16A16_SFLOAT:
    case VK_FORMAT_R16G16B16_SFLOAT:
    case VK_FORMAT_R8G8B8A8_SRGB:
    case VK_FORMAT_B8G8R8A8_SRGB:
    case VK_FORMAT_R8G8B8A8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
      best_index = i;
      break;
    default:
      break;
    }
  }
  assert(best_index != formats.size());
  surface_format = formats[best_index];
}

static void create_vulkan_swapchain(AppState& state,
                                    const VkExtent2D& window_dimensions)
{
  VkSurfaceFormatKHR surface_format;
  select_best_surface_format(state, surface_format);

  VkSwapchainCreateInfoKHR swapchain_create_info {};
  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.surface = state.surface;
  swapchain_create_info.minImageCount = 2;
  swapchain_create_info.imageFormat = surface_format.format;
  swapchain_create_info.imageColorSpace = surface_format.colorSpace;
  swapchain_create_info.imageExtent = window_dimensions;
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  swapchain_create_info.clipped = VK_TRUE;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

  VkResult result =
    vkCreateSwapchainKHR(state.device,
                         &swapchain_create_info,
                         nullptr,
                         &state.swapchain);
  assert(result == VK_SUCCESS);
}

static std::string device_type_to_string(VkPhysicalDeviceType type)
{
  switch (type)
  {
  case VK_PHYSICAL_DEVICE_TYPE_OTHER:
    return "Other";
  case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
    return "Integrated GPU";
  case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
    return "Discrete GPU";
  case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
    return "Virtual GPU";
  case VK_PHYSICAL_DEVICE_TYPE_CPU:
    return "CPU";
  default:
    assert(false);
    return "";
  }
}

static std::string pipeline_cache_uuid_to_string(uint8_t uuid[VK_UUID_SIZE])
{
  std::ostringstream stream;
  for (size_t i = 0; i < VK_UUID_SIZE; ++i)
  {
    if ((i > 0) && ((i % 4) == 0))
      stream << '-';
    stream << std::hex
           << std::setw(2)
           << std::setfill('0')
           << static_cast<uint32_t>(uuid[i]);
  }
  return stream.str();
}

static std::vector<VkPhysicalDevice> enumerate_physical_devices(const AppState& state)
{
  VkResult result;
  uint32_t device_count;

  result = vkEnumeratePhysicalDevices(state.instance,
                                      &device_count,
                                      nullptr);
  assert(result == VK_SUCCESS);
  if (device_count == 0)
  {
    std::cerr << "No Vulkan-compatible physical device found.\n";
    assert(false);
  }

  std::vector<VkPhysicalDevice> physical_devices(device_count);
  result = vkEnumeratePhysicalDevices(state.instance,
                                      &device_count,
                                      physical_devices.data());
  assert(result == VK_SUCCESS);
  return physical_devices;
}

// The SDL doesn't seem to offer any way to retrieve the physical
// device it used to create the presentation surface. So we need to
// enumerate the Vulkan-compatible devices that have
// graphics-compatible queue families and ask Vulkan whether the
// (physical device x queue family x surface) triple is valid.
static void find_physical_device(AppState& state)
{
  std::vector<VkPhysicalDevice> physical_devices = enumerate_physical_devices(state);

  for (auto physical_device: physical_devices)
  {
    uint32_t queue_family_property_count;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_property_count, nullptr);

    std::vector<VkQueueFamilyProperties> queue_family_properties(queue_family_property_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                             &queue_family_property_count,
                                             queue_family_properties.data());
    for (uint32_t queue_family_index = 0;
         queue_family_index < queue_family_property_count;
         ++queue_family_index)
    {
      const auto& properties = queue_family_properties[queue_family_index];
      if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      {
        VkBool32 supported;
        vkGetPhysicalDeviceSurfaceSupportKHR(physical_device,
                                             queue_family_index,
                                             state.surface,
                                             &supported);
        if (supported == VK_TRUE)
        {
          state.physical_device = physical_device;
          state.queue_family_index = queue_family_index;
          return;
        }
      }
    }
    assert(false);
  }

  // std::cout << "Vulkan-compatible physical devices:\n";
  // for (const auto& device: physical_devices)
  // {
  //   VkPhysicalDeviceProperties properties;
  //   vkGetPhysicalDeviceProperties(device, &properties);
  //   std::cout << " - " << properties.deviceName << '\n';
  //   std::cout << "   * API version: " << properties.apiVersion << '\n';
  //   std::cout << "   * Driver version: " << properties.driverVersion << '\n';
  //   std::cout << "   * Vendor ID: " << properties.vendorID << '\n';
  //   std::cout << "   * Properties ID: " << properties.deviceID << '\n';
  //   std::cout << "   * Properties type: " << device_type_to_string(properties.deviceType) << '\n';
  //   std::cout << "   * UUID: " << pipeline_cache_uuid_to_string(properties.pipelineCacheUUID) << '\n';
  // }
}

static void enumerate_device_extensions(VkPhysicalDevice device,
                                        std::map<std::string, VkExtensionProperties>& extension_map)
{
  VkResult result;
  uint32_t extension_count;
  result = vkEnumerateDeviceExtensionProperties(device,
                                                nullptr,
                                                &extension_count,
                                                nullptr);
  assert(result == VK_SUCCESS);
  
  std::vector<VkExtensionProperties> extensions(extension_count);
  result = vkEnumerateDeviceExtensionProperties(device,
                                                nullptr,
                                                &extension_count,
                                                extensions.data());
  assert(result == VK_SUCCESS);

  // std::cout << "Available extensions for device:\n";
  for (const auto& extension: extensions)
  {
    // std::cout << " - " << extension.extensionName
    //           << " (spec version " << extension.specVersion << ")\n";
    extension_map[extension.extensionName] = extension;
  }
}

static void check_device_extensions(VkPhysicalDevice physical_device,
                                    std::vector<const char*> wanted_extensions)
{
  std::map<std::string, VkExtensionProperties> available_extensions;
  enumerate_device_extensions(physical_device, available_extensions);

  bool missing_extension = false;
  size_t terminal_width = get_terminal_width();
  std::cout << "Checking device extensions:\n";
  for (auto extension: wanted_extensions)
  {
    std::cout << " - " << extension
              << std::setfill('.')
              << std::setw(terminal_width - 3 - std::string(extension).size())
              << std::right;
    if (available_extensions.find(extension) == available_extensions.end())
    {
      missing_extension = true;
      std::cout << "MISSING\n";
    }
    else
    {
      std::cout << "PRESENT\n";
    }
  }
  std::cout << std::left << std::setfill(' ');
  assert(missing_extension == false);
}

static uint32_t get_queue_family_index(VkPhysicalDevice physical_device)
{
  uint32_t queue_family_count;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                           &queue_family_count,
                                           nullptr);
  assert(queue_family_count > 0);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                           &queue_family_count,
                                           queue_families.data());
  // Let's return the first family that supports graphics operations,
  // as we only need that for the moment.
  for (size_t i = 0; i < queue_families.size(); ++i)
  {
    const auto& family = queue_families[i];
    if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      return i;
  }
  std::cerr << "Found no queue family capable of graphics operations"
            << " for selected physical device.\n";
  assert(false);
}

static void create_device(AppState& state)
{
  std::vector<const char*> extensions {
    // "VK_KHR_get_physical_device_properties2",
    // "VK_KHR_portability_subset",
    "VK_KHR_swapchain"
  };
  check_device_extensions(state.physical_device, extensions);

  float queue_priorities = 1.0f;
  VkDeviceQueueCreateInfo queue_info {};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = state.queue_family_index;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &queue_priorities;
  
  VkDeviceCreateInfo info {};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.queueCreateInfoCount = 1;
  info.pQueueCreateInfos = &queue_info;
  info.enabledExtensionCount = extensions.size();
  info.ppEnabledExtensionNames = extensions.data();
  
  VkResult result = vkCreateDevice(state.physical_device,
                                   &info,
                                   nullptr,
                                   &state.device);
  assert(result == VK_SUCCESS);

  vkGetDeviceQueue(state.device,
                   state.queue_family_index,
                   0,
                   &state.queue);
}

static void create_vulkan_command_pool(AppState& state)
{
  VkCommandPoolCreateInfo create_info {};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT
    | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  create_info.queueFamilyIndex = state.queue_family_index;

  VkResult result = vkCreateCommandPool(state.device,
                                        &create_info,
                                        nullptr,
                                        &state.command_pool);
  assert(result == VK_SUCCESS);
}

static void create_vulkan_command_buffer(AppState& state)
{
  VkCommandBufferAllocateInfo info {};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.commandPool = state.command_pool;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  info.commandBufferCount = 1;

  VkResult result = vkAllocateCommandBuffers(state.device,
                                             &info,
                                             &state.command_buffer);
  assert(result == VK_SUCCESS);
}

// static void create_vulkan_pipeline(AppState& state)
// {
//   VkGraphicsPipelineCreateInfo info {};
//   info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
//   info.stage = VK_SHADER_STAGE_VERTEX_BIT;
//   info.module = state.shader_module;
//   info.pName = "main";
  
//   VkResult result = vkCreateGraphicsPipelines(
//     state.device,
//     VK_NULL_HANDLE,
//     1,
//     &info,
//     nullptr,
//     &state.pipeline);
//   assert(result = VK_SUCCESS);
// }

static void load_shader(AppState& state)
{
  std::ifstream stream("vertex.spv");
  assert(stream);
  stream.seekg(0, std::ios_base::end);
  std::size_t length = stream.tellg();
  stream.seekg(0, std::ios_base::beg);
  std::string sources(length, '\0');
  stream.read(sources.data(), length);

  VkShaderModuleCreateInfo info {};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = sources.length();
  info.pCode = reinterpret_cast<uint32_t*>(sources.data());

  VkResult result = vkCreateShaderModule(state.device,
                                         &info,
                                         nullptr,
                                         &state.shader_module);
  assert(result == VK_SUCCESS);
}

static void init_vulkan(AppState& state)
{
  VkExtent2D window_dimensions = {800, 600};
  state.window = SDL_CreateWindow("Vulkan demo",
                                  0, 0,
                                  window_dimensions.width, window_dimensions.height,
                                  SDL_WINDOW_VULKAN);
  assert(state.window != nullptr);
  create_vulkan_instance(state);
  create_vulkan_surface(state);
  find_physical_device(state);
  create_device(state);
  create_vulkan_swapchain(state, window_dimensions);
  load_shader(state);
  // create_vulkan_pipeline(state);
  create_vulkan_command_pool(state);
  create_vulkan_command_buffer(state);
}

static void cleanup(const AppState& state)
{
  vkDestroyShaderModule(state.device, state.shader_module, nullptr);
  // vkDestroyPipeline(state.device, state.pipeline, nullptr);
  vkDestroyCommandPool(state.device, state.command_pool, nullptr);
  vkDestroySwapchainKHR(state.device, state.swapchain, nullptr);
  vkDestroyDevice(state.device, nullptr);
  vkDestroySurfaceKHR(state.instance, state.surface, nullptr);
  vkDestroyInstance(state.instance, nullptr);
  SDL_DestroyWindow(state.window);
}

int main() {
  AppState app_state; 
  assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
  init_vulkan(app_state);
  cleanup(app_state);
  SDL_Quit();
  return 0;
}
