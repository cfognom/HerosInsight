import subprocess
import sys
import re
import argparse
from pathlib import Path

CHANGELOG_FILE = Path("CHANGELOG.md")
VERSION_FILE = Path("VERSION.txt")
RELEASE_DIR = Path("build/releases")

def read_version_txt() -> str:
    """
    Reads the current version from VERSION.txt
    """
    if not VERSION_FILE.exists():
        raise RuntimeError(f"{VERSION_FILE} not found")
    
    version = VERSION_FILE.read_text(encoding="utf-8").strip()
    if not version:
        raise RuntimeError(f"{VERSION_FILE} is empty")
    return version

def write_version_txt(new_version: str) -> None:
    """
    Writes the new version to VERSION.txt
    """
    VERSION_FILE.write_text(new_version.strip() + "\n", encoding="utf-8")

def format_tag(version: str) -> str:
    return f"v{version}"

def get_current_branch():
    result = subprocess.run(["git", "branch", "--show-current"], capture_output=True, text=True)
    return result.stdout.strip()

def git_clean_except(allowed_files):
    result = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True)
    changes = [line[3:] for line in result.stdout.splitlines()]
    unexpected = [f for f in changes if f not in allowed_files]
    return unexpected

def is_newer_version(old: str, new: str) -> bool:
    """Return True if `new` version is greater than `old` version."""
    old_parts = [int(x) for x in old.split(".")]
    new_parts = [int(x) for x in new.split(".")]

    # Extend the shorter list with zeros, e.g., 1.2 -> 1.2.0
    length = max(len(old_parts), len(new_parts))
    old_parts.extend([0] * (length - len(old_parts)))
    new_parts.extend([0] * (length - len(new_parts)))

    return new_parts > old_parts

def fetch():
    subprocess.run(
        ["git", "fetch", "origin"],
        check=True,
        stdout=subprocess.DEVNULL
    )

def behind(branch="dev", other_branch="main"):
    """
    Returns True if 'other_branch' has commits 'branch' doesn't (we need to pull).
    """
    try:
        # Update remote refs
        subprocess.check_call(['git', 'fetch', 'origin'], 
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        
        # Count commits: origin/main ahead of local main
        result = subprocess.run(
            ['git', 'rev-list', '--count', f'{branch}..{other_branch}'],
            capture_output=True, text=True, check=True
        )
        
        ahead_count = int(result.stdout.strip())
        return ahead_count > 0
        
    except (subprocess.CalledProcessError, ValueError, FileNotFoundError):
        return False

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

def has_local_tag(tag: str) -> bool:
    result = subprocess.run(["git", "tag", "--list", tag], capture_output=True, text=True)
    return bool(result.stdout.strip())

def has_remote_tag(tag: str) -> bool:
    result = subprocess.run(["git", "ls-remote", "--tags", "origin", tag], capture_output=True, text=True)
    return bool(result.stdout.strip())

def has_remote_release(tag: str) -> bool:
    result = subprocess.run(["gh", "release", "view", tag], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return result.returncode == 0

def reset_to_before_commit(commit_hash: str) -> None:
    print(f"\nResetting to before commit {commit_hash}...")
    subprocess.run(["git", "reset", "--mixed", f"{commit_hash}~1"], check=True)

def head_is_tag(tag: str) -> bool:
    result = subprocess.run(["git", "tag", "--points-at", "HEAD"], capture_output=True, text=True)
    return result.stdout.strip() == tag

def get_release_staging_dir(version: str) -> Path:
    return RELEASE_DIR / version

def get_zip(version: str) -> Path:
    path = get_release_staging_dir(version) / f"HerosInsight-{version}.zip"
    if not path.exists():
        raise RuntimeError(f"❌ Error: zip file not found at {path}")
    return path

def stage_release(args):
    new_version, changelog = get_version_and_changelog()
    release_staging_dir = get_release_staging_dir(new_version)

    def build_local_release():
        print(f"\nBuilding {new_version} release...")
        subprocess.run([
            "python", "build.py",
            "--fresh",
            "--preset", "prod",
            "--config", "RelWithDebInfo",
            "--installdir", release_staging_dir,
            "--zipdir", release_staging_dir],
            check=True
        )
        get_zip(new_version)

    new_version_tag = format_tag(new_version)
    has_tag = head_is_tag(new_version_tag)
    if has_tag:
        # Shortcut in case we need to rebuild
        build_local_release()
        print(f"\n✅ Successfully staged local release with tag: {new_version_tag}.")
        sys.exit(0)

    unexpected = git_clean_except([__file__, CHANGELOG_FILE.name])
    if unexpected:
        print(f"❌ Error: The git directory must be clean, except for {CHANGELOG_FILE}.")
        for f in unexpected:
            print(f"  {f}")
        sys.exit(1)

    if (behind("dev", "main")):
        print(f"❌ Error: main branch is ahead of dev branch.")
        sys.exit(1)

    old_version = read_version_txt()
    
    if release_staging_dir.exists():
        print(f"❌ Error: local release already exists at {release_staging_dir}.")
        sys.exit(1)

    if has_local_tag(new_version_tag):
        print(f"❌ Error: local tag is on a different commit than HEAD.")
        sys.exit(1)

    print()
    print(f"Old version: {old_version}")
    print(f"New version: {new_version}")
    print()
    print("CHANGELOG for new version:")
    print(changelog)
    print()

    if not is_newer_version(old_version, new_version):
        print("❌ Error: new version is not greater than old version.")
        sys.exit(1)

    # Single confirmation
    if input(f"\nProceed with update {VERSION_FILE}, build release, commit and create tag? [y/N]: ").strip().lower() != "y":
        print("Aborted.")
        sys.exit(0)

    # Step 1: update version
    print("\nUpdating version...")
    write_version_txt(new_version)

    try:
        # Step 2: build the release
        build_local_release()

        # Step 3: commit
        print("\nCommitting...")
        subprocess.run(["git", "commit", CHANGELOG_FILE, VERSION_FILE, "-m", f"Bump version to {new_version}"], check=True)

        if not has_tag:
            try:
                # Step 4: create tag
                tag_str = format_tag(new_version)
                print(f"\nCreating tag \"{tag_str}\"...")
                subprocess.run([
                    "git", "tag",
                    "-a", tag_str,
                    "-m", changelog
                ], check=True)

                subprocess.run(["git", "checkout", "main"], check=True)
                subprocess.run(["git", "merge", "dev"], check=True)
                subprocess.run(["git", "checkout", "dev"], check=True)

                print(f"\n✅ Sucessfully staged local release with tag: {tag_str}.")
            
            except:
                reset_to_before_commit("HEAD")
                raise

    except Exception:
        print("\nUndoing version change...")
        write_version_txt(old_version)
        raise

def unstage_release(args):
    version = read_version_txt()
    tag_str = format_tag(version)

    if not has_local_tag(tag_str):
        print(f"\n❌ Error: there is no release to unstage.")
        sys.exit(1)
    
    if has_remote_tag(tag_str):
        print(f"\n❌ Error: cannot unstage a release that has been published.")
        sys.exit(1)
    
    if not head_is_tag(tag_str):
        print(f"\n❌ Error: HEAD is not at tag {tag_str}.")
        sys.exit(1)

    reset_to_before_commit(tag_str)

    print("\nRemoving tag...")
    subprocess.run(["git", "tag", "-d", tag_str], check=True)

    print("\nRestoring old version...")
    subprocess.run(["git", "restore", VERSION_FILE], check=True)

    print(f"\n✅ Successfully unstaged local release with tag: {tag_str}.")

def public_release(args):
    version = read_version_txt()
    tag_str = format_tag(version)

    if not has_local_tag(tag_str):
        print(f"\n❌ Error: there is no release to publish.")
        sys.exit(1)

    # Check if user is authenticated
    try:
        subprocess.run(["gh", "auth", "status"], check=True)
    except:
        subprocess.run(["gh", "auth", "login"], check=True)
        subprocess.run(["gh", "auth", "status"], check=True)

    if has_remote_release(tag_str):
        print(f"\n❌ Error: release {tag_str} already exists on GitHub.")
        sys.exit(1)
    
    title = f"Hero's Insight {version}"
    changelog = subprocess.run(["git", "tag", "--list", tag_str, "--format=%(contents)"], capture_output=True, text=True).stdout.strip()
    zip_path = get_zip(version)

    # Print info about release
    print()
    print("You are about to publish the following release:")
    print()
    print(f"title: {title}")
    print(f"version: {version}")
    print(f"zip_file: {zip_path}")
    print(f"changelog: \n{changelog}")
    print()
    print("Make sure you have tested the release before publishing.")
    print()
    
    # Single confirmation
    if input(f"Proceed with publishing release? [y/N]: ").strip().lower() != "y":
        print("Aborted.")
        sys.exit(0)

    if not has_remote_tag(tag_str):
        print("\nPushing...")
        subprocess.run(["git", "push", "--follow-tags"], check=True)

    try:
        print("\nCreating GitHub release...")
        subprocess.run([
            "gh", "release", "create", tag_str, str(zip_path),
            "--title", title,
            "--generate-notes",
            "--notes", changelog,
            "--fail-on-no-commits",
            "--verify-tag"
        ], check=True)

        print(f"\n✅ Release process completed successfully. Version {version} is now live!")

    except:
        print("\n❌ Failed to publish release. Fix any issues and try again.")
        raise

def main():
    # Argument Parser
    parser = argparse.ArgumentParser(prog="release", description="Manage releases")

    # Top-level subcommands: create, remove, publish
    subparsers = parser.add_subparsers(dest="action", required=True)

    # create
    stage_parser = subparsers.add_parser("stage", help="Stages a release")
    stage_parser.set_defaults(func=stage_release)

    # remove
    unstage_parser = subparsers.add_parser("unstage", help="Unstages a release")
    unstage_parser.set_defaults(func=unstage_release)

    # publish
    publish_parser = subparsers.add_parser("publish", help="Publish a release")
    publish_parser.set_defaults(func=public_release)

    # Parse and dispatch
    args = parser.parse_args()
    args.func(args)

if __name__ == "__main__":
    main()
