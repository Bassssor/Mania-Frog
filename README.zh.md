# Mania-Frog 使用说明

[English](README.en.md)

双击 **MilkFrog.exe** 启动，无需安装，也无需在旁边放置 DLL 或角色素材。
右下角系统托盘出现奶蛙图标；Windows 可能将它收纳在 **^** 隐藏图标区。
右键图标可放大、缩小、置顶、修改键位及退出。右键角色打开相同菜单，左键拖动角色移动位置。

## 重开大笑键

按一次 F5 播放一次已确认的透明视频和 `laugh.wav` 笑声，共 9 秒。再次按下 F5 从头播放；按住 F5 的系统重复输入不会不停重播。
右键系统托盘图标选择“重开大笑键”，点击录入按钮，按下完整组合后松开全部按键，再点击“保存”。
支持 Ctrl+Q、Ctrl+Shift+Q 等最多 8 个按键的组合，左右 Ctrl/Shift/Alt 均可使用；组合必须完整匹配，额外修饰键不触发。
“恢复 F5”恢复默认；Esc 取消本次录入，“取消”不保存。新设置会持久保存，并在约 0.3 秒内同步到 OBS，无需重启 OBS。
重开大笑键不能与当前敲键键位重叠；按住组合键不会反复重播。
按下当前绑定的任意敲键键位会立即停止视频和笑声，恢复相应敲键动作；松开后回到待机，视频不会继续。
视频播放完自动恢复待机。若将 F5 本身设为敲键键位，则优先执行敲键动作。

## 修改键位

选择“修改键位…”，点击要更换的位置，再按一下目标键，最后点击“保存”。
四个位置默认是 D/F/J/K，对应角色的左手外侧、左手内侧、右手内侧、右手外侧。
四个键不能重复；“取消”不会保存，“恢复 D / F / J / K”可恢复默认。
保存后动作绑定和键帽文字立即同步。配置位于 `%LOCALAPPDATA%/MilkFrog/config.txt`，重启或更新 EXE 后保留。

| 按键 | 键帽标签 |
| --- | --- |
| 左 / 右 Ctrl | LC / RC |
| 左 / 右 Shift | LS / RS |
| 左 / 右 Alt | LA / RA |
| 左 / 右 Windows | LW / RW |
| Space / Enter / Backspace | SPC / ENT / BSP |
| Tab / Caps Lock | TAB / CAP |
| Page Up / Page Down / Home / End | PGU / PGD / HOM / END |
| Insert / Delete | INS / DEL |
| Num Lock / Scroll Lock / Print Screen / Pause | NUM / SCR / PRT / PAU |
| 数字小键盘 | N0–N9、N+、N-、N*、N/、N. |
| 功能键 | F1–F24 |

标签最多三个字符，居中显示并自动缩放到键帽内部，沿用原有透视方向。
左右修饰键独立绑定；设置中按 Esc 取消本次录入。其他少见硬件键使用 K 加两位十六进制键码。

## OBS 直播

按照 [OBS 中文说明](obs-input-overlay/README.zh.md) 添加真正的输入叠加源、原生动画滤镜和笑声音频源。直播时无需运行桌宠 EXE；关闭 OBS 预览后，角色只显示在直播或录制输出。将笑声音频监听设为关闭，可只让观众听到。

提供的插件面向 64 位 Windows、OBS 32.2.2。保留完整 `obs-input-overlay/` 目录，PNG、JSON、图集、`laugh-frames.mfa` 和 `laugh.wav` 不要分开移动。
OBS 当前敲键固定 DFJK，与桌宠的四键配置独立；“重开大笑键”共享桌宠配置。

## 文件

- `MilkFrog.exe`：可直接运行的单文件桌宠。
- `obs-input-overlay/`：OBS 输入叠加素材和插件。
- `source/`：最终源码、必要依赖、素材来源及构建工具。
- `verification/`：标签边界、组合键及 OBS 验证记录。
- `README.zh.md` / `README.en.md`：中英文说明。

可在 [Releases](https://github.com/Bassssor/Mania-Frog/releases/latest) 直接下载 EXE，完整项目的获取方法见 [首页](README.md)。
构建步骤见 [源码中文说明](source/README.zh.md)。不生成压缩包。
