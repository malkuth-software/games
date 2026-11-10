#pragma once
// Held keys for the games that steer while a key is down (Zen Construction's crane,
// Eggy's walk, the mower in Mowing Man). The window keeps each physical key's state
// (SurfaceRoot::is_key_down: pressed and not yet released, every held key released
// when the window loses activation), so a game asks it instead of keeping its own
// table filled from key downs and ups.
#include "gui_forms/basic_controls.hpp"
#include "gui_forms/surface_root/surface_root.hpp"

#include <cstdint>

namespace games {
namespace gf = gui_forms;

// Whether physical key `key` (a gf::PhysicalKey, a USB HID usage) is held now for
// `view`: its window has it down and the focus is on `view` or inside it. A key held
// while another control has the focus (the shell's capsule, a settings sheet) does
// not steer the game, as it did not when the game kept its table from the key events
// only it received. False while `view` is detached.
[[nodiscard]] inline bool key_held(const gf::Control& view, std::uint32_t key) {
    const gf::SurfaceRoot* root = view.attached_surface_root();
    if (root == nullptr) {
        return false;
    }
    if (!(*root).is_key_down(key)) {
        return false;
    }
    const gf::Control::Ptr focused = (*root).focused_control();
    if (focused == nullptr) {
        return false;
    }
    const bool inside = focused.get() == &view || view.contains(*focused);
    return inside;
}

} // namespace games
