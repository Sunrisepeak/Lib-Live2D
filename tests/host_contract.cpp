#include <huxerui/huxerui.h>
#include <huxerui/macos/external_texture.h>
#include <huxerui/testing/ui_test.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace live2d_contract {

using namespace huxerui;
using namespace huxerui::testing;
using namespace std::chrono_literals;

void Require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

struct Metrics {
  int mounts = 0;
  int unmounts = 0;
  int compositions = 0;
  int updates = 0;
  int ready_events = 0;
  huxerui::Rect geometry{};
  std::weak_ptr<ExternalTexture> texture;
};

struct Connection {
  std::function<void()> wake;
};

struct Control {
  std::weak_ptr<Connection> connection;

  bool Wake() const {
    if (auto mounted = connection.lock()) {
      mounted->wake();
      return true;
    }
    return false;
  }
};

struct Ready : Event<void()> {};

struct Host {
  class Extension;
  std::shared_ptr<Metrics> metrics;
  std::shared_ptr<Control> control;
  State<std::shared_ptr<ExternalTexture>> texture;
  State<std::uint64_t> notification;
  TaskScope tasks;
  EventEmitter events;
  std::uint64_t revision;
};

class Host::Extension final : public NodeExtension {
public:
  Extension(ViewNode& node, const Host& host)
      : metrics_(host.metrics), connection_(std::make_shared<Connection>()), tasks_(host.tasks), output_(host.texture) {
    Require(host.control->connection.expired(), "Control is already attached");
    connection_->wake = [notification = host.notification] { notification += 1; };
    host.control->connection = connection_;
    ++metrics_->mounts;
    texture_ = std::make_shared<macos::MetalTexture>(huxerui::Size{200.0F, 300.0F});
    metrics_->texture = texture_;
    Update(node, host);
  }

  ~Extension() override {
    connection_.reset();
    texture_->Finish();
    ++metrics_->unmounts;
  }

  void Update(ViewNode&, const Host& host) {
    events_ = host.events;
    if (revision_ != host.revision) {
      revision_ = host.revision;
      dirty_ = true;
    }
  }

  FrameResult OnFrame(ViewNode&, const FrameInfo&) override {
    if (!connection_posted_) {
      connection_posted_ = true;
      std::weak_ptr<Connection> alive = connection_;
      tasks_.Post([alive, output = output_, texture = texture_] {
        if (!alive.expired()) {
          output = texture;
        }
      });
    }
    if (dirty_ && metrics_->geometry.width > 0.0F && metrics_->geometry.height > 0.0F) {
      dirty_ = false;
      ++metrics_->updates;
      if (!ready_) {
        ready_ = true;
        events_.Emit<Ready>();
      }
    }
    return {};
  }

  PaintInvalidation PrepareGeometry(ViewNode& node, TextMeasurer&) override {
    const huxerui::Rect geometry = node.ContentBounds();
    if (geometry != metrics_->geometry) {
      metrics_->geometry = geometry;
      dirty_ = true;
      std::weak_ptr<Connection> alive = connection_;
      tasks_.Post([alive] {
        if (auto connection = alive.lock()) {
          connection->wake();
        }
      });
    }
    return PaintInvalidation::None;
  }

private:
  std::shared_ptr<Metrics> metrics_;
  std::shared_ptr<Connection> connection_;
  std::shared_ptr<macos::MetalTexture> texture_;
  TaskScope tasks_;
  State<std::shared_ptr<ExternalTexture>> output_;
  EventEmitter events_;
  std::uint64_t revision_ = 0;
  bool connection_posted_ = false;
  bool dirty_ = true;
  bool ready_ = false;
};

[[huxerui::composable]]
View Probe(std::shared_ptr<Metrics> metrics, std::shared_ptr<Control> control) {
  auto texture = UseState(std::shared_ptr<ExternalTexture>{});
  auto notification = UseState(std::uint64_t{0});
  auto tasks = UseTaskScope();
  auto events = UseEvents();
  ++metrics->compositions;
  const std::shared_ptr<ExternalTexture> current = texture;
  View content = current
                     ? View(Image(current).Fit(ImageFit::Contain))
                     : View(Spacer());
  return Stack {
    content,
  }.With(
      Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
      Host{metrics, control, texture, notification, tasks, events, notification}
  );
}

struct FixtureState {
  std::shared_ptr<Metrics> metrics = std::make_shared<Metrics>();
  std::shared_ptr<Control> control = std::make_shared<Control>();
  State<bool> mounted;
  State<float> width;
  int parent_compositions = 0;
};

std::shared_ptr<FixtureState> active_fixture;

View App() {
  const auto fixture = active_fixture;
  fixture->mounted = UseState(true);
  fixture->width = UseState(200.0F);
  ++fixture->parent_compositions;
  if (!fixture->mounted) {
    return Text("Unmounted");
  }
  return Column {
    Probe(fixture->metrics, fixture->control)
        .On<Ready>([metrics = fixture->metrics] { ++metrics->ready_events; })
        .With(Frame{.width = fixture->width, .height = 300.0F})
        .Key("model-host"),
  };
}

void CheckMountedConnection() {
  auto fixture = std::make_shared<FixtureState>();
  active_fixture = fixture;
  const Application application{App};
  Require(!fixture->control->Wake(), "An unattached control must reject commands");
  {
    UiTest ui(application);
    for (int frame = 0; frame < 4; ++frame) {
      ui.Pump(16ms);
    }
    Require(fixture->metrics->mounts == 1, "Connecting Image remounted the host");
    Require(ui.Find(UiSelector::Type<Image>()).Count() == 1, "Texture was not connected to Image");
    Require(fixture->metrics->ready_events == 1, "The caller's event did not reach the retained host");
    const auto original_texture = fixture->metrics->texture.lock();
    Require(original_texture != nullptr, "Mounted texture expired");

    const int updates = fixture->metrics->updates;
    const int compositions = fixture->metrics->compositions;
    const int parent_compositions = fixture->parent_compositions;
    for (int frame = 0; frame < 5; ++frame) {
      ui.Pump(16ms);
    }
    Require(fixture->metrics->updates == updates, "Idle frames performed model work");
    Require(fixture->metrics->compositions == compositions, "Idle frames recomposed the component");

    Require(fixture->control->Wake(), "A mounted control rejected the command");
    ui.Pump(16ms);
    Require(fixture->metrics->updates == updates + 1, "A command did not reach the retained host");
    Require(fixture->metrics->compositions == compositions + 1, "A command did not recompose its local scope");
    Require(fixture->parent_compositions == parent_compositions, "A command recomposed the caller");
    Require(fixture->metrics->texture.lock() == original_texture, "A command replaced the texture identity");

    fixture->width = 260.0F;
    ui.Pump(16ms);
    ui.Pump(16ms);
    if (fixture->metrics->geometry.width != 260.0F) {
      std::cerr << ui.CaptureSnapshot().ToString() << '\n';
    }
    Require(fixture->metrics->geometry.width == 260.0F, "The host did not observe resized geometry");
    Require(fixture->metrics->updates > updates + 1, "Idle resize did not produce model work");
    Require(fixture->metrics->mounts == 1, "Resize remounted the host");

    fixture->mounted = false;
    ui.Pump();
    Require(fixture->metrics->unmounts == 1, "Unmount did not release the retained host");
    Require(!fixture->control->Wake(), "An unmounted control accepted a command");
    Require(ui.Find(UiSelector::Type<Image>()).Count() == 0, "Unmount retained the Image");
  }
  Require(fixture->metrics->texture.expired(), "Texture survived fixture teardown");
}

void CheckUnmountBeforeConnection() {
  auto fixture = std::make_shared<FixtureState>();
  active_fixture = fixture;
  const Application application{App};
  {
    UiTest ui(application);
  }
  Require(fixture->metrics->mounts == 1, "The initial host was not mounted");
  Require(fixture->metrics->unmounts == 1, "Early teardown did not release the host");
  Require(!fixture->control->Wake(), "Early teardown left the control attached");
  Require(fixture->metrics->texture.expired(), "A late connection retained the texture after teardown");
  Require(fixture->metrics->ready_events == 0, "Early teardown delivered readiness");
}

} // namespace live2d_contract

int main() {
  try {
    live2d_contract::CheckMountedConnection();
    live2d_contract::CheckUnmountBeforeConnection();
    std::cout << "Live2D host contracts passed (no Cubism or pixel rendering)\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
