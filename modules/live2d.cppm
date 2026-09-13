// The C++20 module front door for Lib-Live2D. `import huxerui.live2d;` and
// `#include <huxerui/live2d.h>` describe the SAME entities: the header is
// included in the global module fragment below, so everything it declares
// stays attached to the global module and keeps its ordinary linkage and
// mangling. The module therefore changes no ABI and needs no separate library.
//
// Hand-written, unlike HuxerUI's modules/huxerui.cppm: one header, one
// namespace, and every public name is listed here. A name added to the header
// is added here too, or `import huxerui.live2d;` does not see it.

module;

#include <huxerui/live2d.h>

export module huxerui.live2d;

export namespace huxerui::live2d {
    using huxerui::live2d::LoadErrorCode;
    using huxerui::live2d::LoadError;
    using huxerui::live2d::PlaybackErrorCode;
    using huxerui::live2d::PlaybackError;
    using huxerui::live2d::ParameterInfo;
    using huxerui::live2d::MotionGroup;
    using huxerui::live2d::ModelInfo;
    using huxerui::live2d::ModelAsset;
    using huxerui::live2d::LoadLimits;
    using huxerui::live2d::OpenResourceAsync;
    using huxerui::live2d::LoadModelAsync;
    using huxerui::live2d::PlaybackId;
    using huxerui::live2d::MotionOptions;
    using huxerui::live2d::MotionEndReason;
    using huxerui::live2d::MotionFinished;
    using huxerui::live2d::ModelController;
    using huxerui::live2d::ParameterBlend;
    using huxerui::live2d::ParameterInput;
    using huxerui::live2d::ModelOptions;
    using huxerui::live2d::ModelEvents;
    using huxerui::live2d::ModelView;
}
