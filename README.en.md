# Mania-Frog （4K Only）

[中文](README.zh.md)

Run **MilkFrog.exe** directly. Installation and adjacent DLL/image files are unnecessary.
A frog icon appears in the Windows notification area, possibly inside the **^** hidden-icons menu.
Right-click it to enlarge, shrink, toggle always-on-top, change key bindings, or exit.
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
The four default positions are D/F/J/K: the character's left outer, left inner, right inner, and right outer key.
Bindings must be distinct. “取消” discards changes; “恢复 D / F / J / K” restores the defaults.
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

Follow the [OBS installation guide](obs-input-overlay/README.en.md) to add a real Input Overlay source, the native animation filter, and the laughter audio source. The desktop EXE is unnecessary for streaming. Disable OBS preview to hide the character locally while keeping it in stream/recording output. Set the audio source to Monitor Off for output-only laughter.

The bundled plugin targets 64-bit Windows and OBS 32.2.2. Keep the entire `obs-input-overlay/` directory together. OBS defaults to DFJK. Run `obs-input-overlay/MilkFrogKeySetup.exe` to change its four gameplay keys and the laughter shortcut; saving updates legends and bindings without restarting OBS. Desktop gameplay preferences are independent; the laughter shortcut is shared.

## Files

- `MilkFrog.exe`: directly runnable, single-file desktop application.
- `obs-input-overlay/`: OBS input-overlay assets and native plugin.
- `source/`: final source, required dependencies, art references, and build tools.
- `verification/`: label-bounds, laughter-shortcut, and OBS verification records.
- `README.zh.md` / `README.en.md`: Chinese and English documentation.

Download the EXE from [Releases](https://github.com/Bassssor/Mania-Frog/releases/latest). For the complete project, use Code → Download ZIP or `git clone https://github.com/Bassssor/Mania-Frog.git`.
See the [source instructions](source/README.en.md) to rebuild. No ZIP archive is produced.
