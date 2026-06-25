# CHANGELOG
## 0.8.7
- Fixed sorting by "Activation" not working.
- Relaxed sort-argument syntax so that '!' may also be used as a prefix and there may be multiple '!' per argument.

## 0.8.6
- Fixed compatibility with latest GW update.
- Fixed secondary description not showing if matches were only in its header.
- Fixed weird characters ("]) appearing in skill descriptions.

## 0.8.5
- Fixed compatibility with latest GW update.

## 0.8.4
- Fixed compatibility with latest GW update.
- Made mod more resilient to future skill additions.

## 0.8.3
- Fixed exclusion filters being broken.

## 0.8.2
- Fixed compatibility with latest GW update.

## 0.8.1
- Fixed compatibility with GW 21st anniversary update.

## 0.8.0
- Fixed compatibility with latest GW update. (Sorry for the delay!)
- Updated ImGui to the latest version: 1.92.6.
- Added main menu fadeout. (To restore old behavior: Go to Settings/General and set 'Menu fadeout' to 0 s.)
- Added a setting to control roundness of UI elements.
- Added dynamic text coloring for highlighted text, so it is now possible to distinguish which color is underneath the highlighting.
- Added a color theme setting and settings to broadly tweak them.
- Added a new default style inspired by the Guild Wars UI. (May change in the future.) (To restore old look: Go to Settings/Style, use the "ImGui Redshifted" color theme and set Roundness to 0 px.)
- Font size can now be controlled through GW's interface size setting without having to restart the mod.

## 0.7.1
- Fixed compatibility with latest GW update.
- Renamed 'Attribute level' to 'Attribute rank'.

## 0.7.0
- Added a special sort target: "Matched" that sorts according to whatever is matched by the query, filter by filter. Also added an example in the help menu showcasing how this feature can be used.
- Made "Additional matches in x description" be a bit more relaxed.
- Fixed skill book not refreshing when changing attribute mode.
- Improved handling of '<', '>', ':' and '=' in queries when not part of a number or in an untargeted query.

## 0.6.3
- Fixed settings not being saved when closing GW before the mod.

## 0.6.2
- Made it possible to skip update (for debug purposes).
- Fixed a bug that could cause the crash message to be truncated.
- Relaxed some requirements when opening gw.exe. (Maybe it works on linux now?)
- Improved error reporting.

## 0.6.1
- Fixed a bug that could make the crash handler crash.
- Patched some holes where exceptions could escape.

## 0.6.0
- Added setting to hide focused character.
- Added skill ruleset selector (Mixed, PvE, PvP).
- Replace checkboxes with scope slider.
- Fixed main menu being too small (the other way this time).

## 0.5.7
- Fixed main menu being too small.
- Fixed UI msgs.

## 0.5.6
- Added button to open github repo.

## 0.4.6
- Devbump.

## 0.4.5
- Devbump.

## 0.4.4
- Devbump.

## 0.4.3
- Tweaked release script.

## 0.4.2
- Made buttons and widgets have ambient occlusion like GW.

## 0.4.1
- Added exe icon.
- Now logs version.

## 0.4.0
- Reworked release pipeline.
- Added skillbook help window.
- Other stuff.

## 0.3.2
Test release 3.

## 0.3.1
Improved safety of Skill click and drag.

## 0.3.0
Test release 2.

## 0.2.0
Test release.

## 0.1.0
First release.