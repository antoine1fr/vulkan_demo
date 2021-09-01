#pragma once

#include <map>
#include <unordered_map>
#include <vector>

// Third party headers
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "base.hpp"
#include "render/Frame.hpp"

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
  VkShaderModule vertex_shader_module_;
  VkShaderModule fragment_shader_module_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline pipeline_;
  VkRenderPass render_pass_;
  VkFormat swapchain_image_format_;
  std::vector<VkSemaphore> image_available_semaphores_;
  std::vector<VkSemaphore> render_finished_semaphores_;
  std::vector<VkFence> in_flight_fences_;
  std::vector<VkFence> in_flight_images_;
  size_t current_frame_;
  size_t frame_number_;
  VkBuffer vertex_buffer_;
  VkDeviceMemory vertex_buffer_memory_;
  std::vector<VkBuffer>
      ubos_for_frames_;  ///< uniform buffer objects referenced by frame id
  std::vector<VkDeviceMemory>
      ubo_memories_for_frames_;  ///< ubo memories referenced by frame id
  VkDescriptorSetLayout descriptor_set_layout_;
  VkDescriptorPool descriptor_pool_;
  std::vector<VkDescriptorSet> descriptor_sets_;
  std::unordered_map<ResourceId, VkBuffer> vulkan_buffers_;
  std::unordered_map<ResourceId, VkDeviceMemory> vulkan_device_memories_;
  render::Frame frame_;

 private:
  std::vector<VkPhysicalDevice> enumerate_physical_devices(VkInstance instance);
  void check_extensions(
      std::vector<VkExtensionProperties> available_extensions_vec,
      std::vector<const char*> wanted_extensions);
  void check_layers(std::vector<VkLayerProperties> available_layers_vec,
                    std::vector<const char*> wanted_layers);
  void create_vulkan_instance();
  void create_vulkan_surface();
  void select_best_surface_format(VkSurfaceFormatKHR& surface_format);
  void create_vulkan_swapchain();

  // The SDL doesn't seem to offer any way to retrieve the physical
  // device it used to create the presentation surface. So we need to
  // enumerate the Vulkan-compatible devices that have
  // graphics-compatible queue families and ask Vulkan whether the
  // (physical device x queue family x surface) triple is valid.
  void find_physical_device();
  void enumerate_device_extensions(
      VkPhysicalDevice device,
      std::map<std::string, VkExtensionProperties>& extension_map);
  void check_device_extensions(VkPhysicalDevice physical_device,
                               std::vector<const char*> wanted_extensions);
  void create_device();
  void create_vulkan_command_pool();
  void create_vulkan_command_buffer();
  void create_pipeline_layout();
  void create_vulkan_pipeline();
  std::string load_file(const std::string& path, std::ios::openmode mode);
  VkShaderModule load_shader(const std::string& path);
  void load_shaders();
  void create_sync_objects();
  uint32_t begin_frame();
  void end_frame(uint32_t image_index);
  void update_uniform_block(size_t frame_id,
                            const render::Frame::UniformBlock& block);
  void draw_frame();
  std::vector<VkImage> get_swapchain_images();
  void create_vulkan_framebuffers();
  uint32_t find_memory_type(uint32_t type_filter,
                            VkMemoryPropertyFlags properties);
  void create_vulkan_buffer(VkBufferUsageFlags usage,
                            VkDeviceSize size,
                            VkBuffer* buffer,
                            VkDeviceMemory* memory);
  void create_vulkan_vertex_buffer();
  void create_uniform_buffer_objects();
  void create_descriptor_pool();
  void allocate_descriptor_sets();
  void create_frame_packet();

 public:
  App();

  App(const App&) = delete;
  App(App&&) = delete;
  const App& operator=(const App&) = delete;
  App& operator=(App&&) = delete;

  void init_vulkan();
  void cleanup();
  void run();
};