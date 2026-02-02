import subprocess
import sys
import re
from pathlib import Path

CHANGELOG_FILE = Path("CHANGELOG.md")
BUILD_PRESET = "ReadyForGithub"
VERSION_FILE = Path("VERSION.txt")

def get_old_version() -> str:
    """
    Reads the current version from VERSION.txt
    """
    if not VERSION_FILE.exists():
        raise RuntimeError(f"{VERSION_FILE} not found")
    
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not version:
        raise RuntimeError(f"{VERSION_FILE} is empty")
    return version


def set_new_version(new_version: str) -> None:
    """
    Writes the new version to VERSION.txt
    """
    VERSION_FILE.write_text(new_version.strip() + "\n", encoding="utf-8")

def run(cmd, **kwargs):
    print(f"$ {' '.join(str(c) for c in cmd)}")
    return subprocess.run(cmd, **kwargs)

def get_current_branch():
    result = subprocess.run(["git", "branch", "--show-current"], capture_output=True, text=True)
    return result.stdout.strip()

def git_clean_except(allowed_files):
    result = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True)
    changes = [line[3:] for line in result.stdout.splitlines()]
    unexpected = [f for f in changes if f not in allowed_files]
    return unexpected

def list_build_settings():
    print(f"Using build preset: {BUILD_PRESET}")

def is_newer_version(old: str, new: str) -> bool:
    """Return True if `new` version is greater than `old` version."""
    old_parts = [int(x) for x in old.split(".")]
    new_parts = [int(x) for x in new.split(".")]

    # Extend the shorter list with zeros, e.g., 1.2 -> 1.2.0
    length = max(len(old_parts), len(new_parts))
    old_parts.extend([0] * (length - len(old_parts)))
    new_parts.extend([0] * (length - len(new_parts)))

    return new_parts > old_parts

def get_version_and_changelog():
    """
    Reads CHANGELOG.md and returns a tuple:
        (new_version: str, changelog_entry: str)
    
    Expects CHANGELOG.md to use headers like:
        ## 0.2.0
        - Some changes
    """
    content = CHANGELOG_FILE.read_text(encoding="utf-8")

    # Match first version header (## 0.2.0) and capture everything until the next header or EOF
    match = re.search(
        r'^##\s*([\d.]+)\s*\n(.*?)(?=^##\s|\Z)',
        content,
        re.MULTILINE | re.DOTALL
    )

    if not match:
        raise RuntimeError("No version entry found at top of CHANGELOG.md")

    new_version = match.group(1)
    changelog_entry = match.group(2).strip() if match.group(2).strip() else "(No changelog entry found)"

    return new_version, changelog_entry


def main():
    if get_current_branch() != "dev":
        print("❌ Error: script must be run on 'dev' branch.")
        sys.exit(1)

    old_version = get_old_version()
    new_version, changelog = get_version_and_changelog()

    print(f"\nOld version: {old_version}")
    print(f"New version: {new_version}\n")

    print("CHANGELOG for new version:")
    print(changelog)
    print()

    if not is_newer_version(old_version, new_version):
        print("❌ Error: new version is not greater than old version.")
        sys.exit(1)

    # Git pre-check
    unexpected = git_clean_except(["CHANGELOG.md"])
    if unexpected:
        print("❌ Error: Unexpected changes detected in git:")
        for f in unexpected:
            print(f"  {f}")
        sys.exit(1)

    list_build_settings()

    # Single confirmation
    if input("\nProceed with update version, build release, create tag, merge dev into main, push and create GitHub release? [y/N]: ").strip().lower() != "y":
        print("Aborted.")
        sys.exit(0)

    # Step 1: update version
    set_new_version(new_version)
    run(["cmake", "--preset", "Production", "--fresh"], check=True)

    # Step 2: build the release zip
    print("\nBuilding release zip...")
    run(["cmake", "--build", "--preset", BUILD_PRESET], check=True)
    zip_path = Path(f"build/prod/RelWithDebInfo/HerosInsight_{new_version}.zip")
    if not zip_path.exists():
        print(f"❌ Error: zip file not found at {zip_path}")
        sys.exit(1)

    # Step 3: commit & tag
    run(["git", "add", "CHANGELOG.md", "VERSION.txt"], check=True)
    run(["git", "commit", "-m", f"Bump version to {new_version}"], check=True)

    # Step 4: merge dev into main and push
    run(["git", "checkout", "main"], check=True)
    run(["git", "merge", "dev"], check=True)
    run(["git", "checkout", "dev"], check=True)

    # Step 5: create GitHub release
    run([
        "gh", "release", "create", f"v{new_version}", str(zip_path),
        "--title", f"HerosInsight {new_version}",
        "--generate-notes",
        "--notes", changelog,
        "--fail-on-no-commits"
    ], check=True)

    print("\n✅ Release process completed successfully.")

if __name__ == "__main__":
    main()
