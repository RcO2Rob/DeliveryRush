#!/usr/bin/env python3
"""Find Unreal, build the project, author native content, and open/play it.

No system settings are changed and no software is downloaded. UE_ENGINE_ROOT
can point at an existing installed engine (the directory containing Engine/).
"""
import argparse
import json
import os
import platform
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "DeliveryRush.uproject"
LAYOUT = json.loads((ROOT / "Design/city_layout.json").read_text(encoding="utf-8"))
CURRENT_MAP = LAYOUT["map"]
CONTENT_MARKER = ROOT / "Content" / LAYOUT["marker"]
REQUIRED_ENGINE = json.loads(PROJECT.read_text(encoding="utf-8"))["EngineAssociation"]


def engine_version(engine):
    try:
        version = json.loads((engine / "Engine/Build/Build.version").read_text(encoding="utf-8"))
        return ".".join(str(version[key]) for key in ("MajorVersion", "MinorVersion", "PatchVersion"))
    except (OSError, ValueError, KeyError):
        return None


def engine_matches(engine):
    version = engine_version(engine)
    return version is not None and version.rsplit(".", 1)[0] == REQUIRED_ENGINE


def engine_root(explicit=None):
    candidates = []
    if explicit or os.environ.get("UE_ENGINE_ROOT"):
        # An explicit path is authoritative; do not silently select another install.
        candidate = Path(explicit or os.environ["UE_ENGINE_ROOT"]).expanduser()
        return candidate.resolve() if (candidate / "Engine/Build/BatchFiles").is_dir() else None
    if sys.platform == "darwin":
        candidates += sorted(Path("/Users/Shared/Epic Games").glob("UE_*"), reverse=True)
        # The launcher allows per-user/custom install locations. Read only its
        # installation index, never authentication/configuration files.
        index = Path.home() / "Library/Application Support/Epic/UnrealEngineLauncher/LauncherInstalled.dat"
        if index.is_file():
            try:
                entries = json.loads(index.read_text()).get("InstallationList", [])
                candidates += [Path(item["InstallLocation"]) for item in entries
                               if str(item.get("AppName", "")).startswith("UE_")
                               and item.get("InstallLocation")]
            except (OSError, ValueError, TypeError, KeyError):
                pass
    elif sys.platform == "win32":
        candidates += sorted(Path("C:/Program Files/Epic Games").glob("UE_*"), reverse=True)
        # New Launcher installs can be usable before the legacy installation
        # index/registry is populated. Check common folders on local fixed disks.
        import ctypes
        import string
        for letter in string.ascii_uppercase:
            drive = Path(f"{letter}:/")
            if ctypes.windll.kernel32.GetDriveTypeW(str(drive)) == 3:
                candidates += sorted(drive.glob("UE_*"), reverse=True)
                candidates += sorted((drive / "Epic Games").glob("UE_*"), reverse=True)
        import winreg
        for hive, key in ((winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\EpicGames\Unreal Engine"),
                          (winreg.HKEY_CURRENT_USER, r"Software\Epic Games\Unreal Engine\Builds")):
            try:
                with winreg.OpenKey(hive, key) as registry:
                    if hive == winreg.HKEY_LOCAL_MACHINE:
                        for index in range(winreg.QueryInfoKey(registry)[0]):
                            with winreg.OpenKey(registry, winreg.EnumKey(registry, index)) as item:
                                candidates.append(Path(winreg.QueryValueEx(item, "InstalledDirectory")[0]))
                    else:
                        for index in range(winreg.QueryInfoKey(registry)[1]):
                            candidates.append(Path(winreg.EnumValue(registry, index)[1]))
            except OSError:
                pass
        index = Path(os.environ.get("PROGRAMDATA", "C:/ProgramData")) / "Epic/UnrealEngineLauncher/LauncherInstalled.dat"
        if index.is_file():
            try:
                entries = json.loads(index.read_text(encoding="utf-8")).get("InstallationList", [])
                candidates += [Path(item["InstallLocation"]) for item in entries
                               if str(item.get("AppName", "")).startswith("UE_")
                               and item.get("InstallLocation")]
            except (OSError, ValueError, TypeError, KeyError):
                pass
    candidates += [Path.home() / "UnrealEngine", Path("/opt/unreal-engine")]
    # Prefer the project's version, including installs on drives other than C:.
    # Retain a mismatched installation for doctor to explain the actual problem.
    candidates = [candidate for candidate in candidates
                  if (candidate / "Engine/Build/BatchFiles").is_dir()]
    for candidate in sorted(candidates, key=engine_matches, reverse=True):
        if (candidate / "Engine/Build/BatchFiles").is_dir():
            return candidate.resolve()
    return None


def windows_toolchain():
    import winreg
    vswhere = Path(os.environ.get("ProgramFiles(x86)", "C:/Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
    installations = []
    if vswhere.is_file():
        result = subprocess.run([str(vswhere), "-products", "*", "-requires",
                                 "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                                 "-property", "installationPath"], capture_output=True, text=True)
        installations = [Path(line.strip()) for line in result.stdout.splitlines() if line.strip()]
    compilers = [str(path) for installation in installations
                 for path in installation.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")]
    sdk_versions = []
    sdk = None
    try:
        with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, r"SOFTWARE\Microsoft\Windows Kits\Installed Roots",
                            0, winreg.KEY_READ | winreg.KEY_WOW64_32KEY) as registry:
            sdk = Path(winreg.QueryValueEx(registry, "KitsRoot10")[0])
        sdk_versions = [folder.name for folder in (sdk / "Include").glob("10.*")
                        if (folder / "um/Windows.h").is_file()
                        and (sdk / "Lib" / folder.name / "um/x64/kernel32.lib").is_file()]
    except OSError:
        pass
    return {"msvc_compilers": compilers, "windows_sdk_versions": sdk_versions,
            "windows_sdk_root": str(sdk) if sdk else None,
            "available": bool(compilers and sdk_versions)}


def engine_paths(engine):
    if sys.platform == "darwin":
        folder = engine / "Engine/Binaries/Mac"
        editor = folder / "UnrealEditor.app/Contents/MacOS/UnrealEditor"
        builder = engine / "Engine/Build/BatchFiles/Mac/Build.sh"
        automation = engine / "Engine/Build/BatchFiles/RunUAT.sh"
        target = "Mac"
    elif sys.platform == "win32":
        editor = engine / "Engine/Binaries/Win64/UnrealEditor.exe"
        builder = engine / "Engine/Build/BatchFiles/Build.bat"
        automation = engine / "Engine/Build/BatchFiles/RunUAT.bat"
        target = "Win64"
    else:
        editor = engine / "Engine/Binaries/Linux/UnrealEditor"
        builder = engine / "Engine/Build/BatchFiles/Linux/Build.sh"
        automation = engine / "Engine/Build/BatchFiles/RunUAT.sh"
        target = "Linux"
    return editor, builder, automation, target


def doctor(engine):
    status = {"platform": platform.platform(), "project": str(PROJECT),
              "engine": str(engine) if engine else None,
              "required_engine": REQUIRED_ENGINE,
              "engine_version": engine_version(engine) if engine else None,
              "engine_compatible": bool(engine and engine_matches(engine)),
              "engine_build_check": "not performed by doctor",
              "editor_playtest_check": "not performed by doctor"}
    if sys.platform == "darwin":
        result = subprocess.run(["/usr/bin/xcodebuild", "-version"], capture_output=True, text=True)
        status["xcode_available"] = result.returncode == 0
        status["xcode"] = (result.stdout or result.stderr).strip()
    elif sys.platform == "win32":
        status["windows_toolchain"] = windows_toolchain()
        status["compiler_hint"] = "Install Visual Studio: Game development with C++, MSVC x64/x86 and Windows SDK. UBT validates exact versions during build."
    else:
        status["compiler_hint"] = "UnrealBuildTool will validate the platform toolchain during build."
    if engine:
        editor, builder, _, _ = engine_paths(engine)
        status.update(editor_exists=editor.is_file(), build_script_exists=builder.is_file())
    status["bootstrap_complete"] = CONTENT_MARKER.is_file()
    if status["bootstrap_complete"]:
        import hashlib
        marker = json.loads(CONTENT_MARKER.read_text(encoding="utf-8"))
        if marker.get("layout_sha256") != hashlib.sha256((ROOT / "Design/city_layout.json").read_bytes()).hexdigest():
            raise RuntimeError("Saved map and source layout differ. Regenerate matching content before playing.")
    return status


def run(arguments):
    print("Running:", subprocess.list2cmdline([str(a) for a in arguments]), flush=True)
    # List arguments preserve project paths with spaces; no shell interpolation.
    subprocess.run([str(a) for a in arguments], cwd=ROOT, check=True)


def build(engine):
    from generate_city_layout import generate
    generate()
    _, builder, _, target = engine_paths(engine)
    run([builder, "DeliveryRushEditor", target, "Development", PROJECT, "-WaitMutex", "-NoHotReload"])


def prepare(engine):
    build(engine)
    editor, _, _, _ = engine_paths(engine)
    # Load an engine-provided map before our map has been authored.
    # The full-editor Python command waits for editor initialization.
    run([editor, PROJECT, "/Engine/Maps/Entry", "-unattended", "-nosplash",
         "-ExecutePythonScript=" + str(ROOT / "tools/bootstrap_unreal.py"), "-stdout"])
    map_file = ROOT / "Content" / (CURRENT_MAP.removeprefix("/Game/") + ".umap")
    if not map_file.is_file() or not CONTENT_MARKER.is_file():
        raise RuntimeError("Content generation did not complete. Inspect Saved/Logs/DeliveryRush.log.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("command", choices=["doctor", "build", "prepare", "editor", "play", "package"])
    parser.add_argument("--engine", help="Existing Unreal Engine root directory")
    args = parser.parse_args()
    engine = engine_root(args.engine)
    status = doctor(engine)
    if args.command == "doctor":
        print(json.dumps(status, indent=2, ensure_ascii=False))
        return 0
    if not engine:
        print("Unreal Engine was not found. Install Unreal Engine and the platform toolchain first.\n"
              "On Windows, see README.md and use Open Demo.cmd after setup.\n"
              "On Mac, install full Xcode as well. Use Open Demo.command.\n"
              "For a custom engine path: python tools/ue.py editor --engine <engine-directory>", file=sys.stderr)
        return 2
    if not status["engine_compatible"]:
        print(f"This project requires UE {REQUIRED_ENGINE}. Found {status['engine_version']} at {engine}.\n"
              "Install the matching engine in Epic Games Launcher. Existing maps must not be opened with an older engine.\n"
              "Custom location: python tools/ue.py editor --engine <engine-directory>", file=sys.stderr)
        return 2
    if sys.platform == "win32" and not status["windows_toolchain"]["available"]:
        print("Windows C++ toolchain is incomplete. In Visual Studio Installer, add Game development with C++,\n"
              "MSVC x64/x86 build tools and a Windows SDK. See README.md.", file=sys.stderr)
        return 2
    if sys.platform == "darwin" and not status["xcode_available"]:
        print("A full active Xcode installation is required. Current xcodebuild check failed:\n" + status["xcode"], file=sys.stderr)
        return 2
    if args.command == "prepare":
        prepare(engine)
        return 0
    if args.command == "build":
        build(engine)
        return 0
    if not status["bootstrap_complete"]:
        prepare(engine)
    else:
        build(engine)
    editor, _, automation, target = engine_paths(engine)
    if args.command in ("editor", "play"):
        flags = ["-game", "-windowed", "-ResX=1440", "-ResY=900"] if args.command == "play" else []
        if sys.platform == "darwin":
            # LaunchServices gives the editor/game a normal foreground Mac app
            # session. Executing its binary directly can leave keyboard focus
            # with the terminal even though a Metal window has been rendered.
            run(["/usr/bin/open", "-W", "-n", editor.parents[2], "--args",
                 PROJECT, CURRENT_MAP, *flags])
        else:
            run([editor, PROJECT, CURRENT_MAP, *flags])
    else:
        archive = ROOT / "Builds" / target
        architecture = ["-specifiedarchitecture=arm64"] if sys.platform == "darwin" and platform.machine() == "arm64" else []
        run([automation, "BuildCookRun", "-project=" + str(PROJECT), "-noP4", "-platform=" + target,
             "-unattended", *architecture,
             "-clientconfig=Development", "-build", "-cook", "-map=" + CURRENT_MAP,
             "-stage", "-pak", "-archive", "-nodebuginfo", "-archivedirectory=" + str(archive)])
        if sys.platform == "darwin":
            # UE 5.8's modern Xcode stage creates the complete bundle here.
            # UAT's archive can instead copy the binary-only Binaries/Mac app.
            staged = ROOT / "Saved/StagedBuilds/Mac/DeliveryRush.app"
            packaged = archive / "Mac/DeliveryRush.app"
            containers = Path("Contents/UE/DeliveryRush/Content/Paks")
            if not (staged / containers / "DeliveryRush-Mac.utoc").is_file():
                raise RuntimeError("The staged Mac app is missing cooked content.")
            run(["/usr/bin/ditto", staged, packaged])
            run(["/usr/bin/codesign", "--verify", "--deep", "--strict", packaged])
            print("Packaged Mac app:", packaged)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (subprocess.CalledProcessError, RuntimeError, OSError) as error:
        print("Delivery Rush setup stopped:", error, file=sys.stderr)
        sys.exit(1)
