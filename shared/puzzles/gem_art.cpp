#include "gem_art.hpp"

#include "gui_forms/drawing/brush/linear_gradient_brush.hpp"
#include "gui_forms/drawing/recorder_raster/recorder_raster.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <memory>
#include <span>

namespace games {
namespace {
namespace gd = gui_drawing;
using render::Color;

constexpr double full_turn = 6.283185307;

Color jewel(int color) {
    const Color colors[] = {{220, 230, 240}, {228, 49, 86},   {59, 181, 255},
                            {255, 182, 48},  {126, 224, 111}, {168, 101, 249},
                            {53, 220, 208},  {244, 119, 44},  {233, 127, 203}};
    return colors[std::clamp(color, 0, 8)];
}
Color shade(Color c, double n) {
    c.r = static_cast<float>(std::clamp(c.r * n, 0.0, 255.0));
    c.g = static_cast<float>(std::clamp(c.g * n, 0.0, 255.0));
    c.b = static_cast<float>(std::clamp(c.b * n, 0.0, 255.0));
    return c;
}

struct Point {
    double x = 0, y = 0;
};

// A colour channel (0..255, float as render::Color keeps it) as GUI.Drawing's byte.
std::uint8_t channel(float value) {
    const float clamped = std::clamp(value, 0.F, 255.F);
    return static_cast<std::uint8_t>(std::lround(clamped));
}
gd::Color drawing_color(Color c) {
    return gd::Color::from_argb(channel(c.a), channel(c.r), channel(c.g), channel(c.b));
}
gd::PointF drawing_point(Point p) {
    return gd::PointF{p.x, p.y};
}

// Light falls from the upper left.
struct FacetLight {
    double light = -2.356;
    double operator()(double a, double strength) const {
        return .5 + strength * std::max(0.0, std::cos(a - light)) +
               .12 * std::cos(a - light + 3.14159) * -1;
    }
};

struct GemCorner {
    int n;
    double spin;
    Point center;
    double tilt;
    Point operator()(int i, double r) const {
        const double a = full_turn * i / n + spin;
        return Point{center.x + std::cos(a) * r * tilt, center.y + std::sin(a) * r};
    }
};

// A gem's cut: its sides, colour and how far it is turned.
struct GemCut {
    int sides = 8;
    Color base{};
    double tilt = 1;
    GemCorner corner{8, 0, {}, 1};
};
GemCut cut_of(const GemPlacement& gem) {
    const int color = gem.value % 16;
    int sides = color == 1   ? 6
                : color == 2 ? 4
                : color == 3 ? 8
                : color == 4 ? 5
                : color == 5 ? 6
                : color == 6 ? 4
                             : 8;
    if (gem.value == 48)
        sides = 4;
    const double tilt = std::cos(gem.angle) * .18 + .82;
    const GemCut cut{sides, jewel(color), tilt,
                     GemCorner{sides, gem.angle * .17 + .785398, Point{gem.x, gem.y}, tilt}};
    return cut;
}

// A corner of a shaded facet: where it is and its colour there.
struct ShadedCorner {
    Point at;
    Color color;
};
// A colour at a distance along a facet's shading direction.
struct ShadeStop {
    double along = 0;
    Color color{};
};
struct ShadeStopOrder {
    bool operator()(const ShadeStop& left, const ShadeStop& right) const {
        return left.along < right.along;
    }
};
// The sum of a colour's channels: the facet is shaded along the direction it grows fastest.
double weight_of(Color c) {
    return static_cast<double>(c.r) + static_cast<double>(c.g) + static_cast<double>(c.b);
}
Color mean_of(Color a, Color b) {
    return Color{(a.r + b.r) * .5F, (a.g + b.g) * .5F, (a.b + b.b) * .5F, (a.a + b.a) * .5F};
}

// One colour over a polygon.
void fill_solid(gd::GraphicsRecorder& recorder, std::span<const gd::PointF> points, Color color) {
    const gd::SolidBrush brush(drawing_color(color));
    recorder.fill_polygon(brush, points);
}
void fill_disc(gd::GraphicsRecorder& recorder, Point center, double radius, Color color) {
    const gd::SolidBrush brush(drawing_color(color));
    recorder.fill_ellipse(brush, gd::RectF{center.x - radius, center.y - radius, radius * 2, radius * 2});
}
// A stroke from a to b, `thickness` wide with square ends at a and b: a filled quadrilateral.
void fill_line(gd::GraphicsRecorder& recorder, Point a, Point b, double thickness, Color color) {
    const double length = std::hypot(b.x - a.x, b.y - a.y);
    if (length < 1e-6)
        return;
    const double dx = (b.y - a.y) / length * thickness * .5;
    const double dy = -(b.x - a.x) / length * thickness * .5;
    const std::array<gd::PointF, 4> points{gd::PointF{a.x + dx, a.y + dy}, gd::PointF{b.x + dx, b.y + dy},
                                           gd::PointF{b.x - dx, b.y - dy}, gd::PointF{a.x - dx, a.y - dy}};
    fill_solid(recorder, points, color);
}

// A triangle shaded across from its corners' colours, as r3d's Smooth shader shaded it, by
// a GUI.Drawing linear gradient: the gradient runs along the direction in which the corners'
// colour weight grows, with each corner's colour at its distance along it. A facet's corners
// are one colour scaled by its light, so the colour changes along one direction only and the
// gradient reproduces the interpolation (where a channel was clamped at 255, or the table of
// a hypercube mixes two colours, it matches at the corners and is close between). GUI.Drawing
// interpolates gradients in linear light where r3d interpolated the stored sRGB values.
void fill_shaded(gd::GraphicsRecorder& recorder, ShadedCorner a, ShadedCorner b, ShadedCorner c) {
    const std::array<gd::PointF, 3> points{drawing_point(a.at), drawing_point(b.at), drawing_point(c.at)};
    const double e1x = b.at.x - a.at.x, e1y = b.at.y - a.at.y;
    const double e2x = c.at.x - a.at.x, e2y = c.at.y - a.at.y;
    const double area = e1x * e2y - e1y * e2x;
    if (std::fabs(area) < 1e-9)
        return;  // no area: nothing to fill
    const Color mean{(a.color.r + b.color.r + c.color.r) / 3.F, (a.color.g + b.color.g + c.color.g) / 3.F,
                     (a.color.b + b.color.b + c.color.b) / 3.F, (a.color.a + b.color.a + c.color.a) / 3.F};
    // The gradient of the weight over the triangle, from the two edges' differences.
    const double d1 = weight_of(b.color) - weight_of(a.color);
    const double d2 = weight_of(c.color) - weight_of(a.color);
    const double gx = (d1 * e2y - d2 * e1y) / area;
    const double gy = (d2 * e1x - d1 * e2x) / area;
    const double steepness = std::hypot(gx, gy);
    if (steepness < 1e-9) {
        fill_solid(recorder, points, mean);
        return;
    }
    const double ux = gx / steepness, uy = gy / steepness;
    std::array<ShadeStop, 3> stops{ShadeStop{0.0, a.color}, ShadeStop{e1x * ux + e1y * uy, b.color},
                                   ShadeStop{e2x * ux + e2y * uy, c.color}};
    std::sort(stops.begin(), stops.end(), ShadeStopOrder{});
    const double span = stops[2].along - stops[0].along;
    if (span < 1e-9) {
        fill_solid(recorder, points, mean);
        return;
    }
    // Positions are strictly ordered in GUI.Drawing: a middle corner level with an end is
    // merged with it.
    const double middle = (stops[1].along - stops[0].along) / span;
    gd::ColorBlend blend;
    if (middle <= 1e-6) {
        blend.colors = {drawing_color(mean_of(stops[0].color, stops[1].color)), drawing_color(stops[2].color)};
        blend.positions = {0.0, 1.0};
    } else if (middle >= 1 - 1e-6) {
        blend.colors = {drawing_color(stops[0].color), drawing_color(mean_of(stops[1].color, stops[2].color))};
        blend.positions = {0.0, 1.0};
    } else {
        blend.colors = {drawing_color(stops[0].color), drawing_color(stops[1].color), drawing_color(stops[2].color)};
        blend.positions = {0.0, middle, 1.0};
    }
    // GUI.Drawing's linear gradient runs through the centre of its bounds at its angle, half
    // |cos| x width + |sin| x height to each side: a square centred between the first and last
    // stops, whose side makes that half the span.
    const double first_x = a.at.x + ux * stops[0].along, first_y = a.at.y + uy * stops[0].along;
    const double center_x = first_x + ux * span * .5, center_y = first_y + uy * span * .5;
    const double side = span / (std::fabs(ux) + std::fabs(uy));
    const double degrees = std::atan2(uy, ux) * 360.0 / full_turn;
    const gd::RectF bounds{center_x - side * .5, center_y - side * .5, side, side};
    gd::LinearGradientBrush brush(bounds, blend.colors.front(), blend.colors.back(), degrees, gd::WrapMode::clamp);
    brush.set_interpolation_colors(blend);
    recorder.fill_polygon(brush, points);
}

bool finite_placement(double x, double y, double radius, double glisten) {
    return std::isfinite(x) && std::isfinite(y) && std::isfinite(radius) && std::isfinite(glisten) && radius > 0;
}

// The mean of a 2 x 2 block of premultiplied pixels, two channels to a sum.
std::uint32_t average(const std::uint32_t* top, const std::uint32_t* bottom) {
    const std::uint32_t a = top[0], b = top[1], c = bottom[0], d = bottom[1];
    if ((a | b | c | d) == 0)
        return 0;
    const std::uint32_t rb = (a & 0x00ff00ffU) + (b & 0x00ff00ffU) + (c & 0x00ff00ffU) +
                             (d & 0x00ff00ffU) + 0x00020002U;
    const std::uint32_t ag = ((a >> 8) & 0x00ff00ffU) + ((b >> 8) & 0x00ff00ffU) +
                             ((c >> 8) & 0x00ff00ffU) + ((d >> 8) & 0x00ff00ffU) + 0x00020002U;
    return ((rb >> 2) & 0x00ff00ffU) | (((ag >> 2) & 0x00ff00ffU) << 8);
}


} // namespace

GemCanvas::GemCanvas() {
    gems_.reserve(maximum_gems);
    sparkles_.reserve(maximum_sparkles);
}

void GemCanvas::begin(const render::Target& layer, render::Rect clip) {
    layer_ = layer;
    clip_ = clip.intersected(layer.bounds());
    gems_.clear();
    sparkles_.clear();
}

void GemCanvas::gem(const GemPlacement& placement) {
    gem(placement, clip_);
}

void GemCanvas::gem(const GemPlacement& placement, render::Rect clip) {
    const render::Rect within = clip.intersected(clip_);
    if (gems_.size() >= maximum_gems || within.empty() || !std::isfinite(placement.angle) ||
        !finite_placement(placement.x, placement.y, placement.radius, placement.glisten))
        return;
    gems_.push_back(QueuedGem{placement, within});
}

void GemCanvas::sparkle(const GemSparkle& placement) {
    sparkle(placement, clip_);
}

void GemCanvas::sparkle(const GemSparkle& placement, render::Rect clip) {
    const render::Rect within = clip.intersected(clip_);
    if (sparkles_.size() >= maximum_sparkles || within.empty() ||
        !finite_placement(placement.x, placement.y, placement.radius, placement.glisten))
        return;
    sparkles_.push_back(QueuedSparkle{placement, within});
}

void GemCanvas::use_clip(render::Rect clip) {
    const bool same = clip.x0 == clip_in_force_.x0 && clip.y0 == clip_in_force_.y0 && clip.x1 == clip_in_force_.x1 &&
                      clip.y1 == clip_in_force_.y1;
    if (same)
        return;
    clip_in_force_ = clip;
    recorder_.set_clip(gd::RectF{static_cast<double>(clip.x0), static_cast<double>(clip.y0),
                                 static_cast<double>(clip.x1 - clip.x0), static_cast<double>(clip.y1 - clip.y0)});
}

std::size_t GemCanvas::last_command_count() const {
    return last_command_count_;
}

void GemCanvas::use_smoothing(gd::SmoothingMode mode) {
    if (mode == smoothing_)
        return;
    smoothing_ = mode;
    recorder_.set_quality(mode, gd::InterpolationMode::default_mode, gd::PixelOffsetMode::default_mode,
                          gd::CompositingMode::source_over, gd::CompositingQuality::default_mode);
}

void GemCanvas::record_sparkle(double x, double y, double radius, double glisten) {
    if (glisten <= .7)
        return;
    // Strokes keep their width on screen whatever the layer's density.
    const double stroke = std::max(1.0, radius / 30);
    const double r = radius * (glisten - .7) * 1.7;
    const Color light{255, 255, 255};
    fill_line(recorder_, {x - radius * .3 - r, y - radius * .35}, {x - radius * .3 + r, y - radius * .35},
              1.3 * stroke, light);
    fill_line(recorder_, {x - radius * .3, y - radius * .35 - r}, {x - radius * .3, y - radius * .35 + r},
              1.3 * stroke, light);
}

void GemCanvas::record_gem(const GemPlacement& gem, Depth depth) {
    const GemCut cut = cut_of(gem);
    const Point center{gem.x, gem.y};
    const double radius = gem.radius;
    const double stroke = std::max(1.0, radius / 30);
    switch (depth) {
    case Depth::shadow:
        use_smoothing(gd::SmoothingMode::anti_alias);
        fill_disc(recorder_, {gem.x + radius * .05, gem.y + radius * .12}, radius * .92, Color{6, 2, 16, 110});
        break;
    case Depth::girdle: {
        // A dark outline just outside the stone: the whole outer outline, which the stone then
        // covers but for the band at its edge.
        use_smoothing(gd::SmoothingMode::anti_alias);
        std::array<gd::PointF, 8> outline{};
        for (int i = 0; i < cut.sides; ++i)
            outline[static_cast<std::size_t>(i)] = drawing_point(cut.corner(i, radius * 1.07));
        const Color edge = gem.value == 48 ? Color{40, 40, 60} : shade(cut.base, .28);
        fill_solid(recorder_, std::span<const gd::PointF>(outline.data(), static_cast<std::size_t>(cut.sides)),
                   edge);
        break;
    }
    case Depth::stone: {
        // Hard-edged, as r3d's triangles were: facets that share an edge leave no seam (an
        // anti-aliased edge would let the girdle show through between them). The layer is
        // drawn at twice the device's resolution below 1.5x, which smooths the stone's edge.
        use_smoothing(gd::SmoothingMode::none);
        const FacetLight lit;
        for (int i = 0; i < cut.sides; ++i) {
            const double a = full_turn * (i + .5) / cut.sides + cut.corner.spin;
            const Point p = cut.corner(i, radius), q = cut.corner(i + 1, radius);
            const Point ip = cut.corner(i, radius * .56), iq = cut.corner(i + 1, radius * .56);
            const Color face = gem.value == 48 ? jewel(1 + i % 6) : cut.base;
            const double outer = lit(a, .62), inner = outer + .22;
            // Crown facets: two triangles from the girdle to the table.
            fill_shaded(recorder_, {p, shade(face, outer)}, {q, shade(face, outer)}, {ip, shade(face, inner)});
            fill_shaded(recorder_, {q, shade(face, outer * .94)}, {iq, shade(face, inner)},
                        {ip, shade(face, inner)});
            // Table: brightest at the center, shaded toward the far side.
            const double ta = lit(full_turn * i / cut.sides + cut.corner.spin, .4) + .35;
            const double tb = lit(full_turn * (i + 1) / cut.sides + cut.corner.spin, .4) + .35;
            const Color table = gem.value == 48 ? jewel(1 + (i + 3) % 6) : cut.base;
            fill_shaded(recorder_, {center, shade(table, 1.42)}, {ip, shade(face, ta)}, {iq, shade(face, tb)});
        }
        break;
    }
    case Depth::glint: {
        // Specular glint on the table, upper left.
        use_smoothing(gd::SmoothingMode::anti_alias);
        const double gx = gem.x - radius * .22 * cut.tilt, gy = gem.y - radius * .26, g = radius * .13;
        fill_shaded(recorder_, {{gx - g, gy}, {255, 255, 255}}, {{gx, gy - g * .7}, {255, 255, 255}},
                    {{gx + g * .5, gy + g * .2}, {235, 240, 255}});
        break;
    }
    case Depth::sparkle:
        use_smoothing(gd::SmoothingMode::anti_alias);
        record_sparkle(gem.x, gem.y, radius, gem.glisten);
        break;
    case Depth::mark:
        use_smoothing(gd::SmoothingMode::anti_alias);
        if (gem.value >= 16 && gem.value < 32)
            fill_disc(recorder_, center, radius * .24, Color{32, 27, 52});
        if (gem.value >= 32 && gem.value < 48)
            for (int i = 0; i < 5; ++i) {
                const double a = i * full_turn / 5 - 1.5708, b = (i + 2) * full_turn / 5 - 1.5708;
                fill_line(recorder_, {gem.x + std::cos(a) * radius * .34, gem.y + std::sin(a) * radius * .34},
                          {gem.x + std::cos(b) * radius * .34, gem.y + std::sin(b) * radius * .34}, 2 * stroke,
                          Color{255, 255, 225});
            }
        break;
    case Depth::fuse:
        use_smoothing(gd::SmoothingMode::anti_alias);
        if (gem.value >= 16 && gem.value < 32)
            fill_line(recorder_, {gem.x + radius * .09, gem.y - radius * .12},
                      {gem.x + radius * .25, gem.y - radius * .38}, 2 * stroke, Color{255, 240, 192});
        break;
    default:
        // Not a depth: nothing is drawn.
        break;
    }
}

GemDrawStatus GemCanvas::finish() {
    last_command_count_ = 0;
    const bool nothing = (gems_.empty() && sparkles_.empty()) || clip_.empty();
    const bool usable = layer_.pixels != nullptr && layer_.order == render::Order::bgra && layer_.width > 0 &&
                        layer_.height > 0 && layer_.stride >= layer_.width;
    GemDrawStatus status = nothing ? GemDrawStatus::drawn : GemDrawStatus::refused;
    if (!nothing && usable) {
        // GUI.Drawing refuses what it cannot record or run by throwing (std::invalid_argument
        // for a shape, std::logic_error for a recorder used from another thread): the
        // boundary of this subsystem, so it ends here as a refusal.
        try {
            recorder_.reset();
            smoothing_ = gd::SmoothingMode::anti_alias;
            recorder_.set_quality(smoothing_, gd::InterpolationMode::default_mode, gd::PixelOffsetMode::default_mode,
                                  gd::CompositingMode::source_over, gd::CompositingQuality::default_mode);
            clip_in_force_ = render::Rect{};
            use_clip(clip_);
            const std::array<Depth, 7> depths{Depth::shadow, Depth::girdle,
                                              Depth::stone,  Depth::glint, Depth::sparkle, Depth::mark, Depth::fuse};
            for (const Depth depth : depths) {
                for (const QueuedGem& gem : gems_)
                    if (!gem.placement.lifted) {
                        use_clip(gem.clip);
                        record_gem(gem.placement, depth);
                    }
                if (depth == Depth::sparkle)
                    for (const QueuedSparkle& sparkle : sparkles_) {
                        use_clip(sparkle.clip);
                        record_sparkle(sparkle.placement.x, sparkle.placement.y, sparkle.placement.radius,
                                       sparkle.placement.glisten);
                    }
            }
            // A lifted gem lies above everything, whole.
            for (const QueuedGem& gem : gems_)
                if (gem.placement.lifted) {
                    use_clip(gem.clip);
                    for (const Depth depth : depths)
                        record_gem(gem.placement, depth);
                }
            last_command_count_ = recorder_.commands().size();
            const std::size_t row_bytes = static_cast<std::size_t>(layer_.stride) * 4U;
            const std::unique_ptr<gd::Bitmap> target =
                gd::Bitmap::borrow(static_cast<std::uint32_t>(layer_.width), static_cast<std::uint32_t>(layer_.height),
                                   row_bytes, reinterpret_cast<std::byte*>(layer_.pixels));
            if (target) {
                const gd::RecorderRasterResult result = gd::execute_recorder(recorder_, *target, 0);
                status = result.status == gd::RecorderRasterStatus::executed ? GemDrawStatus::drawn
                                                                              : GemDrawStatus::refused;
            }
        } catch (const std::exception&) {
            status = GemDrawStatus::refused;
        }
    }
    gems_.clear();
    sparkles_.clear();
    return status;
}

void lay_layer(const render::Target& target, render::Rect area, const render::Target& layer,
               int left, int top, int scale) {
    const render::Rect cover{left, top, left + layer.width / scale, top + layer.height / scale};
    area = area.intersected(cover).intersected(target.bounds());
    for (int y = area.y0; y < area.y1; ++y) {
        std::uint32_t* out = target.row(y);
        const std::uint32_t* first = layer.row((y - top) * scale);
        const std::uint32_t* second = layer.row((y - top) * scale + scale - 1);
        for (int x = area.x0; x < area.x1; ++x) {
            const std::uint32_t source =
                scale == 1 ? first[x - left]
                           : average(first + (x - left) * 2, second + (x - left) * 2);
            const std::uint32_t alpha = source >> 24;
            if (alpha == 0)
                continue;
            if (alpha == 255) {
                out[x] = source;
                continue;
            }
            // Premultiplied over: the board keeps (255 - alpha) of itself.
            const std::uint32_t keep = 255 - alpha, d = out[x];
            const std::uint32_t rb = ((d & 0x00ff00ffU) * keep + 0x00800080U) >> 8 & 0x00ff00ffU;
            const std::uint32_t ag = ((d >> 8) & 0x00ff00ffU) * keep + 0x00800080U & 0xff00ff00U;
            out[x] = source + (rb | ag);
        }
    }
}

void rounded_outline(const render::Target& target, render::Rect clip, double x, double y,
                     double w, double h, double radius, Color color, float alpha,
                     double line_width) {
    const render::Rect area = render::pixel_bounds(x - line_width, y - line_width,
                                                   x + w + line_width, y + h + line_width)
                                  .intersected(clip)
                                  .intersected(target.bounds());
    const std::uint32_t value = target.color(Color{color.r, color.g, color.b, 255});
    for (int py = area.y0; py < area.y1; ++py) {
        std::uint32_t* row = target.row(py);
        const double cy = std::clamp(py + .5, y + radius, y + h - radius);
        for (int px = area.x0; px < area.x1; ++px) {
            const double cx = std::clamp(px + .5, x + radius, x + w - radius);
            const double distance = std::hypot(px + .5 - cx, py + .5 - cy) - radius;
            // Centred on the edge, as a stroke is.
            const double edge = std::clamp(line_width * .5 - std::fabs(distance) + .5, 0.0, 1.0);
            if (edge <= 0)
                continue;
            const std::uint32_t k = static_cast<std::uint32_t>(edge * alpha * 255 + .5);
            const std::uint32_t d = row[px];
            const std::uint32_t rb = (((value & 0x00ff00ffU) * k + (d & 0x00ff00ffU) * (255 - k) +
                                       0x00800080U) >> 8) & 0x00ff00ffU;
            const std::uint32_t ag = ((((value >> 8) & 0x00ff00ffU) * k +
                                       ((d >> 8) & 0x00ff00ffU) * (255 - k) + 0x00800080U)) &
                                     0xff00ff00U;
            row[px] = rb | ag;
        }
    }
}

} // namespace games
