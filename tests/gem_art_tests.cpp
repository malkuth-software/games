// The gems drawn through GUI.Forms' raster engine (shared/puzzles/gem_art.cpp): every kind of
// gem is drawn, opaque over its stone, its shadow translucent and the corners clear; a ruby
// and a sapphire keep their colours; a lifted gem lies above its neighbour whatever the order
// they were queued in; the clip and the sparkle hold; and a layer GUI.Drawing cannot take is
// refused and left untouched, as a gem with no finite size is ignored.
#include "gem_art.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

constexpr int side = 160;  // the test layer is side x side pixels, row-major

// A premultiplied BGRA layer: in memory B, G, R, A, so a pixel read as a 32-bit word is
// 0xAARRGGBB.
struct Layer {
    std::vector<std::uint32_t> pixels = std::vector<std::uint32_t>(static_cast<std::size_t>(side * side), 0U);
    [[nodiscard]] render::Target target(render::Order order = render::Order::bgra) {
        return render::Target{pixels.data(), side, side, side, order};
    }
    [[nodiscard]] std::uint32_t at(int x, int y) const {
        return pixels[static_cast<std::size_t>(y) * side + static_cast<std::size_t>(x)];
    }
};
std::uint32_t alpha_of(std::uint32_t pixel) {
    return pixel >> 24;
}
std::uint32_t red_of(std::uint32_t pixel) {
    return (pixel >> 16) & 255U;
}
std::uint32_t green_of(std::uint32_t pixel) {
    return (pixel >> 8) & 255U;
}
std::uint32_t blue_of(std::uint32_t pixel) {
    return pixel & 255U;
}

void check_every_kind() {
    // The seven colours, a bomb, a star and the hypercube.
    const std::array<int, 10> values{1, 2, 3, 4, 5, 6, 7, 17, 34, 48};
    games::GemCanvas canvas;
    for (const int value : values) {
        Layer layer;
        canvas.begin(layer.target(), render::Rect{0, 0, side, side});
        canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 50, .value = value});
        require(canvas.finish() == games::GemDrawStatus::drawn, "GUI.Drawing draws a gem");
        require(canvas.last_command_count() > 0, "A gem is recorded as GUI.Drawing commands");
        require(alpha_of(layer.at(80, 80)) == 255, "The stone is opaque at its centre");
        require(alpha_of(layer.at(80, 50)) == 255, "The stone is opaque across its crown");
        require(alpha_of(layer.at(2, 2)) == 0 && alpha_of(layer.at(157, 2)) == 0, "The corners stay clear");
        require(alpha_of(layer.at(80, 14)) == 0, "Nothing is drawn above the girdle");
        std::size_t shadow = 0;
        for (const std::uint32_t pixel : layer.pixels)
            if (alpha_of(pixel) >= 100 && alpha_of(pixel) <= 120)
                ++shadow;
        require(shadow > 0, "The shadow shows, translucent, beyond the stone");
    }
    Layer ruby;
    canvas.begin(ruby.target(), render::Rect{0, 0, side, side});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 50, .value = 1});
    require(canvas.finish() == games::GemDrawStatus::drawn, "A ruby is drawn");
    const std::uint32_t ruby_table = ruby.at(80, 80);
    require(red_of(ruby_table) > green_of(ruby_table) && red_of(ruby_table) > blue_of(ruby_table),
            "A ruby's table is red");
    Layer sapphire;
    canvas.begin(sapphire.target(), render::Rect{0, 0, side, side});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 50, .value = 2});
    require(canvas.finish() == games::GemDrawStatus::drawn, "A sapphire is drawn");
    const std::uint32_t sapphire_table = sapphire.at(80, 80);
    require(blue_of(sapphire_table) > red_of(sapphire_table), "A sapphire's table is blue");
}

void check_lifted_above() {
    // The sapphire, lifted, is queued first; the ruby beside it second. Where they overlap
    // the lifted sapphire shows.
    games::GemCanvas canvas;
    Layer layer;
    canvas.begin(layer.target(), render::Rect{0, 0, side, side});
    canvas.gem(games::GemPlacement{.x = 100, .y = 80, .radius = 40, .value = 2, .lifted = true});
    canvas.gem(games::GemPlacement{.x = 60, .y = 80, .radius = 40, .value = 1});
    require(canvas.finish() == games::GemDrawStatus::drawn, "Two gems are drawn");
    const std::uint32_t overlap = layer.at(84, 80);
    require(alpha_of(overlap) == 255 && blue_of(overlap) > red_of(overlap), "A lifted gem lies above its neighbour");
}

void check_clip_and_sparkle() {
    // Pixels outside the clip keep what they held (a premultiplied sentinel).
    const std::uint32_t sentinel = 0x40102030U;
    games::GemCanvas canvas;
    Layer clipped;
    for (std::uint32_t& pixel : clipped.pixels)
        pixel = sentinel;
    canvas.begin(clipped.target(), render::Rect{0, 0, 80, side});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 50, .value = 3});
    require(canvas.finish() == games::GemDrawStatus::drawn, "A clipped gem is drawn");
    require(clipped.at(110, 80) == sentinel && clipped.at(80, 80) == sentinel, "Nothing is drawn outside the clip");
    require(clipped.at(50, 80) != sentinel, "The gem is drawn inside the clip");
    // A sparkle lights only past .7 of its glisten.
    Layer dim;
    canvas.begin(dim.target(), render::Rect{0, 0, side, side});
    canvas.sparkle(games::GemSparkle{.x = 80, .y = 80, .radius = 50, .glisten = .5});
    require(canvas.finish() == games::GemDrawStatus::drawn, "An unlit sparkle is no failure");
    for (const std::uint32_t pixel : dim.pixels)
        require(pixel == 0U, "An unlit sparkle draws nothing");
    Layer lit;
    canvas.begin(lit.target(), render::Rect{0, 0, side, side});
    canvas.sparkle(games::GemSparkle{.x = 80, .y = 80, .radius = 50, .glisten = .95});
    require(canvas.finish() == games::GemDrawStatus::drawn, "A lit sparkle is drawn");
    // Its cross is centred .3 radius left and .35 radius above the gem's centre.
    const std::uint32_t cross = lit.at(65, 62);
    require(alpha_of(cross) == 255 && red_of(cross) >= 250 && green_of(cross) >= 250 && blue_of(cross) >= 250,
            "A lit sparkle is a white cross");
}

void check_refusals() {
    // A layer in RGBA order is not one GUI.Drawing draws into: refused and untouched.
    games::GemCanvas canvas;
    Layer layer;
    canvas.begin(layer.target(render::Order::rgba), render::Rect{0, 0, side, side});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 50, .value = 1});
    require(canvas.finish() == games::GemDrawStatus::refused, "A layer GUI.Drawing cannot take is refused");
    for (const std::uint32_t pixel : layer.pixels)
        require(pixel == 0U, "A refused layer is untouched");
    // A gem with no finite size is not queued: nothing drawn, and no failure.
    Layer empty;
    canvas.begin(empty.target(), render::Rect{0, 0, side, side});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = std::numeric_limits<double>::quiet_NaN(), .value = 1});
    canvas.gem(games::GemPlacement{.x = 80, .y = 80, .radius = 0, .value = 1});
    require(canvas.finish() == games::GemDrawStatus::drawn, "Ignored gems are no failure");
    for (const std::uint32_t pixel : empty.pixels)
        require(pixel == 0U, "An ignored gem draws nothing");
}
} // namespace

int main() {
    try {
        check_every_kind();
        check_lifted_above();
        check_clip_and_sparkle();
        check_refusals();
        std::cout << "Gems drawn through GUI.Forms' raster engine: kinds, colours, depth, clip, sparkle and "
                     "refusals passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
