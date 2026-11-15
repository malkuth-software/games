"""Package the native Linux application for Ubuntu 24.04 or compatible systems."""
import argparse
from pathlib import Path
import platform
import shutil
import subprocess
import tarfile
from package_common import (LEGACY_LINUX_PACKAGE, LINUX_PACKAGE, MAINTAINER, ROOT, copy_resources, project_version,
                            write_manifest)

# The LLVM runtime libraries a Linux build links (libunwind on musl only).
LLVM_RUNTIME = ("libc++.so.1", "libc++abi.so.1", "libunwind.so.1")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--toolkit", type=Path, required=True)
    parser.add_argument("--output", type=Path, default=Path("dist"))
    args = parser.parse_args()
    output = args.output.resolve()
    bundle = output / "PlaySuite"
    bundle.mkdir(parents=True, exist_ok=False)
    shutil.copy2(args.build / "games", bundle / "games")
    # GUI.Forms' shared libraries (Application, Threading) the bundle needs, followed
    # through each one's own imports.
    pending = [bundle / "games"]
    while pending:
        binary = pending.pop()
        imports = subprocess.check_output(["patchelf", "--print-needed", str(binary)], text=True).splitlines()
        for name in imports:
            if name.startswith("libgui_forms_") and not (bundle / name).exists():
                shutil.copy2(args.build / "toolkit" / name, bundle / name)
                pending.append(bundle / name)
    if not list(bundle.glob("libgui_forms_application.so*")):
        raise RuntimeError("Expected the GUI.Forms application library import")
    # LLVM's libc++ 22 and libc++abi (and libunwind on musl): GUI.Forms builds against them on
    # Linux (cp33) and Ubuntu's archive has no libc++ 22, so the bundle carries them beside the
    # program, as macOS bundles its runtime dylibs, from where ldd resolves them.
    resolved = subprocess.check_output(["ldd", str(bundle / "games")], text=True)
    runtime = {}
    for line in resolved.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[1] == "=>" and parts[0] in LLVM_RUNTIME:
            runtime[parts[0]] = Path(parts[2])
    for name in ("libc++.so.1", "libc++abi.so.1"):
        if name not in runtime:
            raise RuntimeError("Expected the program to link " + name + " (libc++ 22)")
    for name, source in runtime.items():
        shutil.copy2(source, bundle / name)
    for binary in [bundle / "games"] + list(bundle.glob("*.so*")):
        subprocess.run(["patchelf", "--set-rpath", "$ORIGIN", str(binary)], check=True)
        subprocess.run(["strip", "--strip-unneeded", str(binary)], check=True)
        dependencies = subprocess.check_output(["ldd", str(binary)], text=True)
        if "not found" in dependencies:
            raise RuntimeError(dependencies)
    copy_resources(args.build, args.toolkit, bundle)
    # The bundled LLVM runtime's licences, as GUI.Forms' own Linux packages carry them.
    llvm_notices = args.toolkit / "packaging/licenses/llvm"
    for name, notice in (("LLVM-libcxx.txt", "libcxx-LICENSE.TXT"), ("LLVM-libcxxabi.txt", "libcxxabi-LICENSE.TXT")):
        shutil.copy2(llvm_notices / notice, bundle / "licenses" / name)
    arch = "arm64" if platform.machine() == "aarch64" else "amd64"
    write_manifest(bundle, "Linux", arch)
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
