# Conformance preparation: PlaySuite

Branch `claude/conformance-prep`, on `claude/playsuite-cp22` 19d0799 (open PR #39; the only branch on the
current history). Written against GUI.Forms main 96d3696 (cp29-cp31, version 0.6.0), then brought to the
integrator's `claude/batch-cp33` (cp32/cp33, still 0.6.0). Code only: nothing here has been compiled or run.
The integrator builds it.

Two branches, by the owner's ruling (option C, provisional):
- `claude/conformance-prep` (this one) keeps `games::RoutedInput` as the fallback input layer.
- `claude/conformance-raisers`, stacked on it, has the views on GUI.Forms' raisers directly, with the
  drag-heavy views on GUI.Forms' DragTracker. Either can be chosen at the rebase; see "The raisers branch".

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
   PlaySuite's look is its own theme, the package in themes/playsuite (`games::playsuite_theme()`), which
   paints the capsule, help and the rest of the suite. main passes it in `ApplicationConfig::theme.packages`;
   the base named there only fills what the package leaves unset and is the fallback if the package is refused.
4. **Exports:** every `gf::` name PlaySuite uses is in the 96d3696 export lists or is header-only, except the
   Audio classes (GUIForms::Audio is a static library) and Worker/CancellationFlag (GUIForms::Threading's own
   list). The check works by name only; a partly marked class shows only at link time.
5. **Removed API: `Control::set_child_index`** (cp28). src/collection.cpp now uses
   `set_child_index_from_top(id, 0)`, the topmost place, which is what `children().size() - 1` was.
6. **On_key and on_pointer are already gone at 96d3696.** cp29 (GUI.Forms 56b6627ea) removed
   `Control::on_key` and `Control::on_pointer`; the target phase is on_mouse_down/up/move/wheel and
   on_key_down/up over `MouseEventArgs&` / `KeyEventArgs&`. On this branch the 21 views keep their
   `handle_pointer` / `handle_key` bodies through `games::RoutedInput<Base>` (src/routed_input.hpp), which
   rebuilds the routed event from the raisers. The raiser-direct port went to `claude/conformance-raisers`.
   - cp33's `Control::pointer_location()` closes the crossing gap: RoutedInput's enter and leave now carry
     the window position (tests/routed_input_tests.cpp checks the enter position).
   - `ControlStyles::opaque` is gone (audit A17); no view sets it.
   - src/shelf: ShelfBox and ShelfRows use the raisers directly (`ShelfRows::wheel` is public for
     tests/shelf_help_tests.cpp).
7. **zc_phys hull overflow** (games/zenconstruction/phys/hull.cpp). In the boundary-loop pass, a merged-face
   group with no boundary edge left `start` at -1, and the walk then read `next[-1]` and put -1 into
   `loops`. The pass now skips such a group, and stops a walk that reaches a vertex with no outgoing boundary
   edge. A hull left with fewer than four faces is refused with `std::runtime_error`, as degenerate points
   already were. New test: `phys_tests degenerate` (ctest `rockstack_degenerate_hulls`, unlabelled) cooks a
   revolved lattice with repeated pole points, exactly flat rings, a micron-thin sheet, a sliver and repeated
   corners. Each must be refused or well formed, and the welded repeats must give a tetrahedron. It needs an
   ASan/UBSan run to prove the read is gone. The `incoming/game-zenconstruction-01` staging copy of hull.cpp
   is unchanged; it is not built.

8. **cp33 (claude/batch-cp33).**
   - XML forms: forms/playsuite.form is the `urn:gui-forms:form:1` XML form (tools/convert_form_to_xml.py).
   - Linux: main calls `std::setlocale(LC_CTYPE, "")`; CMakeLists.txt checks for libc++ 22 on Linux
     (`GAMES_LINUX_LIBCXX_22`, `_LIBCPP_VERSION`) when the application builds.
   - One module (owner rule: a shipped program has no native library dependency and is one module):
     libc++, libc++abi and libunwind link statically through gui_forms/cmake/llvm22.cmake (GUI.Forms
     claude/cp33-libcxx-static 39d2ea6b0); CMakeLists.txt refuses a Linux tree whose
     `CMAKE_CXX_STANDARD_LIBRARIES` lacks the static archives; the workflows install libc++-22-dev,
     libc++abi-22-dev and libunwind-22-dev. GUI.Forms links into the executable once GUI.Forms'
     claude/one-module lands (its libraries are still SHARED at cp33). tools/package_linux.py bundles no
     shared library: it strips the program and refuses it if readelf -d or ldd names libc++, libc++abi,
     libunwind or a libgui_forms library, or ldd resolves anything outside the system's library folders
     (tools/test_package_linux.py tests that reading; CI runs it before packaging). The LLVM licences
     (libcxx, libcxxabi, libunwind) still ship.
   - Relative pointer: Zen Construction's camera orbit requests a confined grant (`until_release`, cursor
     shown) and turns on the relative samples; a free grant (headless) is released and the absolute moves
     drive it as before. The collection declares the capsule and the help link as pointer release regions
     (at most 8, window units) at the end of arrange; the capsule is left out while the shelf is open.
   - Final virtuals: PlaySuite derives only from Button, CheckBox, Label, TrackBar, ScrollableControl and
     Painter (none final) and overrides no final class's `on_<property>_changed`.
   - WheelPhase and `timestamp_nanoseconds` are appended fields: nothing to change. FontSpec's `family` and
     `stack` are appended too, so the 50 positional initializers still compile; BundledFontFamily was not used.
   - The window band and WindowCapsule belong to the surface window manager; PlaySuite's native window does
     not use them. The capsule is PlaySuite's own in-game menu (see "The capsule").
   - The std rules are not adopted yet (coordinator's choice; the products keep the std library as they use it).
9. **Business card and diagnostic overlay.** GUI.Forms' card (`gui_forms/components/business_card`)
   replaces src/business_card.*. main sets the program identity (PlaySuite, the project version, the vendor,
   the website, the creators as credits). The help book's page card is a `BusinessCard` (page); its press
   opens the hand card through `show_business_card`, whose website request opens the link. The remarks come
   from `games::CardRemarks` through `set_business_card_remark_source`. The settings row toggles
   `gf::diagnostic_overlay()`. The look stays PlaySuite's through themes/playsuite: make_theme.py writes the
   `business-card::part(...)` rules (stock, ink, foil, hover).
   - With GUI.Forms' card parity (claude/business-card-parity 6741c7da9) nothing PlaySuite's own card did is
     lost: the hand card turns in 3D and says the shell's remarks around itself; the sheet's stock is a
     paper-fibre material (the turned faces paint through it) with a copyright ink and a pale remark ink;
     the help book gives the page card its heading band (`set_heading_band`, a no-paint, no-hit control
     over the book's title); the website goes to the host's open-URL capability, and src/open_link.* is
     gone. Not expressed: the paper engine's finer finish (dimples, varnish, cut plies, raised ink).
     `shared/cards/held_card.*` is removed (no caller; GUI.Forms' turn seen in the dogfood build).
   - tests: collection_ui (the hand card opens with a remark and closes with help), catalog_ui,
     playsuite_theme, business_card_tests (the remarks).

## The raisers branch (`claude/conformance-raisers`)

- dc87433: the 21 views on GUI.Forms' raisers (the 9abfee0 port), with `gf::key_event_arguments` in place
  of src/key_arguments.hpp (cp33 made it public); Zen's relative-pointer orbit merged into its raisers;
  tests/view_input_tests.cpp (ctest `view_input`).
- On GUI.Forms' DragTracker: Four Pegs' peg drag, the card table's card drag, Tumbled Stones' stone drag
  (the neighbours as targets), Specimen Cabinet's stone drag, the puzzles' drags (a Puzzle Solve piece, a
  gem, an Untangle peg, the cat) and SceneView's scenery drag. Where the game hit-tests itself (Four Pegs'
  3D pick, the table's overlap rule, the cabinet's sockets, the puzzles' snaps) the tracker runs the
  session only: a free placement with no targets, the game picking the drop. Escape and a lost capture end
  every one of them the same way (put back, or the scenery let go).
- Kept hand-rolled, and why:
  - the Cube trace (Cube and the puzzles' cube): a path drawn cell by cell through the grid, extended on
    every move and routed round corners. It has no subject that moves or lands, so a placement session
    has nothing to carry; it ends on the release or a lost capture (on_mouse_capture_changed).
  - Zen Construction's orbit: a camera turn, not a drag of anything. It runs on GUI.Forms' relative
    pointer (confined, until the release), which a drag session would fight for the pointer.
  - Switchbox's switch hold and Eggy's hold: a button held down, not moved. Nothing travels or drops; they
    end on the release or a lost capture.
- tests: view_input (Escape ends a card drag), fourpegs_pointer (Escape mid-drag drops nothing; a tracker
  drop into a socket), the view_contract tests (tumbledstones, specimencabinet, the puzzles, stillwater
  and the other ambient scenes), ui_routing, collection_ui, puzzle_frames.
- Behaviour changes to try: only the primary button drags a gem, an Untangle peg or the cat; the scenery
  drag now keeps the pointer past the view's edge; the thresholds are per axis (the tracker's rule)
  rather than by distance.

## The capsule

The capsule is PlaySuite's in-game menu and stays its own. By the owner's decision of 2026-11-15 (apps may
load fonts into GUI.Forms' engine inside composites and subsurfaces; the capsule is declared a composite) it
is now a GUI.Forms composite:
- controls/command-capsule.control is the document: the pill is a themed part (the gradient, gold edge and
  shadow on_paint used to draw are its `<look>`), the title a stock label bound to the `title` property.
  gui_forms_add_control generates `games::CommandCapsuleControl` on game_ui.
- `games::CommandCapsule` (src/capsule.*) derives from it and keeps what a document cannot say: the
  game's commands (they change with the game), the master switches bound to SuiteModel, and the wrapping,
  opening layout. The buttons are still `SuiteButton`s (`gf::Button`, faces through the theme).
- The title is drawn by GUI.Forms' text engine in Barlow Condensed Bold, an application font family loaded
  through GUI.Forms' app-font API (`gf::load_app_font_family`) in the seam src/capsule_title_face.*. The
  shell owns it (`Collection::title_family_`, a `CapsuleTitleFamily` holding the `gf::AppFontFamily`):
  loaded when the shell's tree is made, before the capsule, and released in `~Collection`, the normal
  teardown before the application shuts down; no static holds it, so process termination releases
  nothing (the GUI.Forms and STXlib termination rule). The capsule copies the family's number. GUI.Forms admits the family by
  the control's context: the capsule's title label is a composite child, so it draws it; a stock control
  would drop it. **Dependency:** GUI.Forms claude/app-fonts (2558e326c, being folded). The CMake option
  `GAMES_GUI_FORMS_APP_FONTS` is ON by default; OFF builds against a GUI.Forms without the API, and the
  bundled control face (bold, slightly smaller and tighter) draws the title, as it does when the family is
  refused, quarantined or unloaded.
- Still hand-drawn: the buttons' glyphs (`SuiteButton::paint_glyph`), expressible as button images or
  stock icons the theme replaces by name. Product work, not a gap.
- Pocket Library's shell copy (its src/capsule.cpp) is unchanged; it can follow once this is built.

## The #483 port (`claude/conformance-port`)

Branch `claude/conformance-port`, on `claude/conformance-prep` 3be3dbe, against GUI.Forms #483
(checkpoint/2026-11-15-cp32, 4858cb741 or later; read at 48a8560a6). It fixes the 59 errors of the
integrator's build against the #483 SDK and sweeps the same patterns through the rest of the product,
including the sources that build did not reach (Tumbled Stones, the new-game template, playsuite_theme).
Write only: not compiled here.

- **D15 V1, `stxlib::Optional`:** writes into optional properties name the value
  (`stxlib::Optional<T>::of(...)`): `cursor` in the 17 views (the shape computed into a local
  `pointer_shape` first), Label `font` and `fore_color` (help book, settings sheet, capsule title, Zen's HUD),
  the topic headings' `expanded_state`. `PartPaint::measured_text` and `corner_radius` take `set()`;
  `games::skin_part_key` returns `stxlib::Optional<gf::PartKey>` (its only use is `PartPaint::key`);
  `paint_skin_part` keeps its `std::optional<double>` radius and sets the part's only when given;
  `ButtonContentInk::authored` is `::none()`. Reads: `next_wake()`, `child_index_from_top()`,
  `heading_band_bottom()`, `Window::paint()` and a label's own font read through `present()`, `absent()`
  and `value()` (into a local first) in new-games/kit/view_contract_test.hpp and the collection_ui,
  puzzle_frame, vendor_game_frame and playsuite_theme tests. Product-internal `std::optional` stays.
- **Renames:** `HorizontalAlignment::far`/`near` to `end`/`start` (settings readouts, Zen's sheet note);
  `FontContext::interface` to `factory_bundle` (collection_ui). No form, control or theme file names an
  alignment; nothing uses SplitSeamState, the Ribbon's sizes, DateTimeDropDownAlignment or
  `stxlib::Status`/`Result`.
- **Text engine:** `GUI_FORMS_ENABLE_HARFBUZZ_TEXT` (no longer an option) dropped from CMakeLists.txt and
  tests/portable_text. `find_package` never named HarfBuzz or FreeType components.
- **Not changed:** the workflows still pass `GUI_FORMS_SKIA_PREBUILT`/`GUI_FORMS_SKIA_OUT` and run
  `fetch_skia_cpu.sh` for the in-tree build; they move with the pins at the rebase.
- tests: collection_ui, puzzle_frames, vendor_game_frames, playsuite_theme, every view_contract test, the
  full `ctest -LE design`; by hand, the pointer shape over each game's buttons and the help book's topic
  disclosure.

## The port with the raisers (`claude/conformance-port-raisers`)

The owner's working copy (2026-11-15, PlaySuite 46d3bc6 against GUI.Forms 371bd48aa) was the raisers
branch with every drag on GUI.Forms' DragTracker; `claude/conformance-port` sat on the RoutedInput prep.
This branch merges `claude/conformance-raisers` (46d3bc6) into the port (60e6609), so the #483 build has
the input the owner tested. The views take the raisers' versions (src/routed_input.hpp and its test go,
tests/view_input_tests.cpp comes back), with the port's patterns applied to what the raisers carry:
`cursor` written through `stxlib::Optional<gf::CursorKind>::of` (a computed shape into a local
`pointer_shape` first), Zen's HUD font, ink and sheet-note alignment as in the port, and
`DragStart::source` (now `stxlib::Optional<gf::Rect>`) written with `set()` in the six drag views.
Write only: not compiled here. Tests: view_input, fourpegs_pointer, ui_routing, collection_ui,
shelf_help, puzzle_frames, every view_contract test, the full `ctest -LE design`.

## What still waits on GUI.Forms

- **Pins (placeholders):** the workflows (`ref:`), tools/package_common.py `TOOLKIT_REVISION`,
  AGENTS.md:110 and docs/BUILDING.md:3 still name cp27 757db03a7; the workflow and packaging lines carry a
  PLACEHOLDER comment. harness.lock still pins template_harness 8868f87. They move once, at the conformance
  rebase, to the GUI.Forms release that carries cp33 and the template_harness commit it pins. This branch
  does not build at the cp27 pin.
- **GUI.Forms gaps (docs/TOOLKIT_REQUESTS.md):**
  - W402 `scheme_rules_without_variant` fires on a light-only package over windows_professional (the
    base's dark rules counted as the package's);
  - the business card's five: closed by GUI.Forms 6741c7da9 (business-card parity), adopted here;
  - the capsule title's display face: settled by the owner (composites may load fonts); uses GUI.Forms
    claude/app-fonts 2558e326c (above), which the pin must carry.
  - Closed by cp33: the public `key_event_arguments` and the crossing position (`pointer_location`).
- **Remaining routed-phase overrides:** if a later checkpoint retires `on_key_preview` / `on_key_bubble` /
  `on_pointer_preview` / `on_pointer_bubble`, about 30 sites move (one `on_key_bubble` in each of the 18 game
  and shared views; the card table's bubble and preview; src/collection, src/help_book (now also
  `on_pointer_bubble` for the card's wheel), src/settings_sheet and src/shelf).
- **STXlib boundary types:** `stxlib::Optional` consumed since the #483 port (above). The later D15 stages
  (text views, owned strings, vectors, shared pointers) will need another, smaller port. When GUI.Forms' public signatures move to STXlib's span,
  string or result types, the `std::span` / `std::string` arguments at PlaySuite's call sites follow
  (LiveSurface leases, `load_bgra32_premultiplied`, `draw_text_utf8`, the text masks, the release regions).
- **Harness:** template_harness main must first take `claude/csm-ccache-pch-main` (owner prerequisite); then
  harness.lock follows GUI.Forms' harness.lock.

## How the integrator builds it

Against a local GUI.Forms SDK. On the M4, PlaySuite needs Application, Audio, TextMasks and
AudioLoopTransport, so `--features all`:

```sh
# in a GUI.Forms checkout at claude/batch-cp33 (or the conformance head)
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
- routed_input (the enter position);
- collection_ui (release regions, the GUI.Forms card), catalog_ui, ui_routing, shelf_help, puzzle_frames,
  fourpegs_pointer and the view_contract tests;
- business_card_tests, playsuite_theme (the card rules);
- rockstack_degenerate_hulls under ASan/UBSan;
- on Linux: the libc++ configure check and tools/package_linux.py;
- the full `ctest -LE design`.

## Risks (not checked without a build)

- The Linux package's one-module check and the static libc++ configure check pass only once the GUI.Forms
  pin carries one-module (folded as 66f82629b) and the static libc++ link.
- The system-folder rule allows any library under /lib, /lib64, /usr/lib, /usr/lib64 (X11, ALSA, libc);
  it does not check that each is on the target distribution.

- Release regions are declared during arrange; a window that reads them before the first layout sees none.
- The help book places the page card from the pages' arrange and scroll (set_child_layout inside layout).
- Zen's orbit assumes the relative release sample arrives before `relative_pointer_ended`.
- The theme's `business-card` rules may raise new theme findings (unknown part names show only when compiled).
- The heading band control lies over the book's title with no hit; the card is clipped at its lower edge
  (y 62, the pages' top).
- Crossings now read `pointer_location()` during the crossing; if it lags the crossing event, an enter is
  seen at the previous position.
- A refused degenerate hull now throws where it once went on with fewer than four faces.
- Accessors that delete their rvalue overload can refuse a call on a temporary; only a compile shows these.
