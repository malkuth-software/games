#include "fourpegs_view.hpp"
#include "test_paths.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

namespace gf = gui_forms;
namespace {
void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}
void pointer(gf::Window& window, gf::Control& view, gf::PointerAction action, gf::Point local,
             std::uint32_t clicks = 1) {
    gf::PointerEvent event;
    event.action = action;
    event.button = gf::PointerButton::primary;
    event.position = view.point_to_window(local);
    event.click_count = action == gf::PointerAction::move ? 0 : clicks;
    static_cast<void>(window.dispatch_pointer(event));
}
void click(gf::Window& window, gf::Control& view, gf::Point local, std::uint32_t clicks = 1) {
    pointer(window, view, gf::PointerAction::down, local, clicks);
    pointer(window, view, gf::PointerAction::up, local, clicks);
}
int placed(const fp::FourPegsView& view) {
    int count = 0;
    for (int i = 0; i < fp::kPegs; ++i)
        if (view.board().draft()[static_cast<std::size_t>(i)] >= 0)
            ++count;
    return count;
}
gf::Point projected(const fp::Lair& scene, fp::V3 location) {
    double x = 0, y = 0;
    scene.to_screen(location + fp::V3{0, 0, .12}, x, y);
    return {x * 2, y * 2};
}
}
int main() {
    try {
        games_test::isolate_saves(games_test::scratch_directory("fourpegs-pointer-"));
        std::shared_ptr<fp::FourPegsView> view = gf::make_control<fp::FourPegsView>(
            gf::StableId("pegs"), fp::Options{.dev = true, .hosted = true});
        gf::Window window(view, {1000, 660});
        window.perform_layout();
        (*view).set_cabinet(true, false, false, true);
        fp::Lair scene;
        scene.resize(500, 330);
        scene.r.ax = .42;
        scene.r.set_camera();
        click(window, *view, projected(scene, fp::Lair::palette(0)));
        require((*view).board().draft()[0] == 0, "Palette click places an orb through real pointer routing");
        pointer(window, *view, gf::PointerAction::down, projected(scene, fp::Lair::palette(1)));
        pointer(window, *view, gf::PointerAction::move, projected(scene, fp::Lair::socket(1)));
        pointer(window, *view, gf::PointerAction::up, projected(scene, fp::Lair::socket(1)));
        require((*view).board().draft()[1] == 1, "Drag commits after normal capture release");
        click(window, *view, projected(scene, fp::Lair::palette(2)));
        click(window, *view, projected(scene, fp::Lair::socket(3)));
        require((*view).board().draft()[3] == 2 && (*view).board().draft()[2] < 0,
                "Palette then socket retargets the selected orb");
        click(window, *view, projected(scene, fp::Lair::socket(2)));
        click(window, *view, projected(scene, fp::Lair::palette(3)));
        require((*view).board().draft()[2] == 3, "Socket then palette places in the chosen socket");
        click(window, *view, projected(scene, fp::Lair::check_button()));
        require((*view).board().turns_used() == 1, "Check button submits the complete mouse-entered guess");
        // The console's clicks are GUI.Forms' derived clicks: a quick second press is another
        // click (the view raises no double clicks), a drop is no click, and a capture lost
        // mid-drag cancels the drag. The guess above is still being judged (no peg goes in
        // while it is), so these run on a fresh console with its own empty saves.
        games_test::isolate_saves(games_test::scratch_directory("fourpegs-pointer-fresh-"));
        std::shared_ptr<fp::FourPegsView> fresh = gf::make_control<fp::FourPegsView>(
            gf::StableId("pegs-fresh"), fp::Options{.dev = true, .hosted = true});
        gf::Window fresh_window(fresh, {1000, 660});
        fresh_window.perform_layout();
        (*fresh).set_cabinet(true, false, false, true);
        require(placed(*fresh) == 0, "A fresh console starts with an empty guess");
        click(fresh_window, *fresh, projected(scene, fp::Lair::palette(4)), 1);
        click(fresh_window, *fresh, projected(scene, fp::Lair::palette(5)), 2);
        require(placed(*fresh) == 2, "A quick second click on the palette places a second orb");
        pointer(fresh_window, *fresh, gf::PointerAction::down, projected(scene, fp::Lair::palette(0)));
        pointer(fresh_window, *fresh, gf::PointerAction::move, projected(scene, fp::Lair::socket(3)));
        (*fresh).set_pointer_capture(false);
        pointer(fresh_window, *fresh, gf::PointerAction::up, projected(scene, fp::Lair::socket(3)));
        require(placed(*fresh) == 2, "A drag whose capture was lost drops nothing");
        require(!(*fresh).has_pointer_capture(), "The cancelled drag holds no capture");
        std::cout << "Four Pegs pointer routing passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
