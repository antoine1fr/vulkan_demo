#include "render/vulkan/Image.hpp"
#include "render/vulkan/Memory.hpp"

namespace render {
namespace vulkan {
Image::Image(VkPhysicalDevice physical_device,
             VkDevice device,
             size_t width,
             size_t height)
    : physical_device_(physical_device), device_(device) {
  VkImageCreateInfo image_info{};
  image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  image_info.imageType = VK_IMAGE_TYPE_2D;
  image_info.extent.width = static_cast<uint32_t>(width);
  image_info.extent.height = static_cast<uint32_t>(height);
  image_info.extent.depth = 1;
  image_info.mipLevels = 1;
  image_info.arrayLayers = 1;
  image_info.format = VK_FORMAT_R8G8B8A8_SRGB;
  image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  image_info.usage =
      VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  image_info.samples = VK_SAMPLE_COUNT_1_BIT;

  VK_CHECK(vkCreateImage(device_, &image_info, nullptr, &image_));

  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(device, image_, &memory_requirements);
  AllocateVulkanMemory(memory_requirements, physical_device_, device_, &memory_,
                       VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  VK_CHECK(vkBindImageMemory(device, image_, memory_, 0));
}

Image::~Image() {
  vkFreeMemory(device_, memory_, nullptr);
  vkDestroyImage(device_, image_, nullptr);
}
}  // namespace vulkan
}  // namespace render