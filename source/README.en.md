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
Legends contain at most three characters and fit within 31×26 native pixels, retaining their center and perspective.
LC/RC, LS/RS, LA/RA, and LW/RW preserve distinct left/right modifier bindings.

Verification entry points:

```powershell
MilkFrog.exe --verify-lanes output-directory --mode 4
MilkFrog.exe --verify-labels output-directory
MilkFrog.exe --verify-controls output-directory
MilkFrog.exe --verify-assets output-directory --default-keys
MilkFrog.exe --smoke-test
```

The label test rasterizes all 255 virtual-key codes at all positions across 4K–8K and checks every visible legend pixel
against an inset keycap polygon. The control test covers tray registration/recreation, resizing, capture,
cancellation, duplicate rejection, save/reload, and all 16 remapped combinations.
`--default-keys` uses the selected mode default bindings.
`tools/VerifyObsParts.ps1 -ObsRoot <OBS installation>` checks all 496 production filter compositions in an isolated D3D11 context; it does not change installed plugins or OBS scenes.

Final reports are in `../verification/`. See `MODIFICATIONS.md` for the development history and `UPSTREAM.md` for credits.
Licenses are in `obs-native/LICENSE.txt` and `vendor/SFML-2.6.2/license.md`.



## OBS key setup utility

`tools/BuildObsKeySetup.ps1` builds the standalone `../obs-input-overlay/MilkFrogKeySetup.exe`. `tools/ExportInputOverlay.py` exports unlabelled atlas layers and preserves existing OBS bindings. The native plugin watches `keybindings.json`; legends are rendered behind fingers before particle composition. `shared/KeyNames.hpp` keeps desktop and OBS abbreviations consistent.

Verification: `MilkFrogKeySetup.exe --verify output-directory` checks 255 key names at all positions across 4K–8K. `tools/VerifyObsKeySetup.py` exercises real UI capture, Ctrl+Shift+Q, save/reset/cancel, 16 remapped OBS poses, old-key removal, modifier distinction and video cancellation. Set `OBS_EXE` to your OBS executable before the live test.

## Compact portable build

`python tools/BuildPortableExe.py` defaults to WebP colour quality 0.95 for the embedded laughter clip, with lossless alpha and unchanged dimensions/frame timing. It computes the actual DLL import closure and losslessly compresses PE files and WAV resources with the Windows Compression API. Original source frames and OBS assets remain unchanged. Pass `--image-quality 1.0` to retain the original fully lossless PNG frame pack. Visual and runtime checks: `../verification/executable-size/`.

The v1.2.0 release asset preserves the verified JPEG XR build (25,602,048 bytes). Use `--frame-codec jxr` for that codec; default source builds use WebP.

Artwork preprocessing requires Pillow, NumPy, SciPy and aggdraw. Run `python tools/PrepareTargetHandArtwork.py` using the retained target-v11 references, inspect the results, then copy hands-4k/6k/8k.png from runtime-candidate to `assets/milk-frog/four-finger/`. `build.ps1` generates keyboards from `KeyboardSurface.hpp`. `PrepareExitArtwork.py` prepares the complete exit pose. `VerifyHandEdges.py` checks the lower forearm seam.

v1.2.1 memory and storage build: `build.ps1` automatically runs `tools/PackRuntimeHands.py`; `python tools/VerifyRuntimeHands.py` checks the pixels and placement of all 32 poses. Measurements and final package checks are in `../verification/memory-storage-v18/README.md`.
