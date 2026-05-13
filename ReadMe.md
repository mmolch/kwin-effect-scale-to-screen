# ScaleToScreen – A KWin Effect for Full‑Screen Window Scaling
**ScaleToScreen** is a custom **KWin 6 effect** (written against KDE 6.6) that temporarily scales the active window to fill the entire screen. It's basically a **poor-man's gamescope** implemented as a KWin Effect. This is simply the first little project that came to mind when I decided to learn a little about the KWin internals and while it certainly has its limits and bugs, it's actually working rather well if you need a quick-and-easy game upscaler.

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

Once you have enabled the plugin, you can press **`Ctrl + Alt + Shift + A`**
to upscale your currently active window. Once the focus changes to another window goes back to normal.

## Known bugs
 - The most obvious bug is a 1-pixel-jitter when moving the mouse if the display is set to a scale other than 1 and the window doesn't round well with the target rectangle. I found it to be not much of a problem though in most cases (your mileage may vary). If you don't use a mouse / touchpad in a game this bug is of no consequence to you.

## How it works
 - Unfortunately it's not possible to redirect input for a KWin plugin, so I had to implement a "Moving Window" scaler. I move the actual window behind the upscaled texture that's rendered to the screen so that the mouse cursor overlaps with the physical pixels of the window.
 - The window itself is rendered to a texture and then upscaled to show on the screen.


## Contributing
 - If you love linear algebra and came up with a fix for the jittering, please send it to me!!!
 - All other contributions are of course welcome as well :-)

## License

GPL
