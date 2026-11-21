# Milk Frog one-click OBS setup

[中文](README.zh.md) · [Project home](../README.md)

## Recommended: choose a destination and install

Run **MilkFrogOBS.exe** in the parent folder. Explicitly select the scene collection and scene, enter the source name, and click Install. For an existing unified Milk Frog source, choose Update and enter its exact name. On first installation, restart OBS normally and open the selected collection; the source is added only to the specified scene. If the plugin is already loaded, requests can apply immediately.

The single native source includes transparency, global keyboard input, particles, laughter video and audio. It needs no input-overlay plugin, manually selected assets, filter, separate audio source or desktop companion. Repeated imports reuse the existing source; updates preserve its transform and visibility. A same-named source of another type is rejected. Change bindings from the source properties. Audio monitoring defaults to off.

The installer uses the Windows OBS 33 plugin layout. It needs write access to `%ProgramData%\obs-studio\plugins\milk-frog-easy`, or `plugins\milk-frog-easy` inside a portable OBS installation. Keep the full download folder together until installation. Existing key bindings are preserved. For assistance through commands without popups, see [agent.md](../agent.md).

## Find and uninstall Milk Frog

Run **MilkFrogOBS.exe** and click **“查找 / 卸载奶蛙”** (Find / uninstall). The checklist shows each source's collection, name, type and all scene/group paths; hover to read the full locations. A shared source is removed from every listed location. The separate plugin/material directory list shows actual paths, sizes and legacy copies, even when no scene sources remain. Exit OBS normally, refresh the list, select the entries, then click **“执行卸载 / 清理”** (Uninstall / clean up). The installer does not terminate OBS.

Plugin files, OBS artwork and key bindings are kept by default. Select the optional cleanup checkbox to remove the one-click plugin directories as well, including the old copy under the configuration directory. Cleanup requires all remaining Milk Frog sources and pending imports to be selected or already removed. It also works if you previously deleted every source in OBS. Desktop shared settings, downloaded files, other plugins and manually installed legacy DLLs are preserved.

Scene configuration files and cancelled imports are backed up before removal; the completion message shows the backup folder. Legacy Milk Frog filters are removed without deleting their host sources. Legacy laughter audio and pending imports can be selected separately. For commands without windows, see [agent.md](../agent.md).

## Legacy manual integration

The instructions below apply to the older input-overlay integration. New installations should use the unified source above.

A real `input-overlay` source plus the **“奶蛙 DFJK · 原生动作与粒子”** native filter. 4K–8K gameplay inputs drive 16/32/64/128/256 hand combinations, the held-key laughing expression, and yellow particle fountains directly inside OBS. Streaming does not require the desktop executable, a browser source, or a background service.

## Quickly change OBS keys and the laughter shortcut

Run **MilkFrogKeySetup.exe** from this directory; the desktop companion is unnecessary. Select 4K–8K, then click a gameplay position and press its replacement key. Click **“重开大笑键”** to record F5, Ctrl+Q, Ctrl+Shift+Q, or another chord, release all keys, then choose **“保存并应用”** (Save and apply).

The tool updates `keybindings.json`, `key-labels.png`, and `milk-frog.png`. The native filter switches bindings and keycap legends automatically in approximately 0.3 seconds, without restarting OBS. Reset mode defaults / reset F5 also requires Save. Closing discards unsaved edits.

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
- Defaults are DFJK / SDFJK / SDFJKL / ASDFJKL / ASDFJKL; for 4K / 5K / 6K / 7K / 8K. OBS bindings are independent of desktop preferences.

Keep PNG/JSON files, `body-parts.png`, `desk-parts.png`, `hand-parts.png`, `shadow-parts.png`, `laugh-frames.mfa`, and `laugh.wav` together. Atlas paths are relative to the JSON. See the [plugin source guide](../source/obs-native/README.en.md) for building and [LICENSE.txt](LICENSE.txt) for the plugin license.
