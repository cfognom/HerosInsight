# How to create a new release
There are multiple python scripts that help to automate the release process.

## Build and test
1. Make a test build with `python build.py --preset prod --config RelWithDebInfo --installdir`.
2. Find the built launcher in [build/prod/RelWithDebInfo/HerosInsight/Launch_HerosInsight.exe](build/prod/RelWithDebInfo/HerosInsight/Launch_HerosInsight.exe). Run it while GW is running and test the build for any issues.

## Release
1. Document changes and add the desired version number in [CHANGELOG.md](CHANGELOG.md). You may leave these changes uncommitted as the script will commit them.
2. Run `python release.py stage` and follow any further instructions in the terminal. This will build a release and stage it in the [build/releases/[VERSION]](build/releases) directory. In the directory a zip-package of the mod will be created along an unpackaged mirror of the zip contents that can be used for testing. Additionally, a subdirectory for pdbs will be created.
3. Do a final test of the release using the local mirror. If issues are found you can undo the staged release by:
    1. Undo the last git commit.
    2. Remove the git tag named `v[VERSION]`.
    3. Delete the [build/releases/[VERSION]](build/releases) directory.
    
    After undoing, fix the issues and go back to step 1.

4. Run `python release.py publish` and follow any further instructions in the terminal. This will upload the release to GitHub. If successfull the new version of the mod is now live, and users will be able to download it when they start the mod.