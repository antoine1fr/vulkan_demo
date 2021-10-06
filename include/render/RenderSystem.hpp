#pragma once

#include <map>
#include <memory>
#include <tuple>
#include <unordered_map>
#include <vector>

// Third party headers
#include <SDL.h>
#include <SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include "base.hpp"
#include "render/Frame.hpp"
#include "render/Vertex.hpp"
#include "render/vulkan/Buffer.hpp"
#include "render/vulkan/DescriptorPoolCache.hpp"
#include "render/vulkan/Image.hpp"

namespace render {
struct UniformBufferDescriptor {
  struct Block {
    uint32_t binding;
    size_t offset;
    size_t range;
  };
  size_t size;
  std::list<Block> blocks;
};

// Fat, messy god object. Yeaaah.
class RenderSystem {
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
  std::vector<vulkan::Buffer*>
      ubos_for_frames_;  ///< uniform buffer objects referenced by frame id
  VkDescriptorSetLayout descriptor_set_layout_;
  VkDescriptorPool descriptor_pool_;
  std::vector<VkDescriptorSet> ubo_descriptor_sets_;
  std::unordered_map<ResourceId, std::unique_ptr<vulkan::Buffer>>
      vulkan_buffers_;

  using Material = std::tuple<std::unique_ptr<vulkan::Image>, VkImageView, VkSampler>;
  std::unordered_map<ResourceId, Material> materials_;
  ResourceId debug_material_id_ = 0;

 private:
  std::vector<VkPhysicalDevice> EnumeratePhysicalDevices(VkInstance instance);
  void CheckExtensions(
      std::vector<VkExtensionProperties> available_extensions_vec,
      std::vector<const char*> wanted_extensions);
  void CheckLayers(std::vector<VkLayerProperties> available_layers_vec,
                   std::vector<const char*> wanted_layers);
  void CreateVulkanInstance();
  void CreateVulkanSurface();
  void SelectBestSurfaceFormat(VkSurfaceFormatKHR& surface_format);
  void CreateSwapchain();
  void FindPhysicalDevice();
  void EnumerateDeviceExtensions(
      VkPhysicalDevice device,
      std::map<std::string, VkExtensionProperties>& extension_map);
  void CheckDeviceExtensions(VkPhysicalDevice physical_device,
                             std::vector<const char*> wanted_extensions);
  void CreateDevice();
  void CreateCommandPool();
  void CreateCommandBuffer();
  void CreatePipelineLayout(
      const UniformBufferDescriptor& uniform_buffer_descriptor);
  void CreatePipeline();
  std::string LoadFile(const std::string& path, std::ios::openmode mode);
  VkShaderModule LoadShader(const std::string& path);
  void LoadShaders();
  void CreateSyncObjects();
  uint32_t BeginFrame();
  void EndFrame(uint32_t image_index);
  void UpdateUniformBlock(size_t frame_id,
                          const render::Frame::UniformBlock& block);
  std::vector<VkImage> GetSwapchainImages();
  void CreateFramebuffers();
  void CreateUniformBufferObjects(size_t buffer_size);
  void CreateDescriptorPool();
  void AllocateUboDescriptorSets(
      const UniformBufferDescriptor& uniform_buffer_descriptor);

  // Resource management
  VkCommandBuffer BeginCommands();
  void EndCommands(VkCommandBuffer command_buffer);
  ResourceId LoadImageFromFile(const std::string& path);
  VkSampler CreateSampler();
  VkImageView GenerateImageView(VkImage image);
  void CopyBufferToImage(vulkan::Buffer& buffer,
                         vulkan::Image* image,
                         uint32_t width,
                         uint32_t height);
  void ChangeImageLayout(VkImage image,
                         VkImageLayout src_layout,
                         VkImageLayout dst_layout);
  void CopyBuffer(VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

 public:
  RenderSystem();

  RenderSystem(const RenderSystem&) = delete;
  RenderSystem(RenderSystem&&) = delete;
  const RenderSystem& operator=(const RenderSystem&) = delete;
  RenderSystem& operator=(RenderSystem&&) = delete;

  void Cleanup();
  size_t CreateVertexBuffer(const std::string& name,
                            const std::vector<render::Vertex>& vertices);
  void DrawFrame(const Frame&);
  void Init(const UniformBufferDescriptor& uniform_buffer_descriptor);
  std::tuple<uint32_t, uint32_t> GetWindowDimensions() const;
  void WaitIdle();
  std::vector<VkDescriptorSet> AllocateDescriptorSets(
      VkDescriptorSetLayout layout,
      size_t descriptor_set_count,
      const std::vector<VkDescriptorPoolSize>& pool_sizes);
};
}  // namespace render