# ScaleToScreen – A KWin effect to scale windows (e.g. games) to fullscreen 
**ScaleToScreen** is a custom **KWin 6 effect** (written against KDE 6.6) that scales the active window to fullscreen. It's basically a **poor-man's gamescope** implemented as a KWin Effect. This is simply the first little project that came to mind when I decided to learn a little about the KWin internals and while it certainly might have its limits, it's actually working rather well if you need a quick-and-easy game upscaler.

## Build & Install
You have to install the necessary development packages on your distro and then you can build it with
```bash
make
```

If the plugin built correctly you'll have a `ScaleToScreen.so` inside the build/ directory. It depends on your distro where to put it, on Kubuntu 26.04 the correct directory is \
`/usr/lib/x86_64-linux-gnu/qt6/plugins/kwin/effects/plugins/`
Yes, that's correct. You might have to create the additional `plugins` directory under `effects`.

## Usage
Once installed, you'll find the plugin in `System Settings -> Desktop Effects -> Focus -> Scale to Screen`.

After enabling the plugin, you can press **`Ctrl + Alt + Shift + A`** \
to toggle the effect (The key combination can be changed in the system settings under `Window Management -> Scale the active window to fullscreen`). Once the focus changes to another window the effect ends and the window goes back to the previous state. It remembers the window though and will go back into scale-mode when you activate the window again.

## How it works
 - I didn't find a way to redirect input in a KWin plugin and worked around it by implementing a "Moving Window"-scaler. I move the actual window behind the upscaled image that's rendered to the screen, so that the mouse cursor overlaps with the physical pixels of the window.
 - The window itself is rendered through renderItem() using the scene's renderer, avoiding additional overhead.

## Known bugs

## Contributing
While this is a typical "it works for me"-project, I welcome all contributions, whether it's issue-reporting or patches.

## License

GPL
