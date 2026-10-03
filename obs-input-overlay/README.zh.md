# OBS 原生输入叠加

[English](README.en.md) · [项目首页](../README.md)

使用真正的 `input-overlay` 输入叠加源，配合“奶蛙 DFJK · 原生动作与粒子”原生滤镜。它在 OBS 内捕获全局 DFJK 并绘制 16 种手臂组合、按住时的大笑表情和黄色喷泉粒子。直播期间无需运行桌宠、网页或后台服务。

## 安装

1. 安装官方 [input-overlay 插件](https://github.com/univrsal/input-overlay/releases)。开发验证使用 input-overlay 5.0.5。
2. 将本目录的 `milk-frog-native.dll` 复制到 OBS 安装目录的 `obs-plugins/64bit/` 下，然后重启 OBS。提供的 DLL 面向 **64 位 Windows、OBS 32.2.2**。
3. 将本素材目录放在固定位置，保留所有文件，不要分开移动。
4. 添加“输入叠加”源：贴图选择 `milk-frog.png`，配置选择 `milk-frog.json`，输入源使用本机输入。
5. 打开该源的“滤镜”，添加“奶蛙 DFJK · 原生动作与粒子”，配置选择同一个 `milk-frog.json`。
6. 调整源的位置和大小。
7. 添加“奶蛙 · F5 笑声音频”原生音频源，文件选择本目录的 `laugh.wav`，在高级音频属性中将监听设为“监听关闭”。

仅让观众看到：关闭 OBS 预览，角色仍进入直播/录制输出；无需启动桌宠 EXE。

## 行为

- 按住 D/F/J/K：对应整条前臂按下、闭眼大笑、黄色喷泉粒子。全部松开恢复微笑，已产生的粒子约 0.40 秒内淡出。
- F5：播放一次 9 秒透明捧腹大笑视频与配音；再次按下从头播放，长按不重复触发。按任意 DFJK 立即停止视频与配音，恢复敲键动作。播放结束回到待机。
- 在桌宠托盘的“重开大笑键”中保存 Ctrl+Q 等新组合，OBS 会自动同步，无需重启或持续运行桌宠。
- OBS 敲键当前固定 DFJK，独立于桌宠的四键配置。

`milk-frog.json` 使用相对路径寻找图集。必须保留 `milk-frog.png`、JSON、两张 atlas PNG、`laugh-frames.mfa` 和 `laugh.wav`。构建方法见 [插件源码说明](../source/obs-native/README.en.md)，许可证见 [LICENSE.txt](LICENSE.txt)。
