#pragma once

#include <vulkan/vulkan.h>

#include "render/vulkan/Buffer.hpp"

namespace render {
class RenderSystem;

namespace vulkan {
class Image {
 public:
  Image(VkPhysicalDevice physical_device,
        VkDevice device,
        size_t width,
        size_t height);
  ~Image();

  friend class RenderSystem;

 private:
  void AllocateMemory();

 private:
  VkPhysicalDevice physical_device_;
  VkDevice device_;
  VkImage image_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
};
}  // namespace vulkan
}  // namespace render