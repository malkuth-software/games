// games::key_held, the held-key query Zen Construction, Eggy and the ambient scenes
// (Mowing Man) steer with: a key is held while the window has it down and the focus is
// on the view or inside it; the window's release on deactivation lets it go.
#include "held_keys.hpp"
#include "gui_forms/window.hpp"

#include <cassert>
#include <iostream>
#include <memory>

namespace gf = gui_forms;

static void press(gf::Window& window, std::uint32_t physical, gf::KeyAction action) {
    gf::KeyEvent event{};
    event.action = action;
    event.physical_key = physical;
    static_cast<void>(window.dispatch_key(event));
}

int main() {
    const std::shared_ptr<gf::Panel> root = gf::make_control<gf::Panel>(gf::StableId("held.root"));
    const std::shared_ptr<gf::Panel> view = gf::make_control<gf::Panel>(gf::StableId("held.view"));
    const std::shared_ptr<gf::Button> inside = gf::make_control<gf::Button>(gf::StableId("held.inside"));
    const std::shared_ptr<gf::Button> outside = gf::make_control<gf::Button>(gf::StableId("held.outside"));
    (*view).add_child(inside);
    (*root).add_child(view);
    (*root).add_child(outside);
    gf::Window window(root, gf::Size{400, 300});
    window.set_active(true);

    // Detached controls hold nothing.
    const std::shared_ptr<gf::Panel> detached = gf::make_control<gf::Panel>(gf::StableId("held.detached"));
    assert(!games::key_held(*detached, gf::PhysicalKey::left));

    // Focus inside the view: a key is held from its down to its up, modifiers included.
    assert(window.request_focus(inside));
    press(window, gf::PhysicalKey::left, gf::KeyAction::down);
    press(window, gf::PhysicalKey::left_shift, gf::KeyAction::down);
    assert(games::key_held(*view, gf::PhysicalKey::left));
    assert(games::key_held(*view, gf::PhysicalKey::left_shift));
    assert(games::key_held(*inside, gf::PhysicalKey::left));
    assert(!games::key_held(*view, gf::PhysicalKey::right));
    press(window, gf::PhysicalKey::left, gf::KeyAction::up);
    assert(!games::key_held(*view, gf::PhysicalKey::left));
    assert(games::key_held(*view, gf::PhysicalKey::left_shift));

    // Three keys at once (Zen's crane: an arrow, a turn and Shift for fast).
    press(window, gf::PhysicalKey::up, gf::KeyAction::down);
    press(window, gf::PhysicalKey::q, gf::KeyAction::down);
    assert(games::key_held(*view, gf::PhysicalKey::up));
    assert(games::key_held(*view, gf::PhysicalKey::q));
    assert(games::key_held(*view, gf::PhysicalKey::left_shift));

    // Focus elsewhere: the window still has the keys down, but they do not steer the view.
    assert(window.request_focus(outside));
    assert(!games::key_held(*view, gf::PhysicalKey::up));
    assert(games::key_held(*outside, gf::PhysicalKey::up));

    // Deactivation releases every held key.
    assert(window.request_focus(inside));
    assert(games::key_held(*view, gf::PhysicalKey::up));
    window.set_active(false);
    assert(!games::key_held(*view, gf::PhysicalKey::up));
    assert(!games::key_held(*view, gf::PhysicalKey::q));
    assert(!games::key_held(*view, gf::PhysicalKey::left_shift));

    std::cout << "held keys: ok\n";
    return 0;
}
