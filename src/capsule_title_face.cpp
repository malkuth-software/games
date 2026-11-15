#include "capsule_title_face.hpp"

#include "runtime_paths.hpp"

#if defined(GAMES_GUI_FORMS_APP_FONTS)
#include "gui_forms/typography/app_fonts/app_fonts.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ios>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>
#endif

namespace games {

#if defined(GAMES_GUI_FORMS_APP_FONTS)
namespace {

// The family's name as looks would write it; one load per process.
constexpr std::string_view title_family_name = "Barlow Condensed";

// The process's hold on the family: loaded on first use and unloaded when the process ends
// (the handle's destruction; text still shaped with it stays drawable, AppFontUnload::deferred).
gf::AppFontFamily& title_family_holder() {
    static gf::AppFontFamily holder{};
    return holder;
}

// The face's bytes, or none when the file is missing or unreadable.
std::shared_ptr<const std::vector<std::byte>> read_face(const std::filesystem::path& file) {
    std::ifstream in(file, std::ios::binary | std::ios::ate);
    if (!in)
        return {};
    const std::streamoff end = in.tellg();
    if (end <= 0 || static_cast<std::uint64_t>(end) > gf::maximum_app_font_bytes)
        return {};
    std::shared_ptr<std::vector<std::byte>> bytes =
        std::make_shared<std::vector<std::byte>>(static_cast<std::size_t>(end));
    in.seekg(0);
    in.read(reinterpret_cast<char*>((*bytes).data()), static_cast<std::streamsize>(end));
    if (!in)
        return {};
    return bytes;
}

} // namespace
#endif

CapsuleTitleFaceState CapsuleTitleFace::load() {
    if (state_ != CapsuleTitleFaceState::unloaded)
        return state_;
#if defined(GAMES_GUI_FORMS_APP_FONTS)
    try {
        gf::AppFontFamily& holder = title_family_holder();
        if (holder.id() == gf::FontFamilyId{}) {
            const std::filesystem::path file =
                std::filesystem::path(asset_directory()) / "fonts" / "BarlowCondensed-Bold.ttf";
            const std::shared_ptr<const std::vector<std::byte>> bytes = read_face(file);
            if (bytes) {
                std::array<gf::AppFontFace, 1> faces{};
                faces[0].bytes = bytes;
                faces[0].weight = 700;
                gf::AppFontLoad loaded = gf::load_app_font_family(title_family_name, faces);
                if (loaded.validation.status == gf::AppFontStatus::loaded)
                    holder = std::move(loaded.family);
            }
        }
        // A family quarantined or unloaded since draws nothing; the fallback stands then.
        if (holder.id() != gf::FontFamilyId{} && gf::font_family_drawable(holder.id())) {
            family_ = holder.id();
            state_ = CapsuleTitleFaceState::loaded;
        } else {
            state_ = CapsuleTitleFaceState::refused;
        }
    } catch (...) {
        state_ = CapsuleTitleFaceState::refused;
    }
#else
    state_ = CapsuleTitleFaceState::waiting;
#endif
    return state_;
}

gf::FontSpec CapsuleTitleFace::font(double size) const {
    gf::FontSpec spec{};
    spec.role = gf::FontRole::control;
    spec.weight = 700;
    if (state_ == CapsuleTitleFaceState::loaded) {
        spec.size = size;
        spec.family = family_;
    } else {
        // The bundled control face is wider than Barlow Condensed: a little smaller, drawn
        // tighter, so the folded capsule keeps its length.
        spec.size = size * .86;
        spec.letter_spacing = -.3;
    }
    return spec;
}

} // namespace games
