#pragma once
// The capsule title's face: Barlow Condensed Bold (assets/fonts, OFL), loaded into GUI.Forms'
// text engine as an application font family (gui_forms/typography/app_fonts). The owner's
// partition (2026-11-15): an app's families draw inside composites and subsurfaces only;
// GUI.Forms admits them by the control's context (gf::admit_font), so the capsule composite's
// title label draws the family and a stock control given the same font would not.
//
// DEPENDENCY: GUI.Forms claude/app-fonts (2558e326c, being folded). The CMake option
// GAMES_GUI_FORMS_APP_FONTS (ON by default) compiles the load; a GUI.Forms without the API
// needs it OFF, and the title is then drawn in the bundled control face, which is also what
// GUI.Forms draws when the family is refused, quarantined or unloaded.
#include "gui_forms/basic_controls.hpp"

#include <cstdint>

namespace games {
namespace gf = gui_forms;

enum class CapsuleTitleFaceState : std::uint8_t {
    // Not asked yet.
    unloaded,
    // Built without the app-font API (GAMES_GUI_FORMS_APP_FONTS off): the bundled fallback.
    waiting,
    // Loaded: font() names the family.
    loaded,
    // The bytes were missing or refused (gf::AppFontStatus): the bundled fallback.
    refused,
};

// One capsule's view of the title face. The family itself is loaded once per process and
// held for the process (application families are named, and a name loads once); every capsule
// shares it.
class CapsuleTitleFace final {
  public:
    // Loads the family the first time any capsule asks (later calls reuse it). Reads
    // assets/fonts/BarlowCondensed-Bold.ttf from the asset directory. Never throws: a failure
    // is `refused` and the fallback stands.
    CapsuleTitleFaceState load();
    [[nodiscard]] CapsuleTitleFaceState state() const noexcept {
        return state_;
    }
    // The title's font at `size` points: the loaded family first, the control role's bundled
    // faces after it (and alone when the face is not loaded).
    [[nodiscard]] gf::FontSpec font(double size) const;

  private:
    CapsuleTitleFaceState state_ = CapsuleTitleFaceState::unloaded;
    gf::FontFamilyId family_{};
};

} // namespace games
