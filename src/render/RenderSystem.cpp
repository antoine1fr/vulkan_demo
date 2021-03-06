// STL headers
#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <memory>
#include <sstream>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <SDL_image.h>

#include "render/RenderSystem.hpp"
#include "system.hpp"

namespace render {
namespace {
SDL_Surface* CreateMirrorSurface(SDL_Surface* surface) {
  int width = surface->w;
  int height = surface->h;
  SDL_Surface* new_surface = SDL_CreateRGBSurfaceWithFormat(
      0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
  uint32_t* src_pixels = reinterpret_cast<uint32_t*>(surface->pixels);
  uint32_t* dst_pixels = reinterpret_cast<uint32_t*>(new_surface->pixels);

  assert(SDL_LockSurface(new_surface) == 0);
  assert(SDL_LockSurface(surface) == 0);
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      uint32_t* src_ptr = src_pixels + y * width + x;
      uint32_t* dst_ptr = dst_pixels + ((height - y - 1) * width) + x;
      *dst_ptr = *src_ptr;
    }
  }
  SDL_UnlockSurface(surface);
  SDL_UnlockSurface(new_surface);
  return new_surface;
}

SDL_Surface* LoadSdlImageFromFile(const std::string& path) {
  std::cout << "Loading texture from file: " << path << '\n';
  SDL_Surface* original_surface = IMG_Load(path.c_str());
  SDL_Surface* rgba32_surface =
      SDL_ConvertSurfaceFormat(original_surface, SDL_PIXELFORMAT_RGBA32, 0);
  SDL_FreeSurface(original_surface);
  SDL_Surface* mirror_surface = CreateMirrorSurface(rgba32_surface);
  SDL_FreeSurface(rgba32_surface);
  return mirror_surface;
}

void BlitSdlSurfaceToVulkanBuffer(SDL_Surface* surface,
                                  vulkan::Buffer& staging_buffer) {
  SDL_LockSurface(surface);
  auto src_ptr = reinterpret_cast<uint8_t*>(surface->pixels);
  auto dst_ptr = staging_buffer.Map<uint8_t>();
  size_t size = static_cast<size_t>(surface->w) *
                static_cast<size_t>(surface->h) *
                static_cast<size_t>(surface->format->BytesPerPixel);
  std::memcpy(dst_ptr, src_ptr, size);
  staging_buffer.Unmap();
  SDL_UnlockSurface(surface);
  SDL_FreeSurface(surface);
}
}  // namespace

void RenderSystem::CheckExtensions(
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

void RenderSystem::CheckLayers(
    std::vector<VkLayerProperties> available_layers_vec,
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

void RenderSystem::CreateVulkanInstance() {
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
  VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count, nullptr));
  std::vector<VkLayerProperties> layer_properties(layer_count);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&layer_count,
                                              layer_properties.data()));
  std::vector<const char*> layer_names{"VK_LAYER_KHRONOS_validation"};
  CheckLayers(layer_properties, layer_names);

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
  instance_info.enabledLayerCount = static_cast<uint32_t>(layer_names.size());
  instance_info.ppEnabledLayerNames = layer_names.data();
  instance_info.enabledExtensionCount =
      static_cast<uint32_t>(extension_names.size());
  instance_info.ppEnabledExtensionNames = extension_names.data();

  VK_CHECK(vkCreateInstance(&instance_info, nullptr, &instance_));
}

void RenderSystem::CreateVulkanSurface() {
  SDL_bool result = SDL_Vulkan_CreateSurface(window_, instance_, &surface_);
  assert(result == SDL_TRUE);
}

void RenderSystem::SelectBestSurfaceFormat(VkSurfaceFormatKHR& surface_format) {
  struct Compare {
    bool operator()(const VkSurfaceFormatKHR& lhs,
                    const VkSurfaceFormatKHR& rhs) const {
      if (lhs.format < rhs.format)
        return true;
      if (lhs.format > rhs.format)
        return false;
      if (lhs.colorSpace < rhs.colorSpace)
        return true;
      return false;
    }
  };

  const std::map<VkSurfaceFormatKHR, size_t, Compare> kValidFormats{
      {{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, 0},
      {{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}, 1}};

  uint32_t format_count;
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_,
                                                &format_count, nullptr));

  std::vector<VkSurfaceFormatKHR> formats(format_count);
  VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_,
                                                &format_count, formats.data()));

  size_t best_index = 0;
  std::optional<VkSurfaceFormatKHR> best_format;
  for (const auto& format : formats) {
    auto it = kValidFormats.find(format);
    if (it != kValidFormats.cend() && it->second <= best_index) {
      best_index = it->second;
      best_format = format;
    }
  }
  assert(best_format.has_value());
  surface_format = best_format.value();
}

void RenderSystem::CreateSwapchain() {
  VkSurfaceFormatKHR surface_format;
  SelectBestSurfaceFormat(surface_format);
  swapchain_image_format_ = surface_format.format;

  VkSurfaceCapabilitiesKHR capabilities{};
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_,
                                                     &capabilities));

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

  VK_CHECK(vkCreateSwapchainKHR(device_, &swapchain_create_info, nullptr,
                                &swapchain_));
}

std::vector<VkPhysicalDevice> RenderSystem::EnumeratePhysicalDevices(
    VkInstance instance) {
  uint32_t device_count;

  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, nullptr));
  if (device_count == 0) {
    std::cerr << "No Vulkan-compatible physical device found.\n";
    assert(false);
  }

  std::vector<VkPhysicalDevice> physical_devices(device_count);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count,
                                      physical_devices.data()));
  return physical_devices;
}

void RenderSystem::FindPhysicalDevice() {
  // The SDL doesn't seem to offer any way to retrieve the physical
  // device it used to create the presentation surface. So we need to
  // enumerate the Vulkan-compatible devices that have
  // graphics-compatible queue families and ask Vulkan whether the
  // (physical device x queue family x surface) triple is valid.
  std::vector<VkPhysicalDevice> physical_devices =
      EnumeratePhysicalDevices(instance_);

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
}

void RenderSystem::EnumerateDeviceExtensions(
    VkPhysicalDevice device,
    std::map<std::string, VkExtensionProperties>& extension_map) {
  uint32_t extension_count;
  VK_CHECK(vkEnumerateDeviceExtensionProperties(device, nullptr,
                                                &extension_count, nullptr));

  std::vector<VkExtensionProperties> extensions(extension_count);
  VK_CHECK(vkEnumerateDeviceExtensionProperties(
      device, nullptr, &extension_count, extensions.data()));

  for (const auto& extension : extensions) {
    extension_map[extension.extensionName] = extension;
  }
}

void RenderSystem::CheckDeviceExtensions(
    VkPhysicalDevice physical_device,
    std::vector<const char*> wanted_extensions) {
  std::map<std::string, VkExtensionProperties> available_extensions;
  EnumerateDeviceExtensions(physical_device, available_extensions);

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

void RenderSystem::CreateDevice() {
  std::vector<const char*> extensions {
#if defined(DEMO_BUILD_APPLE)
    "VK_KHR_portability_subset",
#endif
        "VK_KHR_swapchain"
  };
  CheckDeviceExtensions(physical_device_, extensions);

  float queue_priorities = 1.0f;
  VkDeviceQueueCreateInfo queue_info{};
  queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_info.queueFamilyIndex = queue_family_index_;
  queue_info.queueCount = 1;
  queue_info.pQueuePriorities = &queue_priorities;

  VkPhysicalDeviceFeatures device_features{};
  device_features.samplerAnisotropy = VK_TRUE;

  VkDeviceCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  info.queueCreateInfoCount = 1;
  info.pQueueCreateInfos = &queue_info;
  info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
  info.ppEnabledExtensionNames = extensions.data();
  info.pEnabledFeatures = &device_features;

  VK_CHECK(vkCreateDevice(physical_device_, &info, nullptr, &device_));

  vkGetDeviceQueue(device_, queue_family_index_, 0, &queue_);
}

void RenderSystem::CreateCommandPool() {
  VkCommandPoolCreateInfo create_info{};
  create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  create_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  create_info.queueFamilyIndex = queue_family_index_;

  VK_CHECK(vkCreateCommandPool(device_, &create_info, nullptr, &command_pool_));
}

void RenderSystem::CreateCommandBuffer() {
  VkCommandBufferAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  info.commandPool = command_pool_;
  info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  info.commandBufferCount = static_cast<uint32_t>(kMaxFrames);

  VK_CHECK(vkAllocateCommandBuffers(device_, &info, command_buffers_.data()));
}

void RenderSystem::CreatePassDescriptorSetLayout(
    const UniformBufferDescriptor& uniform_buffer_descriptor) {
  std::vector<VkDescriptorSetLayoutBinding> bindings(
      uniform_buffer_descriptor.blocks.size());
  size_t i = 0;

  // First the uniform block bindings.
  for (const auto& block : uniform_buffer_descriptor.blocks) {
    bindings[i].binding = block.binding;
    bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    bindings[i].descriptorCount = 1;
    bindings[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    i++;
  }

  VkDescriptorSetLayoutCreateInfo descriptor_layout_info{};
  descriptor_layout_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
  descriptor_layout_info.pBindings = bindings.data();

  VK_CHECK(vkCreateDescriptorSetLayout(device_, &descriptor_layout_info,
                                       nullptr, &pass_descriptor_set_layout_));
}

void RenderSystem::CreateRenderObjectDescriptorSetLayout() {
  std::vector<VkDescriptorSetLayoutBinding> bindings{
      {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
       VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};

  VkDescriptorSetLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  info.bindingCount = static_cast<uint32_t>(bindings.size());
  info.pBindings = bindings.data();

  VK_CHECK(vkCreateDescriptorSetLayout(device_, &info, nullptr,
                                       &render_object_descriptor_set_layout_));
}

void RenderSystem::CreatePipelineLayout() {
  std::vector<VkDescriptorSetLayout> layouts = {
      pass_descriptor_set_layout_, render_object_descriptor_set_layout_};
  VkPipelineLayoutCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  info.setLayoutCount = static_cast<uint32_t>(layouts.size());
  info.pSetLayouts = layouts.data();
  VK_CHECK(vkCreatePipelineLayout(device_, &info, nullptr, &pipeline_layout_));
}

void RenderSystem::CreatePipeline() {
  VkPipelineShaderStageCreateInfo vertex_shader_stage_info{};
  vertex_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  vertex_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
  vertex_shader_stage_info.module = vertex_shader_module_;
  vertex_shader_stage_info.pName = "main";

  VkPipelineShaderStageCreateInfo fragment_shader_stage_info{};
  fragment_shader_stage_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  fragment_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
  fragment_shader_stage_info.module = fragment_shader_module_;
  fragment_shader_stage_info.pName = "main";

  auto binding_description = render::Vertex::get_binding_description();
  auto attribute_descriptions = render::Vertex::get_attribute_descriptions();

  VkPipelineVertexInputStateCreateInfo vertex_input_state_info{};
  vertex_input_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_input_state_info.vertexBindingDescriptionCount = 1;
  vertex_input_state_info.pVertexBindingDescriptions = &binding_description;
  vertex_input_state_info.vertexAttributeDescriptionCount =
      static_cast<uint32_t>(attribute_descriptions.size());
  vertex_input_state_info.pVertexAttributeDescriptions =
      attribute_descriptions.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_info{};
  input_assembly_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

  VkViewport viewport{};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)window_extent_.width;
  viewport.height = (float)window_extent_.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

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
  // rasterization_state_info.rasterizerDiscardEnable = VK_TRUE;
  rasterization_state_info.polygonMode = VK_POLYGON_MODE_FILL;
  rasterization_state_info.cullMode = VK_CULL_MODE_NONE;
  rasterization_state_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  rasterization_state_info.lineWidth = 1.0f;

  VkPipelineMultisampleStateCreateInfo multisampling_state_info{};
  multisampling_state_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling_state_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

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
  color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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

  VK_CHECK(
      vkCreateRenderPass(device_, &render_pass_info, nullptr, &render_pass_));

  VkPipelineShaderStageCreateInfo shader_stages[] = {
      vertex_shader_stage_info, fragment_shader_stage_info};
  VkGraphicsPipelineCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  info.stageCount = 2;
  info.pStages = shader_stages;
  info.pVertexInputState = &vertex_input_state_info;
  info.pInputAssemblyState = &input_assembly_state_info;
  info.pViewportState = &viewport_state_info;
  info.pRasterizationState = &rasterization_state_info;
  info.pMultisampleState = &multisampling_state_info;
  info.pColorBlendState = &color_blend_state_info;
  info.layout = pipeline_layout_;
  info.renderPass = render_pass_;

  VK_CHECK(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr,
                                     &pipeline_));
}

std::string RenderSystem::LoadFile(const std::string& path,
                                   std::ios::openmode mode) {
  std::ifstream stream(path, std::ios::ate | mode);
  assert(stream);
  std::size_t length = stream.tellg();
  stream.seekg(0, std::ios_base::beg);
  std::string sources(length, '\0');
  stream.read(sources.data(), length);
  return sources;
}

VkShaderModule RenderSystem::LoadShader(const std::string& path) {
  std::string sources = LoadFile(path, std::ios::binary);

  VkShaderModuleCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  info.codeSize = sources.length();
  info.pCode = reinterpret_cast<uint32_t*>(sources.data());

  VkShaderModule shader_module;
  VK_CHECK(vkCreateShaderModule(device_, &info, nullptr, &shader_module));
  return shader_module;
}

void RenderSystem::LoadShaders() {
  vertex_shader_module_ = LoadShader("vertex.spv");
  fragment_shader_module_ = LoadShader("fragment.spv");
}

void RenderSystem::CreateSyncObjects() {
  image_available_semaphores_.resize(kMaxFrames);
  render_finished_semaphores_.resize(kMaxFrames);
  in_flight_fences_.resize(kMaxFrames);
  in_flight_images_.resize(swapchain_image_views_.size(), VK_NULL_HANDLE);

  VkSemaphoreCreateInfo semaphore_info{};
  semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

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

uint32_t RenderSystem::BeginFrame() {
  vkWaitForFences(device_, 1, &in_flight_fences_[current_frame_], VK_TRUE,
                  UINT64_MAX);

  uint32_t image_index;
  vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                        image_available_semaphores_[current_frame_],
                        VK_NULL_HANDLE, &image_index);

  if (in_flight_images_[image_index] != VK_NULL_HANDLE) {
    vkWaitForFences(device_, 1, &in_flight_images_[image_index], VK_TRUE,
                    UINT64_MAX);
  }
  in_flight_images_[image_index] = in_flight_fences_[current_frame_];

  VkCommandBuffer command_buffer = command_buffers_[current_frame_];

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  vkResetCommandBuffer(command_buffer, 0);
  vkBeginCommandBuffer(command_buffer, &begin_info);

  VkClearValue clear_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

  VkRenderPassBeginInfo render_pass_info{};
  render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_info.renderPass = render_pass_;
  render_pass_info.framebuffer = framebuffers_[image_index];
  render_pass_info.renderArea.offset = {0, 0};
  render_pass_info.renderArea.extent = window_extent_;
  render_pass_info.clearValueCount = 1;
  render_pass_info.pClearValues = &clear_color;

  vkCmdBeginRenderPass(command_buffer, &render_pass_info,
                       VK_SUBPASS_CONTENTS_INLINE);
  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

  return image_index;
}

void RenderSystem::EndFrame(uint32_t image_index) {
  // finish writing into command buffer

  VkCommandBuffer command_buffer = command_buffers_[current_frame_];

  vkCmdEndRenderPass(command_buffer);
  VK_CHECK(vkEndCommandBuffer(command_buffer));

  // submit command buffer to comand queue

  VkSubmitInfo submit_info{};
  VkPipelineStageFlags wait_dst_stage_masks[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &image_available_semaphores_[current_frame_];
  submit_info.pWaitDstStageMask = wait_dst_stage_masks;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &render_finished_semaphores_[current_frame_];

  vkResetFences(device_, 1, &in_flight_fences_[current_frame_]);
  VK_CHECK(vkQueueSubmit(queue_, 1, &submit_info,
                         in_flight_fences_[current_frame_]));

  // present to render surface

  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &render_finished_semaphores_[current_frame_];
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &swapchain_;
  present_info.pImageIndices = &image_index;
  vkQueuePresentKHR(queue_, &present_info);

  current_frame_ = (current_frame_ + 1) % kMaxFrames;
  frame_number_++;
}

void RenderSystem::UpdateUniformBlock(
    size_t frame_id,
    const render::Frame::UniformBlock& block) {
  VkDeviceMemory memory = ubos_for_frames_[frame_id]->memory_;
  void* data;

  vkMapMemory(device_, memory, static_cast<VkDeviceSize>(block.offset),
              static_cast<VkDeviceSize>(block.data.size()), 0, &data);
  memcpy(data, block.data.data(), block.data.size());
  vkUnmapMemory(device_, memory);
}

void RenderSystem::DrawFrame(const Frame& frame) {
  uint32_t image_index = BeginFrame();

  vkCmdBindDescriptorSets(command_buffers_[current_frame_],
                          VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0,
                          1, &pass_descriptor_sets_[current_frame_], 0,
                          nullptr);
  for (const auto& pass : frame.passes) {
    UpdateUniformBlock(current_frame_, pass.uniform_block);
    for (const auto& render_object : pass.render_objects) {
      VkDescriptorSet render_object_descriptor_set =
          materials_[render_object.material_id];
      const Mesh& mesh = meshes_[render_object.mesh_id];
      VkBuffer vertex_buffers[] = {mesh.vertex_buffer->buffer_};
      VkDeviceSize offsets[] = {0};
      UpdateUniformBlock(current_frame_, render_object.uniform_block);
      vkCmdBindVertexBuffers(command_buffers_[current_frame_], 0, 1,
                             vertex_buffers, offsets);
      vkCmdBindIndexBuffer(command_buffers_[current_frame_],
                           mesh.index_buffer->buffer_, 0, VK_INDEX_TYPE_UINT32);
      vkCmdBindDescriptorSets(command_buffers_[current_frame_],
                              VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_,
                              1, 1, &render_object_descriptor_set, 0, nullptr);
      vkCmdDrawIndexed(command_buffers_[current_frame_],
                       static_cast<uint32_t>(mesh.index_count), 1, 0, 0, 0);
    }
  }

  EndFrame(image_index);
}

std::vector<VkImage> RenderSystem::GetSwapchainImages() {
  uint32_t image_count;
  VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count, nullptr));

  std::vector<VkImage> images(image_count);
  VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &image_count,
                                   images.data()));
  return images;
}

void RenderSystem::CreateFramebuffers() {
  // Create one framebuffer per image in the swapchain.
  std::vector<VkImage> images = GetSwapchainImages();
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
    VK_CHECK(vkCreateImageView(device_, &image_view_info, nullptr,
                               &swapchain_image_views_[i]));

    // Let's create a framebuffer.
    VkImageView attachments[] = {swapchain_image_views_[i]};
    VkFramebufferCreateInfo framebuffer_info{};
    framebuffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebuffer_info.renderPass = render_pass_;
    framebuffer_info.attachmentCount = 1;
    framebuffer_info.pAttachments = attachments;
    framebuffer_info.width = window_extent_.width;
    framebuffer_info.height = window_extent_.height;
    framebuffer_info.layers = 1;
    VK_CHECK(vkCreateFramebuffer(device_, &framebuffer_info, nullptr,
                                 &(framebuffers_[i])));
  }
}

size_t RenderSystem::CreateMesh(const std::string& name,
                                const std::vector<render::Vertex>& vertices,
                                const std::vector<uint32_t>& indices) {
  size_t id = std::hash<std::string>{}(name);
  auto vertex_buffer =
      CreateBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices);
  auto index_buffer = CreateBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices);
  meshes_[id] =
      Mesh{std::move(vertex_buffer), std::move(index_buffer), indices.size()};
  return id;
}

void RenderSystem::CopyBuffer(VkBuffer src_buffer,
                              VkBuffer dst_buffer,
                              VkDeviceSize size) {
  VkBufferCopy copy_info{};
  copy_info.srcOffset = 0;
  copy_info.dstOffset = 0;
  copy_info.size = size;

  VkCommandBuffer command_buffer = BeginCommands();
  vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_info);
  EndCommands(command_buffer);
}

void RenderSystem::CreateUniformBufferObjects(size_t buffer_size) {
  ubos_for_frames_.resize(swapchain_image_views_.size());
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    ubos_for_frames_[i] = new vulkan::Buffer(
        physical_device_, device_, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        static_cast<VkDeviceSize>(buffer_size));
  }
}

void RenderSystem::AllocateUboDescriptorSets(
    const UniformBufferDescriptor& uniform_buffer_descriptor) {
  uint32_t descriptor_count =
      static_cast<uint32_t>(swapchain_image_views_.size());
  size_t block_count = uniform_buffer_descriptor.blocks.size();
  size_t write_count = descriptor_count * block_count;

  pass_descriptor_sets_ = AllocateDescriptorSets(
      pass_descriptor_set_layout_, descriptor_count,
      {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, descriptor_count * 2}});

  std::vector<VkWriteDescriptorSet> write_infos(write_count);
  std::vector<VkDescriptorBufferInfo> buffer_infos(write_count);
  for (size_t set_id = 0, write_info_id = 0; set_id < descriptor_count;
       ++set_id) {
    auto it = uniform_buffer_descriptor.blocks.begin();
    for (size_t block_id = 0; block_id < block_count;
         ++block_id, ++write_info_id) {
      const auto& block_descriptor = *it;
      size_t i = set_id * block_count + block_id;
      buffer_infos[i].buffer = ubos_for_frames_[set_id]->buffer_;
      buffer_infos[i].offset = block_descriptor.offset;
      buffer_infos[i].range = block_descriptor.range;

      write_infos[write_info_id].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
      write_infos[write_info_id].dstSet = pass_descriptor_sets_[set_id];
      write_infos[write_info_id].dstBinding = static_cast<uint32_t>(block_id);
      write_infos[write_info_id].dstArrayElement = 0;
      write_infos[write_info_id].descriptorCount = 1;
      write_infos[write_info_id].descriptorType =
          VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
      write_infos[write_info_id].pBufferInfo = &buffer_infos[block_id];

      ++it;
    }
  }

  vkUpdateDescriptorSets(device_, static_cast<uint32_t>(write_infos.size()),
                         write_infos.data(), 0, nullptr);
}

std::vector<VkDescriptorSet> RenderSystem::AllocateDescriptorSets(
    VkDescriptorSetLayout layout,
    size_t descriptor_set_count,
    const std::vector<VkDescriptorPoolSize>& pool_sizes) {
  std::vector<VkDescriptorSetLayout> layouts(descriptor_set_count, layout);
  std::vector<VkDescriptorSet> descriptor_sets(descriptor_set_count,
                                               VK_NULL_HANDLE);
  VkDescriptorPool pool =
      descriptor_pool_cache_->GetPool(descriptor_sets.size(), pool_sizes);

  VkDescriptorSetAllocateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  info.descriptorPool = pool;
  info.descriptorSetCount = static_cast<uint32_t>(descriptor_set_count);
  info.pSetLayouts = layouts.data();

  VK_CHECK(vkAllocateDescriptorSets(device_, &info, descriptor_sets.data()));

  return descriptor_sets;
}

RenderSystem::RenderSystem() : command_buffers_(kMaxFrames, VK_NULL_HANDLE) {}

void RenderSystem::Init(
    const UniformBufferDescriptor& uniform_buffer_descriptor) {
  window_ = SDL_CreateWindow("Vulkan demo", SDL_WINDOWPOS_CENTERED,
                             SDL_WINDOWPOS_CENTERED, window_extent_.width,
                             window_extent_.height, SDL_WINDOW_VULKAN);
  assert(window_ != nullptr);
  CreateVulkanInstance();
  CreateVulkanSurface();
  FindPhysicalDevice();
  CreateDevice();
  CreateSwapchain();
  LoadShaders();
  CreatePassDescriptorSetLayout(uniform_buffer_descriptor);
  CreateRenderObjectDescriptorSetLayout();
  CreatePipelineLayout();
  CreatePipeline();
  CreateFramebuffers();
  CreateCommandPool();
  CreateUniformBufferObjects(uniform_buffer_descriptor.size);
  CreateCommandBuffer();
  CreateSyncObjects();
  descriptor_pool_cache_ =
      std::make_unique<vulkan::DescriptorPoolCache>(device_);
  AllocateUboDescriptorSets(uniform_buffer_descriptor);
}

void RenderSystem::Cleanup() {
  descriptor_pool_cache_.reset(nullptr);
  for (const auto& texture : textures_) {
    vkDestroyImageView(device_, std::get<1>(texture.second), nullptr);
    vkDestroySampler(device_, std::get<2>(texture.second), nullptr);
  }
  textures_.clear();
  meshes_.clear();
  for (size_t i = 0; i < kMaxFrames; ++i) {
    vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
    vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
    vkDestroyFence(device_, in_flight_fences_[i], nullptr);
  }
  vkDestroyPipelineLayout(device_, pipeline_layout_, nullptr);
  vkDestroyRenderPass(device_, render_pass_, nullptr);
  vkDestroyPipeline(device_, pipeline_, nullptr);
  vkDestroyShaderModule(device_, vertex_shader_module_, nullptr);
  vkDestroyShaderModule(device_, fragment_shader_module_, nullptr);
  vkDestroyCommandPool(device_, command_pool_, nullptr);
  vkDestroyDescriptorSetLayout(device_, pass_descriptor_set_layout_, nullptr);
  vkDestroyDescriptorSetLayout(device_, render_object_descriptor_set_layout_,
                               nullptr);
  for (size_t i = 0; i < swapchain_image_views_.size(); ++i) {
    delete ubos_for_frames_[i];
    vkDestroyImageView(device_, swapchain_image_views_[i], nullptr);
    vkDestroyFramebuffer(device_, framebuffers_[i], nullptr);
  }
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  vkDestroyDevice(device_, nullptr);
  vkDestroySurfaceKHR(instance_, surface_, nullptr);
  vkDestroyInstance(instance_, nullptr);
  SDL_DestroyWindow(window_);
}

void RenderSystem::WaitIdle() {
  vkDeviceWaitIdle(device_);
}

void RenderSystem::LoadMaterial(ResourceId id,
                                const std::vector<std::string>& paths) {
  std::vector<VkDescriptorImageInfo> image_info(paths.size());
  std::vector<VkWriteDescriptorSet> write_info(paths.size());
  std::vector<VkDescriptorSet> descriptor_sets =
      AllocateDescriptorSets(render_object_descriptor_set_layout_, 1,
                             {{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                               static_cast<uint32_t>(paths.size())}});
  VkDescriptorSet descriptor_set = descriptor_sets[0];

  for (size_t i = 0; i < paths.size(); ++i) {
    ResourceId id = LoadImageFromFile(paths[i]);

    image_info[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    image_info[i].imageView = std::get<1>(textures_[id]);
    image_info[i].sampler = std::get<2>(textures_[id]);

    write_info[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write_info[i].dstSet = descriptor_set;
    write_info[i].dstBinding = static_cast<uint32_t>(i);
    write_info[i].dstArrayElement = 0;
    write_info[i].descriptorCount = 1;
    write_info[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write_info[i].pImageInfo = &image_info[i];
  }

  vkUpdateDescriptorSets(device_, static_cast<uint32_t>(write_info.size()),
                         write_info.data(), 0, nullptr);

  materials_[id] = descriptor_set;
}

std::tuple<uint32_t, uint32_t> RenderSystem::GetWindowDimensions() const {
  return std::make_tuple(window_extent_.width, window_extent_.height);
}

ResourceId RenderSystem::LoadImageFromFile(const std::string& path) {
  SDL_Surface* surface = LoadSdlImageFromFile(path);
  VkDeviceSize staging_buffer_size =
      static_cast<VkDeviceSize>(surface->w) *
      static_cast<VkDeviceSize>(surface->h) *
      static_cast<VkDeviceSize>(surface->format->BytesPerPixel);

  vulkan::Buffer staging_buffer(physical_device_, device_,
                                VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
                                staging_buffer_size);
  BlitSdlSurfaceToVulkanBuffer(surface, staging_buffer);
  auto image = std::make_unique<vulkan::Image>(
      physical_device_, device_, static_cast<uint32_t>(surface->w),
      static_cast<uint32_t>(surface->h));

  ChangeImageLayout(image->image_, VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  CopyBufferToImage(staging_buffer, image.get(), surface->w, surface->h);
  ChangeImageLayout(image->image_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VkImageView image_view = GenerateImageView(image->image_);
  VkSampler sampler = CreateSampler();

  ResourceId id = std::hash<std::string>{}(path);
  textures_[id] = Texture(std::move(image), image_view, sampler);
  return id;
}

VkSampler RenderSystem::CreateSampler() {
  VkSampler sampler;

  // TODO: cache physical device properties.
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device_, &properties);

  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = VK_FILTER_LINEAR;
  info.minFilter = VK_FILTER_LINEAR;
  info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  info.anisotropyEnable = VK_TRUE;
  info.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
  info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  info.compareEnable = VK_FALSE;
  info.compareOp = VK_COMPARE_OP_ALWAYS;

  VK_CHECK(vkCreateSampler(device_, &info, nullptr, &sampler));

  return sampler;
}

VkImageView RenderSystem::GenerateImageView(VkImage image) {
  VkImageView image_view;

  VkImageViewCreateInfo image_view_info{};
  image_view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_info.image = image;
  image_view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  image_view_info.subresourceRange.layerCount = 1;
  image_view_info.subresourceRange.levelCount = 1;
  image_view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  VK_CHECK(vkCreateImageView(device_, &image_view_info, nullptr, &image_view));
  return image_view;
}

VkCommandBuffer RenderSystem::BeginCommands() {
  VkCommandBuffer command_buffer;
  VkCommandBufferAllocateInfo buffer_info{};
  buffer_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  buffer_info.commandBufferCount = 1;
  buffer_info.commandPool = command_pool_;
  buffer_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

  VK_CHECK(vkAllocateCommandBuffers(device_, &buffer_info, &command_buffer));

  VkCommandBufferBeginInfo begin_info{};
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begin_info);
  return command_buffer;
}

void RenderSystem::EndCommands(VkCommandBuffer command_buffer) {
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;

  VK_CHECK(vkQueueSubmit(queue_, 1, &submit_info, VK_NULL_HANDLE));
  VK_CHECK(vkQueueWaitIdle(queue_));
  vkFreeCommandBuffers(device_, command_pool_, 1, &command_buffer);
}

void RenderSystem::CopyBufferToImage(vulkan::Buffer& buffer,
                                     vulkan::Image* image,
                                     uint32_t width,
                                     uint32_t height) {
  VkCommandBuffer command_buffer = BeginCommands();
  VkBufferImageCopy copy_info{};
  copy_info.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  copy_info.imageSubresource.layerCount = 1;
  copy_info.imageExtent.width = width;
  copy_info.imageExtent.height = height;
  copy_info.imageExtent.depth = 1;
  vkCmdCopyBufferToImage(command_buffer, buffer.buffer_, image->image_,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info);
  EndCommands(command_buffer);
}

void RenderSystem::ChangeImageLayout(VkImage image,
                                     VkImageLayout src_layout,
                                     VkImageLayout dst_layout) {
  VkCommandBuffer command_buffer = BeginCommands();
  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.newLayout = dst_layout;
  barrier.oldLayout = src_layout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags src_stage;
  VkPipelineStageFlags dst_stage;

  if (src_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
      dst_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  } else if (src_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             dst_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else {
    assert(false);
  }

  vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
  EndCommands(command_buffer);
}
}  // namespace render