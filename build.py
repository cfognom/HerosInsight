#!/usr/bin/env python3
import argparse
import os
import subprocess
import sys
import zipfile
import tempfile

# Helpers
def run(cmd, cwd=None):
    print(f"> {' '.join(cmd)}")
    result = subprocess.run(cmd, cwd=cwd)
    if result.returncode != 0:
        sys.exit(result.returncode)

def ensure_absolute(path):
    return os.path.abspath(path) if not os.path.isabs(path) else path

def zip_directory(source_dir, zip_file):
    with zipfile.ZipFile(zip_file, 'w', zipfile.ZIP_DEFLATED) as zf:
        for root, dirs, files in os.walk(source_dir):
            for f in files:
                abs_path = os.path.join(root, f)
                rel_path = os.path.relpath(abs_path, source_dir)
                zf.write(abs_path, rel_path)

def get_cmake_cache_value(binary_dir, key):
    """
    Read a value from CMakeCache.txt.

    Raises:
        FileNotFoundError if CMakeCache.txt does not exist
        KeyError if the key is not found
    """
    cache_path = os.path.join(binary_dir, "CMakeCache.txt")

    if not os.path.exists(cache_path):
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

def get_cmake_presets():
    # Run the command
    result = subprocess.run(
        ["cmake", "--list-presets"],
        capture_output=True,
        text=True  # ensures output is a string, not bytes
    )

    if result.returncode != 0:
        raise RuntimeError(f"CMake failed: {result.stderr}")

    lines = result.stdout.splitlines()
    presets = []

    for line in lines:
        line = line.strip()
        if line.startswith('"') and line.endswith('"'):
            # Remove the quotes
            presets.append(line.strip('"'))

    return presets

configure_presets = get_cmake_presets()

# Argument Parser
parser = argparse.ArgumentParser(description="Build + Install CMake project with optional zip packaging")
parser.add_argument(
    '--preset', 
    '-p',
    choices=configure_presets,
    default='dev',
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
    '--outdir', 
    '-o',
    default='build/testbuild',
    help='Destination directory for install (relative to working dir unless absolute path)'
)
parser.add_argument(
    '--zip',
    action='store_true',
    help='Output as zip file'
)

args = parser.parse_args()

# Setup Paths
working_dir = os.getcwd()
output_dir = ensure_absolute(args.outdir)
os.makedirs(output_dir, exist_ok=True)

# Subdirectory for configure/build
build_dir = os.path.join(working_dir, "build")
binary_dir = os.path.join(build_dir, args.type)

project_name = get_cmake_cache_value(binary_dir, "CMAKE_PROJECT_NAME")
project_version = get_cmake_cache_value(binary_dir, "CMAKE_PROJECT_VERSION")

# # Step 1: Configure
# run([
#     "cmake",
#     "--preset", args.type
# ])

# Step 2: Build
run([
    "cmake",
    "--build", binary_dir,
    "--config", args.config
])

temp_dir = None
if args.zip:
    temp_dir = tempfile.TemporaryDirectory()
    virtual_output_dir = temp_dir.name
else:
    virtual_output_dir = output_dir
install_dir = os.path.join(virtual_output_dir, project_name)

# Step 3: Install
print (f"Installing to: {install_dir}")
run([
    "cmake",
    "--install", binary_dir,
    "--prefix", install_dir,
    "--config", args.config
])

# Step 4: Optional Zip
if args.zip:
    zip_name = f"{project_name}-{project_version}.zip"
    zip_path = os.path.join(output_dir, zip_name)
    zip_directory(virtual_output_dir, zip_path)
    print(f"Zip created: {zip_path}")

    temp_dir.cleanup()

print("✅ Build + Install complete!")
