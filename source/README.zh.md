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

本地生成新素材：运行 `python tools/GenerateImage.py --prompt "素材描述"`，使用已配置的中转站；本次重生成使用 `--model gpt-image-2.5-sunburst`。密钥配置、参考图和透明背景用法见 [生图脚本说明](tools/GenerateImage.zh.md)。

配置位于 `%LOCALAPPDATA%/MilkFrog/config.txt`，与版本缓存独立。
支持 4K–8K 五种轨道模式；标签最多三个字符，各模式的宽高均限制在 31×26 原生像素，并保持中心和透视方向。
LC/RC、LS/RS、LA/RA、LW/RW 分别保留左右修饰键绑定。

验证入口：

```powershell
MilkFrog.exe --verify-lanes output-directory
MilkFrog.exe --verify-labels output-directory
MilkFrog.exe --verify-controls output-directory
MilkFrog.exe --verify-assets output-directory --default-keys
MilkFrog.exe --smoke-test
```

标签测试覆盖 255 个虚拟键码 × 4K–8K 的全部键帽位置，逐像素检查文字没有超出键帽内缩区域。
托盘测试覆盖注册、Explorer 恢复、缩放、录入、取消、重复检查、保存重载和 16 种组合。
`--default-keys` 让 OBS 导出沿用当前模式默认绑定：4K 为 DFJK，5K 为 SDFJK，6K 为 SDFJKL，7K 为 ASDFJKL，8K 为 ASDFJKL;。

最终验证记录位于上级 `verification/`。历史修改见 `MODIFICATIONS.md`，上游说明和致谢见 `UPSTREAM.md`。
许可证位于 `obs-native/LICENSE.txt` 与 `vendor/SFML-2.6.2/license.md`。



## OBS key setup utility

`tools/BuildObsKeySetup.ps1` builds the standalone `../obs-input-overlay/MilkFrogKeySetup.exe`. `tools/ExportInputOverlay.py` exports unlabelled atlas layers and preserves existing OBS bindings. The native plugin watches `keybindings.json`; legends are rendered behind fingers before particle composition. `shared/KeyNames.hpp` keeps desktop and OBS abbreviations consistent.

Verification: `MilkFrogKeySetup.exe --verify output-directory` checks 255 key names at all positions across 4K–8K. `tools/VerifyObsKeySetup.py` exercises real UI capture, Ctrl+Shift+Q, save/reset/cancel, 16 remapped OBS poses, old-key removal, modifier distinction and video cancellation. Set `OBS_EXE` to your OBS executable before the live test.

## Compact portable build

`python tools/BuildPortableExe.py` defaults to WebP colour quality 0.95 for the embedded laughter clip, with lossless alpha and unchanged dimensions/frame timing. It computes the actual DLL import closure and losslessly compresses PE files and WAV resources with the Windows Compression API. Original source frames and OBS assets remain unchanged. Pass `--image-quality 1.0` to retain the original fully lossless PNG frame pack. Visual and runtime checks: `../verification/executable-size/`.

OBS 拆分图集验收：`tools/VerifyObsParts.ps1 -ObsRoot <OBS安装目录>`，在独立 libobs/D3D11 上对照桌宠的全部 496 种组合，不修改已安装插件和用户场景。

The v1.2.0 release asset preserves the verified JPEG XR build (25,602,048 bytes). Use `--frame-codec jxr` for that codec; default source builds use WebP.

重新加工手势素材需要 Pillow、NumPy、SciPy、aggdraw。运行 `python tools/PrepareTargetHandArtwork.py`，输入位于 `assets/milk-frog/regenerated-hands/target-v11/`；验收后将 runtime-candidate 内的 hands-4k/6k/8k.png 复制至 `assets/milk-frog/four-finger/`。键盘由 `KeyboardSurface.hpp` 和 `build.ps1` 生成。退出姿态由 `tools/PrepareExitArtwork.py` 处理。`python tools/VerifyHandEdges.py` 检查前臂下缘接缝。

本地低占用版：`build.ps1` 自动运行 `tools/PackRuntimeHands.py`；`python tools/VerifyRuntimeHands.py` 验证 32 个姿态的像素与位置。实测与最终安装包检查见 `../verification/memory-storage-v18/README.md`。
