// The Solitaire deal, frame by frame, headless (the owner, 2026-11-17: "solitaire animation
// simply didnt run right"). New game starts the deal; the test then steps the window's frame
// schedule as a host would (poll at each wake, paint, sleep to the next wake) and records, for
// every frame, its time from the command, the next wake the window asks for, whether it had
// damage, and where every card was drawn. The deal's intended sequence is the table's own
// (shared/cards/table.cpp, layout_cards and tick): Solitaire's 28 tableau cards leave the top
// middle of the table one after another, card k (row i, column p: k = 7 i + p) after k x 14 ms,
// each flying 440 ms; the stock's two visible cards follow theirs (index x 14 ms). The test
// checks the seams that sequence depends on: Timer and the UI-timer scheduler (a wake at least
// every 20 ms while cards move), next_wake (absent or far once the deal has landed), damage and
// paint (a damaged frame for every tick, the moving cards drawn where they fly), and that the
// command returns without holding the UI thread. Each frame is printed, so a failure shows
// how the deal went.
#include "gui_forms/window.hpp"
#include "storage.hpp"
#include "table.hpp"
#include "test_paths.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
namespace gf = gui_forms;

void require(bool condition, const char* message) {
    if (!condition)
        throw std::runtime_error(message);
}

// Records every image drawn in one paint; nothing is rasterised.
class DealRecorder final : public gf::Painter {
  public:
    std::vector<gf::Rect> images;
    void save() override {}
    void restore() override {}
    void translate(gf::Point) override {}
    void clip_rect(gf::Rect) override {}
    void fill_rect(gf::Rect, gf::Color) override {}
    void stroke_rect(gf::Rect, gf::Color, double) override {}
    void draw_line(gf::Point, gf::Point, gf::Color, double) override {}
    void draw_text_utf8(gf::Point, std::string_view, gf::FontSpec, gf::Color) override {}
    void draw_image(gf::ImageId, gf::Rect destination, double) override {
        images.push_back(destination);
    }
};

struct Frame {
    double seconds = 0;          // from the New game command
    double wake_in = -1;         // the next wake the window asked for, from this frame; -1 none
    bool damaged = false;        // the frame had damage to paint
    double paint_seconds = 0;    // the poll, damage and whole paint of this frame
    std::vector<gf::Point> cards; // the centre of every card drawn
};

gf::Point centre(gf::Rect r) {
    const gf::Point result{r.x + r.width * .5, r.y + r.height * .5};
    return result;
}

// The cards among the drawn images: a card's height is the card height, up to 7.5 % more while
// it rises in the air (a turning card is narrower, never shorter). A card's layers (its face,
// its finish) share one rectangle and count once; its shadow is taller (584 for a card's 504)
// and the room's light is the whole table.
std::vector<gf::Point> card_centres(const std::vector<gf::Rect>& images, double card_height) {
    std::vector<gf::Point> result;
    for (const gf::Rect& image : images) {
        const bool card = image.height >= card_height * .99 && image.height <= card_height * 1.075;
        if (!card)
            continue;
        const gf::Point at = centre(image);
        bool seen = false;
        for (const gf::Point& other : result)
            seen = seen || (std::abs(other.x - at.x) < .5 && std::abs(other.y - at.y) < .5);
        if (!seen)
            result.push_back(at);
    }
    return result;
}

bool near(gf::Point a, gf::Point b, double tolerance) {
    const bool result = std::abs(a.x - b.x) <= tolerance && std::abs(a.y - b.y) <= tolerance;
    return result;
}

// The card height on a settled table: the smallest height drawn at least 20 times (each card
// and its shadow once, the shadow taller; nothing else is drawn that often).
double settled_card_height(const std::vector<gf::Rect>& images) {
    double best = 0;
    for (const gf::Rect& candidate : images) {
        int count = 0;
        for (const gf::Rect& other : images)
            if (std::abs(other.height - candidate.height) < .5)
                ++count;
        const bool smaller = best == 0 || candidate.height < best;
        if (count >= 20 && candidate.height > 20 && smaller)
            best = candidate.height;
    }
    return best;
}

// The deal's intended landings at `seconds` after it started: the cards whose delay and flight
// are over.
int intended_landed(double seconds) {
    int landed = 0;
    for (int k = 0; k < 28; ++k)
        if (k * .014 + .44 <= seconds)
            ++landed;
    for (int index = 22; index < 24; ++index)
        if (index * .014 + .44 <= seconds)
            ++landed;
    return landed;
}

void paint_whole(gf::Window& window, DealRecorder& recorder, gf::Size size) {
    recorder.images.clear();
    static_cast<void>(window.paint(recorder, gf::Rect{0, 0, size.width, size.height}));
}

} // namespace

int main() {
    try {
        const std::filesystem::path scratch = games_test::scratch_directory("solitaire-deal-");
        games_test::isolate_saves(scratch);
        games::Cabinet preferences;
        preferences.games[static_cast<int>(games::Kind::solitaire)].deal(games::Kind::solitaire, 42);
        preferences.started[static_cast<int>(games::Kind::solitaire)] = true;
        preferences.active = static_cast<int>(games::Kind::solitaire);
        preferences.sound = preferences.music = false;
        preferences.reduced = false;
        require(games::save_cabinet(games::cabinet_path(), preferences), "Save test preferences");

        const gf::Size size{1060, 720};
        std::shared_ptr<games::Table> table = gf::make_control<games::Table>(gf::StableId("deal.table"));
        gf::Window window(table, size);
        window.perform_layout();
        (*table).show_kind(games::Kind::solitaire);
        (*table).activate();
        DealRecorder recorder;
        // Let the opening settle (the saved deal laid out and printed).
        const gf::FrameTime opened = gf::FrameClock::now();
        while (gf::FrameClock::now() - opened < std::chrono::milliseconds(1500)) {
            static_cast<void>(window.poll_frame_schedule(gf::FrameClock::now()));
            paint_whole(window, recorder, size);
            static_cast<void>(window.take_damage());
            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        // New game: the command itself must not hold the UI thread (the deal's clock starts in
        // it, so a long hold would land the first cards before a frame shows them).
        const gf::FrameTime started = gf::FrameClock::now();
        (*table).run_command("new");
        const double command_seconds = std::chrono::duration<double>(gf::FrameClock::now() - started).count();
        std::printf("new game command: %.1f ms\n", command_seconds * 1000);

        // Step the schedule as a host would, for 1.6 s. Each frame's drawn images are kept as
        // rectangles until the settled frame gives the card height.
        std::vector<Frame> frames;
        std::vector<std::vector<gf::Rect>> drawn;
        frames.reserve(512);
        drawn.reserve(512);
        while (gf::FrameClock::now() - started < std::chrono::milliseconds(1600)) {
            const gf::FrameTime now = gf::FrameClock::now();
            static_cast<void>(window.poll_frame_schedule(now));
            Frame frame;
            frame.seconds = std::chrono::duration<double>(now - started).count();
            frame.damaged = !window.take_damage().empty();
            paint_whole(window, recorder, size);
            frame.paint_seconds = std::chrono::duration<double>(gf::FrameClock::now() - now).count();
            const stxlib::Optional<gf::FrameTime> wake = window.next_wake();
            if (wake.present())
                frame.wake_in = std::chrono::duration<double>(*wake.value() - now).count();
            drawn.push_back(recorder.images);
            frames.push_back(frame);
            gf::FrameTime next = now + std::chrono::milliseconds(50);
            if (wake.present() && *wake.value() < next)
                next = *wake.value();
            std::this_thread::sleep_until(next);
        }
        const double height = settled_card_height(drawn.back());
        require(height > 0, "The settled table draws its cards");
        for (std::size_t index = 0; index < frames.size(); ++index)
            frames[index].cards = card_centres(drawn[index], height);

        const std::vector<gf::Point>& landed_places = frames.back().cards;
        require(landed_places.size() >= 28, "The settled deal shows the tableau and the stock");
        // A card waiting for its turn lies at the deal's start, its top at 86 in the middle.
        const double dealt_from_x = size.width * .5;
        int in_flight_frames = 0;
        int damaged_deal_frames = 0;
        double longest_wake = 0;
        double longest_gap = 0;
        double previous_seconds = 0;
        bool order_kept = true;
        for (const Frame& frame : frames) {
            int landed = 0;
            int in_flight = 0;
            for (const gf::Point& card : frame.cards) {
                bool at_place = false;
                for (const gf::Point& place : landed_places)
                    at_place = at_place || near(card, place, 1.0);
                const bool at_start = std::abs(card.x - dealt_from_x) <= 1.0 && card.y < 86 + height;
                if (at_place)
                    ++landed;
                else if (!at_start)
                    ++in_flight;
            }
            const int intended = intended_landed(frame.seconds);
            const int earliest = intended_landed(frame.seconds - .06);
            const int latest = intended_landed(frame.seconds + .06);
            std::printf("t %7.1f ms  paint %5.1f ms  wake %6.1f ms  damage %d  cards %2zu  landed %2d (intended %2d)  "
                        "flying %2d\n",
                        frame.seconds * 1000, frame.paint_seconds * 1000, frame.wake_in * 1000, frame.damaged ? 1 : 0,
                        frame.cards.size(), landed, intended, in_flight);
            const bool dealing = frame.seconds < .85;
            if (dealing) {
                if (in_flight > 0)
                    ++in_flight_frames;
                if (frame.damaged)
                    ++damaged_deal_frames;
                longest_wake = std::max(longest_wake, frame.wake_in < 0 ? 1.0 : frame.wake_in);
                longest_gap = std::max(longest_gap, frame.seconds - previous_seconds);
                // Allow two cards either way (a card hidden under another is not drawn).
                if (landed + 2 < earliest || landed > latest + 2)
                    order_kept = false;
            }
            previous_seconds = frame.seconds;
        }
        require(command_seconds < .1, "New game returns within 100 ms");
        require(longest_wake <= .02, "While the deal flies the table wakes at least every 20 ms");
        require(longest_gap <= .04, "No two deal frames are more than 40 ms apart");
        require(damaged_deal_frames >= 25, "The deal paints at least 25 damaged frames");
        require(in_flight_frames >= 20, "Cards are seen in flight in at least 20 frames");
        require(order_kept, "Cards land in the deal's order and time (within 60 ms and two cards)");
        const Frame& last = frames.back();
        require(last.wake_in < 0 || last.wake_in >= .1, "The landed deal stops ticking");
        std::cout << "solitaire deal: " << frames.size() << " frames, " << in_flight_frames
                  << " with cards in flight\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
