# Mania-Frog (4K–8K)

[中文](README.zh.md)

Run **MilkFrog.exe** directly. Installation and adjacent DLL/image files are unnecessary.
A frog icon appears in the Windows notification area, possibly inside the **^** hidden-icons menu.
Right-click it to select 4K–8K, resize, toggle always-on-top, change key bindings, or exit.
The character offers the same context menu. Drag it with the left mouse button to move it.
The application menus currently use Chinese labels.

## Restart laughter hotkey

Press F5 to play the approved 9-second transparent video once, with the replacement `laugh.wav` audio.
Press F5 again to restart from the beginning; holding it does not repeatedly restart playback.
Right-click the tray icon and choose “重开大笑键” (Restart laughter hotkey). Click the capture button,
press the complete combination, release every key, and save. Ctrl+Q and Ctrl+Shift+Q are supported,
with up to eight keys. Left/right Ctrl, Shift and Alt work interchangeably; extra modifiers do not match.
Restore F5 resets the default; Esc cancels capture and Cancel discards changes. Settings persist and
update the OBS trigger within approximately 0.3 seconds without restarting OBS.
The chosen hotkey must not overlap the current gameplay bindings.
Any currently bound gameplay key immediately stops video and audio and restores that key's ordinary pose.
Releasing it returns to idle without resuming the video. Completion returns to idle automatically.
If F5 is bound as a gameplay key, gameplay takes precedence.

OBS uses the same F5 playback in the native input overlay. Its paired “奶蛙 · F5 笑声” audio source
has monitoring disabled, so viewers hear the laughter without playing it locally.
Keep `laugh-frames.mfa` and `laugh.wav` in `obs-input-overlay` along with the existing preset files.

## Change key bindings

Choose “修改键位…” (Change keys), click a position, press the desired key, then click “保存” (Save).
Select a lane mode first. Defaults: 4K DFJK, 5K SDFJK, 6K SDFJKL, 7K ASDFJKL, 8K ASDFJKL;. Each mode saves its own bindings.
Each hand has four short rounded fingers, with thumbs hidden. The character's left hand is on screen-right.
4K uses left middle/index and right index/middle. 5K adds left ring before them; 6K adds right ring after them.
7K maps left pinky/ring/middle/index to lanes 1–4 and right index/middle/ring to lanes 5–7. 8K adds right pinky as lane 8.
Bindings must be distinct. “取消” discards changes; “恢复当前模式默认键” restores the selected mode defaults.
Saving updates both the input mapping and the keycap legends immediately.
Preferences persist across restarts and EXE updates in `%LOCALAPPDATA%/MilkFrog/config.txt`.

| Key | Legend |
| --- | --- |
| Left / right Ctrl | LC / RC |
| Left / right Shift | LS / RS |
| Left / right Alt | LA / RA |
| Left / right Windows | LW / RW |
| Space / Enter / Backspace | SPC / ENT / BSP |
| Tab / Caps Lock | TAB / CAP |
| Page Up / Page Down / Home / End | PGU / PGD / HOM / END |
| Insert / Delete | INS / DEL |
| Num Lock / Scroll Lock / Print Screen / Pause | NUM / SCR / PRT / PAU |
| Numeric keypad | N0–N9, N+, N-, N*, N/, N. |
| Function keys | F1–F24 |

Legends contain at most three characters. They stay centered, face the character, and scale to fit inside the keycap.
Left and right modifiers are separate bindings. Esc cancels an ongoing capture.
Uncommon hardware keys use K followed by a two-digit hexadecimal key code.

## OBS streaming

**Windows OBS 33: download and extract the [OBS installer ZIP](https://github.com/Bassssor/Mania-Frog/releases/latest/download/MilkFrogOBS.zip), run `MilkFrogOBS.exe`, and choose the collection, scene, and source name.** Restart OBS normally after the first install. The source includes the transparent character, key animations, particles, and laughter audio and video. Change key bindings in the source properties. For installation through commands with no popups, read [agent.md](agent.md). The following describes the legacy integration.

Follow the [OBS installation guide](obs-input-overlay/README.en.md) to add a real Input Overlay source, the native animation filter, and the laughter audio source. The desktop EXE is unnecessary for streaming. Disable OBS preview to hide the character locally while keeping it in stream/recording output. Set the audio source to Monitor Off for output-only laughter.

The bundled plugin targets 64-bit Windows and OBS 32.2.2. Keep the entire `obs-input-overlay/` directory together. OBS defaults to DFJK. Run `obs-input-overlay/MilkFrogKeySetup.exe` to select 4K–8K and change gameplay keys and the laughter shortcut; saving updates legends and bindings without restarting OBS. Desktop gameplay preferences are independent; the laughter shortcut is shared.

## Files

- `MilkFrog.exe`: directly runnable, single-file desktop application.
- `obs-input-overlay/`: OBS input-overlay assets and native plugin.
- `source/`: final source, required dependencies, art references, and build tools.
- `verification/`: label-bounds, laughter-shortcut, and OBS verification records.
- `README.zh.md` / `README.en.md`: Chinese and English documentation.

Download the EXE from [Releases](https://github.com/Bassssor/Mania-Frog/releases/latest). For the complete project, use Code → Download ZIP or `git clone https://github.com/Bassssor/Mania-Frog.git`.
See the [source instructions](source/README.en.md) to rebuild. No ZIP archive is produced.

## v1.2.2 OBS installer and smaller download

The OBS ZIP shrank from 60.16 MB to approximately **14.66 MB**. Laughter uses WebP quality 95 for color and lossless alpha, retaining 720×720, 24 fps, 216 frames, and nine seconds. Idle/key artwork and WAV audio are unchanged. [Download v1.2.2](https://github.com/Bassssor/Mania-Frog/releases/tag/v1.2.2).

## v1.2.1 memory and storage improvements

v1.2.1 includes memory and storage improvements. The EXE is 14,923,264 bytes, 41.7% smaller than v1.2.0. Cropped hand/shadow textures, a compact runtime atlas and an exiting bootstrap process reduce resident memory. Measured idle working set including the launcher fell from about 201 MB to 97 MB on this machine. All 194 state/scale/label PNGs match v1.2.0 pixel-for-pixel. The clip uses WebP quality 0.95 with lossless alpha.

Download [v1.2.1](https://github.com/Bassssor/Mania-Frog/releases/tag/v1.2.1). See the [optimization record](verification/memory-storage-v18/README.md) for measurements and validation.
