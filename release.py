import subprocess
import sys
import re
from pathlib import Path

CHANGELOG_FILE_NAME = "CHANGELOG.md"
CHANGELOG_FILE = Path(CHANGELOG_FILE_NAME)
VERSION_FILE_NAME = "VERSION.txt"
VERSION_FILE = Path(VERSION_FILE_NAME)
BUILD_PRESET = "ReadyForGithub"

def read_version() -> str:
    """
    Reads the current version from VERSION.txt
    """
    if not VERSION_FILE.exists():
        raise RuntimeError(f"{VERSION_FILE} not found")
    
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not version:
        raise RuntimeError(f"{VERSION_FILE} is empty")
    return version


def write_version(new_version: str) -> None:
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
        raise RuntimeError(f"No version entry found at top of {CHANGELOG_FILE}")

    new_version = match.group(1)
    changelog_entry = match.group(2).strip() if match.group(2).strip() else "(No changelog entry found)"

    return new_version, changelog_entry

def preflight_checks():
    # Ensure clean tree
    unexpected = git_clean_except([CHANGELOG_FILE_NAME])
    if unexpected:
        print("❌ Error: Unexpected changes detected in git:")
        for f in unexpected:
            print(f"  {f}")
        sys.exit(1)

    try:
        # Ensure gh exists
        subprocess.run(["gh", "--version"], check=True, stdout=subprocess.DEVNULL)

        # Ensure cmake preset exists
        subprocess.run(["cmake", "--list-presets"], check=True, stdout=subprocess.DEVNULL)
    
    except subprocess.CalledProcessError as e:
        print(f"❌ Error: {e}")
        sys.exit(1)
    
    try:
        run(["gh", "auth", "status"], check=True)
    except:
        run(["gh", "auth", "login"], check=True)
        run(["gh", "auth", "status"], check=True)

def main():
    preflight_checks()

    old_version = read_version()
    new_version, changelog = get_version_and_changelog()

    print(f"\nOld version: {old_version}")
    print(f"New version: {new_version}\n")

    print("CHANGELOG for new version:")
    print(changelog)

    if not is_newer_version(old_version, new_version):
        print("\n❌ Error: new version is not greater than old version.")
        sys.exit(1)

    list_build_settings()

    # Single confirmation
    if input("\nProceed with update version, build release, create tag, commit, push and create GitHub release? [y/N]: ").strip().lower() != "y":
        print("Aborted.")
        sys.exit(0)

    # Step 1: update version
    write_version(new_version)

    try:
        run(["cmake", "--preset", "Production", "--fresh"], check=True)

        # Step 2: build the release zip
        print("\nBuilding release zip...")
        run(["cmake", "--build", "--preset", BUILD_PRESET], check=True)

        zip_path = Path(f"build/prod/RelWithDebInfo/HerosInsight_{new_version}.zip")
        if not zip_path.exists():
            raise RuntimeError(f"❌ Error: zip file not found at {zip_path}")
        
        # Step 3: create tag
        tag_str = f"v{new_version}"
        print(f"\nCreating tag \"{tag_str}\"...")
        run([
            "git", "tag",
            "-a", tag_str,
            "-m", f"HerosInsight {new_version}\n\n{changelog}"
        ], check=True)

        try:
            # step 4: stage changes
            print("\nStaging changes...")
            run(["git", "add", CHANGELOG_FILE, VERSION_FILE], check=True)

            # step 5: commit
            print("\nCommitting...")
            run(["git", "commit", "-m", f"Bump version to {new_version}"], check=True)

            # Step 6: push changes + tag
            print("\nPushing...")
            run(["git", "push", "--follow-tags"], check=True)

            # Step 7: create GitHub release
            print("\nCreating GitHub release...")
            run([
                "gh", "release", "create", tag_str, str(zip_path),
                "--title", f"\"HerosInsight {new_version}\"",
                "--notes-from-tag",
                "--fail-on-no-commits",
                "--verify-tag"
            ], check=True)

            print(f"\n✅ Release process completed successfully. Version {new_version} is now live. Dont forget to merge into main!")
        
        except:
            print("\n❌ Annoying fail: Manual rollback of some steps required.")

            # rollback tag creation
            print("\nUndoing tag...")
            run(["git", "tag", "-d", tag_str], check=True)
            raise

    except Exception:
        # rollback version change
        print("\nUndoing version change...")
        write_version(old_version)
        raise

if __name__ == "__main__":
    main()
