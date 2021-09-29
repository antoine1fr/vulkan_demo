#pragma once

#include <vulkan/vulkan.h>

#include "base.hpp"

namespace render {
class RenderSystem;

namespace vulkan {
class Buffer {
 private:
  VkBuffer buffer_ = VK_NULL_HANDLE;
  VkDeviceMemory memory_ = VK_NULL_HANDLE;
  VkDeviceSize size_;
  VkDevice device_;

 public:
  Buffer(VkPhysicalDevice physical_device,
         VkDevice device,
         VkBufferUsageFlags usage,
         VkMemoryPropertyFlags properties,
         VkDeviceSize size);
  ~Buffer();

  template <typename T>
  T* Map();
  void Unmap();

  friend class ::render::RenderSystem;
};

template <typename T>
T* Buffer::Map() {
  void* dst_ptr;
  vkMapMemory(device_, memory_, 0, size_, 0, &dst_ptr);
  return reinterpret_cast<T*>(dst_ptr);
}
}  // namespace vulkan
}  // namespace render