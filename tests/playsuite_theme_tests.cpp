// PlaySuite's theme package and its form window (themes/playsuite, forms/playsuite.form):
// the package compiles over Windows Professional with no finding at all (an error
// refuses the theme; a warning drops a declaration, and the look would no longer be
// PlaySuite's exactly), and the form's window is made with the shell as its root, the
// shell's controls carrying the classes the sheet styles.
#include "collection.hpp"
#include "help_book.hpp"
#include "playsuite_form.hpp"
#include "playsuite_theme.hpp"
#include "suite.hpp"
#include "test_paths.hpp"
#include "gui_forms/application.hpp"

#include <algorithm>
#include <filesystem>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace gf = gui_forms;

static bool has_class(const gf::Control& control, const std::string& name) {
    const std::vector<std::string> names = control.classes.read();
    const bool found = std::find(names.begin(), names.end(), name) != names.end();
    return found;
}

int main() {
    const std::filesystem::path scratch = games_test::scratch_directory("games-theme-");
    games_test::isolate_saves(scratch);
    gf::ApplicationConfig config{};
    config.theme.base = gf::ThemeBase::windows_professional;
    config.theme.packages.push_back(games::playsuite_theme());
    gf::ApplicationCreateResult created = gf::Application::create(std::move(config));
    // The package is taken with no finding but W201: every PlaySuite colour is a literal
    // (custom properties; GUI.Forms has no application palette roles), which forced high
    // contrast cannot remap. Anything else is a declaration or rule the engine dropped.
    std::size_t unexpected = 0;
    for (const gf::ThemeCompileDiagnostic& finding : created.findings) {
        if (finding.code == "W201")
            continue;
        ++unexpected;
        std::cerr << finding.code << ' ' << finding.name << ' ' << finding.file << ':' << finding.line << ':'
                  << finding.column << ' ' << finding.rule << ' ' << finding.property << ' ' << finding.detail
                  << '\n';
    }
    assert(created);
    assert(unexpected == 0);
    {
        games::PlaySuiteWindow window(*created.application);
        assert(window.client_size().width == 1060 && window.client_size().height == 680);
        const std::shared_ptr<games::Collection> shell = window.games_table;
        assert(shell != nullptr);
        assert(has_class(*shell, "skin-ivory"));
        window.perform_layout();
        const gf::Control::Ptr help = window.control("collection.help").control;
        assert(help != nullptr);
        assert(has_class(*help, "help-glyph") && has_class(*help, "help-link"));
        // The window shows the application's theme, which carries the package's rules
        // beyond the base theme's, and in the light scheme main forces there is no
        // fallback to GUI.Forms' own theme (a silent fallback would leave the base look).
        const std::shared_ptr<const gf::Theme> theme = window.theme_ptr();
        assert(theme == (*(*created.application).resources()).theme_ptr());
        const gf::CompiledThemeRules& rules = (*theme).engine().rules();
        assert(rules.rule_count() > rules.built_in_rule_count());
        gf::PresentationOverrides light{};
        light.colour_scheme = gf::ColourScheme::light;
        window.set_presentation_overrides(light);
        assert((*theme).supports(gf::ColourScheme::light));
        assert(!window.theme_fallback().has_value());
        assert(window.theme_ptr() == theme);
        // The sheet's rules reach a skinned part: a smoke circle's face (keyed "face", as
        // games::paint_skin_part paints it) takes its material from PlaySuite's rules,
        // not the base theme's.
        const std::vector<std::string> circle{"gloss", "smoke", "rounded-end"};
        gf::CompiledThemeRules::PartFacts facts{};
        facts.control_type = "button";
        facts.classes = circle;
        facts.part = "face";
        facts.role = gf::PartRole::face;
        facts.family = "button";
        gf::CompiledThemeRules::PartProvenance provenance{};
        rules.resolve_provenance(facts, provenance);
        std::size_t authored = 0;
        for (const gf::CompiledThemeRules::FieldProvenance& field : provenance.materials)
            authored += field.authored ? 1U : 0U;
        assert(authored > 0);
        assert(games::skin_part_key(gf::PartRole::face).has_value());
    }
    {
        // A gloss button's classes follow its tone and radius.
        const std::shared_ptr<games::SuiteButton> button =
            gf::make_control<games::SuiteButton>(gf::StableId("theme.gloss"), "Play", games::GlossTone::navy);
        assert(has_class(*button, "gloss") && has_class(*button, "navy") && !has_class(*button, "rounded-end"));
        (*button).set_tone(games::GlossTone::smoke);
        (*button).set_radius(games::SuiteButton::round);
        assert(has_class(*button, "smoke") && !has_class(*button, "navy") && has_class(*button, "rounded-end"));
        (*button).set_checked(true);
        assert((*button).anatomy_states().contains(gf::AnatomyState::checked));
    }
    created.application.reset();
    std::cout << "playsuite theme: ok\n";
    return 0;
}
