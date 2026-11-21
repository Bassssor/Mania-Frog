param(
    [switch]$ListTargets,
    [string]$Collection,
    [string]$Scene,
    [string]$SourceName,
    [ValidateSet('Add','Update')][string]$Action='Add',
    [string]$ConfigRoot,
    [string]$ObsRoot,
    [switch]$Quiet,
    [switch]$Json,
    [switch]$Status,
    [string]$RequestId,
    [int]$WaitSeconds=3
)
$ErrorActionPreference='Stop'
$utf8=New-Object System.Text.UTF8Encoding($false)
[Console]::OutputEncoding=$utf8
$payload=$PSScriptRoot
if(-not(Test-Path -LiteralPath (Join-Path $payload 'milk-frog-easy.dll'))){
    $payload=Join-Path (Split-Path -Parent (Split-Path -Parent $PSScriptRoot)) 'obs-input-overlay'
}
function Test-PortableMarker([string]$root){
    $root -and ((Test-Path -LiteralPath (Join-Path $root 'portable_mode.txt')) -or (Test-Path -LiteralPath (Join-Path $root 'portable_mode')))
}
function Read-Targets {
    $directory=Join-Path $ConfigRoot 'basic/scenes'
    if(-not(Test-Path -LiteralPath $directory)){throw '找不到 OBS 场景配置。请先正常启动一次 OBS，便携版请指定 -ObsRoot 或 -ConfigRoot。'}
    foreach($file in Get-ChildItem -LiteralPath $directory -Filter '*.json' -File){
        $document=Get-Content -LiteralPath $file.FullName -Encoding UTF8 -Raw | ConvertFrom-Json
        foreach($target in $document.sources | Where-Object {$_.id -eq 'scene'}){
            $sources=@(foreach($item in $target.settings.items){
                $source=$document.sources | Where-Object {$_.name -ceq $item.name} | Select-Object -First 1
                [pscustomobject]@{name=$item.name;kind=$source.id}
            })
            [pscustomobject]@{collection=$document.name;scene=$target.name;sources=$sources;file=$file.FullName}
        }
    }
}
function Write-Answer($value){
    if($Json){$value | ConvertTo-Json -Depth 12 -Compress}else{$value | Format-List | Out-String | Write-Output}
}
function File-Hash([string]$path){
    $stream=[IO.File]::OpenRead($path);$hash=[Security.Cryptography.SHA256]::Create()
    try{[BitConverter]::ToString($hash.ComputeHash($stream))}finally{$stream.Dispose();$hash.Dispose()}
}
function Install-Target([string]$selectedCollection,[string]$selectedScene,[string]$selectedSource,[string]$selectedAction){
    if([string]::IsNullOrWhiteSpace($selectedCollection) -or [string]::IsNullOrWhiteSpace($selectedScene) -or [string]::IsNullOrWhiteSpace($selectedSource)){throw '必须明确指定场景集合、场景和源名称。先运行 -ListTargets -Json 查看目标。'}
    $target=@(Read-Targets | Where-Object {$_.collection -ceq $selectedCollection -and $_.scene -ceq $selectedScene})
    if($target.Count -ne 1){throw '目标场景不存在或不唯一，请重新运行 -ListTargets。'}
    $document=Get-Content -LiteralPath $target[0].file -Encoding UTF8 -Raw | ConvertFrom-Json
    $sameName=@($document.sources | Where-Object {$_.name -ceq $selectedSource})
    if($sameName.Count -and $sameName[0].id -ne 'milk_frog_source'){throw '该源名称已被其他类型的源占用。请使用新名称；旧输入叠加源不会被替换。'}
    if($selectedAction -eq 'Update' -and -not(@($target[0].sources | Where-Object {$_.name -ceq $selectedSource -and $_.kind -eq 'milk_frog_source'}).Count)){throw '更新操作需要指定场景中已有的一体奶蛙源。'}
    $files=@('milk-frog-easy.dll','milk-frog.json','keybindings.json','key-labels.png','MilkFrogKeySetup.exe','laugh-frames.mfa','laugh.wav','body-parts.png','desk-parts.png','hand-parts.png','shadow-parts.png','milk-frog-unlabeled.png')
    $files+=@(4..8 | ForEach-Object {"milk-frog-$($_)k-unlabeled.png"})
    foreach($name in $files){if(-not(Test-Path -LiteralPath (Join-Path $payload $name) -PathType Leaf)){throw "安装包缺少 $name，请保留完整 obs-input-overlay 目录。"}}
    $dll=Join-Path $plugin 'milk-frog-easy.dll'
    $changed=-not(Test-Path -LiteralPath $dll) -or ((File-Hash $dll) -ne (File-Hash (Join-Path $payload 'milk-frog-easy.dll')))
    if($running.Count -and (Test-Path -LiteralPath $dll) -and $changed){throw '奶蛙插件需要更新，请先正常关闭 OBS，再重新安装。不会强制结束 OBS。'}
    New-Item -ItemType Directory -Path $dataFolder,(Join-Path $dataFolder 'requests'),(Join-Path $dataFolder 'results') -Force | Out-Null
    $legacyData=Join-Path $ConfigRoot 'plugins/milk-frog-easy/data'
    foreach($name in $files | Where-Object {$_ -ne 'milk-frog-easy.dll'}){
        $destination=Join-Path $dataFolder $name
        if($name -in @('keybindings.json','key-labels.png')){
            if(Test-Path -LiteralPath $destination){continue}
            $previous=Join-Path $legacyData $name
            if(Test-Path -LiteralPath $previous -PathType Leaf){
                Copy-Item -LiteralPath $previous -Destination $destination
                continue
            }
        }
        if((Test-Path -LiteralPath $destination) -and (File-Hash $destination) -eq (File-Hash (Join-Path $payload $name))){continue}
        if($running.Count -and -not $changed -and (Test-Path -LiteralPath $destination)){throw '配套素材需要更新，请先正常关闭 OBS，再重新安装。'}
        Copy-Item -LiteralPath (Join-Path $payload $name) -Destination $destination -Force
    }
    if($changed){Copy-Item -LiteralPath (Join-Path $payload 'milk-frog-easy.dll') -Destination $dll -Force}
    $id=[Guid]::NewGuid().ToString('N')
    $request=[ordered]@{requestId=$id;collection=$selectedCollection;scene=$selectedScene;source=$selectedSource;action=$selectedAction}
    $path=Join-Path $dataFolder "requests/$id.json"
    [IO.File]::WriteAllText(($path+'.tmp'),($request | ConvertTo-Json -Compress),$utf8)
    Move-Item -LiteralPath ($path+'.tmp') -Destination $path
    $result=Join-Path $dataFolder "results/$id.json"
    $end=[DateTime]::UtcNow.AddSeconds([Math]::Max(0,[Math]::Min(30,$WaitSeconds)))
    while($running.Count -and [DateTime]::UtcNow -lt $end -and -not(Test-Path -LiteralPath $result)){Start-Sleep -Milliseconds 150}
    if(Test-Path -LiteralPath $result){return (Get-Content -LiteralPath $result -Encoding UTF8 -Raw | ConvertFrom-Json)}
    $loaded=$running.Count -and @($running[0].Modules | Where-Object {$_.FileName -ieq $dll}).Count
    $state=if($running.Count -and -not $loaded){'restart_required'}else{'pending'}
    [pscustomobject]@{status=$state;requestId=$id;collection=$selectedCollection;scene=$selectedScene;source=$selectedSource;action=$selectedAction;configRoot=$ConfigRoot;pluginPath=$dll;message='安装文件已准备好。OBS 加载插件并打开指定场景集合后，自动添加到指定场景。'}
}
function Show-Installer {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $targets=@(Read-Targets)
    if(-not $targets.Count){throw 'OBS 中没有可用场景，请先创建一个场景。'}
    $form=New-Object Windows.Forms.Form
    $form.Text='奶蛙 · OBS 一键安装';$form.ClientSize=New-Object Drawing.Size(520,350);$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
    $form.Font=New-Object Drawing.Font('Microsoft YaHei UI',10)
    function Label($text,$y){$control=New-Object Windows.Forms.Label;$control.Text=$text;$control.SetBounds(20,$y,480,24);$form.Controls.Add($control)}
    Label '选择要放入奶蛙的场景（不会替换其他源）' 18
    $sceneBox=New-Object Windows.Forms.ComboBox;$sceneBox.DropDownStyle='DropDownList';$sceneBox.SetBounds(20,48,480,30)
    foreach($target in $targets){[void]$sceneBox.Items.Add($target.collection+' / '+$target.scene)}
    $form.Controls.Add($sceneBox)
    Label '操作' 92
    $actionBox=New-Object Windows.Forms.ComboBox;$actionBox.DropDownStyle='DropDownList';$actionBox.SetBounds(20,120,480,30)
    [void]$actionBox.Items.Add('添加奶蛙源（同名的一体源会复用，不重复添加）');[void]$actionBox.Items.Add('更新指定的已有一体奶蛙源');$actionBox.SelectedIndex=0;$form.Controls.Add($actionBox)
    Label '源名称（更新时请填写已有一体奶蛙源的名称）' 164
    $nameBox=New-Object Windows.Forms.TextBox;$nameBox.Text='奶蛙';$nameBox.SetBounds(20,192,480,30);$form.Controls.Add($nameBox)
    Label '包含透明画面、全局敲键、粒子和笑声。无需另装输入叠加。' 239
    $button=New-Object Windows.Forms.Button;$button.Text='安装到所选场景';$button.SetBounds(310,285,190,40);$button.Enabled=$false;$form.Controls.Add($button)
    $sceneBox.Add_SelectedIndexChanged({$button.Enabled=$sceneBox.SelectedIndex -ge 0})
    $button.Add_Click({
        $button.Enabled=$false
        try{
            $target=$targets[$sceneBox.SelectedIndex];$selectedAction=if($actionBox.SelectedIndex -eq 1){'Update'}else{'Add'}
            $answer=Install-Target $target.collection $target.scene $nameBox.Text.Trim() $selectedAction
            if($answer.status -eq 'error'){throw $answer.message}
            $message=switch($answer.status){
                'complete' {'奶蛙已添加到所选场景。可在源属性中修改键位。'}
                'restart_required' {'插件文件已放好。请正常重启 OBS 并打开「'+$target.collection+'」场景集合，奶蛙会自动加入「'+$target.scene+'」。'}
                default {'安装请求已排队。请打开 OBS 的「'+$target.collection+'」场景集合，奶蛙会自动加入「'+$target.scene+'」。'}
            }
            [void][Windows.Forms.MessageBox]::Show($form,$message,'奶蛙 OBS 安装');$form.Close()
        }catch{[void][Windows.Forms.MessageBox]::Show($form,$_.Exception.Message,'安装未完成')}
        finally{$button.Enabled=$true}
    })
    [void]$form.ShowDialog()
}
try{
    $running=@(Get-Process obs64 -ErrorAction SilentlyContinue)
    if($running.Count -gt 1){throw '检测到多个 OBS 实例。请保留目标实例后重试，避免安装到错误配置。'}
    $portable=Test-PortableMarker $ObsRoot
    if($running.Count){
        $runningRoot=Split-Path -Parent (Split-Path -Parent (Split-Path -Parent $running[0].Path))
        if($ObsRoot -and [IO.Path]::GetFullPath($ObsRoot).TrimEnd('\','/') -ne $runningRoot){throw '指定的 OBS 目录与正在运行的实例不一致。'}
        $ObsRoot=$runningRoot
        $commandLine=(Get-CimInstance Win32_Process -Filter ("ProcessId="+$running[0].Id)).CommandLine
        $portable=(Test-PortableMarker $ObsRoot) -or $commandLine -match '(?i)(?:^|\s)(?:--portable|-p)(?:\s|$)'
    }
    $defaultConfig=Join-Path $env:APPDATA 'obs-studio'
    $pluginsRoot=Join-Path $env:ProgramData 'obs-studio/plugins'
    if($portable){
        $defaultConfig=Join-Path $ObsRoot 'config/obs-studio'
        $pluginsRoot=Join-Path $ObsRoot 'plugins'
    }
    if(-not $ConfigRoot){$ConfigRoot=$defaultConfig}
    $ConfigRoot=[IO.Path]::GetFullPath($ConfigRoot).TrimEnd('\','/')
    if($running.Count -and $ConfigRoot -ne [IO.Path]::GetFullPath($defaultConfig)){throw '正在运行的 OBS 使用另一份配置。请核对配置目录后重试。'}
    $plugin=[IO.Path]::GetFullPath((Join-Path $pluginsRoot 'milk-frog-easy'))
    $dataFolder=Join-Path $plugin 'data'
    if($ListTargets){Write-Answer ([pscustomobject]@{configRoot=$ConfigRoot;targets=@(Read-Targets)});exit 0}
    if($Status){
        if($RequestId -notmatch '^[0-9a-f]{32}$'){throw '-Status 需要有效的 -RequestId。'}
        $result=Join-Path $dataFolder "results/$RequestId.json"
        if(Test-Path -LiteralPath $result){Write-Answer (Get-Content -LiteralPath $result -Encoding UTF8 -Raw | ConvertFrom-Json)}
        elseif(Test-Path -LiteralPath (Join-Path $dataFolder "requests/$RequestId.json")){Write-Answer ([pscustomobject]@{status='pending';requestId=$RequestId})}
        else{throw '找不到该安装请求。'}
        exit 0
    }
    if($Quiet -or $Json -or $Collection -or $Scene -or $SourceName){
        $answer=Install-Target $Collection $Scene $SourceName $Action
        Write-Answer $answer
        if($answer.status -eq 'error'){exit 1};exit 0
    }
    Show-Installer
}catch{
    if($Quiet -or $Json -or $ListTargets -or $Status -or $Collection -or $Scene -or $SourceName){Write-Answer ([pscustomobject]@{status='error';message=$_.Exception.Message});exit 1}
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'奶蛙 OBS 安装');exit 1
}
