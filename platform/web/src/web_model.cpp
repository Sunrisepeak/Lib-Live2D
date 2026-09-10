#include "model_internal.h"
#include <huxerui/task.h>
#include <huxerui/web/external_texture.h>
#include <emscripten/val.h>
#include <chrono>
#include <stdexcept>

namespace huxerui::live2d::detail {
namespace {

using emscripten::val;
using namespace std::chrono_literals;

val Bridge() { return val::global("HuxerLive2D"); }
val Buffer(const Bytes& bytes) {
  return val::global("Uint8Array").new_(val(emscripten::typed_memory_view(
      bytes.size(), reinterpret_cast<const unsigned char*>(bytes.data()))))["buffer"];
}
std::string String(const val& object, const char* key) { return object[key].as<std::string>(); }

struct WebAsset {
  val handle;
  explicit WebAsset(val value) : handle(std::move(value)) {}
  ~WebAsset() { Bridge().call<void>("release", handle); }
};

template<class... Args>
val Invoke(const val& model, const char* method, Args... arguments) {
  auto args = val::array();
  (args.call<void>("push", arguments), ...);
  return Bridge().call<val>("invoke", model, std::string(method), args);
}

class WebPlayer final : public Player {
public:
  WebPlayer(std::shared_ptr<const AssetData> asset, val model)
      : asset_(std::move(asset)), model_(std::move(model)),
        texture_(std::make_shared<huxerui::web::VideoFrameTexture>(asset_->info.canvas)) {}
  ~WebPlayer() override { Invoke(model_, "dispose"); texture_->Finish(); }
  std::shared_ptr<ExternalTexture> Texture() const override { return texture_; }
  PlaybackError Prepare() override { return Error(Invoke(model_, "prepare")); }
  PlaybackError Render(Size pixels, double delta, const ModelOptions& options) override {
    auto config = val::object();
    config.set("auto_blink", options.auto_blink);
    config.set("auto_breath", options.auto_breath);
    auto parameters = val::array();
    for (const auto& parameter : options.parameters) {
      auto input = val::object();
      input.set("id", parameter.id); input.set("value", parameter.value);
      input.set("weight", parameter.weight); input.set("blend", static_cast<int>(parameter.blend));
      parameters.call<void>("push", input);
    }
    config.set("parameters", parameters);
    auto result = Invoke(model_, "render", static_cast<int>(pixels.width), static_cast<int>(pixels.height), delta, config);
    const auto error = String(result, "error");
    if (!error.empty()) return {PlaybackErrorCode::BackendFailed, error};
    auto frame = result["value"];
    try { texture_->Publish(frame); }
    catch (...) { frame.call<void>("close"); throw; }
    frame.call<void>("close");
    return {};
  }
  PlaybackError PlayMotion(PlaybackId id, std::string_view group, int index, MotionOptions options) override {
    return Error(Invoke(model_, "play", std::to_string(id.value), std::string(group), index,
                        options.priority, options.force, options.loop));
  }
  PlaybackError StopMotion() override { return Error(Invoke(model_, "stop")); }
  PlaybackError SetExpression(std::string_view name) override { return Error(Invoke(model_, "expression", std::string(name))); }
  PlaybackError ClearExpression() override { return Error(Invoke(model_, "clear")); }
  std::vector<MotionFinished> TakeFinished() override {
    auto result = Invoke(model_, "takeFinished");
    std::vector<MotionFinished> events;
    auto values = result["value"];
    if (!String(result, "error").empty()) return events;
    for (unsigned i = 0; i < values["length"].as<unsigned>(); ++i) {
      events.push_back({{std::stoull(String(values[i], "playback"))},
                        static_cast<MotionEndReason>(values[i]["reason"].as<int>())});
    }
    return events;
  }
  std::string Hit(Point normalized) const override {
    auto result = Invoke(model_, "hit", normalized.x, normalized.y);
    return String(result, "error").empty() ? result["value"].as<std::string>() : "";
  }
  bool IsAnimating() const override {
    auto result = Invoke(model_, "animating");
    return String(result, "error").empty() && result["value"].as<bool>();
  }
private:
  static PlaybackError Error(const val& result) {
    const auto message = String(result, "error");
    if (!message.empty()) return {PlaybackErrorCode::BackendFailed, message};
    const auto code = static_cast<PlaybackErrorCode>(result["value"].as<int>());
    return {code, code == PlaybackErrorCode::None ? "" : "Cubism rejected the playback command"};
  }
  std::shared_ptr<const AssetData> asset_;
  val model_;
  std::shared_ptr<huxerui::web::VideoFrameTexture> texture_;
};

} // namespace

Result<Manifest, LoadError> ParseManifest(AssetData& asset) {
  using Parsed = Result<Manifest, LoadError>;
  if (Bridge().isUndefined()) return Parsed::Failure({LoadErrorCode::UnsupportedBackend, asset.entry, "Cubism Web bridge is missing"});
  auto result = Bridge().call<val>("manifest", Buffer(asset.files.at(asset.entry)), asset.entry);
  auto error = String(result, "error");
  if (!error.empty()) {
    const auto code = result["invalidPath"].as<bool>() ? LoadErrorCode::InvalidPath : LoadErrorCode::InvalidModel;
    return Parsed::Failure({code, asset.entry, error});
  }
  auto json = result["value"];
  Manifest manifest;
  manifest.moc = String(json, "moc");
  auto strings = [](const val& values) {
    std::vector<std::string> result;
    for (unsigned i = 0; i < values["length"].as<unsigned>(); ++i) result.push_back(values[i].as<std::string>());
    return result;
  };
  manifest.textures = strings(json["textures"]);
  manifest.dependencies = strings(json["dependencies"]);
  auto motions = json["motions"];
  for (unsigned i = 0; i < motions["length"].as<unsigned>(); ++i) {
    manifest.motions.push_back({String(motions[i], "group"), motions[i]["index"].as<int>(), String(motions[i], "path")});
  }
  auto expressions = json["expressions"];
  for (unsigned i = 0; i < expressions["length"].as<unsigned>(); ++i) {
    manifest.expressions.emplace(String(expressions[i], "name"), String(expressions[i], "path"));
  }
  return Parsed::Success(std::move(manifest));
}

Task<Result<ModelInfo, LoadError>> PrepareAssetAsync(AssetData& asset) {
  using Prepared = Result<ModelInfo, LoadError>;
  auto files = val::object();
  for (const auto& [path, bytes] : asset.files) files.set(path, Buffer(bytes));
  auto prepared = std::make_shared<WebAsset>(Bridge().call<val>("prepare", files, asset.entry,
      asset.limits.texture_dimension, static_cast<double>(asset.limits.total_bytes)));
  while (!prepared->handle["ready"].as<bool>()) co_await Delay(5ms);
  const auto error = String(prepared->handle, "error");
  if (!error.empty()) co_return Prepared::Failure({LoadErrorCode::InvalidModel, asset.entry, error});
  auto json = prepared->handle["info"];
  ModelInfo info;
  info.canvas = {json["width"].as<float>(), json["height"].as<float>()};
  auto parameters = json["parameters"];
  for (unsigned i = 0; i < parameters["length"].as<unsigned>(); ++i) {
    auto parameter = parameters[i];
    info.parameters.push_back({String(parameter, "id"), parameter["minimum"].as<float>(),
        parameter["maximum"].as<float>(), parameter["default_value"].as<float>()});
  }
  auto hit_areas = json["hit_areas"];
  for (unsigned i = 0; i < hit_areas["length"].as<unsigned>(); ++i) info.hit_areas.push_back(hit_areas[i].as<std::string>());
  std::map<std::string, int> groups;
  for (const auto& motion : asset.manifest.motions) ++groups[motion.group];
  for (const auto& [name, count] : groups) info.motion_groups.push_back({name, count});
  for (const auto& [name, path] : asset.manifest.expressions) info.expressions.push_back(name);
  asset.runtime = std::move(prepared);
  co_return Prepared::Success(std::move(info));
}

Task<Result<std::shared_ptr<Player>, PlaybackError>> CreatePlayerAsync(std::shared_ptr<const AssetData> asset) {
  using Created = Result<std::shared_ptr<Player>, PlaybackError>;
  auto data = std::static_pointer_cast<WebAsset>(asset->runtime);
  auto result = Bridge().call<val>("create", data->handle);
  const auto error = String(result, "error");
  if (!error.empty()) co_return Created::Failure({PlaybackErrorCode::BackendFailed, error});
  co_return Created::Success(std::make_shared<WebPlayer>(std::move(asset), result["value"]));
}

} // namespace huxerui::live2d::detail
