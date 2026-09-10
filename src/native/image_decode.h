#pragma once

#include "model_internal.h"

namespace huxerui::live2d::detail {

DecodedImage DecodeImage(const Bytes& bytes, const LoadLimits& limits);
Task<std::vector<DecodedImage>> DecodeImagesAsync(const AssetData& asset);

} // namespace huxerui::live2d::detail
