#include "cpu_image.h"

#include <cstring>
#include <utility>

#include "../../stb_image.h"
#include "../utils.h"

CpuImage::CpuImage(
    std::vector<uint8_t> image_data,
    VkExtent3D extent,
    VkFormat format)
    :   m_image_data(std::move(image_data)),
        m_extent(extent),
        m_format(format) {}

VkExtent3D CpuImage::extent() const noexcept {
    return m_extent;
}

VkExtent2D CpuImage::extent2d() const noexcept {
    return {
        m_extent.width,
        m_extent.height
    };
}

VkFormat CpuImage::format() const noexcept {
    return m_format;
}

VkDeviceSize CpuImage::size_bytes() const {
    return Utils::image_size_bytes(m_extent, m_format);
}

std::vector<uint8_t>& CpuImage::image_data() {
    return m_image_data;
}

const std::vector<uint8_t>& CpuImage::image_data() const {
    return m_image_data;
}

CpuImage CpuImage::load_rgba8_image(const std::filesystem::path& path) {
    LOG_NAMED("CpuImage");

    std::vector<std::uint8_t> file_data = Utils::read_binary_file(path);

    int width, height, channels;
    stbi_uc* raw_pixels = stbi_load_from_memory(
        file_data.data(),
        static_cast<int>(file_data.size()),
        &width,
        &height,
        &channels,
        STBI_rgb_alpha
    );

    logger.check(width > 0, "Loaded image width is invalid");
    logger.check(height > 0, "Loaded image height is invalid");

    const char* failure_reason = stbi_failure_reason();
    logger.check(raw_pixels != nullptr)
        << "Failed to load image: "
        << clr(path.string(), LoggerPalette::blue)
        << "\nReason: "
        << clr(failure_reason != nullptr ? failure_reason : "unknown reason", LoggerPalette::red)
        << "\n";

    VkDeviceSize image_size =
        static_cast<VkDeviceSize>(width) *
        static_cast<VkDeviceSize>(height) *
        4;
    std::vector<uint8_t> pixels(static_cast<size_t>(image_size));
    std::memcpy(pixels.data(), raw_pixels, static_cast<size_t>(image_size));
    
    stbi_image_free(raw_pixels);

    return CpuImage(
        pixels, 
        VkExtent3D{
            static_cast<uint32_t>(width), 
            static_cast<uint32_t>(height), 
            1
        }, 
        VK_FORMAT_R8G8B8A8_SRGB
    );
}
