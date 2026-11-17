"""Shared app resources and notices, read from an installed GUI.Forms SDK prefix (--toolkit)."""
from pathlib import Path
import hashlib
import json
import os
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def toolkit_revision():
    """The GUI.Forms commit gui_forms.lock pins, the one place the pin is written."""
    text = (ROOT / "gui_forms.lock").read_text(encoding="utf-8")
    match = re.search(r'^commit\s*=\s*"([^"]+)"', text, re.MULTILINE)
    if match is None:
        raise RuntimeError("gui_forms.lock has no commit line")
    return match.group(1)


TOOLKIT_REVISION = toolkit_revision()


def sdk_notices(prefix):
    """GUI.Forms' licences in an installed SDK prefix (`cmake --install` of GUI.Forms, or
    its tools/build_sdk.py): share/licenses/GUIForms, every notice for what the SDK links
    (its stx packages, HarfBuzz, FreeType, Skia, SheenBidi, Unicode, miniaudio, BFFT, the
    LLVM runtime on Linux) under the names its install gives them."""
    notices = prefix / "share/licenses/GUIForms"
    if not (notices / "LICENSE").is_file():
        raise RuntimeError(str(prefix) + " is not an installed GUI.Forms SDK prefix "
                           "(no share/licenses/GUIForms/LICENSE); pass --toolkit <prefix>")
    return notices


def sdk_fonts(prefix):
    """The SDK's bundled fonts (GUIForms_FONT_DIR): share/GUIForms/fonts."""
    fonts = prefix / "share/GUIForms/fonts"
    if not fonts.is_dir():
        raise RuntimeError(str(prefix) + " has no share/GUIForms/fonts")
    return fonts


def miniaudio_license(notices, build, cache):
    """miniaudio's licence: the SDK's copy first; otherwise the source the configure used,
    named by FETCHCONTENT_SOURCE_DIR_GUI_FORMS_MINIAUDIO, under FETCHCONTENT_BASE_DIR, or in
    the build's _deps. Raises naming every place looked at."""
    candidates = [notices / "miniaudio-LICENSE.txt"]
    override = re.search(r"^FETCHCONTENT_SOURCE_DIR_GUI_FORMS_MINIAUDIO:PATH=(.+)$", cache, re.MULTILINE)
    if override:
        candidates.append(Path(override.group(1).strip()) / "LICENSE")
    base = re.search(r"^FETCHCONTENT_BASE_DIR:PATH=(.+)$", cache, re.MULTILINE)
    if base:
        candidates.append(Path(base.group(1).strip()) / "gui_forms_miniaudio-src/LICENSE")
    candidates.append(build / "_deps/gui_forms_miniaudio-src/LICENSE")
    for candidate in candidates:
        if candidate.is_file():
            return candidate
    raise RuntimeError("miniaudio's licence not found; looked at " + ", ".join(str(c) for c in candidates))


def source_revision(given):
    """The packaged source commit: `given` (--source-revision) when set, else this checkout's
    HEAD through git; a package built from an exported tree passes it."""
    if given:
        return given
    try:
        return subprocess.check_output(["git", "-C", str(ROOT), "rev-parse", "HEAD"], text=True,
                                       stderr=subprocess.DEVNULL).strip()
    except (OSError, subprocess.CalledProcessError) as error:
        raise RuntimeError("No git checkout at " + str(ROOT) + "; pass --source-revision") from error


def missing_prepared_files(assets):
    """Every card, sound and game resource the prepared manifests list, absent from `assets`."""
    listed = []
    cards = assets / "cards/manifest.json"
    if cards.is_file():
        listed += ["cards/" + record["file"] for record in json.loads(cards.read_text(encoding="utf-8"))]
    audio = assets / "audio/portable_manifest.json"
    if audio.is_file():
        manifest = json.loads(audio.read_text(encoding="utf-8"))
        listed += ["audio/" + record["file"] for record in manifest.get("files", [])]
        listed += [record["file"] for record in manifest.get("module_resources", [])]
    return [name for name in listed if not (assets / name).is_file()]


# The vendor identity every product shares (owner, 2026-10-27), and the names 0.5.x
# installed under, which an upgrade must still find. Save folders (src/runtime_paths.cpp,
# Rainstar/Games) are user data and keep their names.
VENDOR = "Malkuth Software"
BUNDLE_IDENTIFIER = "com.malkuthsoft.playsuite"
LINUX_PACKAGE = "playsuite"  # the vendor name stays out of package names (owner, 2026-10-27)
LEGACY_LINUX_PACKAGE = "rainstar-games"
# The .deb Maintainer of every product (owner, 2026-10-27).
MAINTAINER = VENDOR + " <maintainer@malkuthsoft.com>"
# The Windows uninstall entry 0.5.x registered; upgrades rewrite the same entry.
WINDOWS_UNINSTALL_KEY = "RainstarGames"
# Where 0.5.x installed on Windows; an upgrade over it stays there.
LEGACY_WINDOWS_INSTALL = "$LOCALAPPDATA\\Rainstar\\Games"


def project_version():
    from publication_version import baseline, validate
    version = os.environ.get("GAMES_RELEASE_VERSION") or baseline(ROOT)
    validate(version)
    return version


def copy_resources(build, toolkit, destination):
    cache = (build / "CMakeCache.txt").read_text(encoding="utf-8")
    compiled_version = re.search(r"^CMAKE_PROJECT_VERSION:STATIC=(.+)$", cache, re.MULTILINE)
    if not compiled_version or compiled_version.group(1) != project_version():
        raise RuntimeError("Packaging version differs from the configured native application")
    destination.mkdir(parents=True, exist_ok=True)
    assets = build / "assets"
    if not assets.is_dir():
        assets = build / "games.app/Contents/Resources/assets"
    shutil.copytree(assets, destination / "assets", dirs_exist_ok=True)
    # A package that lost a prepared file would only show it when the game opens.
    missing = missing_prepared_files(destination / "assets")
    if missing:
        raise RuntimeError("Packaged assets are missing prepared files: " + ", ".join(missing[:10]))
    shutil.copytree(sdk_fonts(toolkit), destination / "fonts", dirs_exist_ok=True)
    notices = destination / "licenses"
    notices.mkdir(exist_ok=True)
    # GUI.Forms and everything it links, as its installed SDK carries the notices.
    toolkit_notices = sdk_notices(toolkit)
    shutil.copytree(toolkit_notices, notices / "GUIForms", dirs_exist_ok=True)
    shutil.copy2(miniaudio_license(toolkit_notices, build, cache), notices / "miniaudio.txt")
    # PlaySuite's own downloaded dependencies: the build's _deps, or wherever the configure
    # placed them (CI keeps them in a cached FETCHCONTENT_BASE_DIR).
    base = re.search(r"^FETCHCONTENT_BASE_DIR:PATH=(.+)$", cache, re.MULTILINE)
    fetched = Path(base.group(1).strip()) if base else build / "_deps"
    for name, source in {
        "QuickJS.txt": fetched / "quickjs-src/LICENSE",
        "Plan-Paint.txt": ROOT / "shared/felt/LICENSE",
    }.items():
        shutil.copy2(source, notices / name)
    (destination / "README.txt").write_text(
        "PlaySuite\n\nA native collection of card games, puzzles and arcade scenes.\n"
        "Click a box to play or continue. Each game has Help in its command capsule.\n"
        "Music, Sound and Motion in the collection are master controls.\n"
        "Saves are per-user and remain separate from this application folder.\n"
        "Atom Probe, Four Pegs and Switchbox keep separate v2 saves; v1 saves remain intact.\n\n"
        "Source and release notes: https://github.com/falseywinchnet/games\n"
        "Font licenses accompany the fonts; other dependency notices are in licenses/.\n\n"
        "Crafted by Anthropic Claude General Reasoning Agents and OpenAI Codex General\n"
        "Reasoning Agents.\nSponsor: Rainstar\n", encoding="utf-8")


def write_manifest(bundle, system, architecture, revision=None):
    files = {}
    for path in sorted(bundle.rglob("*")):
        if path.is_file():
            files[path.relative_to(bundle).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    manifest = {"version": project_version(), "source_revision": source_revision(revision),
                "system": system, "architecture": architecture,
                "toolkit_source_revision": TOOLKIT_REVISION, "files": files}
    (bundle / "package-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
