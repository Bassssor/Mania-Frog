# OBS 简化安装验证

新增 `MilkFrogOBS.exe` 作为普通用户的双击入口；`obs-input-overlay/InstallObsEasy.ps1` 提供不弹窗的命令接口，`agent.md` 约定先明确目标再安装。新插件 `milk-frog-easy.dll` 注册独立的 `milk_frog_source`，不依赖旧 input-overlay 插件。

验证使用本机 OBS 33.0.1 的真实 `obs.dll`、D3D11 图形模块和音频输出，在独立进程内运行，不打开 OBS 界面，不改变现有 OBS 配置。前端“当前场景集合”与保存回调使用测试桩；插件加载、源创建、场景插入、GPU 渲染与音频回调均为真实 libobs 调用。

验证覆盖：

- PowerShell 5.1 静默列举目标；缺少目标、未知场景、非奶蛙同名源及不存在的更新对象均报错。
- 安装将场景配置和插件写入位置分开，覆盖标准版与便携版。错误目录里的自定义键位会迁移，重复安装保留键位，原场景 JSON 未被修改。
- 安装排队和状态查询返回真实状态，不把 pending 当作 complete。
- 安装器新增“查找 / 卸载奶蛙”入口，静默接口为 `-ListInstalled` 与 `-Action Uninstall -TargetId ...`。独立回归命令：`python source/tools/VerifyObsUninstall.py`。
- 隔离配置验证卸载发现共享源、嵌套场景、分组、无引用源、旧版滤镜与笑声音频；按 UUID 匹配引用，不把同名的其他插件源当作奶蛙。
- 运行 OBS、选择不明确、查找结果过期及清理时仍有未选奶蛙均在修改前拒绝。选择卸载会移除该源的全部场景引用、取消两个安装目录中对应的待安装请求，并保存原始配置备份。
- 旧版滤镜卸载保留宿主和其他滤镜；可选文件清理仅删除一键版专属目录，保留其他插件、桌宠共享配置与备份。覆盖便携版、手动删源后的遗留文件，以及自定义配置和目录联接的清理保护。
- 独立发现实际插件 / 素材目录，在界面与 `-ListInstalled` 的 `fileInstalls[]` 中显示路径、旧路径副本、文件数和大小。验证有源、无源时两份文件都可见，清理后列表为空，便携版不会误报标准版路径；无窗口构建真实界面控件并核对两处目录均已填入。专项结果见 `uninstall-report.json`。
- 使用 OBS 33.0.1 的插件扫描器从安装后的目录发现并加载奶蛙。单个源同时具备画面和音频，监听默认关闭。
- 只添加到指定集合与场景，其他集合的请求等待，不改投当前集合。
- 重复 Add 不重复添加；Update 保留位置和可见状态；源名称冲突时拒绝覆盖。
- 透明 GPU 截图与原始预览加键帽文字对比，透明度逐像素一致，RGB 最大误差不超过 2/255。
- 销毁场景及插件后无重复销毁错误。
- 紧凑大笑动画全部 216 帧的 alpha 与原始 PNG 帧逐字节一致，保持尺寸、帧率和时长；颜色为 WebP 95 有损编码。
- 无窗口测试直接调用生产版渲染和音频实现：216 帧 WIC 解码、首/中/末帧 GPU 透明度对比、共享时钟选帧、笑声非静音输出、敲键同时取消音画和九秒结束。测试不会安装键盘钩子或向系统发送按键。

重新验证：先运行 `source/tools/BuildObsEasy.ps1`，然后运行 `python source/tools/VerifyObsEasy.py --obs-root <OBS目录> --sdk-root <libobs头文件目录>`。原始运行日志、测试配置及截图保留在不纳入版本管理的 `source/build/obs-easy/verification/` 中。

重新生成紧凑动画（在 `source/` 下运行）：

```powershell
python tools/PrepareLaughPlayback.py encode-pack --input assets/milk-frog/laugh/laugh-frames.mfa --output ../obs-input-overlay/laugh-frames.mfa --codec webp --quality .95 --report build/obs-easy/laugh-encoding.json
```
