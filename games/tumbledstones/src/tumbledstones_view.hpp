#pragma once
// Tumbled Stones as a PlaySuite control: the only file that knows about GUI.Forms and the
// shell. It turns the pointer and the keys into a stone in hand (picked up, turned over,
// turned round, set down), swaps and juggles; it runs the motion while there is something
// to move and draws through the render engine's live surface; it offers its commands to
// the capsule and saves after every move.
//
// A settled tray does no work: the timer stops once nothing moves, and starts again on
// input, a command, a resize or a return from the shelf.
#include "suite.hpp"

#include "rules.hpp"
#include "scene.hpp"
#include "stage.hpp"
#include "surface.hpp"

#include "gui_forms/basic_controls.hpp"
#include "gui_forms/drag/drag_tracker/drag_tracker.hpp"
#include "gui_forms/threading.hpp"
#include "gui_forms/timer.hpp"
#include "gui_forms/events/key_events/key_events.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ts {
namespace gf = gui_forms;

struct Options {
    bool hosted = false;  // inside PlaySuite: the capsule carries the commands
    bool dev = false;     // a separate save file, for tests and development
};

class TumbledStonesView final : public gf::Control, public games::CommandSource {
public:
    TumbledStonesView(gf::StableId id, Options options);
    ~TumbledStonesView() override;
    static constexpr bool initialize_tree_after_construction = true;
    void initialize_control_tree() {}
    void arrange(gf::Rect bounds) override;
    void on_paint(gf::Painter& painter, gf::Rect damage) override;
    void on_mouse_down(gf::MouseEventArgs& event) override;
    void on_mouse_up(gf::MouseEventArgs& event) override;
    void on_mouse_move(gf::MouseEventArgs& event) override;
    void on_mouse_leave() override;
    void on_mouse_click(gf::MouseEventArgs& event) override;
    void on_key_down(gf::KeyEventArgs& event) override;
    void on_key_bubble(gf::KeyEvent& event) override;

    void activate();
    void set_cabinet(bool foreground, bool music, bool sound, bool reduced = false);
    [[nodiscard]] std::vector<games::GameCommand> commands() const override;
    void run_command(std::string_view id) override;
    [[nodiscard]] std::vector<games::GameSetting> settings() const override;
    void change_setting(std::string_view id, double value) override;
    // Development scripts (--dev only): "flip", "turn", "set", "juggle", "pick <cell>".
    bool scripted_action(std::string_view action);

private:
    // The stone in hand: lifted from its socket, turned over and round as the player asks,
    // its turns shown one after another.
    struct Hand {
        std::uint32_t serial = 0;  // 0: nothing in hand
        int cell = -1;
        int start = 0;             // its orientation when picked up
        int orientation = 0;       // as it is now
        struct Turn {
            int axis = 1;          // 1 over (about the upright), 2 round (about the eye's axis)
            double angle = 0;
            int from = 0;          // the orientation it began from
        };
        std::vector<Turn> turns;   // shown in order
        double progress = 0;       // of the first, 0..1
    };

    Options options_;
    Session session_;
    Motion motion_;
    Scene scene_;
    Layout layout_;
    render::Surface surface_;
    bool surface_attached_ = false;
    std::unique_ptr<gf::Timer> timer_;
    std::vector<gf::SubscriptionToken> subscriptions_;
    std::chrono::steady_clock::time_point last_tick_{};
    double scale_ = 1;
    int device_width_ = 0;
    int device_height_ = 0;
    bool laid_out_ = false;
    bool render_dirty_ = true;
    bool front_ = true;
    bool music_ = true;
    bool sound_ = true;
    bool reduced_ = false;

    Hand hand_;
    Panel panel_ = Panel::none;
    int choosing_ = 7;
    bool new_best_ = false;
    bool win_waiting_ = false;    // the win panel opens once the pull has been shown
    double pour_wait_ = -1;       // a new game's pour, held until its stones are drawn: seconds held
    int hover_ = -1;
    double pointer_x_ = 0, pointer_y_ = 0;  // points
    double hover_time_ = 10;      // seconds since the pointer came over its stone
    int cursor_ = -1;
    bool keyboard_ = false;       // the keyboard was used last: show its socket
    // A press on the tray: where, which button, and whether it became a drag.
    int press_cell_ = -1;
    double press_x_ = 0, press_y_ = 0;
    bool dragging_ = false;
    double drag_x_ = 0, drag_y_ = 0;
    int drag_to_ = -1;
    int pressed_button_ = 0;
    // What a release ended, for the click that follows it: the panel button
    // pressed, or the tray cell pressed without a drag.
    int released_button_ = 0;
    int released_cell_ = -1;
    // The stone drag runs on GUI.Forms' DragTracker: a primary press on a stone arms it, past
    // 0.2 of a cell it starts (plan_drag lifts the stone), and its targets are the pressed
    // cell's up to four neighbours, hit while the pointer is over one; the release swaps with
    // the target or sets the stone down (finish_drag). Escape, a lost capture or the window's
    // deactivation set it down without a swap. `drag_targets_` is the plan's borrowed storage.
    gf::DragTracker drag_{*this};
    std::array<gf::DragTarget, 4> drag_targets_{};
    void plan_drag(gf::DragPlan& plan);
    void preview_drag(const gf::DragTrackerFrame& frame);
    void finish_drag(const gf::DragTrackerFrame& frame);
    int land_sounds_ = 0;
    bool help_ = false;
    // The workshop: shapes, minerals and lit pictures made on a worker while play goes on.
    std::unique_ptr<gf::Worker> studio_;
    std::atomic<bool> studio_done_{false};         // landing clicks this frame, so a row lands as a patter

    void on_attached_to_window() override;
    void on_detaching_from_window(gf::SurfaceRoot& window) noexcept override;
    void window_activation(bool active);
    void tick();
    void keep_studio_busy();
    void stop_studio() noexcept;
    static void run_studio(const gf::CancellationFlag& cancelled, void* context);
    void request_frame();
    void publish();
    void play(const std::string& name, float gain, float rate);
    void sync_music();
    [[nodiscard]] Frame frame() const;
    [[nodiscard]] bool busy() const;

    // The game.
    void pick_up(int cell);
    void turn_over();
    void turn_round();
    void set_down();
    void put_back();
    void swap_with(int from, int to);
    void juggle();
    void show_outcome(const Board& before, const Outcome& outcome);
    void open_chooser();
    void set_help(bool open);
    void begin_game();
    void step_hand(double seconds);
    [[nodiscard]] Pose hand_pose() const;
    void move_cursor(int rows, int columns, bool swap);
    // The keyboard and the pointer share one stone: the keys move the lift and the glint
    // to the cursor's stone, as the pointer moves the cursor to its own.
    void follow_cursor();
    [[nodiscard]] int panel_button(double x, double y) const;
    void press_panel_button(int button);
    void cancel_press();
    void show_hover(int hover);
    void key_down(gf::KeyEventArgs& event);

    void persist();
    [[nodiscard]] bool restore();
    [[nodiscard]] std::string_view save_name() const;
};

}  // namespace ts
