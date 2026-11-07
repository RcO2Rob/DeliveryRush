#!/usr/bin/env python3
"""Validate source configuration and execute the production C++ simulation.

These checks do NOT compile Unreal code, load a .umap, or verify rendering.
"""
import ast
import configparser
import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def simulation_command(folder):
    sources = [ROOT / "Source/DeliveryRush" / name for name in
               ("RideSimulation.cpp", "TerrainSurface.cpp", "LevelLayout.cpp")]
    sources.append(ROOT / "tests/ride_checks.cpp")
    executable = str(Path(folder) / ("ride_checks.exe" if sys.platform == "win32" else "ride_checks"))
    environment = os.environ.copy()
    if sys.platform == "win32" and not os.environ.get("CXX"):
        from ue import windows_toolchain
        toolchain = windows_toolchain()
        if not toolchain["available"]:
            raise RuntimeError("Install MSVC x64/x86 build tools and Windows SDK in Visual Studio Installer first")
        compiler = Path(sorted(toolchain["msvc_compilers"])[-1])
        msvc = compiler.parents[3]
        sdk = Path(toolchain["windows_sdk_root"])
        version = sorted(toolchain["windows_sdk_versions"])[-1]
        environment["PATH"] = str(compiler.parent) + os.pathsep + environment.get("PATH", "")
        environment["INCLUDE"] = os.pathsep.join(str(path) for path in
            [msvc / "include", *(sdk / "Include" / version / part for part in ("ucrt", "shared", "um"))])
        environment["LIB"] = os.pathsep.join(str(path) for path in
            [msvc / "lib/x64", *(sdk / "Lib" / version / part / "x64" for part in ("ucrt", "um"))])
        command = [str(compiler), "/nologo", "/std:c++20", "/permissive-", "/EHsc", "/W4", "/WX", "/utf-8",
                   "/fsanitize=address", "/Zi", "/I" + str(ROOT / "Source/DeliveryRush"),
                   *map(str, sources), "/Fe:" + executable]
        return command, executable, environment, ["address"]
    compiler = os.environ.get("CXX") or shutil.which("clang++") or shutil.which("g++")
    if not compiler:
        raise RuntimeError("clang++ or g++ is required for standalone simulation checks")
    command = [compiler, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-pedantic",
               "-fsanitize=address,undefined", "-fno-omit-frame-pointer", "-g",
               "-I", str(ROOT / "Source/DeliveryRush"), *map(str, sources), "-o", executable]
    return command, executable, environment, ["address", "undefined"]


def main():
    from generate_city_layout import generate
    layout = generate()
    project = json.loads((ROOT / "DeliveryRush.uproject").read_text(encoding="utf-8"))
    assert project["Modules"][0]["Name"] == "DeliveryRush"
    for path in (ROOT / "tools").glob("*.py"):
        ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    engine = configparser.ConfigParser(strict=False)
    engine.read(ROOT / "Config/DefaultEngine.ini", encoding="utf-8")
    assert engine["/Script/EngineSettings.GameMapsSettings"]["GlobalDefaultGameMode"] == "/Script/DeliveryRush.DeliveryGameMode"
    maps = engine["/Script/EngineSettings.GameMapsSettings"]
    assert maps["GameDefaultMap"] == layout["map"] and maps["EditorStartupMap"] == layout["map"]
    packaged_maps = {line.strip() for line in (ROOT / "Config/DefaultGame.ini").read_text(encoding="utf-8").splitlines()
                     if line.startswith("+MapsToCook=")}
    assert '+MapsToCook=(FilePath="' + layout["map"] + '")' in packaged_maps
    assert '+MapsToCook=(FilePath="/Game/Maps/DeliveryPark")' in packaged_maps
    marker = ROOT / "Content" / layout["marker"]
    if marker.is_file():
        assert json.loads(marker.read_text(encoding="utf-8"))["layout_sha256"] == hashlib.sha256((ROOT / "Design/city_layout.json").read_bytes()).hexdigest()
    with tempfile.TemporaryDirectory(prefix="delivery-checks-") as folder:
        command, executable, environment, sanitizers = simulation_command(folder)
        subprocess.run(command, cwd=folder, env=environment, check=True)
        result = subprocess.run([executable], cwd=folder, env=environment, capture_output=True, text=True,
                                encoding="utf-8", errors="replace")
        print(result.stdout, end="")
        print(result.stderr, end="", file=sys.stderr)
        if result.returncode:
            return result.returncode
    report = {
        "checked": ["project descriptor", "Python syntax", "default game mode", "startup/package map consistency", "saved layout hash if present", "production C++ simulation"],
        "platform": sys.platform,
        "compiler": command[0],
        "sanitizers": sanitizers,
        "engine_build": "NOT RUN by this tool: standalone simulation only",
        "map_generation": "NOT RUN: requires Unreal Editor",
        "editor_runtime_and_visuals": "NOT RUN",
        "core_output": result.stdout.strip().splitlines(),
    }
    folder = ROOT / "docs/validation"
    folder.mkdir(parents=True, exist_ok=True)
    report_name = "core-checks-windows.json" if sys.platform == "win32" else "core-checks.json"
    (folder / report_name).write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print("Standalone simulation checks passed. See the separate Unreal validation record for engine results.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
