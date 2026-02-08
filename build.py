#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import zipfile
import tempfile
import json
from pathlib import Path

# Helpers
def run(cmd, cwd=None):
    print(f"> {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        sys.exit(result.returncode)

def zip_directory(source_dir, zip_file):
    with zipfile.ZipFile(zip_file, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(source_dir):
            for f in files:
                abs_path = os.path.join(root, f)
                rel_path = os.path.relpath(abs_path, source_dir)
                zf.write(abs_path, rel_path)

def get_cmake_cache_value(binary_dir: Path, key):
    """
    Read a value from CMakeCache.txt.

    Raises:
        FileNotFoundError if CMakeCache.txt does not exist
        KeyError if the key is not found
    """
    cache_path = binary_dir / "CMakeCache.txt"

    if not cache_path.exists():
        raise FileNotFoundError(
            f"CMake cache not found at '{cache_path}'. "
            "Has the project been configured?"
        )

    prefix = f"{key}:"
    with open(cache_path, "r", encoding="utf-8") as f:
        for line in f:
            if line.startswith(prefix):
                # Format: KEY:TYPE=VALUE
                _, value = line.split("=", 1)
                return value.strip()

    raise KeyError(f"CMake cache key '{key}' not found in {cache_path}")

def load_cmake_presets(presets_file=Path("CMakePresets.json")):
    """
    Load CMakePresets.json as a Python object.
    """
    if not presets_file.exists():
        raise FileNotFoundError(f"{presets_file} not found")

    with open(presets_file, "r", encoding="utf-8") as f:
        return json.load(f)

def main():
    cmake_presets = load_cmake_presets()

    configure_presets = cmake_presets.get("configurePresets")
    configure_preset_names = [preset["name"] for preset in configure_presets]

    # Argument Parser
    parser = argparse.ArgumentParser(description="Build + Install CMake project with optional zip packaging")
    parser.add_argument(
        '--preset', 
        '-p',
        choices=configure_preset_names,
        default=configure_preset_names[0],
        help='Build configuration preset'
    )
    parser.add_argument(
        '--config', 
        '-c',
        choices=['Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel'],
        default='Debug',
        help='Build config'
    )
    parser.add_argument(
        '--installdir', 
        '-i',
        type=Path,
        nargs='?',
        default=None,
        const='auto',
        help='Destination directory for install (relative to working dir unless absolute path)'
    )
    parser.add_argument(
        '--zipdir',
        '-z',
        type=Path,
        nargs='?',
        default=None,
        const='auto',
        help='Destination directory for zip (relative to working dir unless absolute path)'
    )
    parser.add_argument(
        '--fresh',
        '-f',
        action='store_false',
        help='Rebuild project from scratch'
    )

    args = parser.parse_args()

    binary_dir: Path = None
    for preset in configure_presets:
        if preset["name"] == args.preset:
            binary_dir = Path(preset["binaryDir"])
    if not binary_dir:
        raise ValueError(f"Missing binaryDir field in preset: {args.preset}")
    
    working_dir = os.getcwd()
    
    def install_to(install_dir):
        run([
            "cmake",
            "--install", str(binary_dir),
            "--prefix", str(install_dir),
            "--config", args.config
        ])

    project_name = get_cmake_cache_value(binary_dir, "CMAKE_PROJECT_NAME")
    project_version = get_cmake_cache_value(binary_dir, "CMAKE_PROJECT_VERSION")

    # Auto dir is <binary_dir>/<config>
    autodir = binary_dir / args.config
    if args.installdir == 'auto':
        args.installdir = autodir
    if args.zipdir == 'auto':
        args.zipdir = autodir
    
    # Ensure absolute
    if args.installdir:
        args.installdir = args.installdir.absolute()
    if args.zipdir:
        args.zipdir = args.zipdir.absolute()

    output_dir = args.installdir

    # # Step 1: Configure
    # run([
    #     "cmake",
    #     "--preset", args.type
    # ])

    # Step 2: Build
    cmd = [
        "cmake",
        "--build", str(binary_dir),
        "--config", args.config
    ]
    if args.fresh:
        cmd.insert(1, "--fresh")
    run(cmd)

    # Optional Step 3: Install
    if args.installdir:
        install_dir = args.installdir / project_name
        install_to(install_dir)
        print (f"Installed to: {install_dir}")
    
    # Optional Step 4: Zip
    if args.zipdir:
        temp = tempfile.TemporaryDirectory()
        temp_dir = Path(temp.name)
        zip_name = f"{project_name}-{project_version}.zip"
        zip_path = args.zipdir / zip_name
        os.makedirs(args.zipdir, exist_ok=True)
        install_to(temp_dir / project_name)
        zip_directory(temp_dir, zip_path)
        print(f"Zip created: {zip_path}")

    print("✅ Build + Install complete!")

if (__name__ == "__main__"):
    main()
