"""Package the native Linux application for Ubuntu 24.04 or compatible systems."""
import argparse
from pathlib import Path
import platform
import re
import shutil
import subprocess
import tarfile
from package_common import (LEGACY_LINUX_PACKAGE, LINUX_PACKAGE, MAINTAINER, ROOT, copy_resources, project_version,
                            write_manifest)

# A shipped program is one module with no native library dependency beyond the system's own
# (owner, 2026-11-15): libc++, libc++abi and LLVM's libunwind are linked statically
# (gui_forms/cmake/llvm22.cmake, GUI.Forms claude/cp33-libcxx-static 39d2ea6b0), and GUI.Forms
# is linked into the executable (GUI.Forms claude/one-module). The package refuses a binary
# that names any of these, or any library ldd resolves outside the system's library folders.
REFUSED_LIBRARY = re.compile(r"(?<![\w+.-])(lib(?:c\+\+abi|c\+\+|unwind)\.so\.\d+|libgui_forms_\S*?\.so(?:\.\d+)*)(?![\w+.-])")
# Where the system's own libraries live (glibc and musl, both layouts); /usr/lib/llvm-* is not one.
SYSTEM_LIBRARY_FOLDERS = ("/lib/", "/lib64/", "/usr/lib/", "/usr/lib64/")
# One ldd line: "\tname => /path (0x...)", "\tname => not found", "\tname (0x...)" (vdso) or
# "\t/lib64/ld-linux... (0x...)" (the loader).
LDD_LINE = re.compile(r"^\s*(\S+)(?:\s+=>\s+(not found|\S+))?(?:\s+\(0x[0-9a-fA-F]+\))?\s*$")


def foreign_libraries(readelf_output, ldd_output):
    """The libraries a binary needs that are not the system's, in order, each once.

    readelf -d's NEEDED entries and ldd's lines are both read: a refused name (the LLVM C++
    runtime or a GUI.Forms library) fails wherever it appears; an ldd line that resolves
    nowhere, or outside SYSTEM_LIBRARY_FOLDERS, fails too. linux-vdso and the dynamic loader
    are the system's.
    """
    found = []
    for match in REFUSED_LIBRARY.finditer(readelf_output + "\n" + ldd_output):
        if match.group(1) not in found:
            found.append(match.group(1))
    for line in ldd_output.splitlines():
        match = LDD_LINE.match(line)
        if match is None or not line.strip():
            continue
        name = match.group(1)
        target = match.group(2)
        if name.startswith("linux-vdso") or name.startswith("linux-gate"):
            continue
        where = target if target is not None else name
        system = where.startswith(SYSTEM_LIBRARY_FOLDERS) and not where.startswith("/usr/lib/llvm-")
        if (where == "not found" or not system) and name not in found:
            found.append(name)
    return found


def require_one_module(binary):
    """Fails unless `binary` needs only the system's libraries (see foreign_libraries)."""
    readelf_output = subprocess.check_output(["readelf", "-d", str(binary)], text=True)
    ldd_output = subprocess.check_output(["ldd", str(binary)], text=True)
    found = foreign_libraries(readelf_output, ldd_output)
    if found:
        raise RuntimeError(str(binary) + " needs libraries that are not the system's: " + ", ".join(found)
                           + "\n" + ldd_output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    # An installed GUI.Forms SDK prefix (cmake --install or tools/build_sdk.py): its notices in
    # share/licenses/GUIForms and its fonts in share/GUIForms/fonts.
    parser.add_argument("--toolkit", type=Path, required=True, help="installed GUI.Forms SDK prefix")
    # The packaged source commit; this checkout's HEAD through git when omitted.
    parser.add_argument("--source-revision", help="source commit recorded in package-manifest.json")
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()
    output = args.output.resolve()
    bundle = output / "PlaySuite"
    bundle.mkdir(parents=True, exist_ok=False)
    shutil.copy2(args.build / "games", bundle / "games")
    # One module: GUI.Forms and the LLVM C++ runtime are linked into the program, so the bundle
    # holds no shared library and the program needs only the system's.
    subprocess.run(["strip", "--strip-unneeded", str(bundle / "games")], check=True)
    require_one_module(bundle / "games")
    copy_resources(args.build, args.toolkit, bundle)
    # The statically linked LLVM runtime's licences, as GUI.Forms' own Linux SDK installs them
    # (copied with its other notices into licenses/GUIForms).
    for notice in ("libcxx-LICENSE.txt", "libcxxabi-LICENSE.txt", "libunwind-LICENSE.txt"):
        if not (bundle / "licenses/GUIForms" / notice).is_file():
            raise RuntimeError("The GUI.Forms SDK carries no " + notice + " for the static LLVM runtime")
    arch = "arm64" if platform.machine() == "aarch64" else "amd64"
    write_manifest(bundle, "Linux", arch, args.source_revision)
    stem = "playsuite-" + project_version() + "-linux-" + arch
    with tarfile.open(output / (stem + ".tar.gz"), "w:gz") as archive:
        archive.add(bundle, arcname="PlaySuite")
    root = output / "deb-root"
    installed = root / "usr/lib" / LINUX_PACKAGE
    shutil.copytree(bundle, installed)
    launcher = root / "usr/bin" / LINUX_PACKAGE
    launcher.parent.mkdir(parents=True)
    launcher.write_text('#!/bin/sh\nexec /usr/lib/' + LINUX_PACKAGE + '/games "$@"\n', encoding="utf-8")
    launcher.chmod(0o755)
    # 0.5.x's command keeps working for scripts and old menu entries.
    (root / "usr/bin" / LEGACY_LINUX_PACKAGE).symlink_to(LINUX_PACKAGE)
    desktop = root / "usr/share/applications" / (LINUX_PACKAGE + ".desktop")
    desktop.parent.mkdir(parents=True)
    desktop.write_text("[Desktop Entry]\nType=Application\nName=PlaySuite\nExec=" + LINUX_PACKAGE + "\n"
                       "Icon=" + LINUX_PACKAGE + "\nCategories=Game;\nTerminal=false\n", encoding="utf-8")
    icon = root / "usr/share/icons/hicolor/256x256/apps" / (LINUX_PACKAGE + ".png")
    icon.parent.mkdir(parents=True)
    shutil.copy2(ROOT / "assets/Games.png", icon)
    control = root / "DEBIAN/control"
    control.parent.mkdir()
    # dpkg-shlibdeps uses a Debian source metadata directory even for -O.
    metadata = output / "debian/control"
    metadata.parent.mkdir()
    metadata.write_text("Source: " + LINUX_PACKAGE + "\nSection: games\nPriority: optional\n"
                        "Maintainer: " + MAINTAINER + "\n\n"
                        "Package: " + LINUX_PACKAGE + "\nArchitecture: any\nDescription: PlaySuite native game collection\n", encoding="utf-8")
    binaries = [installed / "games"] + list(installed.glob("*.so*"))
    dependencies = subprocess.check_output(
        ["dpkg-shlibdeps", "-O", "-l" + str(installed), "--ignore-missing-info"] +
        ["-e" + str(binary) for binary in binaries], cwd=output, text=True).strip().removeprefix("shlibs:Depends=")
    # Conflicts and Replaces with 0.5.x's package: dpkg and apt remove rainstar-games and
    # install this in its place, so an upgrade leaves one copy; Provides keeps anything
    # that depended on the old name satisfied.
    control.write_text("Package: " + LINUX_PACKAGE + "\nVersion: " + project_version() + "\nArchitecture: " + arch +
        "\nMaintainer: " + MAINTAINER + "\nSection: games\nPriority: optional\nDepends: " + dependencies +
        ", libasound2t64\nConflicts: " + LEGACY_LINUX_PACKAGE + "\nReplaces: " + LEGACY_LINUX_PACKAGE +
        "\nProvides: " + LEGACY_LINUX_PACKAGE +
        "\nDescription: Native card games, puzzles and arcade scenes\n", encoding="utf-8")
    subprocess.run(["dpkg-deb", "--root-owner-group", "--build", str(root), str(output / (stem + ".deb"))], check=True)


if __name__ == "__main__":
    main()
