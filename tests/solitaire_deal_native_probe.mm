// The Solitaire deal in a real AppKit window (the owner, 2026-11-17: "solitaire animation
// simply didnt run right"; the headless solitaire_deal test runs as designed, so this probe
// looks at what the window itself presents). PlaySuite's own window (forms/playsuite.form and
// its theme, as src/main.cpp makes it) opens on Solitaire; after it settles, New game deals and
// the macOS host's draw trace records every drawRect: of the deal: its start, its duration
// (paint and copy into AppKit's backing store), its dirty rectangle and whether it presented.
// GUI.Forms' window metrics over the deal (presents, present and screen-copy time, painted
// damage, fired and coalesced frame requests) are printed beside it, and the host's own JSON
// (hostJSON: draw counts and the paint/copy phase timings) is printed when the window closes.
//
// Judged (exit status 1 on failure):
//   - the New game command returns within 100 ms (the deal's clock starts inside it);
//   - no gap between presented draws exceeds 33 ms while the cards fly (the deal's first
//     1112 ms: row i of column p leaves after (7 i + p) x 14 ms and flies 440 ms);
//   - every card's flight spans at least 8 presented draws (a 60 Hz window shows about 26).
// The window's backing scale must be 2 (the owner's displays); at another scale the probe
// reports and exits 77 (skipped) unless PLAYSUITE_DEAL_ANY_SCALE is set.
//
// With PLAYSUITE_DEAL_CAPTURE_DIR set, a second deal saves every 3rd presented frame (at most
// 20) as <dir>/frame_NNNN.png; that pass's draws include PNG encoding and are not judged.
//
// The draw trace is the host view's private probe interface (GUI.Forms
// tests/macos_live_resize_probe.mm uses it the same way): the view is found by the selector
// and the record's layout is GUI.Forms src/host/macos/application/display_link_trace.hpp's
// MacDrawRecord at 31b48906e (unchanged since 2cc50c3df), which is not installed with the SDK.
// The probe's own 50 ms step timer wakes the window but damages nothing.
#import <AppKit/AppKit.h>

#include "collection.hpp"
#include "gui_forms/application.hpp"
#include "playsuite_form.hpp"   // generated from forms/playsuite.form
#include "playsuite_theme.hpp"  // generated from themes/playsuite
#include "runtime_paths.hpp"
#include "test_paths.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// GUI.Forms' MacDrawRecord (display_link_trace.hpp at 31b48906e), field for field: one
// drawRect: of a view whose draw trace is enabled. Times are steady-clock nanoseconds; sizes
// and the dirty rectangle are logical view units, origin top left.
struct MacDrawRecord final {
    std::uint64_t started_ns{0};
    // From the start of drawRetainedRect: to the end of the copy into AppKit's backing store.
    std::uint64_t duration_ns{0};
    double width{0.0};
    double height{0.0};
    double dirty_x{0.0};
    double dirty_y{0.0};
    double dirty_width{0.0};
    double dirty_height{0.0};
    bool presented{false};
    bool live_resize{false};
};

@interface NSView (SolitaireDealProbe)
- (void)setDrawTraceEnabled:(BOOL)enabled
           captureDirectory:(std::string)directory
                     stride:(std::size_t)stride
                      limit:(std::size_t)limit;
- (BOOL)readDrawTrace:(std::vector<MacDrawRecord>&)records;
@end

namespace {
namespace gf = gui_forms;

constexpr const char* probe_title = "PlaySuite Solitaire deal probe";
// The deal's flights (shared/cards/table.cpp, layout_cards): 28 tableau cards and the stock's
// two visible ones, each 440 ms.
constexpr double flight_seconds = .44;
constexpr double card_step_seconds = .014;
constexpr double deal_seconds = (6 * 7 + 6) * card_step_seconds + flight_seconds;  // 1.112
constexpr double gap_limit_seconds = .033;
constexpr std::size_t draws_per_flight = 8;
constexpr double command_limit_seconds = .1;

enum class Phase : std::uint8_t { settling, dealing, capturing, done };

// The run's state, owned by main through a shared pointer for the whole run; read and written
// on the main thread only (ready and the step timer's ticks).
struct ProbeState final {
    std::weak_ptr<games::Collection> collection;
    gf::Window* window = nullptr;
    gf::ApplicationWindowHandle handle;
    gf::FrameRequestToken timer{};
    Phase phase = Phase::settling;
    std::chrono::steady_clock::time_point opened{};
    // The New game command's return: the deal's clock started inside the command, at most
    // command_seconds before.
    std::chrono::steady_clock::time_point dealt{};
    double command_seconds = 0;
    std::string capture_directory;
    std::vector<MacDrawRecord> records;
    std::vector<std::string> failures;
    bool skipped = false;
    bool judged = false;
};

NSView* find_host_view(NSView* view) {
    if (view == nil)
        return nil;
    if ([view respondsToSelector:@selector(readDrawTrace:)])
        return view;
    for (NSView* child in [view subviews]) {
        NSView* found = find_host_view(child);
        if (found != nil)
            return found;
    }
    return nil;
}

NSView* probe_view() {
    NSString* title = [NSString stringWithUTF8String:probe_title];
    for (NSWindow* candidate in [NSApp windows])
        if ([[candidate title] isEqualToString:title])
            return find_host_view([candidate contentView]);
    return nil;
}

double seconds_between(std::uint64_t later_ns, std::uint64_t earlier_ns) {
    const double result = (static_cast<double>(later_ns) - static_cast<double>(earlier_ns)) * 1e-9;
    return result;
}

std::uint64_t steady_nanoseconds(std::chrono::steady_clock::time_point at) {
    const std::uint64_t result = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(at.time_since_epoch()).count());
    return result;
}

// The flights' start delays from the deal's start: tableau row i of column p after
// (7 i + p) steps, the stock's visible cards (indexes 22 and 23) after their index.
std::vector<double> flight_delays() {
    std::vector<double> delays;
    delays.reserve(30);
    for (int column = 0; column < 7; ++column)
        for (int row = 0; row <= column; ++row)
            delays.push_back((row * 7 + column) * card_step_seconds);
    delays.push_back(22 * card_step_seconds);
    delays.push_back(23 * card_step_seconds);
    return delays;
}

// Prints the deal's presented draws and judges them; failures are appended to the state.
void judge(ProbeState& state, const gf::MetricsSnapshot& metrics, bool overflow) {
    const std::uint64_t origin = steady_nanoseconds(state.dealt);
    std::vector<std::uint64_t> presents;
    presents.reserve(state.records.size());
    std::printf("deal draws (t from the New game command's return):\n");
    for (const MacDrawRecord& record : state.records) {
        const double at = seconds_between(record.started_ns, origin);
        std::printf("draw t %8.1f ms  took %6.2f ms  dirty %6.0f x %-6.0f at %6.0f,%-6.0f  presented %d\n",
                    at * 1000, static_cast<double>(record.duration_ns) * 1e-6, record.dirty_width,
                    record.dirty_height, record.dirty_x, record.dirty_y, record.presented ? 1 : 0);
        if (record.presented)
            presents.push_back(record.started_ns);
    }
    const double presented_frames = static_cast<double>(metrics.frames_presented);
    const double per_frame = presented_frames > 0 ? 1e-6 / presented_frames : 0;
    std::printf("metrics over the deal: frames_presented %llu  present %.2f ms/frame (worst %.2f ms)  "
                "screen copy %.2f ms/frame  painted damage %.0f  requested damage %.0f  deadlines fired %llu  "
                "requests coalesced %llu  scheduled requests %llu  scale %.2f  overflow %d\n",
                static_cast<unsigned long long>(metrics.frames_presented),
                static_cast<double>(metrics.present_duration_nanoseconds) * per_frame,
                static_cast<double>(metrics.worst_present_duration_nanoseconds) * 1e-6,
                static_cast<double>(metrics.screen_copy_nanoseconds) * per_frame, metrics.painted_damage_area,
                metrics.requested_damage_area, static_cast<unsigned long long>(metrics.frame_deadlines_fired),
                static_cast<unsigned long long>(metrics.frame_requests_coalesced),
                static_cast<unsigned long long>(metrics.scheduled_frame_requests), (*state.window).scale(),
                overflow ? 1 : 0);
    if (state.command_seconds >= command_limit_seconds)
        state.failures.push_back("New game held the UI thread " + std::to_string(state.command_seconds * 1000) +
                                 " ms");
    // Gaps while the cards fly, the command's return counting as the first frame.
    const std::uint64_t deal_end = origin + static_cast<std::uint64_t>(deal_seconds * 1e9);
    std::uint64_t previous = origin;
    double longest = 0;
    for (const std::uint64_t present : presents) {
        if (present < origin || present > deal_end)
            continue;
        longest = std::max(longest, seconds_between(present, previous));
        previous = present;
    }
    longest = std::max(longest, seconds_between(deal_end, previous));
    std::printf("longest gap between presented draws while the cards fly: %.1f ms\n", longest * 1000);
    if (longest > gap_limit_seconds)
        state.failures.push_back("A gap of " + std::to_string(longest * 1000) + " ms between presented draws");
    // Every flight across several presented draws.
    const std::vector<double> delays = flight_delays();
    std::size_t fewest = presents.size();
    for (std::size_t card = 0; card < delays.size(); ++card) {
        const std::uint64_t from = origin + static_cast<std::uint64_t>(delays[card] * 1e9);
        const std::uint64_t to = from + static_cast<std::uint64_t>(flight_seconds * 1e9);
        std::size_t shown = 0;
        for (const std::uint64_t present : presents)
            if (present >= from && present <= to)
                ++shown;
        fewest = std::min(fewest, shown);
        if (shown < draws_per_flight)
            state.failures.push_back("Card flight " + std::to_string(card) + " (leaving at " +
                                     std::to_string(delays[card] * 1000) + " ms) spans only " +
                                     std::to_string(shown) + " presented draws");
    }
    std::printf("fewest presented draws in one card's flight: %zu (of %zu flights)\n", fewest, delays.size());
    state.judged = true;
}

void finish(ProbeState& state) {
    state.phase = Phase::done;
    static_cast<void>(state.handle.request_close());
}

// The probe's steps, every 50 ms on the main thread.
struct ProbeTick final {
    std::weak_ptr<ProbeState> weak;
    void operator()(gf::FrameTime) const {
        const std::shared_ptr<ProbeState> held = weak.lock();
        if (!held)
            return;
        ProbeState& state = *held;
        const std::shared_ptr<games::Collection> collection = state.collection.lock();
        if (!collection || state.phase == Phase::done)
            return;
        const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
        NSView* view = probe_view();
        if (view == nil) {
            state.failures.push_back("The probe found no GUI.Forms host view with a draw trace");
            finish(state);
            return;
        }
        if (state.phase == Phase::settling) {
            if (now - state.opened < std::chrono::milliseconds(2500))
                return;
            const double scale = (*state.window).scale();
            if (std::abs(scale - 2.0) > .01 && std::getenv("PLAYSUITE_DEAL_ANY_SCALE") == nullptr) {
                std::printf("backing scale %.2f, not 2: skipped (PLAYSUITE_DEAL_ANY_SCALE runs it anyway)\n", scale);
                state.skipped = true;
                finish(state);
                return;
            }
            [view setDrawTraceEnabled:YES captureDirectory:std::string() stride:1 limit:0];
            (*state.window).reset_activity_metrics();
            const std::chrono::steady_clock::time_point command = std::chrono::steady_clock::now();
            (*collection).dispatch_command("new");
            state.dealt = std::chrono::steady_clock::now();
            state.command_seconds = std::chrono::duration<double>(state.dealt - command).count();
            state.phase = Phase::dealing;
            return;
        }
        if (now - state.dealt < std::chrono::milliseconds(1600))
            return;
        if (state.phase == Phase::dealing) {
            const BOOL overflow = [view readDrawTrace:state.records];
            const gf::MetricsSnapshot metrics = (*state.window).metrics_snapshot();
            [view setDrawTraceEnabled:NO captureDirectory:std::string() stride:1 limit:0];
            judge(state, metrics, overflow == YES);
            if (state.capture_directory.empty()) {
                finish(state);
                return;
            }
            std::filesystem::create_directories(state.capture_directory);
            [view setDrawTraceEnabled:YES captureDirectory:state.capture_directory + "/" stride:3 limit:20];
            (*collection).dispatch_command("new");
            state.dealt = std::chrono::steady_clock::now();
            state.phase = Phase::capturing;
            return;
        }
        [view setDrawTraceEnabled:NO captureDirectory:std::string() stride:1 limit:0];
        std::printf("captured frames of a second deal in %s\n", state.capture_directory.c_str());
        finish(state);
    }
};

struct ProbeReady final {
    std::shared_ptr<ProbeState> state;
    void operator()(gf::Window& window, gf::ApplicationWindowHandle handle) const {
        ProbeState& live = *state;
        const std::shared_ptr<games::Collection> collection = live.collection.lock();
        if (!collection)
            return;
        live.window = &window;
        live.handle = handle;
        (*collection).open_entry(games::Entry::solitaire);
        (*collection).activate();
        static_cast<void>(window.request_focus(collection));
        live.opened = std::chrono::steady_clock::now();
        const std::chrono::milliseconds interval(50);
        live.timer = window.schedule_ui_timer(*collection, interval, gf::FrameClock::now() + interval,
                                              ProbeTick{state});
    }
};

} // namespace

int main(int argc, char** argv) {
    try {
        if (argc < 1)
            return 1;
        const std::filesystem::path scratch = games_test::scratch_directory("solitaire-deal-native-");
        games_test::isolate_saves(scratch);
        games::initialize_assets(argv[0]);
        gf::ApplicationConfig config{};
        config.theme.base = gf::ThemeBase::windows_professional;
        config.theme.packages.push_back(games::playsuite_theme());
        gf::ApplicationCreateResult created = gf::Application::create(std::move(config));
        if (!created)
            throw std::runtime_error("The application could not be made");
        gf::Application& application = *created.application;
        std::shared_ptr<ProbeState> state = std::make_shared<ProbeState>();
        const char* capture = std::getenv("PLAYSUITE_DEAL_CAPTURE_DIR");
        if (capture != nullptr)
            (*state).capture_directory = capture;
        std::unique_ptr<games::PlaySuiteWindow> suite = std::make_unique<games::PlaySuiteWindow>(application);
        std::shared_ptr<games::Collection> collection = (*suite).games_table;
        (*state).collection = collection;
        // PlaySuite's one light look, as src/main.cpp sets it.
        gf::PresentationOverrides light{};
        light.colour_scheme = stxlib::Optional<gf::ColourScheme>::of(gf::ColourScheme::light);
        (*suite).set_presentation_overrides(light);
        gf::ApplicationWindowOptions options;
        options.title = probe_title;
        options.initial_size = {1060, 680};
        options.minimum_size = {600, 420};
        // The host's JSON (hostJSON: draw counts, paint and copy phases) is printed on close.
        options.print_metrics_on_close = true;
        options.ready = ProbeReady{state};
        const gf::ApplicationResult result = application.run(std::move(suite), std::move(options));
        const bool skipped = (*state).skipped;
        const bool judged = (*state).judged;
        const std::vector<std::string> failures = (*state).failures;
        state.reset();
        collection.reset();
        created.application.reset();
        static_cast<void>(gf::Application::shutdown());
        if (result.callback_exception)
            std::rethrow_exception(result.callback_exception);
        if (skipped)
            return 77;
        for (const std::string& failure : failures)
            std::cerr << "solitaire deal (native): " << failure << '\n';
        if (!judged)
            std::cerr << "solitaire deal (native): the deal was not judged\n";
        std::filesystem::remove_all(scratch);
        const bool passed = judged && failures.empty() && result.accepted();
        if (passed)
            std::cout << "solitaire deal (native): smooth\n";
        return passed ? 0 : 1;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
