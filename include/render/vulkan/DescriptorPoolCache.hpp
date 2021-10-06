#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace render {
namespace vulkan {
class DescriptorPoolCache {
 public:
  DescriptorPoolCache(VkDevice device);
  VkDescriptorPool GetPool(size_t descriptor_count,
                           const std::vector<VkDescriptorPoolSize>& sizes);

 private:
  VkDevice device_;
};
}  // namespace vulkan
}  // namespace render