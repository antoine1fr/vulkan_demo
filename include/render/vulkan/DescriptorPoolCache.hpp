#pragma once

#include <vulkan/vulkan.h>
#include <list>
#include <vector>

namespace render {
namespace vulkan {
class DescriptorPoolCache {
 public:
  DescriptorPoolCache(VkDevice device);
  ~DescriptorPoolCache();
  VkDescriptorPool GetPool(size_t descriptor_count,
                           const std::vector<VkDescriptorPoolSize>& sizes);

 private:
  VkDevice device_;
  std::list<VkDescriptorPool> pools_;
};
}  // namespace vulkan
}  // namespace render