#pragma once
// The capsule title's face: Barlow Condensed Bold (assets/fonts, OFL), loaded into GUI.Forms'
// text engine for the capsule composite only. The owner's partition (2026-11-15): an app may
// load fonts inside composites and subsurfaces, never into stock controls or GUI.Forms' own
// interface; PlaySuite's capsule is declared a composite.
//
// DEPENDENCY: GUI.Forms claude/app-fonts (the app-font loading API, being written). This
// header is the seam: the capsule asks for its title font here and nowhere else. Until that
// API is in the GUI.Forms PlaySuite builds against, GAMES_GUI_FORMS_APP_FONTS is undefined,
// load() reports `waiting`, and the title is drawn in the bundled control face (Portsmouth
// Rapids, bold, condensed by letter spacing), which is GUI.Forms' own fallback for a family
// that could not be loaded. When the API lands, only capsule_title_face.cpp changes.
#include "gui_forms/basic_controls.hpp"

#include <cstdint>

namespace gui_forms {
class CompositeControl;
}

namespace games {
namespace gf = gui_forms;

enum class CapsuleTitleFaceState : std::uint8_t {
    // Not asked yet.
    unloaded,
    // The app-font API is not in this GUI.Forms yet; the bundled fallback draws the title.
    waiting,
    // Loaded for the composite: font() names its family.
    loaded,
    // The face was refused (missing or rejected bytes); the bundled fallback draws the title.
    refused,
};

// One capsule's title face. Owned by the capsule, which outlives its use; the face is loaded
// for that composite instance and released with it.
class CapsuleTitleFace final {
  public:
    // Loads the face for `composite` (once; later calls return the first outcome). Reads
    // assets/fonts/BarlowCondensed-Bold.ttf from the asset directory. Never throws: a failure
    // is `refused` and the fallback stands.
    CapsuleTitleFaceState load(gf::CompositeControl& composite);
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
