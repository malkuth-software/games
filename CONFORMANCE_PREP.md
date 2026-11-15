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
     (`GAMES_LINUX_LIBCXX_22`, `_LIBCPP_VERSION`) when the application builds; the workflows install
     libc++-22-dev and libc++abi-22-dev; tools/package_linux.py bundles libc++.so.1, libc++abi.so.1 (and
     libunwind.so.1 where ldd resolves it) with the LLVM licences.
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
   - The std rules are not adopted (owner choice; the products keep the std library as they use it).
9. **Business card and diagnostic overlay.** GUI.Forms' card (`gui_forms/components/business_card`)
   replaces src/business_card.*. main sets the program identity (PlaySuite, the project version, the vendor,
   the website, the creators as credits). The help book's page card is a `BusinessCard` (page); its press
   opens the hand card through `show_business_card`, whose website request opens the link. The remarks come
   from `games::CardRemarks` through `set_business_card_remark_source`. The settings row toggles
   `gf::diagnostic_overlay()`. The look stays PlaySuite's through themes/playsuite: make_theme.py writes the
   `business-card::part(...)` rules (stock, ink, foil, hover). The card gaps are in docs/TOOLKIT_REQUESTS.md.
   - tests: collection_ui (the hand card opens with a remark and closes with help), catalog_ui,
     playsuite_theme, business_card_tests (held_card and the remarks).

## The raisers branch (`claude/conformance-raisers`)

- dc87433: the 21 views on GUI.Forms' raisers (the 9abfee0 port), with `gf::key_event_arguments` in place
  of src/key_arguments.hpp (cp33 made it public); Zen's relative-pointer orbit merged into its raisers;
  tests/view_input_tests.cpp (ctest `view_input`).
- On GUI.Forms' DragTracker: Four Pegs' peg drag (09d04ba), the card table (0135809), Tumbled Stones'
  stone drag with the neighbours as targets (ed415bf), Specimen Cabinet's stone drag (b09651d). Where the
  game hit-tests itself (Four Pegs' 3D pick, the table's overlap rule, the cabinet's sockets and felt) the
  tracker runs the session only: a free placement with no targets, the game picking the drop.
- Kept as hand-rolled, by design: the Cube trace (a path, not a placement), Zen's orbit (the relative
  pointer), Switchbox's switch hold and Eggy's hold (holds, not drags).
- Still to do: the puzzles' per-kind drags (shared/puzzles, slide, gem, untangle node, orbit) and the
  scenery drag in shared/ambient's SceneView.
- tests: view_input (Escape ends a card drag), fourpegs_pointer (Escape mid-drag drops nothing; a tracker
  drop into a socket), the view_contract tests, ui_routing, collection_ui, puzzle_frames.

## The capsule

The capsule (src/capsule.*, `CommandCapsule`) is PlaySuite's in-game menu and stays its own. It is a
`gf::Control` whose children are `SuiteButton`s (`gf::Button`, faces, focus and labels painted through the
theme's parts) and the master switches bound to SuiteModel; opening and wrapping are its own arrange. Three
things are still painted by hand:
1. the pill (shadow, gradient, gold edge) in on_paint: expressible as a keyed theme part in themes/playsuite.
   Product work, not a gap.
2. the glyphs, drawn with lines in `SuiteButton::paint_glyph`: expressible as button images or stock icons
   that the theme replaces by name. Product work, not a gap.
3. the title, in Barlow Condensed through PlaySuite's own TextSprites. Interface text uses bundled faces
   only (the 2026-11-13 interface/content split), so a `gf::Label` cannot show it. **GUI.Forms gap or owner
   decision:** move the title to a bundled face, or admit product display faces for interface text.

## What still waits on GUI.Forms

- **Pins (placeholders):** the workflows (`ref:`), tools/package_common.py `TOOLKIT_REVISION`,
  AGENTS.md:110 and docs/BUILDING.md:3 still name cp27 757db03a7; the workflow and packaging lines carry a
  PLACEHOLDER comment. harness.lock still pins template_harness 8868f87. They move once, at the conformance
  rebase, to the GUI.Forms release that carries cp33 and the template_harness commit it pins. This branch
  does not build at the cp27 pin.
- **GUI.Forms gaps (docs/TOOLKIT_REQUESTS.md):**
  - W402 `scheme_rules_without_variant` fires on a light-only package over windows_professional (the
    base's dark rules counted as the package's);
  - the business card: the 3D turn, the card stock material, remarks around the card, the page card under
    the heading band, the URL service;
  - the capsule title's display face (above).
  - Closed by cp33: the public `key_event_arguments` and the crossing position (`pointer_location`).
- **Remaining routed-phase overrides:** if a later checkpoint retires `on_key_preview` / `on_key_bubble` /
  `on_pointer_preview` / `on_pointer_bubble`, about 30 sites move (one `on_key_bubble` in each of the 18 game
  and shared views; the card table's bubble and preview; src/collection, src/help_book (now also
  `on_pointer_bubble` for the card's wheel), src/settings_sheet and src/shelf).
- **STXlib boundary types:** none consumed yet. When GUI.Forms' public signatures move to STXlib's span,
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

- Release regions are declared during arrange; a window that reads them before the first layout sees none.
- The help book places the page card from the pages' arrange and scroll (set_child_layout inside layout).
- Zen's orbit assumes the relative release sample arrives before `relative_pointer_ended`.
- The theme's `business-card` rules may raise new theme findings (unknown part names show only when compiled).
- `held_card` in src/card_remarks is left without a caller; business_card_tests still covers it.
- Crossings now read `pointer_location()` during the crossing; if it lags the crossing event, an enter is
  seen at the previous position.
- A refused degenerate hull now throws where it once went on with fewer than four faces.
- Accessors that delete their rvalue overload can refuse a call on a temporary; only a compile shows these.
