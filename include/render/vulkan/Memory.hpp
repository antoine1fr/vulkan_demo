#pragma once

namespace render {
namespace vulkan {
void AllocateVulkanMemory(const VkMemoryRequirements& memory_requirements,
                          const VkPhysicalDevice& physical_device,
                          const VkDevice& device,
                          VkDeviceMemory* memory,
                          VkMemoryPropertyFlags properties);
}  // namespace vulkan
}  // namespace render