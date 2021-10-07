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
#include "render/Mesh.hpp"
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

  VkExtent2D window_extent_ = {800, 600};
  SDL_Window* window_ = nullptr;
  VkInstance instance_ = VK_NULL_HANDLE;
  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  uint32_t queue_family_index_ = 0;
  VkDevice device_ = VK_NULL_HANDLE;
  VkQueue queue_ = VK_NULL_HANDLE;
  VkCommandPool command_pool_ = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> command_buffers_;
  VkSurfaceKHR surface_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  std::vector<VkImageView> swapchain_image_views_ = {};
  std::vector<VkFramebuffer> framebuffers_ = {};
  VkShaderModule vertex_shader_module_ = VK_NULL_HANDLE;
  VkShaderModule fragment_shader_module_ = VK_NULL_HANDLE;
  VkPipelineLayout pipeline_layout_ = VK_NULL_HANDLE;
  VkPipeline pipeline_ = VK_NULL_HANDLE;
  VkRenderPass render_pass_ = VK_NULL_HANDLE;
  VkFormat swapchain_image_format_ = VK_FORMAT_UNDEFINED;
  std::vector<VkSemaphore> image_available_semaphores_ = {};
  std::vector<VkSemaphore> render_finished_semaphores_ = {};
  std::vector<VkFence> in_flight_fences_ = {};
  std::vector<VkFence> in_flight_images_ = {};
  size_t current_frame_ = 0;
  size_t frame_number_ = 0;
  std::vector<vulkan::Buffer*> ubos_for_frames_ =
      {};  ///< uniform buffer objects referenced by frame id
  VkDescriptorSetLayout pass_descriptor_set_layout_ = VK_NULL_HANDLE;
  std::vector<VkDescriptorSet> pass_descriptor_sets_ = {};
  VkDescriptorSetLayout render_object_descriptor_set_layout_ = VK_NULL_HANDLE;
  VkDescriptorSet render_object_descriptor_set_ = VK_NULL_HANDLE;
  std::unordered_map<ResourceId, Mesh> meshes_ = {};

  using Texture =
      std::tuple<std::unique_ptr<vulkan::Image>, VkImageView, VkSampler>;
  std::unique_ptr<vulkan::DescriptorPoolCache> descriptor_pool_cache_ = {};
  std::unordered_map<ResourceId, Texture> textures_ = {};
  std::unordered_map<ResourceId, VkDescriptorSet> materials_ = {};

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
  void CreatePassDescriptorSetLayout(
      const UniformBufferDescriptor& uniform_buffer_descriptor);
  void CreateRenderObjectDescriptorSetLayout();
  void CreatePipelineLayout();
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

  template <typename T>
  std::unique_ptr<vulkan::Buffer> CreateBuffer(VkBufferUsageFlags usage,
                                               const std::vector<T>& data);

 public:
  RenderSystem();

  RenderSystem(const RenderSystem&) = delete;
  RenderSystem(RenderSystem&&) = delete;
  const RenderSystem& operator=(const RenderSystem&) = delete;
  RenderSystem& operator=(RenderSystem&&) = delete;

  void Cleanup();
  size_t CreateMesh(const std::string& name,
                    const std::vector<render::Vertex>& vertices,
                    const std::vector<uint32_t>& indices);
  void DrawFrame(const Frame&);
  void Init(const UniformBufferDescriptor& uniform_buffer_descriptor);
  std::tuple<uint32_t, uint32_t> GetWindowDimensions() const;
  void WaitIdle();
  void LoadMaterial(ResourceId id, const std::vector<std::string>& paths);
  std::vector<VkDescriptorSet> AllocateDescriptorSets(
      VkDescriptorSetLayout layout,
      size_t descriptor_set_count,
      const std::vector<VkDescriptorPoolSize>& pool_sizes);
};

template <typename T>
std::unique_ptr<vulkan::Buffer> RenderSystem::CreateBuffer(
    VkBufferUsageFlags usage,
    const std::vector<T>& vertices) {
  const size_t size = sizeof(vertices[0]) * vertices.size();
  auto staging_vertex_buffer = std::make_unique<vulkan::Buffer>(
      physical_device_, device_, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
      static_cast<VkDeviceSize>(size));

  // Fill up buffer memory with data:
  void* data = staging_vertex_buffer->Map<void>();
  memcpy(data, vertices.data(), size);
  staging_vertex_buffer->Unmap();

  auto vertex_buffer = std::make_unique<vulkan::Buffer>(
      physical_device_, device_, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, static_cast<VkDeviceSize>(size));
  CopyBuffer(staging_vertex_buffer->buffer_, vertex_buffer->buffer_,
             static_cast<VkDeviceSize>(size));

  return vertex_buffer;
}
}  // namespace render