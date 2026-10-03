# Seedance 捧腹大笑视频

- `milk-frog-laugh-final-preview.mp4`：用于查看效果。棋盘格仅展示透明区域；原声已移除，仅使用提供的 laugh.wav。
- `milk-frog-laugh-final-transparent.webm`：实际透明素材，VP9 Alpha + Opus，720×720、24 fps、216 帧，视频部分正好 9 秒。
- `laugh.wav`：用户提供的替换笑声，保持原音量和起始静音，不叠加原声。
- `make_transparent.py`：可复现的本地视频处理程序，需要 NumPy、SciPy、OpenCV 和 FFmpeg。不会写入桌宠或 OBS 运行素材。
- `verify_media.py`：核对输出帧数、实际透明度以及编码后音频与 laugh.wav 的相关性。
- `verification.json`：视频规格及重新解码后的透明通道检查。

抠像通过轮廓内外区分背景，保留嘴巴和瞳孔里的黑色，并对外轮廓估计软透明度，减轻黑底残边。已处理移动的“豆包AI生成”水印。原片最后一秒桌沿出现明显阶梯状变形，因此按用户要求删除，保留 0–9 秒；音频同步截至 9 秒，实际笑声结束后的静音仍保留。

这段视频的真实 3D 表情、动作和形变比 HyperFrames 的现有素材动画更接近目标。HyperFrames 可合成和渲染素材，但不独立生成这样的角色动作。

这段视频已接入桌宠与 OBS 原生输入叠加。默认 F5 开始或从头播放，敲键立即中断；结束后恢复既有待机素材。原生视频帧保存在 `source/assets/milk-frog/laugh/` 与 `obs-input-overlay/`。

处理工具使用 PATH 中的 ffmpeg/ffprobe，也可通过 FFMPEG/FFPROBE 环境变量指定。

复现命令：

```powershell
python .\make_transparent.py "path/to/source.mp4"
```
