# Conformance preparation: PlaySuite

Branch `claude/conformance-prep`, on `claude/playsuite-cp22` 19d0799 (open PR #39; the only branch on the
current history). Written against GUI.Forms main 96d3696 (cp29-cp31, version 0.6.0). Code only: nothing here
has been compiled or run. The integrator builds it.

## What was adapted

1. **Flat control headers** (tools/control_header_moves.json): 9 includes in 7 files now name
   `gui_forms/controls/<name>.hpp` (text_box, scrollable_control, check_box, track_bar). A sweep of every
   GUI.Forms include against 96d3696's `include/` found no other missing header.
2. **Version 0.6:** `find_package(GUIForms 0.6 ...)` in CMakeLists.txt and tests/portable_audio. The
   package is `SameMinorVersion`, so 0.6 accepts any 0.6.x SDK. The staging copies under `incoming/` still ask
   for 0.1; the root build does not build them.
3. **theme.hpp:** control.hpp no longer includes it. No PlaySuite file needs an addition: the only complete
   use of Theme (tests/playsuite_theme_tests.cpp) reaches it through application.hpp. shared/render/ui/surface.hpp
   and src/help_route.hpp do not use Theme.
4. **Exports:** every `gf::` name PlaySuite uses is in the 96d3696 export lists or is header-only, except the
   Audio classes (GUIForms::Audio is a static library) and Worker/CancellationFlag (GUIForms::Threading's own
   list). The check works by name only; a partly marked class shows only at link time.
5. **Removed API: `Control::set_child_index`** (cp28). src/collection.cpp now uses
   `set_child_index_from_top(id, 0)`, the topmost place, which is what `children().size() - 1` was.
6. **On_key and on_pointer are already gone at 96d3696.** The coordinator's brief listed the per-event
   raisers as still to come. In fact cp29 (GUI.Forms 56b6627ea, "Raisers take the target phase: on_pointer and
   on_key removed") removed `Control::on_key` and `Control::on_pointer`. The target phase is now
   on_mouse_down/up/move/wheel and on_key_down/up, which take `MouseEventArgs&` / `KeyEventArgs&`. So the
   products cannot compile against 96d3696 without this port, and it is done here:
   - `src/routed_input.hpp`: `games::RoutedInput<Base>`, a base that rebuilds the routed event from each
     raiser's arguments (`pointer_event_of`, `key_event_of`) and calls the view's `handle_pointer` /
     `handle_key`. It follows GUI.Forms' own tests/support/routed_input_probe.hpp. A mouse raiser runs the
     handler first, copies `handled` back (which ends the route, as before) and then raises the event. A key
     raiser raises first and calls the handler only for a key nobody handled. A key up raised by deactivation
     does not reach the handler, as before. Enter and leave come from on_mouse_enter/leave and carry no position.
   - 21 classes derive from `games::RoutedInput<gf::Control>`: their 20 `on_key` and 21 `on_pointer`
     overrides are now `handle_key` / `handle_pointer`, and the 20 `on_key_bubble` / `on_key_preview`
     forwarders call `handle_key`. The classes are the views of atomprobe, catchingthieves, cube, eggy,
     fourpegs, koikoi, liarsdice, maze, parrots, penthesheep, specimencabinet, sudoku, switchbox,
     tumbledstones and zenconstruction, plus new-games/template, shared/ambient SceneView and its HelpCard,
     shared/cards Table, shared/puzzles PuzzleView, and src BusinessCard.
   - src/shelf: ShelfBox (a Button) overrides the five mouse raisers and wakes the shelf when its hot state
     changes. `forget_pointer` calls `Button::on_mouse_leave()`. ShelfRows scrolls on `on_mouse_wheel`, and
     `wheel()` is public, so tests/shelf_help_tests.cpp drives it directly (it called `on_pointer`).
   - src/help_book: the business card's wheel scrolls the pages through
     `HelpPages::wheel_from_card` → `ScrollableControl::scroll_pointer(PointerEvent&)`, replacing a direct
     `on_pointer` call.
   - New test: tests/routed_input_tests.cpp (ctest `routed_input`). It checks the order, `handled` ending the
     route, the key-subscriber precedence, enter/leave, and the deactivation release.
   - The 29 `on_key_bubble` / `on_pointer_preview` / `on_pointer_bubble` / `on_key_preview` overrides stay.
     They are still virtual at 96d3696.
7. **zc_phys hull overflow** (games/zenconstruction/phys/hull.cpp). In the boundary-loop pass, a merged-face
   group with no boundary edge left `start` at -1, and the walk then read `next[-1]` and put -1 into
   `loops`. The pass now skips such a group, and stops a walk that reaches a vertex with no outgoing boundary
   edge. A hull left with fewer than four faces is refused with `std::runtime_error`, as degenerate points
   already were. New test: `phys_tests degenerate` (ctest `rockstack_degenerate_hulls`, unlabelled) cooks a
   revolved lattice with repeated pole points, exactly flat rings, a micron-thin sheet, a sliver and repeated
   corners. Each must be refused or well formed, and the welded repeats must give a tetrahedron. It needs an
   ASan/UBSan run to prove the read is gone. The `incoming/game-zenconstruction-01` staging copy of hull.cpp
   is unchanged; it is not built.

## What still waits on GUI.Forms

- **Pins (placeholders):** the three workflows (`ref:`), tools/package_common.py `TOOLKIT_REVISION`,
  AGENTS.md:110 and docs/BUILDING.md:3 still name cp27 757db03a7. The workflow and packaging lines carry a
  PLACEHOLDER comment. harness.lock still pins template_harness 8868f87. All of them move once, at the
  conformance rebase: to the GUI.Forms release that carries cp32/cp33 (0.6 numbering, with its cache assets),
  and to the template_harness main commit that GUI.Forms pins. This branch does not build at the cp27 pin.
- **cp32/cp33 APIs:**
  - font family: `FontSpec` gains `family` (FontFamilyId) and `stack`. PlaySuite's 50 positional
    `FontSpec{role, size, weight, italic, ...}` initializers do not name a family, and
    `BundledFontFamily` is not used.
  - the DataGridView grid engine and stock icons: not used.
  - the final-virtuals cleanup: PlaySuite overrides no `on_<property>_changed` hook of a final class.
  - Expect only rebuild drift.
- **Remaining routed-phase overrides:** if a later checkpoint retires `on_key_preview` / `on_key_bubble` /
  `on_pointer_preview` / `on_pointer_bubble`, these 29 sites move:
  - one `on_key_bubble` in each of the 18 game and shared views (atomprobe, catchingthieves, cube, eggy,
    fourpegs, koikoi, liarsdice, maze, parrots, penthesheep, specimencabinet, sudoku, switchbox, tumbledstones,
    zenconstruction, new-games/template, ambient SceneView, puzzles PuzzleView);
  - two in shared/cards/table.hpp (bubble and preview);
  - two each in src/collection.hpp (pointer and key preview), src/help_book.hpp and src/settings_sheet.hpp;
  - three in src/shelf.hpp.
- **STXlib boundary types:** none are consumed yet. When GUI.Forms' public signatures move to STXlib's owned
  span, string or result types (the A2 swap-in), the `std::span` / `std::string` arguments at PlaySuite's call
  sites follow. The main ones are LiveSurface leases, `load_bgra32_premultiplied`, `draw_text_utf8` and the
  text masks.
- **Harness:** template_harness main must first take `claude/csm-ccache-pch-main` (owner prerequisite); then
  harness.lock follows GUI.Forms' harness.lock.

## How the integrator builds it

Against a local GUI.Forms SDK. On the M4, PlaySuite needs Application, Audio, TextMasks and
AudioLoopTransport, so `--features all`:

```sh
# in a GUI.Forms checkout at 96d3696 (or the conformance head)
python3 -B tools/build_sdk.py --target macos-arm64 --prefix <sdk> --features all
# in this branch's checkout, with template_harness at harness.lock's commit in template_harness/
cmake -S . -B .build/conformance -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=$PWD/template_harness/harness/cmake/llvm22.cmake \
  -DCMAKE_PREFIX_PATH=<sdk> -DGAMES_RUNTIME_ASSET_DIR=<prepared runtime assets>
cmake --build .build/conformance --parallel 2
ctest --test-dir .build/conformance -LE design --output-on-failure
```

Or build GUI.Forms in-tree: leave `CMAKE_PREFIX_PATH` unset and pass
`-DGAMES_TOOLKIT_SOURCE_DIR=<gui_forms checkout>`. CI does it this way. Use a fresh build folder when switching
between the two. Tests that should cover this branch:
- routed_input;
- ui_routing, collection_ui, shelf_help, puzzle_frames, fourpegs_pointer and the view_contract tests (the
  routed handlers in real views);
- rockstack_degenerate_hulls under ASan/UBSan;
- playsuite_theme;
- the full `ctest -LE design`.

## Risks (not checked without a build)

- A view header that includes routed_input.hpp from a target without `src` on its include path. Every game
  and shared target already includes help_route.hpp from src through game_paths, so none is expected.
- A crossing carries no position. A handler that reads the position of an enter sees the window origin until
  the next move. AtomProbe, FourPegs, Switchbox and Thieves compute `mouse_x_`/`mouse_y_` on enter but use them
  only on move and leave.
- A refused degenerate hull now throws where it once went on with fewer than four faces. Generated rocks
  should never be that degenerate; a caller that cooks unchecked input should catch it.
- The 51 GUI.Forms accessors that delete their rvalue overload can refuse a call on a temporary; only a
  compile shows these.
