# Scale to Screen – A KWin effect to scale windows (e.g. games) to fullscreen 
**Scale to Screen** is a custom **KWin 6 effect** (written against KDE 6.6) that scales the active window to fullscreen. It's basically a **poor-man's gamescope** implemented as a KWin Effect. This is simply the first little project that came to mind when I decided to learn about the KWin internals and while it certainly might have its bugs and limits (read how it works and the bugs and issues below), it's actually working rather well at least for my purposes. If you need a quick-and-easy window up/down-scaler or if you're in the same situation as me and have older CPUs/GPUs with no or incomplete Vulkan support, just give it a try. Gamescope doesn't run on these at all. My reference platform here is my trusty old 14 years old Celeron N2840 7.5 Watts CPU, which can actually run Crysis fine through wine's D3D to OpenGL layer (it's absolutely playable at 800x500 with lowest settings :-)).

## Build & Install
### Build requirements
 - Ubuntu 26.04: `cmake ninja-build build-essential kwin-dev extra-cmake-modules libkf6globalaccel-dev libkf6i18n-dev pkgconf libdrm-dev`

### Building
```bash
cmake -G Ninja -B build
cmake --build build
```

### Installation
If the plugin built correctly you'll have a `ScaleToScreen.so` inside the `build/` directory. It depends on your Linux distribution where to put it. It needs to go into the qt6 plugins directory under `kwin/effects/plugins/`
You might have to create the additional `plugins` directory under `effects`.
- Ubuntu 26.04: `/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/`

## Usage
Once installed, you'll find the plugin in `System Settings -> Desktop Effects -> Focus -> Scale to Screen`.

After enabling the plugin, you can press **`Ctrl + Alt + Shift + A`** \
to toggle the effect (The key combination can be changed in the system settings under `Window Management -> Scale the active window to fullscreen`). Once the focus changes to another window the effect ends and the window goes back to the previous state. It remembers the window though and will go back into scale-mode when you activate the window again.

## How it works
 - I didn't find a way to redirect input in a KWin plugin and worked around it by implementing a "Moving Window"-scaler. I move the actual window behind the upscaled image that's rendered to the screen, so that the mouse cursor overlaps with the physical pixels of the window.
 - The window itself is rendered through renderItem() using the scene's renderer directly to te screen, avoiding additional overhead.

## Bugs and issues
 - The "Moving Window"-technique can be quirky at times while sometimes it doesn't work at all depending on the application or game. Your milage my vary.

## Contributing
While this is a typical "it works for me"-project, I welcome all contributions, whether it's issue-reporting or patches.
Ideas for contributions (not sorted in any way):
 - Allow one upscaled window per physical display
 - Configure things like aspect ratio or margins in the system settings
 - Window-specific profiles
 - Allow having multiple windows to be marked for scaling when they get the focus
 - …to be continued

## License

GPL
