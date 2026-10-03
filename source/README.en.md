# MilkFrog source

[中文](README.zh.md)

The desktop executable is `../MilkFrog.exe`; OBS assets are in `../obs-input-overlay/`.
The desktop uses SFML 2.6.2. The native OBS filter targets OBS 32.2.2.
`vendor/SFML-2.6.2/` contains the required headers, libraries, and runtime DLLs.

## Build

Run `tools/BuildFinal.ps1` in PowerShell from this directory.
Requirements: Visual Studio C++ Build Tools, Python, and Pillow.
The script builds the single executable, validates artwork and labels, tests tray/keybinding controls,
and exports the OBS atlases. It creates no ZIP archive.
For a desktop-only update, run `build.ps1` followed by `python tools/BuildPortableExe.py`.
Build the native OBS plugin separately with `tools/BuildObsNative.ps1`; it downloads the fixed OBS headers as needed.
Close OBS before replacing its installed DLL. Reload OBS after atlas updates.

## Implementation and checks

- `SFML/SFML/PortableLauncher.cpp`: embeds the native renderer and the runtime files as PE resources.
- `SFML/SFML/Source.cpp`: transparent rendering, notification-area controls, key capture, and keycap legends.
- `obs-native/`: native OBS filter; see `obs-native/README.en.md`.
- `assets/milk-frog/`: transparent assets, character references, and generation prompts.

Preferences live in `%LOCALAPPDATA%/MilkFrog/config.txt`, independently of versioned runtime caches.
Legends contain at most three characters and fit within 52×26 native pixels, retaining their center and perspective.
LC/RC, LS/RS, LA/RA, and LW/RW preserve distinct left/right modifier bindings.

Verification entry points:

```powershell
MilkFrog.exe --verify-labels output-directory
MilkFrog.exe --verify-controls output-directory
MilkFrog.exe --verify-assets output-directory --default-keys
MilkFrog.exe --smoke-test
```

The label test rasterizes all 255 virtual-key codes at all four key positions and checks every visible legend pixel
against an inset keycap polygon. The control test covers tray registration/recreation, resizing, capture,
cancellation, duplicate rejection, save/reload, and all 16 remapped combinations.
`--default-keys` preserves the established OBS DFJK export independently of desktop preferences.

Final reports are in `../verification/`. See `MODIFICATIONS.md` for the development history and `UPSTREAM.md` for credits.
Licenses are in `obs-native/LICENSE.txt` and `vendor/SFML-2.6.2/license.md`.
