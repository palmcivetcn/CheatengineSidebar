# CE Sidebar Dock (Cheat Engine 7.6 plugin)

A CE plugin that shows every CE form in a sidebar and lets you click to switch between them. The sidebar is promoted to a normal Windows top-level window so it behaves like an independent window for easy toggling.

## Demo
https://github.com/user-attachments/assets/2f89748d-c55d-4219-aa8c-5f89030f22b2

## Features
- Hides the default "CE window list" menu entry and shows a compact CE sidebar window title instead.
- Promotes Lua forms to Windows top-level windows via the plugin layer (`WS_EX_APPWINDOW`), enabling Alt+Tab and multi-window switching.
- Bundles `ce_sidebar_reload()` for convenient hot-reloading of Lua scripts.

## Project layout
- `src/ce_sidebar_plugin.cpp`: Plugin implementation (C++ with the CE 7.6 SDK).
- `cepluginsdk.h`: SDK header extracted from `Cheat Engine\plugin\cepluginsdk.h` in the CE 7.6 install for direct builds.

## Usage
1. Download `ce_sidebar_plugin.dll` from the release.
2. Copy it into the `plugins` folder under your CE root directory.
3. In Cheat Engine, go to Edit -> Settings -> Plugins -> Add new.
<img width="692" height="584" alt="image" src="https://github.com/user-attachments/assets/03f638b1-53dd-4e5b-9849-563ce06f4aa6" />
4. Select the DLL you just copied and confirm.
5. Do not forget to check the plugin and click OK at the bottom.
6. Restart Cheat Engine and enjoy.

## Notes
- Built against the CE 7.6 API; other versions may differ.
- The plugin removes window owners and adds `WS_EX_APPWINDOW`. If you see issues with custom skins or multi-monitor setups, tweak `xOffset/yOffset` in Lua or remove the `WS_OVERLAPPEDWINDOW` flag in C++.
- Uninstall: remove the DLL from `plugins` and restart CE.

## Developer notes
There are still small bugs. PRs, issues, and suggestions are welcome; PRs will be reviewed when time permits. Core logic lives in `src`, though files are a bit messy.
