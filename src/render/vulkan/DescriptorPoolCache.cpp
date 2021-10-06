#include "base.hpp"

#include "render/vulkan/DescriptorPoolCache.hpp"

namespace render {
namespace vulkan {
DescriptorPoolCache::DescriptorPoolCache(VkDevice device) : device_(device) {}

VkDescriptorPool DescriptorPoolCache::GetPool(
    size_t descriptor_count,
    const std::vector<VkDescriptorPoolSize>& sizes) {
  VkDescriptorPool descriptor_pool;
  VkDescriptorPoolCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  info.maxSets = descriptor_count;
  info.poolSizeCount = static_cast<uint32_t>(sizes.size());
  info.pPoolSizes = sizes.data();

  VK_CHECK(vkCreateDescriptorPool(device_, &info, nullptr, &descriptor_pool));
  return descriptor_pool;
}
}  // namespace vulkan
}  // namespace render