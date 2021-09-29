#include <vulkan/vulkan.hpp>

#include "base.hpp"
#include "render/vulkan/Memory.hpp"

namespace render {
namespace vulkan {
namespace {
uint32_t FindMemoryType(VkPhysicalDevice physical_device,
                        uint32_t type_filter,
                        VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

  for (uint32_t i = 0; i < memory_properties.memoryTypeCount; ++i) {
    if (type_filter & (1 << i) &&
        (memory_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }
  assert(false);
  return 0;
}
}  // namespace

void AllocateVulkanMemory(const VkMemoryRequirements& memory_requirements,
                          const VkPhysicalDevice& physical_device,
                          const VkDevice& device,
                          VkDeviceMemory* memory) {
  VkMemoryAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  alloc_info.allocationSize = memory_requirements.size;
  alloc_info.memoryTypeIndex =
      FindMemoryType(physical_device, memory_requirements.memoryTypeBits,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  VK_CHECK(vkAllocateMemory(device, &alloc_info, nullptr, memory));
}
}  // namespace vulkan
}  // namespace render