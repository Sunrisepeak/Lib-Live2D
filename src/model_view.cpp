#include "model_internal.h"

#include <huxerui/huxerui.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <set>

namespace huxerui::live2d {
namespace {

struct Output {
  ModelAsset asset;
  std::shared_ptr<ExternalTexture> texture;
  bool operator==(const Output&) const = default;
};

struct Host {
  class Extension;
  ModelAsset asset;
  ModelOptions options;
  State<Output> output;
  State<std::uint64_t> notification;
  TaskScope tasks;
  EventEmitter events;
  std::uint64_t revision;
};

class Host::Extension final : public NodeExtension {
public:
  Extension(ViewNode& node, const Host& host) : output_(host.output), tasks_(host.tasks) { Update(node, host); }

  ~Extension() override { preparation_.Cancel(); }

  void Update(ViewNode&, const Host& host) {
    if (!binding_ || asset_ != host.asset || options_.controller != host.options.controller) {
      auto control = detail::Access::Control(*host.options.controller);
      if (auto existing = control->binding.lock(); existing && existing != binding_) {
        throw std::logic_error("A Live2D controller can attach to only one ModelView");
      }
      preparation_.Cancel();
      preparing_ = false;
      binding_.reset();
      asset_ = host.asset;
      binding_ = std::make_shared<detail::Binding>();
      binding_->wake = [notification = host.notification] { notification += 1; };
      control->binding = binding_;
      connected_ = false;
      failure_emitted_ = false;
      advancing_ = false;
    }
    options_ = host.options;
    events_ = host.events;
    dirty_ = true;
  }

  FrameResult OnFrame(ViewNode&, const FrameInfo& frame) override {
    if (binding_->failure.code != PlaybackErrorCode::None) {
      if (!failure_emitted_) {
        failure_emitted_ = true;
        events_.Emit<ModelEvents::PlaybackFailed>(binding_->failure);
      }
      return {};
    }
    if (bounds_.width <= 0 || bounds_.height <= 0) { advancing_ = false; return {}; }
    if (!binding_->player) {
      if (!preparing_) {
        preparing_ = true;
        preparation_ = tasks_.Launch(PreparePlayer(detail::Access::Data(asset_), binding_));
      }
      return {.needs_frame = true};
    }
    auto player = binding_->player;
    auto prepared = player->Prepare();
    if (prepared.code == PlaybackErrorCode::NotReady) return {.needs_frame = true};
    if (prepared.code != PlaybackErrorCode::None) {
      binding_->failure = std::move(prepared);
      return {.needs_frame = true};
    }
    const bool animate = !options_.paused && options_.playback_rate > 0 && !frame.reduced_motion &&
                         (options_.auto_blink || options_.auto_breath || player->IsAnimating());
    if (dirty_ || animate) {
      auto destination = Destination();
      const auto canvas = asset_.Info().canvas;
      const float scale = std::min(2.0F * std::max(destination.width / canvas.width,
                                                destination.height / canvas.height),
                                   4096.0F / std::max(canvas.width, canvas.height));
      const Size pixels{std::max(1.0F, std::ceil(canvas.width * scale)),
                        std::max(1.0F, std::ceil(canvas.height * scale))};
      const double elapsed = animate && advancing_
                                 ? std::clamp(frame.delta_time, 0.0, 0.1) * options_.playback_rate : 0;
      auto error = player->Render(pixels, elapsed, options_);
      if (error.code != PlaybackErrorCode::None) {
        binding_->failure = std::move(error);
        return {.needs_frame = true};
      }
      dirty_ = false;
      if (!connected_) {
        connected_ = true;
        std::weak_ptr<detail::Binding> alive = binding_;
        tasks_.Post([alive, output = output_, asset = asset_, texture = player->Texture()] {
          if (!alive.expired()) output = Output{asset, texture};
        });
      }
      if (!binding_->ready) {
        binding_->ready = true;
        events_.Emit<ModelEvents::Ready>();
      }
    }
    advancing_ = animate;
    for (auto event : player->TakeFinished()) events_.Emit<ModelEvents::MotionFinished>(event);
    return {.needs_frame = animate};
  }

  PaintInvalidation PrepareGeometry(ViewNode& node, TextMeasurer&) override {
    if (bounds_ != node.ContentBounds()) {
      bounds_ = node.ContentBounds();
      dirty_ = true;
      std::weak_ptr<detail::Binding> alive = binding_;
      tasks_.Post([alive] { if (auto binding = alive.lock()) binding->wake(); });
    }
    return PaintInvalidation::None;
  }

  bool HitTest(ViewNode& node, Point position) const override {
    return node.IsEnabled() && binding_->ready && Contains(bounds_, position);
  }

  PointerResult OnPointer(ViewNode&, const PointerEvent& event) override {
    if (event.type == PointerEventType::Down && event.changed_button == PointerButton::Primary && !pointer_) {
      pointer_ = event.pointer_id;
      down_ = event.position;
      return PointerResult::Observe;
    }
    if (pointer_ != event.pointer_id) return PointerResult::Ignored;
    if (event.type == PointerEventType::Cancel ||
        std::hypot(event.position.x - down_.x, event.position.y - down_.y) > 8) {
      pointer_.reset();
      return PointerResult::Ignored;
    }
    if (event.type == PointerEventType::Up) {
      pointer_.reset();
      const auto destination = Destination();
      if (binding_->ready && binding_->failure.code == PlaybackErrorCode::None &&
          Contains(bounds_, event.position) && Contains(destination, event.position)) {
        const auto name = binding_->player->Hit({(event.position.x - destination.x) / destination.width,
                                               (event.position.y - destination.y) / destination.height});
        if (!name.empty()) events_.Emit<ModelEvents::HitAreaTapped>(name);
      }
    }
    return PointerResult::Observe;
  }

private:
  static Task<void> PreparePlayer(std::shared_ptr<const detail::AssetData> asset,
                                  std::weak_ptr<detail::Binding> alive) {
    auto created = co_await detail::CreatePlayerAsync(std::move(asset));
    if (auto binding = alive.lock()) {
      if (created.Succeeded()) binding->player = std::move(created).Value();
      else binding->failure = created.Error();
      binding->wake();
    }
  }

  TaskHandle preparation_;
  bool preparing_ = false;

  static bool Contains(Rect rect, Point point) {
    return point.x >= rect.x && point.y >= rect.y && point.x < rect.x + rect.width && point.y < rect.y + rect.height;
  }
  Rect Destination() const {
    if (options_.fit == ImageFit::Fill) return bounds_;
    const auto canvas = asset_.Info().canvas;
    float scale = options_.fit == ImageFit::Cover
                      ? std::max(bounds_.width / canvas.width, bounds_.height / canvas.height)
                      : std::min(bounds_.width / canvas.width, bounds_.height / canvas.height);
    if (options_.fit == ImageFit::None) scale = 1;
    if (options_.fit == ImageFit::ScaleDown) scale = std::min(scale, 1.0F);
    const float width = canvas.width * scale, height = canvas.height * scale;
    return {bounds_.x + (bounds_.width - width) / 2, bounds_.y + (bounds_.height - height) / 2, width, height};
  }

  ModelAsset asset_;
  ModelOptions options_;
  std::shared_ptr<detail::Binding> binding_;
  State<Output> output_;
  TaskScope tasks_;
  EventEmitter events_;
  Rect bounds_{};
  std::optional<std::int64_t> pointer_;
  Point down_{};
  bool dirty_ = true;
  bool connected_ = false;
  bool failure_emitted_ = false;
  bool advancing_ = false;
};

} // namespace

[[huxerui::composable]]
View ModelView(ModelAsset asset, ModelOptions configuration) {
  auto options = configuration;
  const ModelController default_controller = UseState(ModelController{});
  if (!options.controller) options.controller = default_controller;
  if (!asset.HasValue()) throw std::invalid_argument("ModelView requires a loaded asset");
  if (!std::isfinite(options.playback_rate) || options.playback_rate < 0 || options.playback_rate > 8) {
    throw std::invalid_argument("Live2D playback rate must be between zero and eight");
  }
  std::set<std::string> parameter_ids;
  for (const auto& parameter : options.parameters) {
    if (!std::isfinite(parameter.value) || !std::isfinite(parameter.weight) ||
        parameter.weight < 0 || parameter.weight > 1 || !parameter_ids.insert(parameter.id).second ||
        std::none_of(asset.Info().parameters.begin(), asset.Info().parameters.end(),
                     [&](const auto& info) { return info.id == parameter.id; })) {
      throw std::invalid_argument("Invalid Live2D parameter input");
    }
  }
  auto output = UseState(Output{});
  auto notification = UseState(std::uint64_t{0});
  auto tasks = UseTaskScope();
  auto events = UseEvents();
  const Output current = output;
  View content = current.asset == asset && current.texture
                     ? View(Image(current.texture).Fit(options.fit))
                     : View(Spacer().With(Frame{.width = asset.Info().canvas.width, .height = asset.Info().canvas.height}));
  return Stack {
    content,
  }.With(
      Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
      Host{asset, options, output, notification, tasks, events, notification}
  );
}

} // namespace huxerui::live2d
