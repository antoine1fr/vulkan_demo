// Unix stuff
#include <sys/ioctl.h>
#include <unistd.h>

// Third parties
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// STL
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <vector>

#if defined(__APPLE__)
#define DEMO_BUILD_APPLE
#elif defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#define DEMO_BUILD_WINDOWS
#elif defined(__linux__)
#define DEMO_BUILD_LINUX
#endif

struct Vertex {
  glm::vec4 position;
  glm::vec3 normal;
  glm::vec2 uv;
};

static size_t get_terminal_width() {
  winsize window_size;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
  return window_size.ws_col;
}

static std::vector<VkPhysicalDevice> enumerate_physical_devices(
    VkInstance instance) {
  VkResult result;
  uint32_t device_count;

  result = vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
  assert(result == VK_SUCCESS);
  if (device_count == 0) {
    std::cerr << "No Vulkan-compatible physical device found.\n";
    assert(false);
  }

  std::vector<VkPhysicalDevice> physical_devices(device_count);
  result = vkEnumeratePhysicalDevices(instance, &device_count,
                                      physical_devices.data());
  assert(result == VK_SUCCESS);
  return physical_devices;
}

// Fat, messy god object. Yeaaah.
class App {
 private:
  const size_t kMaxFrames = 2;

  VkExtent2D window_extent_;
  SDL_Window* window_;
  VkInstance instance_;
  VkPhysicalDevice physical_device_;
  uint32_t queue_family_index_;
  VkDevice device_;
  VkQueue queue_;
  VkCommandPool command_pool_;
  std::vector<VkCommandBuffer> command_buffers_;
  VkSurfaceKHR surface_;
  VkSwapchainKHR swapchain_;
  std::vector<VkImageView> swapchain_image_views_;
  std::vector<VkFramebuffer> framebuffers_;
  VkShaderModule shader_module_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
  VkRenderPass render_pass_;
  VkFormat swapchain_image_format_;
  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;
  std::vector<VkFence> in_flight_images_;
  size_t current_frame_;

 private:
  void check_extensions(
      std::vector<VkExtensionProperties> available_extensions_vec,
      std::vector<const char*> wanted_extensions) {
    std::map<std::string, VkExtensionProperties> available_extensions;
    for (const auto& extension : available_extensions_vec) {
      available_extensions[extension.extensionName] = extension;
    }

    bool missing_extension = false;
    size_t terminal_width = get_terminal_width();
    std::cout << "Checking device extensions:\n";
    for (auto extension : wanted_extensions) {
      std::cout << " - " << extension << std::setfill('.')
                << std::setw(terminal_width - 3 - std::string(extension).size())
                << std::right;
      if (available_extensions.find(extension) == available_extensions.end()) {
        missing_extension = true;
        std::cout << "MISSING\n";
      } else {
        std::cout << "PRESENT\n";
      }
    }
    std::cout << std::left << std::setfill(' ');
    assert(missing_extension == false);
  }

  void check_layers(std::vector<VkLayerProperties> available_layers_vec,
                    std::vector<const char*> wanted_layers) {
    std::map<std::string, VkLayerProperties> available_layers;
    for (const auto& layer : available_layers_vec) {
      available_layers[layer.layerName] = layer;
    }

    bool missing_layer = false;
    size_t terminal_width = get_terminal_width();
    std::cout << "Checking device layers:\n";
    for (auto layer : wanted_layers) {
      std::cout << " - " << layer << std::setfill('.')
                << std::setw(terminal_width - 3 - std::string(layer).size())
                << std::right;
      if (available_layers.find(layer) == available_layers.end()) {
        missing_layer = true;
        std::cout << "MISSING\n";
      } else {
        std::cout << "PRESENT\n";
      }
    }
    std::cout << std::left << std::setfill(' ');
    assert(missing_layer == false);
  }

  void create_vulkan_instance() {
    unsigned int extension_count;
    SDL_Vulkan_GetInstanceExtensions(window_, &extension_count, nullptr);
    std::vector<const char*> extension_names(
        static_cast<size_t>(extension_count));
    SDL_Vulkan_GetInstanceExtensions(window_, &extension_count,
                                     extension_names.data());

#if defined(DEMO_BUILD_APPLE)
    extension_names.push_back("VK_KHR_get_physical_device_properties2");
#endif

    uint32_t layer_count;
    VkResult result = vkEnumerateInstanceLayerProperties(&layer_count, nullptr);
    assert(result == VK_SUCCESS);
    std::vector<VkLayerProperties> layer_properties(layer_count);
    result = vkEnumerateInstanceLayerProperties(&layer_count,
                                                layer_properties.data());
    assert(result == VK_SUCCESS);
    // std::cout << "Instance layer names:\n";
    // for (auto properties: layer_properties)
    // {
    //   std::cout << " - " << properties.layerName << ": " <<
    //   properties.description << '\n';
    // }
    std::vector<const char*> layer_names{"VK_LAYER_KHRONOS_validation"};
    check_layers(layer_properties, layer_names);

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pNext = nullptr;
    app_info.pApplicationName = "Vulkan demo";
    app_info.applicationVersion = 0;
    app_info.pEngineName = "Custom stuff";
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instance_info{};
    instance_info.pNext = nullptr;
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.enabledLayerCount = layer_names.size();
    instance_info.ppEnabledLayerNames = layer_names.data();
    instance_info.enabledExtensionCount = extension_names.size();
    instance_info.ppEnabledExtensionNames = extension_names.data();

    result = vkCreateInstance(&instance_info, nullptr, &instance_);
    assert(result == VK_SUCCESS);
  }

  void create_vulkan_surface() {
    SDL_bool result = SDL_Vulkan_CreateSurface(window_, instance_, &surface_);
    assert(result == SDL_TRUE);
  }

  void select_best_surface_format(VkSurfaceFormatKHR& surface_format) {
    uint32_t format_count;
    VkResult result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device_, surface_, &format_count, nullptr);
    assert(result == VK_SUCCESS);

    std::vector<VkSurfaceFormatKHR> formats(format_count);
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(
        physical_device_, surface_, &format_count, formats.data());
    assert(result == VK_SUCCESS);

    size_t best_index = 0;
    for (size_t i = 0; i < formats.size(); ++i) {
      switch (formats[i].format) {
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

  void create_vulkan_swapchain() {
    VkResult result;
    VkSurfaceFormatKHR surface_format;
    select_best_surface_format(surface_format);
    swapchain_image_format_ = surface_format.format;

    VkSurfaceCapabilitiesKHR capabilities{};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_,
                                                       surface_, &capabilities);
    assert(result == VK_SUCCESS);

    VkSwapchainCreateInfoKHR swapchain_create_info{};
    swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchain_create_info.surface = surface_;
    swapchain_create_info.minImageCount =
        std::max<uint32_t>(capabilities.minImageCount, 3);
    swapchain_create_info.imageFormat = swapchain_image_format_;
    swapchain_create_info.imageColorSpace = surface_format.colorSpace;
    swapchain_create_info.imageExtent = window_extent_;
    swapchain_create_info.imageArrayLayers = 1;
    swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchain_create_info.presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
    swapchain_create_info.clipped = VK_TRUE;
    swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

    result = vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr,
                                  &swapchain_);
    assert(result == VK_SUCCESS);
  }

  // static std::string device_type_to_string(VkPhysicalDeviceType type)
  // {
  //   switch (type)
  //   {
  //   case VK_PHYSICAL_DEVICE_TYPE_OTHER:
  //     return "Other";
  //   case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
  //     return "Integrated GPU";
  //   case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
  //     return "Discrete GPU";
  //   case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
  //     return "Virtual GPU";
  //   case VK_PHYSICAL_DEVICE_TYPE_CPU:
  //     return "CPU";
  //   default:
  //     assert(false);
  //     return "";
  //   }
  // }

  // static std::string pipeline_cache_uuid_to_string(uint8_t
  // uuid[VK_UUID_SIZE])
  // {
  //   std::ostringstream stream;
  //   for (size_t i = 0; i < VK_UUID_SIZE; ++i)
  //   {
  //     if ((i > 0) && ((i % 4) == 0))
  //       stream << '-';
  //     stream << std::hex
  //            << std::setw(2)
  //            << std::setfill('0')
  //            << static_cast<uint32_t>(uuid[i]);
  //   }
  //   return stream.str();
  // }

  // The SDL doesn't seem to offer any way to retrieve the physical
  // device it used to create the presentation surface. So we need to
  // enumerate the Vulkan-compatible devices that have
  // graphics-compatible queue families and ask Vulkan whether the
  // (physical device x queue family x surface) triple is valid.
  void find_physical_device() {
    std::vector<VkPhysicalDevice> physical_devices =
        enumerate_physical_devices(instance_);

    for (auto physical_device : physical_devices) {
      uint32_t queue_family_property_count;
      vkGetPhysicalDeviceQueueFamilyProperties(
          physical_device, &queue_family_property_count, nullptr);

      std::vector<VkQueueFamilyProperties> queue_family_properties(
          queue_family_property_count);
      vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
                                               &queue_family_property_count,
                                               queue_family_properties.data());
      for (uint32_t queue_family_index = 0;
           queue_family_index < queue_family_property_count;
           ++queue_family_index) {
        const auto& properties = queue_family_properties[queue_family_index];
        if (properties.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
          VkBool32 supported;
          vkGetPhysicalDeviceSurfaceSupportKHR(
              physical_device, queue_family_index, surface_, &supported);
          if (supported == VK_TRUE) {
            physical_device_ = physical_device;
            queue_family_index_ = queue_family_index;
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
    //   std::cout << "   * Driver version: " << properties.driverVersion <<
    //   '\n'; std::cout << "   * Vendor ID: " << properties.vendorID << '\n';
    //   std::cout << "   * Properties ID: " << properties.deviceID << '\n';
    //   std::cout << "   * Properties type: " <<
    //   device_type_to_string(properties.deviceType) << '\n'; std::cout << "
    //   * UUID: " <<
    //   pipeline_cache_uuid_to_string(properties.pipelineCacheUUID) << '\n';
    // }
  }

  void enumerate_device_extensions(
      VkPhysicalDevice device,
      std::map<std::string, VkExtensionProperties>& extension_map) {
    VkResult result;
    uint32_t extension_count;
    result = vkEnumerateDeviceExtensionProperties(device, nullptr,
                                                  &extension_count, nullptr);
    assert(result == VK_SUCCESS);

    std::vector<VkExtensionProperties> extensions(extension_count);
    result = vkEnumerateDeviceExtensionProperties(
        device, nullptr, &extension_count, extensions.data());
    assert(result == VK_SUCCESS);

    // std::cout << "Available extensions for device:\n";
    for (const auto& extension : extensions) {
      // std::cout << " - " << extension.extensionName
      //           << " (spec version " << extension.specVersion << ")\n";
      extension_map[extension.extensionName] = extension;
    }
  }

  void check_device_extensions(VkPhysicalDevice physical_device,
                               std::vector<const char*> wanted_extensions) {
    std::map<std::string, VkExtensionProperties> available_extensions;
    enumerate_device_extensions(physical_device, available_extensions);

    bool missing_extension = false;
    size_t terminal_width = get_terminal_width();
    std::cout << "Checking device extensions:\n";
    for (auto extension : wanted_extensions) {
      std::cout << " - " << extension << std::setfill('.')
                << std::setw(terminal_width - 3 - std::string(extension).size())
                << std::right;
      if (available_extensions.find(extension) == available_extensions.end()) {
        missing_extension = true;
        std::cout << "MISSING\n";
      } else {
        std::cout << "PRESENT\n";
      }
    }
    std::cout << std::left << std::setfill(' ');
    assert(missing_extension == false);
  }

  // static uint32_t get_queue_family_index(VkPhysicalDevice physical_device)
  // {
  //   uint32_t queue_family_count;
  //   vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
  //                                            &queue_family_count,
  //                                            nullptr);
  //   assert(queue_family_count > 0);

  //   std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  //   vkGetPhysicalDeviceQueueFamilyProperties(physical_device,
  //                                            &queue_family_count,
  //                                            queue_families.data());
  //   // Let's return the first family that supports graphics operations,
  //   // as we only need that for the moment.
  //   for (size_t i = 0; i < queue_families.size(); ++i)
  //   {
  //     const auto& family = queue_families[i];
  //     if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
  //       return i;
  //   }
  //   std::cerr << "Found no queue family capable of graphics operations"
  //             << " for selected physical device.\n";
  //   assert(false);
  // }

  void create_device() {
    std::vector<const char*> extensions {
#if defined(DEMO_BUILD_APPLE)
      "VK_KHR_portability_subset",
#endif
          "VK_KHR_swapchain"
    };
    check_device_extensions(physical_device_, extensions);

    float queue_priorities = 1.0f;
    VkDeviceQueueCreateInfo queue_info{};
    queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_info.queueFamilyIndex = queue_family_index_;
    queue_info.queueCount = 1;
    queue_info.pQueuePriorities = &queue_priorities;

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 1;
    info.pQueueCreateInfos = &queue_info;
    info.enabledExtensionCount = extensions.size();
    info.ppEnabledExtensionNames = extensions.data();

    VkResult result =
        vkCreateDevice(physical_device_, &info, nullptr, &device_);
    assert(result == VK_SUCCESS);

    vkGetDeviceQueue(device_, queue_family_index_, 0, &queue_);
  }

  void create_vulkan_command_pool() {
    VkCommandPoolCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    create_info.queueFamilyIndex = queue_family_index_;

    VkResult result =
        vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_);
    assert(result == VK_SUCCESS);
  }

  void create_vulkan_command_buffer() {
    VkCommandBufferAllocateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    info.commandPool = command_pool_;
    info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    info.commandBufferCount = kMaxFrames;

    VkResult result =
      vkAllocateCommandBuffers(device_, &info, command_buffers_.data());
    assert(result == VK_SUCCESS);
  }

  void create_pipeline_layout() {
    VkPipelineLayoutCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    assert(vkCreatePipelineLayout(device_, &info, nullptr, &pipeline_layout_) ==
           VK_SUCCESS);
  }

  void create_vulkan_pipeline() {
    VkResult result;

    VkPipelineShaderStageCreateInfo shader_stage_info{};
    shader_stage_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    shader_stage_info.module = shader_module_;
    shader_stage_info.pName = "main";

    VkVertexInputBindingDescription vertex_input_bindings[] = {
        {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}};

    VkVertexInputAttributeDescription vertex_attributes[] = {
        {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)},
        {2, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)}};

    VkPipelineVertexInputStateCreateInfo vertex_input_state_info{};
    vertex_input_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_state_info.vertexBindingDescriptionCount = 1;
    vertex_input_state_info.pVertexBindingDescriptions = vertex_input_bindings;
    vertex_input_state_info.vertexAttributeDescriptionCount = 1;
    vertex_input_state_info.pVertexAttributeDescriptions = vertex_attributes;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info{};
    input_assembly_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;

    VkViewport viewport{
        0.0f, 0.0f,   (float)window_extent_.width, (float)window_extent_.height,
        0.1,  1000.0f};

    VkRect2D scissor = {{0, 0}, window_extent_};

    VkPipelineViewportStateCreateInfo viewport_state_info{};
    viewport_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_info.viewportCount = 1;
    viewport_state_info.pViewports = &viewport;
    viewport_state_info.scissorCount = 1;
    viewport_state_info.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterization_state_info{};
    rasterization_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_info.rasterizerDiscardEnable = VK_TRUE;
    rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization_state_info.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    VkPipelineColorBlendStateCreateInfo color_blend_state_info{};
    color_blend_state_info.sType =
        VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_info.attachmentCount = 1;
    color_blend_state_info.pAttachments = &color_blend_attachment;

    VkAttachmentDescription color_attachment{};
    color_attachment.format = swapchain_image_format_;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_reference{};
    color_attachment_reference.layout =
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_reference;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &subpass_dependency;

    result =
        vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_);
    assert(result == VK_SUCCESS);

    VkGraphicsPipelineCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.stageCount = 1;
    info.pStages = &shader_stage_info;
    info.pVertexInputState = &vertex_input_state_info;
    info.pInputAssemblyState = &input_assembly_state_info;
    info.pViewportState = &viewport_state_info;
    info.pRasterizationState = &rasterization_state_info;
    info.pColorBlendState = &color_blend_state_info;
    info.layout = pipeline_layout_;
    info.renderPass = render_pass_;

    result = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info,
                                       nullptr, &pipeline_);
    assert(result == VK_SUCCESS);
  }

  void load_shader() {
    // TODO: load a vertex shader and a fragment shader.
    std::ifstream stream("vertex.spv", std::ios::ate);
    assert(stream);
    std::size_t length = stream.tellg();
    stream.seekg(0, std::ios_base::beg);
    std::string sources(length, '\0');
    stream.read(sources.data(), length);

    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = sources.length();
    info.pCode = reinterpret_cast<uint32_t*>(sources.data());

    VkResult result =
        vkCreateShaderModule(device_, &info, nullptr, &shader_module_);
    assert(result == VK_SUCCESS);
  }

  void create_sync_objects() {
    image_available_semaphores_.resize(kMaxFrames);
    render_finished_semaphores_.resize(kMaxFrames);
    in_flight_fences_.resize(kMaxFrames);
    in_flight_images_.resize(swapchain_image_views_.size(), VK_NULL_HANDLE);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    for (size_t i = 0; i < kMaxFrames; ++i) {
      if (vkCreateSemaphore(device_, &semaphore_info, nullptr,
                            &image_available_semaphores_[i]) != VK_SUCCESS ||
          vkCreateSemaphore(device_, &semaphore_info, nullptr,
                            &render_finished_semaphores_[i]) != VK_SUCCESS ||
          vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]) !=
              VK_SUCCESS) {
        assert(false);
      }
    }
  }

  void draw_frame() {
    vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE,
                    UINT64_MAX);
    vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);

    uint32_t image_index;
    vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                          image_available_semaphores_[current_frame_],
                          VK_NULL_HANDLE, &image_index);

    if (in_flight_images_[image_index] != VK_NULL_HANDLE) {
      vkWaitForFences(device_, 1, &in_flight_images_[image_index], VK_TRUE,
                      UINT64_MAX);
    }
    in_flight_images_[image_index] = in_flight_fences_[current_frame_];

    VkSubmitInfo submit_info {};
    VkPipelineStageFlags wait_dst_stage_masks[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = &image_available_semaphores_[current_frame_];
    submit_info.pWaitDstStageMask = wait_dst_stage_masks;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers_[current_frame_];
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &render_finished_semaphores_[current_frame_];
    VkResult result = vkQueueSubmit(queue_, 1, &submit_info, in_flight_fences_[current_frame_]);
    assert(result == VK_SUCCESS);

    VkPresentInfoKHR present_info {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores_[current_frame_];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &image_index;
    vkQueuePresentKHR(queue_, &present_info);

    current_frame_ = (current_frame_ + 1) % kMaxFrames;
  }

  std::vector<VkImage> get_swapchain_images() {
    uint32_t image_count;
    VkResult result = vkGetSwapchainImagesKHR(device_,
                                              swapchain_,
                                              &image_count,
                                              nullptr);
    assert(result == VK_SUCCESS);

    std::vector<VkImage> images(image_count);
    result = vkGetSwapchainImagesKHR(device_,
                                     swapchain_,
                                     &image_count,
                                     images.data());
    assert(result == VK_SUCCESS);
    return images;
  }

  void create_vulkan_framebuffers() {
    // Create one framebuffer per image in the swapchain.
    std::vector<VkImage> images = get_swapchain_images();
    swapchain_image_views_.resize(images.size());
    framebuffers_.resize(images.size());
    for (size_t i = 0; i < images.size(); ++i) {
      // A Vulkan image can only be manipulated via an image
      // view. Let's create one.
      auto image = images[i];
      VkImageViewCreateInfo image_view_info{};
      image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      image_view_info.image = image;
      image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      image_view_info.format = swapchain_image_format_;
      image_view_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
      image_view_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
      image_view_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
      image_view_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
      image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      image_view_info.subresourceRange.baseMipLevel = 0;
      image_view_info.subresourceRange.levelCount = 1;
      image_view_info.subresourceRange.baseArrayLayer = 0;
      image_view_info.subresourceRange.layerCount = 1;
      VkResult result = vkCreateImageView(device_,
                                          &image_view_info,
                                          nullptr,
                                          &swapchain_image_views_[i]);
      assert(result == VK_SUCCESS);

      // Let's create a framebuffer.
      VkImageView attachments[] = {
        swapchain_image_views_[i]
      };
      VkFramebufferCreateInfo framebuffer_info{};
      framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
      framebuffer_info.renderPass = render_pass_;
      framebuffer_info.attachmentCount = 1;
      framebuffer_info.pAttachments = attachments;
      framebuffer_info.width = window_extent_.width;
      framebuffer_info.height = window_extent_.height;
      framebuffer_info.layers = 1;
      result = vkCreateFramebuffer(device_,
                                   &framebuffer_info,
                                   nullptr,
                                   &(framebuffers_[i]));
      assert(result == VK_SUCCESS);
    }
  }

 public:
  App()
      : window_extent_{800, 600},
        window_(nullptr),
        instance_(VK_NULL_HANDLE),
        physical_device_(VK_NULL_HANDLE),
        queue_family_index_(VK_NULL_HANDLE),
        device_(VK_NULL_HANDLE),
        queue_(VK_NULL_HANDLE),
        command_pool_(VK_NULL_HANDLE),
        command_buffers_(kMaxFrames, VK_NULL_HANDLE),
        surface_(VK_NULL_HANDLE),
        swapchain_(VK_NULL_HANDLE),
        swapchain_image_views_{},
        framebuffers_{},
        shader_module_(VK_NULL_HANDLE),
        pipeline_layout_(VK_NULL_HANDLE),
        pipeline_(VK_NULL_HANDLE),
        render_pass_(VK_NULL_HANDLE),
        swapchain_image_format_(VK_FORMAT_UNDEFINED),
        image_available_semaphores_{},
        render_finished_semaphores_{},
        in_flight_fences_{},
        in_flight_images_{},
        current_frame_(0) {}

  App(const App&) = delete;
  App(App&&) = delete;
  const App& operator=(const App&) = delete;

  void init_vulkan() {
    window_ = SDL_CreateWindow("Vulkan demo", 0, 0, window_extent_.width,
                               window_extent_.height, SDL_WINDOW_VULKAN);
    assert(window_ != nullptr);
    create_vulkan_instance();
    create_vulkan_surface();
    find_physical_device();
    create_device();
    create_vulkan_swapchain();
    load_shader();
    create_pipeline_layout();
    create_vulkan_pipeline();
    create_vulkan_framebuffers();
    create_vulkan_command_pool();
    create_vulkan_command_buffer();
    create_sync_objects();
  }

  void cleanup() {
    for (size_t i = 0; i < kMaxFrames; ++i) {
      vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
      vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
      vkDestroyFence(device_, in_flight_fences_[i], nullptr);
    }
    vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
    vkDestroyRenderPass(device_, render_pass_, nullptr);
    vkDestroyPipeline(device_, pipeline_, nullptr);
    vkDestroyShaderModule(device_, shader_module_, nullptr);
    vkDestroyCommandPool(device_, command_pool_, nullptr);
    for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
      vkDestroyImageView(device_, swapchain_image_views_[i], nullptr);
      vkDestroyFramebuffer(device_, framebuffers_[i], nullptr);
    }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    vkDestroyDevice(device_, nullptr);
    vkDestroySurfaceKHR(instance_, surface_, nullptr);
    vkDestroyInstance(instance_, nullptr);
    SDL_DestroyWindow(window_);
  }

  void run() {
    bool run = true;

    while (run) {
      SDL_Event event;
      SDL_PollEvent(&event);
      if (event.type == SDL_QUIT)
        run = false;
      draw_frame();
      SDL_Delay(16);
    }
  }
};

int main() {
  App app;
  assert(SDL_Init(SDL_INIT_EVERYTHING) == 0);
  app.init_vulkan();
  app.run();
  app.cleanup();
  SDL_Quit();
  return 0;
}
