# MilkFrog 源码

[English](README.en.md)

成品位于上级 `MilkFrog.exe`；OBS 素材位于上级 `obs-input-overlay/`。
桌宠使用 SFML 2.6.2，OBS 原生动画滤镜使用 OBS 32.2.2 API。
`vendor/SFML-2.6.2/` 保留实际构建所需头文件、库及运行 DLL。

## 构建

在此目录用 PowerShell 运行 `tools/BuildFinal.ps1`，需要 Visual Studio C++ Build Tools、Python 和 Pillow。
它构建单文件 EXE、验证素材与标签、检查托盘和改键，再导出 OBS 图集，不生成 ZIP。
仅更新桌宠可运行 `build.ps1` 和 `python tools/BuildPortableExe.py`。
原生 OBS 插件单独运行 `tools/BuildObsNative.ps1`；脚本按需获取固定版本的 OBS 头文件。
替换已安装插件时先关闭 OBS；更新图集后需让 OBS 重新加载。

## 实现与验证

- `SFML/SFML/PortableLauncher.cpp`：将原生程序及运行文件作为 PE 资源内置到单个 EXE。
- `SFML/SFML/Source.cpp`：透明渲染、托盘、键位设置及键帽标签。
- `obs-native/`：原生 OBS 滤镜；说明见 `obs-native/README.zh.md`。
- `assets/milk-frog/`：保留的透明素材、角色参考及生成提示词。

配置位于 `%LOCALAPPDATA%/MilkFrog/config.txt`，与版本缓存独立。
标签最多三个字符，宽高分别限制在 52×26 原生像素，并保持中心和透视方向。
LC/RC、LS/RS、LA/RA、LW/RW 分别保留左右修饰键绑定。

验证入口：

```powershell
MilkFrog.exe --verify-labels output-directory
MilkFrog.exe --verify-controls output-directory
MilkFrog.exe --verify-assets output-directory --default-keys
MilkFrog.exe --smoke-test
```

标签测试覆盖 255 个虚拟键码 × 4 个键帽，逐像素检查文字没有超出键帽内缩区域。
托盘测试覆盖注册、Explorer 恢复、缩放、录入、取消、重复检查、保存重载和 16 种组合。
`--default-keys` 让 OBS 导出沿用既有 DFJK 绑定，不受桌宠个人配置影响。

最终验证记录位于上级 `verification/`。历史修改见 `MODIFICATIONS.md`，上游说明和致谢见 `UPSTREAM.md`。
许可证位于 `obs-native/LICENSE.txt` 与 `vendor/SFML-2.6.2/license.md`。



## OBS key setup utility

`tools/BuildObsKeySetup.ps1` builds the standalone `../obs-input-overlay/MilkFrogKeySetup.exe`. `tools/ExportInputOverlay.py` exports unlabelled atlas layers and preserves existing OBS bindings. The native plugin watches `keybindings.json`; legends are rendered behind fingers before particle composition. `shared/KeyNames.hpp` keeps desktop and OBS abbreviations consistent.

Verification: `MilkFrogKeySetup.exe --verify output-directory` checks 255 key names at four positions. `tools/VerifyObsKeySetup.py` exercises real UI capture, Ctrl+Shift+Q, save/reset/cancel, 16 remapped OBS poses, old-key removal, modifier distinction and video cancellation. Set `OBS_EXE` to your OBS executable before the live test.
