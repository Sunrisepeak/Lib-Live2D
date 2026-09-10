#include "image_decode.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include <stb_image.h>
#include <limits>
#include <stdexcept>

namespace huxerui::live2d::detail {

DecodedImage DecodeImage(const Bytes& bytes, const LoadLimits& limits) {
  if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    throw std::runtime_error("Model texture is too large");
  }
  const auto* data = reinterpret_cast<const stbi_uc*>(bytes.data());
  DecodedImage result;
  int channels = 0;
  if (!stbi_info_from_memory(data, static_cast<int>(bytes.size()), &result.width, &result.height, &channels) ||
      result.width <= 0 || result.height <= 0 || result.width > limits.texture_dimension ||
      result.height > limits.texture_dimension ||
      static_cast<std::uint64_t>(result.width) * result.height * 4 > limits.total_bytes) {
    throw std::runtime_error("Invalid or oversized PNG texture");
  }
  std::unique_ptr<stbi_uc, decltype(&stbi_image_free)> decoded(
      stbi_load_from_memory(data, static_cast<int>(bytes.size()), &result.width, &result.height, &channels, 4),
      stbi_image_free);
  if (!decoded) throw std::runtime_error("Cannot decode PNG texture");
  const auto* begin = reinterpret_cast<const std::byte*>(decoded.get());
  result.rgba.assign(begin, begin + static_cast<std::size_t>(result.width) * result.height * 4);
  return result;
}

Task<std::vector<DecodedImage>> DecodeImagesAsync(const AssetData& asset) {
  std::vector<DecodedImage> images;
  std::uint64_t decoded_bytes = 0;
  for (const auto& path : asset.manifest.textures) {
    auto image = co_await RunWorker(DecodeImage, asset.files.at(path), asset.limits);
    decoded_bytes += image.rgba.size();
    if (decoded_bytes > asset.limits.total_bytes) throw std::runtime_error("Decoded texture budget exceeded");
    images.push_back(std::move(image));
  }
  co_return images;
}

} // namespace huxerui::live2d::detail
