# 本地生图脚本

`GenerateImage.py` 默认连接 `https://ubuntu.0x40e.eu.org:2083/v1`，模型名为
`image2.5sunburst`。使用 Python 3 标准库，无需安装额外依赖。

接口约定：纯文字使用 `POST /v1/images/generations`；带参考图使用
`POST /v1/images/edits`。这是 OpenAI Images 兼容格式，实际是否支持该模型、
参考图和透明背景，需要中转站账号有相应权限。不会自动切换模型或重试付费请求。

## 配置密钥

在你自己的 PowerShell 窗口运行以下命令，粘贴中转站 API Key；输入会隐藏。
密钥保存在当前 Windows 用户的环境变量中，脚本也能在已有的 Codex 会话里读取。
不要把密钥写进提示词或项目文件。

```powershell
$relaySecret = Read-Host '中转站 API Key' -AsSecureString
$relayCredential = [System.Net.NetworkCredential]::new('', $relaySecret)
[Environment]::SetEnvironmentVariable('MILKFROG_IMAGE_API_KEY', $relayCredential.Password, 'User')
Remove-Variable relaySecret, relayCredential
```

也可以不保存，直接运行脚本，在控制台按提示临时输入 Key。
脚本只读取 `MILKFROG_IMAGE_API_KEY`，不会拿其他服务的密钥请求此中转站。

## 文字生图

在项目的 `source` 目录运行：

```powershell
python tools/GenerateImage.py --prompt "奶白色小青蛙，圆润可爱，正面全身，桌宠素材"
```

默认生成一张 `1024x1024` 图片，保存到 `assets/milk-frog/generated/`。
旁边的 `.json` 记录提示词、模型、参数和参考图路径，不包含密钥。
生成结果供挑选和加工，不会自动替换桌宠正在使用的素材。

## 使用项目参考图

```powershell
python tools/GenerateImage.py --image "assets/milk-frog/character-three-views.jpg" --prompt "参考这只奶蛙，保持角色造型、颜色和线条风格，生成正面全身待机姿势"
```

重复 `--image` 可提供多张参考图。参考图需要上传给中转站。
若返回 404、405 或“不支持模型”，需向站方确认 `/v1/images/edits` 支持情况。

## 透明素材、提示词文件和指定输出

```powershell
python tools/GenerateImage.py --prompt "奶蛙正面待机姿势，完整身体，透明背景" --background transparent --output "assets/milk-frog/generated/idle-candidate.png"
python tools/GenerateImage.py --prompt-file "assets/milk-frog/sprite-generation-prompt.txt" --size 1536x1024
```

`--output` 指向新文件，已有文件不会被覆盖。
可用 `--quality high` 调整质量，`--timeout 600` 延长等待；支持情况以中转站为准。
接口地址和模型默认已配好，也可分别用 `--base-url`、`--model` 覆盖。

## 常见错误

- `401`：密钥未生效、错误或已过期。
- `403`：账号、分组或模型权限不足。
- `404` / `405`：检查接口路径及中转站是否支持 Images API。
- `400` / `422`：查看返回信息，检查模型名、尺寸及可选参数。
- 超时：先查中转站请求记录和扣费状态，再决定是否重试。

脚本支持 Images API 的 `data[0].b64_json` 和 `data[0].url` 返回格式。
下载结果图片时不携带中转站 Key。
