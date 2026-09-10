#pragma once

#include <huxerui/live2d.h>
#include <huxerui/external_texture.h>

#include <map>
#include <thread>

namespace huxerui::live2d::detail {

struct MotionFile {
  std::string group;
  int index = 0;
  std::string path;
};

struct Manifest {
  std::string moc;
  std::vector<std::string> textures;
  std::vector<MotionFile> motions;
  std::map<std::string, std::string> expressions;
  std::vector<std::string> dependencies;
};

struct DecodedImage {
  int width = 0;
  int height = 0;
  Bytes rgba;
};

struct AssetData {
  std::shared_ptr<void> runtime;
  std::vector<DecodedImage> images;
  std::string entry;
  Manifest manifest;
  std::map<std::string, Bytes> files;
  ModelInfo info;
  LoadLimits limits;
};

class Player {
public:
  virtual ~Player() = default;
  virtual std::shared_ptr<ExternalTexture> Texture() const = 0;
  virtual PlaybackError Prepare() { return {}; }
  virtual PlaybackError Render(Size pixels, double delta, const ModelOptions& options) = 0;
  virtual PlaybackError PlayMotion(PlaybackId id, std::string_view group, int index, MotionOptions options) = 0;
  virtual PlaybackError StopMotion() = 0;
  virtual PlaybackError SetExpression(std::string_view name) = 0;
  virtual PlaybackError ClearExpression() = 0;
  virtual std::vector<MotionFinished> TakeFinished() = 0;
  virtual std::string Hit(Point normalized) const = 0;
  virtual bool IsAnimating() const = 0;
};

struct Binding {
  std::thread::id owner = std::this_thread::get_id();
  std::shared_ptr<Player> player;
  std::function<void()> wake;
  bool ready = false;
  PlaybackError failure;
};

struct ControllerData {
  std::weak_ptr<Binding> binding;
  std::uint64_t next_playback = 1;
};

struct Access {
  static ModelAsset Asset(std::shared_ptr<AssetData> data) {
    ModelAsset result;
    result.data_ = std::move(data);
    return result;
  }
  static const std::shared_ptr<const AssetData>& Data(const ModelAsset& asset) { return asset.data_; }
  static const std::shared_ptr<ControllerData>& Control(const ModelController& controller) { return controller.data_; }
};

Result<std::string, LoadError> ResolvePath(std::string_view base, std::string_view reference);
Result<Manifest, LoadError> ParseManifest(AssetData& asset);
Task<Result<ModelInfo, LoadError>> PrepareAssetAsync(AssetData& asset);
Task<Result<std::shared_ptr<Player>, PlaybackError>> CreatePlayerAsync(std::shared_ptr<const AssetData> asset);

} // namespace huxerui::live2d::detail
