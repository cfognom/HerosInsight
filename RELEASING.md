# How to create a new release

## Build and test
1. Make a test build with `cmake --build --preset Test`.
2. Find the built launcher in [build/prod/RelWithDebInfo/HerosInsight/Launch_HerosInsight.exe](build/prod/RelWithDebInfo/HerosInsight/Launch_HerosInsight.exe). Run it while GW is running and test the build for any issues.

## Release
//OUTDATED
1. Document changes in [CHANGELOG.md](CHANGELOG.md).
2. Bump version in [CMakeLists.txt](CMakeLists.txt).
3. Make a git tag with with the new version.
4. Merge the dev branch into main.
5. Push changes to remote.
6. Run `cmake --build --preset ReadyForGithub`.
7. Make a release draft on GitHub.
    1. Assign the new tag.
    2. Paste the changes written in the [CHANGELOG.md](CHANGELOG.md).
    3. Add build/prod/RelWithDebInfo/HerosInsight_***[VERSION]***.zip as an asset.

Done.