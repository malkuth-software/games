#include "capsule_title_face.hpp"

#include "runtime_paths.hpp"

#include "gui_forms/composites/composite_control/composite_control.hpp"

#if defined(GAMES_GUI_FORMS_APP_FONTS)
// GUI.Forms claude/app-fonts. The names below follow its design as the coordinator relayed it
// (an app-loaded face is admitted for one composite or subsurface and released with it) and
// are to be matched to the landed header when the pin moves.
#include "gui_forms/typography/app_fonts/app_fonts.hpp"
#include <filesystem>
#endif

namespace games {

CapsuleTitleFaceState CapsuleTitleFace::load(gf::CompositeControl& composite) {
    if (state_ != CapsuleTitleFaceState::unloaded)
        return state_;
#if defined(GAMES_GUI_FORMS_APP_FONTS)
    try {
        const std::filesystem::path file =
            std::filesystem::path(asset_directory()) / "fonts" / "BarlowCondensed-Bold.ttf";
        const gf::AppFontLoad loaded = gf::load_app_font(composite, file);
        if (loaded) {
            family_ = loaded.family;
            state_ = CapsuleTitleFaceState::loaded;
        } else {
            state_ = CapsuleTitleFaceState::refused;
        }
    } catch (...) {
        state_ = CapsuleTitleFaceState::refused;
    }
#else
    static_cast<void>(composite);
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
