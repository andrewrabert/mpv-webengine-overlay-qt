# MPV + WebEngine Overlay for Qt
Examples of a transparent Qt WebEngine overlaying mpv video playback.

![screenshot](screenshot.jpg)

| Example                      | Qt    | Notes                                                                                                                                              |
|------------------------------|-------|----------------------------------------------------------------------------------------------------------------------------------------------------|
| `example_qt5_opengl`         | Qt5   | libmpv (opengl)                                                                                                                                    |
| `example_qt6_nested_wayland` | Qt6.5 | embedded mpv process window via nested Wayland compositor                                                                                          |
| `example_qt6_opengl`         | Qt6.5 | libmpv (opengl)                                                                                                                                    |
| `example_qt6_vulkan`         | Qt6.7 | `vo=gpu` and vulkan via non-upstream [libmpv](https://github.com/andrewrabert/mpv/tree/libmpv-vulkan)                                              |
| `example_qt6_hdr_wayland`    | Qt6.7 | HDR10 via Wayland subsurface; `vo=gpu-next` and vulkan via non-upstream [libmpv](https://github.com/andrewrabert/mpv/tree/libmpv-vulkan-gpu-nextx) |

## Build
```bash
cd example_qt6_vulkan
cmake -B build
cmake --build build
./build/mpv-webengine-overlay video.mkv
```
