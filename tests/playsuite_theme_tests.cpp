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
    for (const gf::ThemeCompileDiagnostic& finding : created.findings) {
        std::cerr << finding.code << ' ' << finding.name << ' ' << finding.file << ':' << finding.line << ':'
                  << finding.column << ' ' << finding.rule << ' ' << finding.property << ' ' << finding.detail
                  << '\n';
    }
    assert(created);
    assert(created.findings.empty());
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
