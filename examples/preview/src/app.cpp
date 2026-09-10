#include <huxerui/huxerui.h>
#include <huxerui/live2d.h>
#include "app_resources.h"

#include <algorithm>
#include <array>
#include <map>
#include <variant>

using namespace huxerui;
using namespace huxerui::live2d;

namespace {

struct Character {
  std::string_view name;
  StringResource description;
  RawResource entry;
};

const std::array characters{
    Character{"Mao", app::strings::character_mao, app::raw::Mao_Mao_model3_json},
    Character{"Haru", app::strings::character_haru, app::raw::Haru_Haru_model3_json},
    Character{"Hiyori", app::strings::character_hiyori, app::raw::Hiyori_Hiyori_model3_json},
};

const std::array model_resources{
    app::raw::Haru_Haru_2048_texture_00_png,
    app::raw::Haru_Haru_2048_texture_01_png,
    app::raw::Haru_Haru_cdi3_json,
    app::raw::Haru_Haru_moc3,
    app::raw::Haru_Haru_model3_json,
    app::raw::Haru_Haru_physics3_json,
    app::raw::Haru_Haru_pose3_json,
    app::raw::Haru_Haru_userdata3_json,
    app::raw::Haru_expressions_F01_exp3_json,
    app::raw::Haru_expressions_F02_exp3_json,
    app::raw::Haru_expressions_F03_exp3_json,
    app::raw::Haru_expressions_F04_exp3_json,
    app::raw::Haru_expressions_F05_exp3_json,
    app::raw::Haru_expressions_F06_exp3_json,
    app::raw::Haru_expressions_F07_exp3_json,
    app::raw::Haru_expressions_F08_exp3_json,
    app::raw::Haru_motions_haru_g_idle_motion3_json,
    app::raw::Haru_motions_haru_g_m01_motion3_json,
    app::raw::Haru_motions_haru_g_m02_motion3_json,
    app::raw::Haru_motions_haru_g_m03_motion3_json,
    app::raw::Haru_motions_haru_g_m04_motion3_json,
    app::raw::Haru_motions_haru_g_m05_motion3_json,
    app::raw::Haru_motions_haru_g_m06_motion3_json,
    app::raw::Haru_motions_haru_g_m07_motion3_json,
    app::raw::Haru_motions_haru_g_m08_motion3_json,
    app::raw::Haru_motions_haru_g_m09_motion3_json,
    app::raw::Haru_motions_haru_g_m10_motion3_json,
    app::raw::Haru_motions_haru_g_m11_motion3_json,
    app::raw::Haru_motions_haru_g_m12_motion3_json,
    app::raw::Haru_motions_haru_g_m13_motion3_json,
    app::raw::Haru_motions_haru_g_m14_motion3_json,
    app::raw::Haru_motions_haru_g_m15_motion3_json,
    app::raw::Haru_motions_haru_g_m16_motion3_json,
    app::raw::Haru_motions_haru_g_m17_motion3_json,
    app::raw::Haru_motions_haru_g_m18_motion3_json,
    app::raw::Haru_motions_haru_g_m19_motion3_json,
    app::raw::Haru_motions_haru_g_m20_motion3_json,
    app::raw::Haru_motions_haru_g_m21_motion3_json,
    app::raw::Haru_motions_haru_g_m22_motion3_json,
    app::raw::Haru_motions_haru_g_m23_motion3_json,
    app::raw::Haru_motions_haru_g_m24_motion3_json,
    app::raw::Haru_motions_haru_g_m25_motion3_json,
    app::raw::Haru_motions_haru_g_m26_motion3_json,
    app::raw::Haru_sounds_haru_Info_04_wav,
    app::raw::Haru_sounds_haru_Info_14_wav,
    app::raw::Haru_sounds_haru_normal_6_wav,
    app::raw::Haru_sounds_haru_talk_13_wav,
    app::raw::Hiyori_Hiyori_2048_texture_00_png,
    app::raw::Hiyori_Hiyori_2048_texture_01_png,
    app::raw::Hiyori_Hiyori_cdi3_json,
    app::raw::Hiyori_Hiyori_moc3,
    app::raw::Hiyori_Hiyori_model3_json,
    app::raw::Hiyori_Hiyori_physics3_json,
    app::raw::Hiyori_Hiyori_pose3_json,
    app::raw::Hiyori_Hiyori_userdata3_json,
    app::raw::Hiyori_motions_Hiyori_m01_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m02_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m03_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m04_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m05_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m06_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m07_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m08_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m09_motion3_json,
    app::raw::Hiyori_motions_Hiyori_m10_motion3_json,
    app::raw::Mao_Mao_2048_texture_00_png,
    app::raw::Mao_Mao_cdi3_json,
    app::raw::Mao_Mao_moc3,
    app::raw::Mao_Mao_model3_json,
    app::raw::Mao_Mao_physics3_json,
    app::raw::Mao_Mao_pose3_json,
    app::raw::Mao_expressions_exp_01_exp3_json,
    app::raw::Mao_expressions_exp_02_exp3_json,
    app::raw::Mao_expressions_exp_03_exp3_json,
    app::raw::Mao_expressions_exp_04_exp3_json,
    app::raw::Mao_expressions_exp_05_exp3_json,
    app::raw::Mao_expressions_exp_06_exp3_json,
    app::raw::Mao_expressions_exp_07_exp3_json,
    app::raw::Mao_expressions_exp_08_exp3_json,
    app::raw::Mao_motions_mtn_01_motion3_json,
    app::raw::Mao_motions_mtn_02_motion3_json,
    app::raw::Mao_motions_mtn_03_motion3_json,
    app::raw::Mao_motions_mtn_04_motion3_json,
    app::raw::Mao_motions_sample_01_motion3_json,
    app::raw::Mao_motions_special_01_motion3_json,
    app::raw::Mao_motions_special_02_motion3_json,
    app::raw::Mao_motions_special_03_motion3_json,
};

using Resources = std::map<std::string, RawAsset>;

struct LoadedModel {
  std::size_t index = 0;
  ModelAsset asset;
  bool operator==(const LoadedModel&) const = default;
};

struct MotionChoice {
  std::string group;
  int index;
  bool operator==(const MotionChoice&) const = default;
};

using Status = std::variant<StringVariant, MotionChoice>;

Task<IoResult<AsyncInputStream>> OpenSample(Resources resources, std::string path) {
  auto found = resources.find(path);
  if (found == resources.end()) {
    co_return IoResult<AsyncInputStream>::Failure({IoErrorCode::NotFound, "Sample resource not found"});
  }
  co_return IoResult<AsyncInputStream>::Success(co_await found->second.OpenReadAsync());
}

Task<void> LoadSample(Resources resources, std::size_t index, State<std::size_t> selected,
                      State<LoadedModel> model, State<Status> status, State<bool> failed) {
  auto loaded = co_await LoadModelAsync(characters[index].entry, [resources](std::string path) {
    return OpenSample(resources, std::move(path));
  });
  if (selected != index) co_return;
  if (loaded.Succeeded()) {
    model = LoadedModel{index, std::move(loaded).Value()};
    status = StringVariant(app::strings::preparing_message);
  } else {
    failed = true;
    status = StringVariant::Format(app::strings::load_error, loaded.Error().message);
  }
}

std::vector<MotionChoice> MotionChoices(const ModelAsset& asset) {
  std::vector<MotionChoice> choices;
  if (!asset.HasValue()) return choices;
  for (const auto& group : asset.Info().motion_groups) {
    for (int index = 0; index < group.count; ++index) {
      choices.push_back({group.name, index});
    }
  }
  return choices;
}

StringVariant MotionLabel(const MotionChoice& motion) {
  if (motion.group == "Idle") return StringVariant::Format(app::strings::idle_motion, motion.index + 1);
  if (motion.group == "TapBody") return StringVariant::Format(app::strings::interaction_motion, motion.index + 1);
  return StringVariant::Format(app::strings::named_motion, motion.group, motion.index + 1);
}

bool Play(const ModelController& controller, const MotionChoice& motion, State<Status> status) {
  const auto result = controller.PlayMotion(motion.group, motion.index,
                                          {.force = true, .loop = motion.group == "Idle"});
  if (!result.Succeeded()) {
    status = StringVariant::Format(app::strings::playback_error, result.Error().message);
    return false;
  }
  return true;
}

View Caption(StringVariant text, Color color) {
  return Text(std::move(text)).Style({Font::System(12.0F), color});
}

[[huxerui::composable]]
View CharacterPicker(State<std::size_t> selected, bool compact) {
  auto style = UseEnvironment<ChipStyle>();
  const auto colors = UseTheme().colors;
  style.minimum_height = 44.0F;
  style.corner_radii = CornerRadii{12.0F};
  style.padding = EdgeInsets::Symmetric(12.0F, 4.0F);
  style.label_style = {Font::System(13.0F).WithWeight(FontWeight::SemiBold), colors.on_surface_variant};
  style.background = Color::Transparent();
  style.border = {Color::Transparent(), 0.0F};
  style.selected_background = colors.primary_container;
  style.selected_label = colors.on_primary_container;
  style.selected_border = Border{Color::Transparent(), 0.0F};
  ThemeDefinition theme;
  theme.Set(style);
  Views cards;
  for (std::size_t index = 0; index < characters.size(); ++index) {
    cards.Add(Chip(std::string(characters[index].name), selected == index)
        .OnChanged([selected, index](bool checked) { if (checked) selected = index; })
        .With(Grow())
        .Key(std::string(characters[index].name)));
  }
  return Theme(theme, Row { cards }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch),
                                        Frame{.width = compact ? std::nullopt : std::optional<float>{252.0F}}));
}

[[huxerui::composable]]
View Gallery() {
  const auto colors = UseTheme().colors;
  const bool compact = UseViewportClass() != ViewportClass::Expanded;
  auto selected = UseState(std::size_t{0});
  auto model = UseState(LoadedModel{});
  const ModelController controller = UseState(ModelController{});
  auto panel_open = UseState(false);
  auto paused = UseState(false);
  auto ready = UseState(false);
  auto failed = UseState(false);
  auto motion_index = UseState(std::size_t{0});
  auto expression_index = UseState(std::size_t{0});
  auto interaction_index = UseState(std::size_t{0});
  auto reload = UseState(std::uint64_t{0});
  auto status = UseState(Status{StringVariant(app::strings::loading_message)});
  auto tasks = UseTaskScope();
  Resources resources;
  for (const auto& resource : model_resources) {
    // The sample package root corresponds to the generated raw resource root.
    resources.emplace(std::string(resource.Key().substr(4)), UseRawResource(resource));
  }
  const std::size_t index = selected;
  Lifecycle([tasks, resources, index, selected, model, status, ready, failed, paused,
             motion_index, expression_index, interaction_index, panel_open] {
    panel_open = false;
    ready = false;
    failed = false;
    paused = false;
    motion_index = 0;
    expression_index = 0;
    interaction_index = 0;
    status = StringVariant(app::strings::loading_message);
    auto request = tasks.Launch(LoadSample(resources, index, selected, model, status, failed));
    return [request] { request.Cancel(); };
  }, selected, reload);

  const auto asset = model->index == index ? model->asset : ModelAsset{};
  const auto motions = MotionChoices(asset);
  const bool available = asset.HasValue() && ready && !failed;
  std::vector<StringVariant> expressions{app::strings::default_expression};
  if (asset.HasValue()) {
    for (std::size_t i = 0; i < asset.Info().expressions.size(); ++i) {
      expressions.push_back(StringVariant::Format(app::strings::numbered_expression, i + 1));
    }
  }
  const auto play_idle = [controller, motions, motion_index, status] {
    for (std::size_t i = 0; i < motions.size(); ++i) {
      if (motions[i].group != "Idle") continue;
      if (Play(controller, motions[i], status)) {
        motion_index = i;
        status = StringVariant(app::strings::interaction_hint);
      }
      return;
    }
  };
  const auto interact = [controller, motions, interaction_index, motion_index, status, paused] {
    std::vector<std::size_t> choices;
    for (std::size_t i = 0; i < motions.size(); ++i) if (motions[i].group == "TapBody") choices.push_back(i);
    if (choices.empty()) return;
    const auto choice = choices[interaction_index % choices.size()];
    const auto& motion = motions[choice];
    if (Play(controller, motion, status)) {
      interaction_index += 1;
      motion_index = choice;
      paused = false;
      status = motion;
    }
  };

  View character = asset.HasValue()
      ? ModelView(asset, {.controller = controller, .paused = paused})
            .On<ModelEvents::Ready>([ready, status, play_idle] {
              ready = true;
              status = StringVariant(app::strings::interaction_hint);
              play_idle();
            })
            .On<ModelEvents::PlaybackFailed>([ready, failed, status](PlaybackError error) {
              ready = false;
              failed = true;
              status = StringVariant::Format(app::strings::render_error, error.message);
            })
            .On<ModelEvents::HitAreaTapped>([interact](std::string) { interact(); })
            .On<ModelEvents::MotionFinished>([play_idle](MotionFinished event) {
              if (event.reason == MotionEndReason::Completed) play_idle();
            })
            .Key(std::string(characters[index].name))
      : View(Spacer());

  const Status current_status = status;
  const StringVariant status_label = std::holds_alternative<MotionChoice>(current_status)
      ? StringVariant::Format(app::strings::playing_motion,
                              UseString(MotionLabel(std::get<MotionChoice>(current_status))))
      : std::get<StringVariant>(current_status);

  Views overlay;
  if (failed) {
    overlay.Add(Column {
      Caption(status_label, colors.error),
      Button(app::strings::retry).OnClick([reload] { reload += 1; }),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center), Frame{.max_width = 280.0F}));
  } else if (!available) {
    overlay.Add(Column {
      ProgressCircle(),
      Caption(StringVariant(app::strings::preparing_message), colors.on_surface_variant),
    }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)));
  }

  View header = Row {
    Text(std::string(characters[index].name))
        .Style({Font::System(20.0F).WithWeight(FontWeight::SemiBold), colors.on_surface}),
    Caption(characters[index].description, colors.on_surface_variant),
    Spacer(),
    Caption(failed ? app::strings::status_failed
                         : available ? (paused ? app::strings::status_paused : app::strings::status_live)
                                     : app::strings::status_loading, colors.primary)
        .With(Padding(EdgeInsets::Symmetric(10.0F, 6.0F)), Background(Color::Rgb(255, 255, 255, 0.7F)),
              CornerRadius(12.0F)),
  }.With(Spacing(10.0F), CrossAlign(CrossAxisAlignment::Center),
         Padding(EdgeInsets::Symmetric(compact ? 20.0F : 32.0F, 12.0F)));

  View stage = Column {
    header,
    Stack {
      character,
      Stack { overlay }.With(Align(HorizontalAlignment::Center, VerticalAlignment::Center)),
    }.With(Grow(), Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch)),
    Spacer().With(Grow(0.0F), Frame{.height = compact ? 116.0F : 80.0F}),
  }.With(CrossAlign(CrossAxisAlignment::Stretch));

  Views selectors;
  if (!motions.empty()) {
    selectors.Add(Select(motions, std::min<std::size_t>(motion_index, motions.size() - 1),
                        [](const MotionChoice& motion) { return Text(MotionLabel(motion)); })
        .Label(app::strings::motion_label)
        .OnChanged([controller, motions, motion_index, paused, status, panel_open](std::size_t next) {
          if (Play(controller, motions[next], status)) {
            motion_index = next;
            paused = false;
            status = motions[next];
            panel_open = false;
          }
        }).With(Enabled(available), Grow()));
  }
  if (expressions.size() > 1) {
    selectors.Add(Select(expressions, std::min<std::size_t>(expression_index, expressions.size() - 1),
                        [](const StringVariant& label) { return Text(label); })
        .Label(app::strings::expression_label)
        .OnChanged([controller, asset, expression_index, status, panel_open](std::size_t next) {
          const auto error = next == 0 ? controller.ClearExpression()
                                      : controller.SetExpression(asset.Info().expressions[next - 1]);
          if (error.code == PlaybackErrorCode::None) {
            expression_index = next;
            panel_open = false;
          }
          else status = StringVariant::Format(app::strings::expression_error, error.message);
        }).With(Enabled(available), Grow()));
  }
  auto button_style = UseEnvironment<ButtonStyle>();
  button_style.minimum_height = 44.0F;
  button_style.corner_radii = CornerRadii{12.0F};
  button_style.label_style.font = Font::System(13.0F).WithWeight(FontWeight::SemiBold);
  auto chip_style = UseEnvironment<ChipStyle>();
  chip_style.minimum_height = 44.0F;
  chip_style.corner_radii = CornerRadii{12.0F};
  chip_style.background = Color::Transparent();
  chip_style.border = {Color::Transparent(), 0.0F};
  chip_style.label_style = {Font::System(13.0F), colors.on_surface};
  auto icon_style = UseEnvironment<IconButtonStyle>();
  icon_style.minimum_interactive_size = 44.0F;
  icon_style.foreground = colors.on_surface_variant;
  ThemeDefinition control_theme;
  control_theme.Set(button_style);
  control_theme.Set(chip_style);
  control_theme.Set(icon_style);

  View actions = Row {
    Chip(paused ? app::images::play : app::images::pause, paused ? app::strings::play : app::strings::pause)
        .OnClick([paused] { paused = !paused; }).With(Enabled(available), Grow()),
    Button(app::strings::interact).OnClick(interact).With(Enabled(available), Grow()),
    Chip(app::images::adjust, panel_open ? app::strings::collapse : app::strings::adjust)
        .OnClick([panel_open] { panel_open = !panel_open; }).With(Grow()),
  }.With(Spacing(6.0F), CrossAlign(CrossAxisAlignment::Center));

  View dock = compact
      ? View(Column {
          CharacterPicker(selected, true),
          actions,
        }.With(Spacing(4.0F), CrossAlign(CrossAxisAlignment::Stretch)))
      : View(Row {
          CharacterPicker(selected, false),
          Divider(Axis::Vertical).With(Frame{.height = 24.0F}),
          std::move(actions).With(Grow()),
        }.With(Spacing(12.0F), CrossAlign(CrossAxisAlignment::Center)));

  Views panel;
  if (panel_open) {
    panel.Add(Column {
      Row {
        Text(app::strings::adjustment_title).Style({Font::System(14.0F).WithWeight(FontWeight::SemiBold), colors.on_surface}),
        Spacer(),
        IconButton(app::images::close, app::strings::close_adjustments).OnClick([panel_open] { panel_open = false; }),
      }.With(CrossAlign(CrossAxisAlignment::Center)),
      Row { selectors }.With(Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch)),
      Caption(asset.HasValue() && expressions.size() == 1 ? StringVariant(app::strings::no_expressions) : status_label,
              colors.on_surface_variant),
    }.With(Padding(16.0F), Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch),
           Background(colors.surface), CornerRadius(20.0F), Border{Color::Rgb(226, 219, 237), 1.0F}));
  }

  return Stack {
    stage,
    Stack {
      Theme(control_theme, Column {
        panel,
        Stack { dock }.With(Padding(8.0F), Align(HorizontalAlignment::Stretch, VerticalAlignment::Center),
                            Background(Color::Rgb(255, 255, 255, 0.96F)), CornerRadius(20.0F),
                            Border{Color::Rgb(226, 219, 237), 1.0F},
                            Shadow{.color = Color::Rgb(79, 54, 111, 0.08F), .offset = {0, 4}, .blur_radius = 20.0F}),
      }.With(Frame{.width = compact ? 376.0F : 640.0F}, Padding(compact ? 12.0F : 20.0F),
             Spacing(8.0F), CrossAlign(CrossAxisAlignment::Stretch))),
    }.With(Align(HorizontalAlignment::Center, VerticalAlignment::End)),
  }.With(Align(HorizontalAlignment::Stretch, VerticalAlignment::Stretch),
         Background(LinearGradient{.start = {0, 0}, .end = {1, 1},
             .stops = {{0, Color::Rgb(237, 230, 250)}, {0.6F, Color::Rgb(250, 244, 248)},
                       {1, Color::Rgb(232, 236, 251)}}}));

}

} // namespace

View App() {
  auto theme = MaterialLightThemeSpec();
  theme.colors.primary = Color::Rgb(112, 78, 180);
  theme.colors.primary_container = Color::Rgb(236, 228, 249);
  theme.colors.on_primary_container = Color::Rgb(62, 38, 109);
  theme.colors.background = Color::Rgb(248, 246, 252);
  theme.colors.surface = Color::White();
  theme.colors.on_surface = Color::Rgb(44, 35, 65);
  theme.colors.on_surface_variant = Color::Rgb(120, 110, 138);
  theme.colors.outline = Color::Rgb(205, 195, 220);
  theme.shapes.small = 12.0F;
  theme.shapes.medium = 16.0F;
  return MaterialTheme(theme, Gallery());
}

const Application application{
    App,
    {
        .window = {
            .title = "Live2D",
            .initial_size = {1100.0F, 820.0F},
            .minimum_size = Size{360.0F, 640.0F},
        },
    }
};
