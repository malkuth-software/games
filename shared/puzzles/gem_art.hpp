#pragma once
// Gems drawn by GUI.Forms' raster engine (GUI.Drawing). A GemCanvas takes the gems of one
// drawing, records their shapes as GUI.Drawing commands (filled polygons and ellipses, each
// facet shaded by a linear gradient), farthest first, and runs the recording into a
// premultiplied BGRA layer it borrows for the drawing (gui_drawing::execute_recorder). The
// board shows through the layer, which is then laid into the window's own buffer, averaged
// down when it was drawn finer than the window.
//
// Depth: the shapes of a gem lie at fixed depths (shadow, girdle, stone, glint, sparkle,
// marks: the z values of the r3d drawing this replaces), so drawing every queued gem's
// shadow, then every girdle, and so on, puts each shape where a depth test would. A lifted
// gem is drawn whole after all the others, above them. Within one depth a later gem covers
// an earlier one, as r3d's depth test (z <= depth) let it.
#include "gui_forms/drawing/bitmap/bitmap.hpp"
#include "gui_forms/drawing/graphics_recorder/graphics_recorder.hpp"
#include "target.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace games {

// One gem: its centre (x, y) and radius in the layer's pixels; `value` is the board's cell
// value (colour, bomb, star, hypercube), `angle` turns it, `glisten` (0..1) lights its
// sparkle, and a lifted gem lies above every other gem (the dragged one).
struct GemPlacement {
    double x = 0;
    double y = 0;
    double radius = 0;
    int value = 0;
    double angle = 0;
    double glisten = 0;
    bool lifted = false;
};

// A gem's sparkle alone, a cross of light growing as `glisten` passes .7 (nothing below): for
// a gem copied from a picture drawn without it (PuzzleView's resting board).
struct GemSparkle {
    double x = 0;
    double y = 0;
    double radius = 0;
    double glisten = 0;
};

enum class GemDrawStatus : std::uint8_t {
    // Every queued shape is in the layer (or nothing was queued).
    drawn,
    // GUI.Drawing refused the layer, a shape or the run: the layer keeps the shapes drawn
    // before the refusal (the borrowed bitmap is written in place), possibly none.
    refused,
};

// Records queued gems and runs them into a borrowed layer. One thread, the owner's (the UI
// thread for PuzzleView), which then also owns the recorder and the borrowed bitmap, as
// GUI.Drawing requires. The recorder keeps its command storage between drawings.
class GemCanvas final {
  public:
    // A gem queued once for each separate area it is redrawn in counts once for each.
    static constexpr std::size_t maximum_gems = 512;
    static constexpr std::size_t maximum_sparkles = 512;

    GemCanvas();

    // Starts a drawing over `layer` (premultiplied, render::Order::bgra, `layer.stride` pixels
    // a row), drawing only inside `clip` (layer pixels). Forgets anything queued and not
    // drawn. The layer's pixels must stay alive and unmoved until finish() returns.
    void begin(const render::Target& layer, render::Rect clip);
    // Queues a gem or a sparkle, drawn inside the drawing's clip, or inside `clip` (layer
    // pixels) within it: one drawing can then redraw several separate areas of the layer, each
    // shape kept to its own. Ignored past the maximum count, or when a coordinate, the radius,
    // the angle or the glisten is not finite or the radius is not positive.
    void gem(const GemPlacement& placement);
    void gem(const GemPlacement& placement, render::Rect clip);
    void sparkle(const GemSparkle& placement);
    void sparkle(const GemSparkle& placement, render::Rect clip);
    // Records the queued shapes farthest first and runs them into the layer, then forgets
    // them. What a refusal leaves is GemDrawStatus::refused's.
    [[nodiscard]] GemDrawStatus finish();
    // The commands the last finish() recorded (for tests and the bench).
    [[nodiscard]] std::size_t last_command_count() const;

  private:
    // A gem's shapes, farthest first.
    enum class Depth : std::uint8_t { shadow, girdle, stone, glint, sparkle, mark, fuse };
    void record_gem(const GemPlacement& gem, Depth depth);
    void record_sparkle(double x, double y, double radius, double glisten);
    void use_smoothing(gui_drawing::SmoothingMode mode);
    // Records a clip change when `clip` (already within the drawing's) is not the one in force.
    void use_clip(render::Rect clip);

    struct QueuedGem {
        GemPlacement placement;
        render::Rect clip;  // within the drawing's clip, not empty
    };
    struct QueuedSparkle {
        GemSparkle placement;
        render::Rect clip;  // within the drawing's clip, not empty
    };
    gui_drawing::GraphicsRecorder recorder_;
    std::vector<QueuedGem> gems_;          // reserved to maximum_gems, in queued order
    std::vector<QueuedSparkle> sparkles_;  // reserved to maximum_sparkles, in queued order
    render::Target layer_{};
    render::Rect clip_{};
    render::Rect clip_in_force_{};
    gui_drawing::SmoothingMode smoothing_ = gui_drawing::SmoothingMode::anti_alias;
    std::size_t last_command_count_ = 0;
};

// Lays `layer` (premultiplied, in the target's byte order, `scale` (1 or 2) layer pixels to
// one target pixel, its origin at the target's (left, top)) over the target inside `area`,
// each target pixel the mean of the layer pixels over it.
void lay_layer(const render::Target& target, render::Rect area, const render::Target& layer,
               int left, int top, int scale);

// The outline of a rounded rectangle in device pixels, anti-aliased, at `alpha` (0..1).
void rounded_outline(const render::Target& target, render::Rect clip, double x, double y,
                     double w, double h, double radius, render::Color color, float alpha,
                     double line_width);

} // namespace games
