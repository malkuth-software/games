// The views' input on GUI.Forms' raisers, through real window dispatch: a card table's
// double click is the derived double click (its second press only notes it, the release
// places the card), a drag's release suppresses the click, a press whose release lands off
// the view raises no click, Escape ends a card drag (GUI.Forms' DragTracker), and a key bubbling from a child reaches the view's own key
// handling with its fields intact (gf::key_event_arguments, public in GUI.Forms cp33).
#include "gui_forms/window.hpp"
#include "gui_forms/events/key_events/key_events.hpp"
#include "table.hpp"
#include "test_paths.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace gf = gui_forms;
namespace {

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

void pointer(gf::Window& window, gf::PointerAction action, gf::Point point, std::uint32_t clicks) {
    gf::PointerEvent event;
    event.action = action;
    event.button = gf::PointerButton::primary;
    event.position = point;
    event.click_count = clicks;
    static_cast<void>(window.dispatch_pointer(event));
}

// Moves the card of `rank` and `suit` to the top of `destination`, keeping every card.
void expose(games::Table& table, int rank, int suit, int destination) {
    for (games::Pile& pile : table.game.state.piles)
        for (games::Card& card : pile)
            if (card.rank == rank && card.suit == suit) {
                std::swap(card, table.game.state.piles[destination].back());
                return;
            }
}

int foundation_cards(const games::Table& table) {
    int count = 0;
    for (int pile = 10; pile < 14; ++pile)
        count += static_cast<int>(table.game.state.piles[pile].size());
    return count;
}

void double_click_places_on_release() {
    std::shared_ptr<games::Table> table = gf::make_control<games::Table>(gf::StableId("test.table"));
    (*table).game.deal(games::Kind::freecell, 42);
    std::unique_ptr<gf::Window> window = std::make_unique<gf::Window>(table, gf::Size{1180, 800});
    (*window).perform_layout();
    expose(*table, 1, 0, 0);
    (*table).arrange({0, 0, 1180, 800});
    const int ace = (*table).game.state.piles[0].back().id;
    const gf::Point column{80, 500};
    pointer(*window, gf::PointerAction::down, column, 1);
    pointer(*window, gf::PointerAction::up, column, 1);
    require(foundation_cards(*table) == 0, "a single click only selects the ace");
    pointer(*window, gf::PointerAction::down, column, 2);
    require(foundation_cards(*table) == 0, "the second press of a double click places nothing yet");
    pointer(*window, gf::PointerAction::up, column, 2);
    require(foundation_cards(*table) == 1, "the derived double click places the ace on a foundation");
    bool found = false;
    for (int pile = 10; pile < 14; ++pile)
        found = found || (!(*table).game.state.piles[pile].empty() && (*table).game.state.piles[pile].back().id == ace);
    require(found, "the ace placed is the one double-clicked");
    require((*table).game.state.moves == 1, "the double click is one move");
    require((*table).game.invariant(), "double click keeps every card");

    // A double click whose second release lands off the table raises no double click.
    require((*table).game.undo(), "undo the double click");
    (*table).arrange({0, 0, 1180, 800});
    pointer(*window, gf::PointerAction::down, column, 1);
    pointer(*window, gf::PointerAction::up, column, 1);
    pointer(*window, gf::PointerAction::down, column, 2);
    pointer(*window, gf::PointerAction::up, {-40, -40}, 2);
    require(foundation_cards(*table) == 0, "a second release off the view places nothing");
}

void drag_release_is_not_a_click() {
    std::shared_ptr<games::Table> table = gf::make_control<games::Table>(gf::StableId("test.table.drag"));
    (*table).game.deal(games::Kind::freecell, 42);
    std::unique_ptr<gf::Window> window = std::make_unique<gf::Window>(table, gf::Size{1180, 800});
    (*window).perform_layout();
    // Select column zero's card with a click, then drag column one's card to a free cell:
    // the drop is one move, and its release completes no click-to-place.
    pointer(*window, gf::PointerAction::down, {80, 500}, 1);
    pointer(*window, gf::PointerAction::up, {80, 500}, 1);
    pointer(*window, gf::PointerAction::down, {220, 500}, 1);
    pointer(*window, gf::PointerAction::move, {80, 180}, 0);
    pointer(*window, gf::PointerAction::up, {80, 180}, 1);
    require((*table).game.state.moves == 1, "a drag's release is one move and no click");
    require(!(*table).has_pointer_capture(), "the drag released the capture");
}

// The card drag is a GUI.Forms DragTracker session: Escape ends it (the window routes the key
// to the drag engine first, before the table's own Escape opens its panel), no move is made,
// and the release that follows is no click.
void escape_ends_a_card_drag() {
    std::shared_ptr<games::Table> table = gf::make_control<games::Table>(gf::StableId("test.table.escape"));
    (*table).game.deal(games::Kind::freecell, 42);
    std::unique_ptr<gf::Window> window = std::make_unique<gf::Window>(table, gf::Size{1180, 800});
    (*window).perform_layout();
    (*window).set_active(true);
    pointer(*window, gf::PointerAction::down, {220, 500}, 1);
    pointer(*window, gf::PointerAction::move, {80, 180}, 0);
    gf::KeyEvent escape{};
    escape.action = gf::KeyAction::down;
    escape.physical_key = gf::PhysicalKey::escape;
    static_cast<void>((*window).dispatch_key(escape));
    pointer(*window, gf::PointerAction::up, {80, 180}, 1);
    require((*table).game.state.moves == 0, "Escape mid-drag makes no move");
    require(!(*table).has_pointer_capture(), "the ended drag holds no capture");
}

void bubbled_key_keeps_its_fields() {
    gf::KeyEvent event{};
    event.action = gf::KeyAction::down;
    event.physical_key = gf::PhysicalKey::escape;
    event.modifiers = gf::Modifier::shift;
    event.repeat = true;
    event.scan_code = 9;
    const gf::KeyEventArgs arguments = gf::key_event_arguments(event);
    require(arguments.physical_key == gf::PhysicalKey::escape, "physical key carried");
    require(arguments.key_code == gf::key_code_from_physical_key(gf::PhysicalKey::escape), "key code derived");
    require(arguments.modifiers == gf::Modifier::shift && arguments.repeat && arguments.scan_code == 9,
            "modifiers, repeat and scan code carried");
    require(!arguments.handled, "an unhandled key stays unhandled");
}

} // namespace

int main() {
    try {
        games_test::isolate_saves(games_test::scratch_directory("games-view-input-"));
        double_click_places_on_release();
        drag_release_is_not_a_click();
        escape_ends_a_card_drag();
        bubbled_key_keeps_its_fields();
        std::cout << "view_input_tests: ok\n";
    } catch (const std::exception& error) {
        std::cerr << "view_input_tests: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
