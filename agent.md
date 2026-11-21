# 奶蛙 OBS 智能体安装与卸载说明

本文件供帮助用户安装或卸载奶蛙的智能体使用。只执行命令，不打开安装界面、资源管理器、桌宠、浏览器或 OBS 窗口。

## 先确认目标

1. 安装脚本是本目录的 `obs-input-overlay/InstallObsEasy.ps1`，双击入口是 `MilkFrogOBS.exe`。智能体只用脚本，**不要运行 EXE，也不要省略静默参数**。脚本兼容 Windows PowerShell 5.1 和 PowerShell 7。
2. 用下列只读命令列出实际的场景集合、场景和源：

   ```powershell
   powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./obs-input-overlay/InstallObsEasy.ps1 -ListTargets -Quiet -Json
   ```

3. 根据用户明确指定的目标匹配 `collection`、`scene`、`sources[].name`。用户没有说明放在哪个场景时，简短询问；**不要默认当前场景、列表第一项或另建场景**。源的默认建议名称可以是“奶蛙”，必须在执行前说明。用户要更新已有源时，必须确认具体源名。
4. “源”是要添加或更新的奶蛙源，不是把奶蛙作为滤镜安装在游戏采集、摄像头等其他源上。`Add` 创建或复用一体奶蛙源；`Update` 只允许更新指定场景内已有的 `milk_frog_source`。旧 `input-overlay` 源不会被替换。

## 静默安装

将以下占位值替换为刚确认的真实名称；所有名称按精确值传递。不要将用户文本拼接成可执行 PowerShell 代码，优先使用参数数组。

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./obs-input-overlay/InstallObsEasy.ps1 -Collection '场景集合名称' -Scene '目标场景名称' -SourceName '奶蛙' -Action Add -Quiet -Json
```

更新用 `-Action Update`，并填写已有一体奶蛙源的真实名称。带便携标记的 OBS 可传 `-ObsRoot 'OBS安装目录'`，运行中的便携实例可自动识别。自定义场景配置目录可用 `-ConfigRoot` 指定；它不改变插件安装位置，卸载时会修改所选配置中的奶蛙引用。

此安装包使用 Windows OBS 33 的目录结构。标准版插件和素材放在 `%ProgramData%\obs-studio\plugins\milk-frog-easy`，便携版放在 OBS 安装目录的 `plugins\milk-frog-easy`。运行安装命令的账户需要对该目录有写权限。安装后可以移动下载目录，已有键位会保留。之前安装到配置目录的版本可重新执行安装命令修复。

## 结果核验

- `status=complete`：OBS 已完成指定场景、指定源的导入；`added=false` 表示复用现有源，没有重复添加。
- `status=restart_required`：文件已安装，但当前 OBS 尚未加载该插件。不要说源已经可用。告诉用户下次正常重启 OBS、打开指定场景集合后自动完成。**不弹窗，不擅自关闭或重启 OBS，不启动任何 OBS 窗口**。
- `status=pending`：请求已排队，通常 OBS 未运行或尚未打开目标场景集合。目标是请求中明确指定的场景，不会改投当前场景。不要把排队称为已经导入。
- `status=error` 或进程退出码非零：按 `message` 处理错误，不重复盲试，不覆盖其他源。

保留响应中的 `requestId`。需要进一步确认时执行：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./obs-input-overlay/InstallObsEasy.ps1 -Status -RequestId '响应中的32位requestId' -Quiet -Json
```

只有拿到 `complete` 才能报告“已添加”。若必须在 OBS 关闭且不打开任何窗口的条件下安装，完成文件安装与指定目标排队即可，明确报告“已安装，等待下次启动 OBS 生效”。

## 静默查找与选择卸载

仅在用户明确要求卸载时执行。先用只读命令查找，列出真实的场景集合、源类型和全部位置：

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./obs-input-overlay/InstallObsEasy.ps1 -ListInstalled -Quiet -Json
```

响应中的每个 `targets[]` 对应一个场景集合内的奶蛙源，`locations[]` 显示“场景 / 嵌套场景或分组 / 源”；没有场景引用的源和待安装请求也会列出。识别依据是奶蛙类型 ID，不按名称模糊匹配。**同一个源可能被多个场景共用，选中后会从列出的全部位置移除。** 旧版奶蛙滤镜只移除滤镜，保留承载它的其他源；旧版笑声音频单独列出。

`fileInstalls[]` 独立列出实际存在的一键版插件 / 素材目录，包括 `path`、`kind`（`plugin` 为正常安装目录、`legacy` 为旧路径副本）、`fileCount` 和 `bytes`。即使 `targets=[]`，这里仍可能有遗留文件；**不能仅凭没有场景源就报告“已完全卸载”**。`pluginFolders[]` 只是检查路径，不表示目录一定存在。图形界面的“插件 / 素材目录”列表也会显示这些路径与大小，勾选“同时清理”才会删除。

按用户明确选择的目标使用返回的 `targetId`，不要凭名称猜测、默认选第一项或自动全选。如果 `obsRunning=true`，请用户正常退出 OBS，随后重新查找。**不要强制结束 OBS、启动 OBS、打开卸载界面或调用任何窗口。** 场景配置保存发生变化后旧 ID 失效，需要重新查找。

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./obs-input-overlay/InstallObsEasy.ps1 -Action Uninstall -TargetId '刚查到的完整targetId' -Quiet -Json
```

选择多项时，在 PowerShell 中用参数数组调用脚本，避免把用户文本拼成可执行命令：

```powershell
$ids = @('第一个targetId', '第二个targetId')
& ./obs-input-overlay/InstallObsEasy.ps1 -Action Uninstall -TargetId $ids -Quiet -Json
```

默认仅移除所选奶蛙及其待安装请求，保留插件、OBS 素材和键位。只有用户同时要求清理这些文件时才加 `-RemoveFiles`，且必须已选中全部奶蛙或已无奶蛙引用。此选项会清理一键版专属目录，包括旧安装器写到配置目录的副本；不会删除其他插件、手动安装的旧版 DLL、下载包或 `%LOCALAPPDATA%/MilkFrog` 的桌宠共享配置。自定义 `-ConfigRoot` 仅支持源卸载，文件清理需在标准配置或指定 `-ObsRoot` 的便携配置下检查。

卸载成功返回 `status=complete`、`removed[]`、`cancelledRequests`、`removedFolders[]` 和 `backupPath`。修改前的场景 JSON 和取消的请求保存在配置目录的 `milk-frog-backups/时间-随机码/`，不会被文件清理删除。失败按 `message` 处理；若提示已有改动，先重新查找并检查备份，不要盲目重试。完成后重新执行 `-ListInstalled -Quiet -Json` 核验目标已消失；不要把“只移除源”说成“插件和素材也已删除”。

## 无法直接拖入的情况

本项目交付的是有全局键盘输入和音频的 OBS 原生源，不是 PNG、视频或独立网页。不要让用户把 DLL、JSON 或 EXE 拖到 OBS 预览区，也不要承诺拖入 HTML 就能获得全局按键。普通用户运行 `MilkFrogOBS.exe` 并选择目标即可；智能体用上述静默命令。

## 开发验证

卸载也有独立的无窗口回归验证：`python source/tools/VerifyObsUninstall.py`。它只操作隔离测试配置，不修改用户安装。

`source/tools/BuildObsEasy.ps1 -ObsRoot 'OBS安装目录' -SdkRoot 'OBS libobs头文件目录'` 构建一体源与无控制台双击入口，并更新随包的安装脚本。
