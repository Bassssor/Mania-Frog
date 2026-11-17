# v1.2.0 边缘修复验收

前臂下缘的尖角来自手势处理范围边界。小指抬起改变了曲线，而范围外仍保留旧轮廓，形成约 8 个源图像素的阶跃。修复将前臂曲线分段，固定连接处切线，并将处理范围完整覆盖变形段。

- `comparison.png`：原生渲染的修复前后局部对照。
- `runtime-final-4k.png`：修复后的完整场景。
- `after-forearm-states.png`：15 个按下手势的前臂下缘。
- `final-poses-0.png`、`final-poses-1.png`：两只手的全部 32 个状态。
- `pose-fix.diff`：本次边缘修复的代码差异。
- `lane-report.json`：496 种组合、1776 个按下和 1776 个松开接触点检查通过。
- `exit-check.txt`：退出表情保持 600 ms、淡出 800 ms 检查通过。
- `package-manifest.json`：发布 EXE 的资源清单。

`tools/VerifyHandEdges.py` 在旧图集复现失败，新图集的 15 种按下手势在 3 个透明度阈值下通过。原生标签、透明窗口与缩放检查通过。OBS 496 种真实 GPU 合成透明度误差为 0、最大 RGB 误差为 1。两只手的全部 32 个姿态已目视检查。

发布 EXE 与已验收的本地“MilkFrog-边缘修正版.exe”逐字节一致：

- 大小：25,602,048 字节。
- SHA-256：54acf6312e44a762bf9817b1d47e75b6a323c109bc7722cf8f1a8523ccb1625e。
- 资源版本：ec2c5fb74c365afaad664f26。

发布源码基于远程 main 整合，保留已有 WebP 构建选项；附件保留本地验收时的 JPEG XR 打包，未在发布前切换编码。
