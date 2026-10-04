# Native OBS input overlay

[中文](README.zh.md) · [Project home](../README.md)

A real `input-overlay` source plus the **“奶蛙 DFJK · 原生动作与粒子”** native filter. Global DFJK presses drive 16 arm combinations, the held-key laughing expression, and yellow particle fountains directly inside OBS. Streaming does not require the desktop executable, a browser source, or a background service.

## Quickly change OBS keys and the laughter shortcut

Run **MilkFrogKeySetup.exe** from this directory; the desktop companion is unnecessary. Click a gameplay position and press its replacement key. Click **“重开大笑键”** to record F5, Ctrl+Q, Ctrl+Shift+Q, or another chord, release all keys, then choose **“保存并应用”** (Save and apply).

The tool updates `keybindings.json`, `key-labels.png`, and `milk-frog.png`. The native filter switches bindings and keycap legends automatically in approximately 0.3 seconds, without restarting OBS. Reset DFJK / reset F5 also requires Save. Closing discards unsaved edits.

Supports distinct left/right Ctrl, Shift, Alt, and Windows keys, Space, and other function keys. Labels use at most three characters (LC/RC, LS/RS, LW/RW, SPC, ENT, etc.) and stay within the keycaps. Gameplay keys must be distinct and cannot overlap the laughter chord. Laughter chords support up to eight keys; holding does not repeat, and gameplay immediately cancels video and audio.

OBS gameplay bindings live in this directory independently of desktop gameplay bindings. The laughter shortcut is shared through `%LOCALAPPDATA%/MilkFrog/config.txt`. Keep the entire folder, including the unlabelled atlases and `milk-frog-unlabeled.png`; the key utility alone does not include the overlay artwork.

## Installation

1. Install the official [input-overlay plugin](https://github.com/univrsal/input-overlay/releases). Development verification used version 5.0.5.
2. Copy `milk-frog-native.dll` into your OBS installation's `obs-plugins/64bit/` directory and restart OBS. This binary targets **64-bit Windows, OBS 32.2.2**.
3. Keep this complete asset directory in a permanent location.
4. Add an **Input Overlay** source. Select `milk-frog.png` as its texture, `milk-frog.json` as its configuration, and local input.
5. Add the **“奶蛙 DFJK · 原生动作与粒子”** filter to that source and select the same JSON.
6. Adjust the source's scene position and size.
7. Add the native audio source **“奶蛙 · F5 笑声音频”**, select `laugh.wav`, and set audio monitoring to **Monitor Off** in Advanced Audio Properties.

Disable OBS preview for viewer-only visibility; the character remains in stream/recording output. Do not launch the desktop companion for this mode.

## Behavior

- DFJK: forearm presses, a laughing expression while held, and particle fountains. Releasing all keys restores the idle smile; existing particles fade in about 0.40 seconds.
- F5: play the nine-second transparent belly-laugh clip and replacement audio. Another press restarts; holding the trigger does not repeat. Any DFJK press stops the clip and audio immediately and restores the corresponding keypress pose. Completion returns to idle.
- Save a new **“重开大笑键”** shortcut, such as Ctrl+Q, in the desktop tray. OBS picks it up without restarting or requiring the companion to remain open.
- OBS gameplay mapping defaults to DFJK and can be changed with the included utility, independently of desktop gameplay preferences.

Keep PNG/JSON files, both atlases, `laugh-frames.mfa`, and `laugh.wav` together. Atlas paths are relative to the JSON. See the [plugin source guide](../source/obs-native/README.en.md) for building and [LICENSE.txt](LICENSE.txt) for the plugin license.
