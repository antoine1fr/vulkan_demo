#include <cassert>

#include "base.hpp"
#include "render/vulkan/Buffer.hpp"
#include "render/vulkan/Memory.hpp"

namespace render {
namespace vulkan {
Buffer::Buffer(VkPhysicalDevice physical_device,
               VkDevice device,
               VkBufferUsageFlags usage,
               VkMemoryPropertyFlags properties,
               VkDeviceSize size)
    : size_(size), device_(device) {
  // Create vertex buffer:

  VkBufferCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  info.size = size;
  info.usage = usage;
  info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  VK_CHECK(vkCreateBuffer(device, &info, nullptr, &buffer_));

  // Allocate memory for buffer:

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(device, buffer_, &memory_requirements);

  AllocateVulkanMemory(memory_requirements, physical_device, device, &memory_,
                       properties);

  // Bind memory to buffer:
  VK_CHECK(vkBindBufferMemory(device, buffer_, memory_, 0));
}

Buffer::~Buffer() {
  vkFreeMemory(device_, memory_, nullptr);
  vkDestroyBuffer(device_, buffer_, nullptr);
}

void Buffer::Unmap() {
  vkUnmapMemory(device_, memory_);
}
}  // namespace vulkan
}  // namespace render
