# Scale to Screen – A KWin effect to scale windows (e.g. games) to fullscreen 
**Scale to Screen** is a **KWin 6 effect** (written against KDE 6.6) that scales the active window to fullscreen. It's probaby most comparable to **magpie** on Winwodws or some kind of **poor-man's gamescope**. This is simply the first little project that came to mind when I decided to learn about the KWin internals and while it certainly might have its bugs and limits (read how it works and the bugs and issues below), it's actually working rather well at least for my purposes. If you need a quick-and-easy window up/down-scaler or if you're in the same situation as me and have older CPUs/GPUs with no or incomplete Vulkan support, just give it a try. Gamescope doesn't run on these at all. My reference platform here is my trusty old 14 years old Celeron N2840 7.5 Watts CPU, which can actually run Crysis through wine's D3D to OpenGL layer (it's playable at 800x500 with lowest settings :-)).

## Disclaimer
Please be aware that this plugin **might cause crashes**. As I wrote in the introduction, I'm completely new to the KWin-codebase and have to rely pretty much only on the headers to figure stuff out. Documentation is sparse and even if there *is* some documentation it's usually vastly outdated. You have been warned. That being said, I've been using it for quite some time without any issues.

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

![Scale to Screen in the system settings](img/system_settings_scale_to_screen.jpg)

After enabling the plugin, you can press **`Ctrl + Alt + Shift + A`** \
to toggle the effect (The key combination can be changed in the system settings under `Window Management -> Scale the active window to fullscreen`). Once the focus changes to another window the effect ends and the window goes back to the previous state. It remembers the window though and will go back into scale-mode when you activate the window again.

## How it works
 - I didn't find a way to redirect input in a KWin plugin and worked around it by implementing a "Moving Window"-scaler. I move the actual window behind the upscaled image that's rendered to the screen, so that the mouse cursor overlaps with the physical pixels of the window.
 - The window itself is rendered directly to the viewport vie renderItem() using the scene's renderer, avoiding any additional overhead.

## Bugs and issues
 - The "Moving Window"-technique can be quirky at times while sometimes it doesn't work at all depending on the application or game. Your milage my vary.
 - The mouse remains constrained when invoking other effects like the workspace grid while the window is scaled.

## Contributing
While this is a typical "it works for me"-project, I welcome all contributions, whether it's issue-reporting or patches.
Ideas for contributions (not sorted in any way):
 - Allow one scaled window per physical display
 - Configure things like aspect ratio or margins in the system settings
 - Window-specific profiles
 - …to be continued

## License

GPL
