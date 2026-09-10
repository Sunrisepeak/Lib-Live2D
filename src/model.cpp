#include "model_internal.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace huxerui::live2d {

bool ModelAsset::HasValue() const noexcept { return static_cast<bool>(data_); }

const ModelInfo& ModelAsset::Info() const {
  if (!data_) throw std::logic_error("Live2D asset has no value");
  return data_->info;
}

ModelController::ModelController() : data_(std::make_shared<detail::ControllerData>()) {}

namespace {

PlaybackError CheckBinding(const std::shared_ptr<detail::Binding>& binding) {
  if (!binding) return {PlaybackErrorCode::NotAttached, "The controller is not attached"};
  if (binding->owner != std::this_thread::get_id()) {
    throw std::logic_error("Live2D commands must run on the mounted UI thread");
  }
  if (binding->failure.code != PlaybackErrorCode::None) return binding->failure;
  if (!binding->ready) return {PlaybackErrorCode::NotReady, "The model has not published its first frame"};
  return {};
}

template<class Command>
PlaybackError Execute(const std::shared_ptr<detail::ControllerData>& control, Command command) {
  auto binding = control ? control->binding.lock() : nullptr;
  auto error = CheckBinding(binding);
  if (error.code != PlaybackErrorCode::None) return error;
  error = command(*binding->player);
  if (error.code == PlaybackErrorCode::None) binding->wake();
  return error;
}

} // namespace

Result<PlaybackId, PlaybackError> ModelController::PlayMotion(
    std::string_view group, int index, MotionOptions options) const {
  if (options.priority <= 0) throw std::invalid_argument("Live2D motion priority must be positive");
  auto binding = data_ ? data_->binding.lock() : nullptr;
  auto error = CheckBinding(binding);
  if (error.code != PlaybackErrorCode::None) return Result<PlaybackId, PlaybackError>::Failure(std::move(error));
  if (data_->next_playback == std::numeric_limits<std::uint64_t>::max()) {
    throw std::overflow_error("Live2D playback identity exhausted");
  }
  const PlaybackId id{data_->next_playback};
  error = binding->player->PlayMotion(id, group, index, options);
  if (error.code != PlaybackErrorCode::None) return Result<PlaybackId, PlaybackError>::Failure(std::move(error));
  ++data_->next_playback;
  binding->wake();
  return Result<PlaybackId, PlaybackError>::Success(id);
}

PlaybackError ModelController::StopMotion() const {
  return Execute(data_, [](detail::Player& player) { return player.StopMotion(); });
}

PlaybackError ModelController::SetExpression(std::string_view name) const {
  return Execute(data_, [&](detail::Player& player) { return player.SetExpression(name); });
}

PlaybackError ModelController::ClearExpression() const {
  return Execute(data_, [](detail::Player& player) { return player.ClearExpression(); });
}

namespace detail {

Result<std::string, LoadError> ResolvePath(std::string_view base, std::string_view reference) {
  using PathResult = Result<std::string, LoadError>;
  auto invalid = [] { return PathResult::Failure({LoadErrorCode::InvalidPath, {}, "Invalid model package path"}); };
  if (reference.empty() || reference.front() == '/' || reference.front() == '\\' ||
      reference.find_first_of(":\\") != std::string_view::npos || reference.find('\0') != std::string_view::npos) {
    return invalid();
  }
  std::string path(base);
  if (!path.empty()) path += '/';
  path += reference;
  std::vector<std::string> segments;
  for (std::size_t start = 0; start <= path.size();) {
    const auto end = path.find('/', start);
    auto segment = path.substr(start, end == std::string::npos ? end : end - start);
    if (segment == "..") {
      if (segments.empty()) return invalid();
      segments.pop_back();
    } else if (!segment.empty() && segment != ".") {
      segments.push_back(std::move(segment));
    }
    if (end == std::string::npos) break;
    start = end + 1;
  }
  if (segments.empty()) return invalid();
  std::string normalized;
  for (const auto& segment : segments) {
    if (!normalized.empty()) normalized += '/';
    normalized += segment;
  }
  return PathResult::Success(std::move(normalized));
}

} // namespace detail

namespace {

Task<Result<ModelAsset, LoadError>> LoadPackageAsync(
    std::string entry_path, OpenResourceAsync open_resource, LoadLimits limits) {
  using LoadResult = Result<ModelAsset, LoadError>;
  if (!open_resource || limits.file_bytes == 0 || limits.total_bytes == 0 || limits.files == 0 ||
      limits.texture_dimension <= 0) throw std::invalid_argument("Invalid Live2D loading configuration");
  auto path = detail::ResolvePath({}, entry_path);
  if (!path.Succeeded()) co_return LoadResult::Failure(path.Error());
  auto asset = std::make_shared<detail::AssetData>();
  asset->entry = path.Value();
  asset->limits = limits;
  std::vector<std::string> pending{asset->entry};
  std::size_t total = 0;
  for (std::size_t index = 0; index < pending.size(); ++index) {
    const std::string current = pending[index];
    if (asset->files.contains(current)) continue;
    if (asset->files.size() >= limits.files) {
      co_return LoadResult::Failure({LoadErrorCode::ResourceLimit, current, "Too many model dependencies"});
    }
    auto opened = co_await open_resource(current);
    if (!opened.Succeeded()) {
      co_return LoadResult::Failure({LoadErrorCode::Io, current, "Cannot open model resource", opened.Error()});
    }
    auto stream = std::move(opened).Value();
    Bytes bytes;
    for (;;) {
      auto read = co_await stream.ReadAsync(64 * 1024);
      if (!read.Succeeded()) {
        co_return LoadResult::Failure({LoadErrorCode::Io, current, "Cannot read model resource", read.Error()});
      }
      const auto& chunk = read.Value();
      if (chunk.empty()) break;
      if (chunk.size() > limits.file_bytes - bytes.size() || chunk.size() > limits.total_bytes - total) {
        co_return LoadResult::Failure({LoadErrorCode::ResourceLimit, current, "Model resource byte limit exceeded"});
      }
      bytes.insert(bytes.end(), chunk.begin(), chunk.end());
      total += chunk.size();
    }
    asset->files.emplace(current, std::move(bytes));
    if (index == 0) {
      auto manifest = detail::ParseManifest(*asset);
      if (!manifest.Succeeded()) co_return LoadResult::Failure(manifest.Error());
      asset->manifest = std::move(manifest).Value();
      pending.insert(pending.end(), asset->manifest.dependencies.begin(), asset->manifest.dependencies.end());
    }
  }
  auto prepared = co_await detail::PrepareAssetAsync(*asset);
  if (!prepared.Succeeded()) co_return LoadResult::Failure(prepared.Error());
  asset->info = std::move(prepared).Value();
  co_return LoadResult::Success(detail::Access::Asset(std::move(asset)));
}

} // namespace

Task<Result<ModelAsset, LoadError>> LoadModelAsync(File entry_file, LoadLimits limits) {
  auto root = entry_file.Parent();
  if (!root || entry_file.Name().empty()) {
    co_return Result<ModelAsset, LoadError>::Failure(
        {LoadErrorCode::InvalidPath, entry_file.Path(), "Model entry must have a parent directory and filename"});
  }
  co_return co_await LoadPackageAsync(entry_file.Name(), [root = *root](std::string path) {
    return root.Resolve(path).OpenReadAsync();
  }, limits);
}

Task<Result<ModelAsset, LoadError>> LoadModelAsync(
    RawResource entry_resource, OpenResourceAsync open_resource, LoadLimits limits) {
  const auto key = entry_resource.Key();
  if (!key.starts_with("raw/") || key.size() == 4) {
    co_return Result<ModelAsset, LoadError>::Failure(
        {LoadErrorCode::InvalidPath, std::string(key), "Model resource key must start with raw/ and name a file"});
  }
  co_return co_await LoadPackageAsync(std::string(key.substr(4)), std::move(open_resource), limits);
}

} // namespace huxerui::live2d
