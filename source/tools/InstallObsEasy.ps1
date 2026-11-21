param(
    [switch]$ListTargets,
    [switch]$ListInstalled,
    [string]$Collection,
    [string]$Scene,
    [string]$SourceName,
    [ValidateSet('Add','Update','Uninstall')][string]$Action='Add',
    [string[]]$TargetId,
    [switch]$RemoveFiles,
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
    if(-not(Test-Path -LiteralPath $directory)){return}
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
function Get-PluginFolders {
    @($plugin,(Join-Path $ConfigRoot 'plugins/milk-frog-easy')) | Select-Object -Unique
}
function Read-FileInstalls {
    foreach($folder in Get-PluginFolders){
        if(-not(Test-Path -LiteralPath $folder -PathType Container)){continue}
        $files=@(Get-ChildItem -LiteralPath $folder -File -Recurse -Force)
        $kind=if($folder -ieq $plugin){'plugin'}else{'legacy'}
        [pscustomobject]@{path=$folder;kind=$kind;fileCount=$files.Count;bytes=[long]($files | Measure-Object Length -Sum).Sum}
    }
}
function Read-Pending {
    foreach($folder in Get-PluginFolders){
        $directory=Join-Path $folder 'data/requests'
        if(-not(Test-Path -LiteralPath $directory)){continue}
        foreach($file in Get-ChildItem -LiteralPath $directory -Filter '*.json' -File){
            $request=Get-Content -LiteralPath $file.FullName -Encoding UTF8 -Raw | ConvertFrom-Json
            if($request.collection -and $request.scene -and $request.source){
                [pscustomobject]@{file=$file.FullName;collection=$request.collection;scene=$request.scene;source=$request.source}
            }
        }
    }
}
function Test-SourceItem($item,$source){
    if($item.source_uuid){return $item.source_uuid -ceq $source.uuid}
    return $item.name -ceq $source.name
}
function Find-SourcePaths($nodes,$container,$wanted,[string]$path,[string[]]$visited){
    if($visited -ccontains $container.name){return}
    $visited+=@($container.name)
    if($container -eq $wanted){$path}
    foreach($item in $container.settings.items){
        if($item.group_item_backup){continue}
        if(Test-SourceItem $item $wanted){$path+' / '+$wanted.name;continue}
        $child=$nodes | Where-Object {$_.id -in @('scene','group') -and (Test-SourceItem $item $_)} | Select-Object -First 1
        if($child){Find-SourcePaths $nodes $child $wanted ($path+' / '+$child.name) $visited}
    }
}
function Target-Key([string]$identity){
    $hash=[Security.Cryptography.SHA256]::Create()
    try{([BitConverter]::ToString($hash.ComputeHash($utf8.GetBytes($identity)))).Replace('-','').ToLowerInvariant()}finally{$hash.Dispose()}
}
function Read-Installed {
    $pending=@(Read-Pending)
    $found=@()
    $directory=Join-Path $ConfigRoot 'basic/scenes'
    if(Test-Path -LiteralPath $directory){
        foreach($file in Get-ChildItem -LiteralPath $directory -Filter '*.json' -File){
            $document=Get-Content -LiteralPath $file.FullName -Encoding UTF8 -Raw | ConvertFrom-Json
            $nodes=@($document.sources)+@($document.groups)
            $revision=File-Hash $file.FullName
            foreach($source in $nodes){
                $kinds=@()
                if($source.id -in @('milk_frog_source','milk_frog_laugh_audio')){$kinds+=@($source.id)}
                if(@($source.filters | Where-Object {$_.id -eq 'milk_frog_native_animation'}).Count){$kinds+=@('milk_frog_native_animation')}
                foreach($kind in $kinds){
                    $locations=@(foreach($root in $nodes | Where-Object {$_.id -eq 'scene'}){Find-SourcePaths $nodes $root $source $root.name @()})
                    if(-not $locations.Count){$locations=@('[未被场景引用] / '+$source.name)}
                    $found+=[pscustomobject]@{targetId=(Target-Key ($file.FullName+'|'+$revision+'|'+$source.name+'|'+$kind));collection=$document.name;source=$source.name;kind=$kind;locations=@($locations | Select-Object -Unique);file=$file.FullName}
                }
            }
        }
    }
    $found
    foreach($group in $pending | Group-Object -CaseSensitive collection,source){
        $request=$group.Group[0]
        if(@($found | Where-Object {$_.collection -ceq $request.collection -and $_.source -ceq $request.source -and $_.kind -eq 'milk_frog_source'}).Count){continue}
        [pscustomobject]@{targetId=(Target-Key ('pending|'+$ConfigRoot+'|'+$request.collection+'|'+$request.source));collection=$request.collection;source=$request.source;kind='pending';locations=@($group.Group | ForEach-Object {'[待安装] '+$_.scene+' / '+$_.source} | Select-Object -Unique);file=$null}
    }
}
function Uninstall-Targets([string[]]$ids,[bool]$removePluginFiles){
    if(@(Get-Process obs64 -ErrorAction SilentlyContinue).Count){throw '请先正常关闭 OBS，再点击“重新查找”后卸载，避免 OBS 退出时写回旧配置。不会强制结束 OBS。'}
    $ids=@($ids | Select-Object -Unique)
    if(-not $ids.Count -and -not $removePluginFiles){throw '请先勾选要卸载的奶蛙。'}
    $installed=@(Read-Installed)
    $selected=@($installed | Where-Object {$ids -ccontains $_.targetId})
    if($selected.Count -ne $ids.Count){throw '查找结果已变化，请重新查找后选择要卸载的奶蛙。'}
    if($removePluginFiles -and @($installed | Where-Object {$ids -cnotcontains $_.targetId}).Count){throw '还有未选择的奶蛙源或待安装请求。请全部卸载后再清理插件和素材。'}
    $folders=@(Get-PluginFolders | Where-Object {Test-Path -LiteralPath $_})
    if($removePluginFiles){
        if($ConfigRoot -ine [IO.Path]::GetFullPath($defaultConfig).TrimEnd('\','/')){throw '自定义配置目录仅支持移除所选源。请在 OBS 的默认配置或便携配置下检查全部引用后清理文件。'}
        foreach($folder in $folders){
            if(((Get-Item -LiteralPath $folder).Attributes -band [IO.FileAttributes]::ReparsePoint) -or @(Get-ChildItem -LiteralPath $folder -Recurse -Force | Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw '插件目录含有链接或联接，已停止自动清理，请手动检查。'}
        }
    }
    $requests=@(Read-Pending | Where-Object {
        $request=$_
        @($selected | Where-Object {$_.collection -ceq $request.collection -and $_.source -ceq $request.source}).Count
    })
    $backup=$null
    if($selected.Count){
        $backup=Join-Path $ConfigRoot ('milk-frog-backups/'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,8))
        New-Item -ItemType Directory -Path $backup | Out-Null
        $index=0
        foreach($path in (@($selected.file)+@($requests.file) | Where-Object {$_} | Select-Object -Unique)){
            Copy-Item -LiteralPath $path -Destination (Join-Path $backup (($index.ToString()+'-')+[IO.Path]::GetFileName($path)));$index++
        }
    }
    try{
        foreach($group in $selected | Where-Object {$_.file} | Group-Object file){
            $path=$group.Name
            $document=Get-Content -LiteralPath $path -Encoding UTF8 -Raw | ConvertFrom-Json
            $nodes=@($document.sources)+@($document.groups)
            $removed=@($nodes | Where-Object {
                $node=$_
                @($group.Group | Where-Object {$_.source -ceq $node.name -and $_.kind -ceq $node.id}).Count
            })
            foreach($node in $nodes){
                if(@($group.Group | Where-Object {$_.source -ceq $node.name -and $_.kind -eq 'milk_frog_native_animation'}).Count){
                    $node.filters=@($node.filters | Where-Object {$_.id -ne 'milk_frog_native_animation'})
                }
                if($node.id -in @('scene','group') -and $node.settings.PSObject.Properties['items']){
                    $node.settings.items=@($node.settings.items | Where-Object {
                        $item=$_
                        -not @($removed | Where-Object {Test-SourceItem $item $_}).Count
                    })
                }
            }
            foreach($property in @('sources','groups')){
                if($document.PSObject.Properties[$property]){$document.$property=@($document.$property | Where-Object {$removed -notcontains $_})}
            }
            $temporary=$path+'.milk-frog-'+[Guid]::NewGuid().ToString('N')+'.tmp'
            try{
                [IO.File]::WriteAllText($temporary,($document | ConvertTo-Json -Depth 100),$utf8)
                [IO.File]::Replace($temporary,$path,[System.Management.Automation.Language.NullString]::Value)
            }finally{if(Test-Path -LiteralPath $temporary){Remove-Item -LiteralPath $temporary -Force}}
        }
        foreach($request in $requests){Remove-Item -LiteralPath $request.file -Force}
        $deleted=@()
        if($removePluginFiles){
            if(@(Read-Installed).Count){throw '仍检测到奶蛙引用，已保留插件文件，请重新查找。'}
            foreach($folder in $folders){Remove-Item -LiteralPath $folder -Recurse -Force;$deleted+=@($folder)}
        }
        [pscustomobject]@{status='complete';removed=@($selected | Select-Object collection,source,kind,locations);cancelledRequests=$requests.Count;removedFolders=$deleted;backupPath=$backup;message='已卸载所选奶蛙；桌宠的共享配置和下载目录保留。'}
    }catch{throw ($_.Exception.Message+' 已有改动可从备份恢复：'+$backup)}
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
function Show-Uninstaller($owner) {
    $dialog=New-Object Windows.Forms.Form
    $dialog.Text='查找并卸载奶蛙';$dialog.ClientSize=New-Object Drawing.Size(800,610);$dialog.StartPosition='CenterParent';$dialog.FormBorderStyle='FixedDialog';$dialog.MaximizeBox=$false;$dialog.Font=$owner.Font
    $label=New-Object Windows.Forms.Label;$label.Text='勾选要卸载的奶蛙源。同一个源会从下方列出的全部场景、分组中移除。';$label.SetBounds(16,16,768,28);$dialog.Controls.Add($label)
    $list=New-Object Windows.Forms.ListView;$list.View='Details';$list.CheckBoxes=$true;$list.FullRowSelect=$true;$list.GridLines=$true;$list.ShowItemToolTips=$true;$list.SetBounds(16,50,768,210)
    [void]$list.Columns.Add('场景集合',130);[void]$list.Columns.Add('源名称',150);[void]$list.Columns.Add('类型',120);[void]$list.Columns.Add('所在场景 / 分组 / 源（悬停查看全部）',340);$dialog.Controls.Add($list)
    $filesLabel=New-Object Windows.Forms.Label;$filesLabel.SetBounds(16,274,768,24);$dialog.Controls.Add($filesLabel)
    $filesList=New-Object Windows.Forms.ListView;$filesList.View='Details';$filesList.FullRowSelect=$true;$filesList.GridLines=$true;$filesList.ShowItemToolTips=$true;$filesList.SetBounds(16,302,768,104)
    [void]$filesList.Columns.Add('目录类型',120);[void]$filesList.Columns.Add('大小',90);[void]$filesList.Columns.Add('实际路径（悬停查看全部）',530);$dialog.Controls.Add($filesList)
    $hint=New-Object Windows.Forms.Label;$hint.SetBounds(16,418,768,48);$dialog.Controls.Add($hint)
    $cleanup=New-Object Windows.Forms.CheckBox;$cleanup.Text='同时清理一键版插件、OBS 素材和键位（需卸载全部奶蛙；保留桌宠配置）';$cleanup.SetBounds(16,480,768,30);$dialog.Controls.Add($cleanup)
    $refresh=New-Object Windows.Forms.Button;$refresh.Text='重新查找';$refresh.SetBounds(16,548,125,40);$dialog.Controls.Add($refresh)
    $remove=New-Object Windows.Forms.Button;$remove.Text='执行卸载 / 清理';$remove.SetBounds(594,548,190,40);$dialog.Controls.Add($remove)
    $reload={
        $list.Items.Clear()
        foreach($entry in @(Read-Installed)){
            $row=New-Object Windows.Forms.ListViewItem($entry.collection);$row.Tag=$entry.targetId
            [void]$row.SubItems.Add($entry.source)
            $kind=switch($entry.kind){'pending' {'待安装请求'};'milk_frog_native_animation' {'旧版滤镜'};'milk_frog_laugh_audio' {'旧版笑声音频'};default {'一体奶蛙源'}}
            [void]$row.SubItems.Add($kind);[void]$row.SubItems.Add(($entry.locations -join '；'))
            $row.ToolTipText=($entry.locations -join [Environment]::NewLine)
            [void]$list.Items.Add($row)
        }
        $fileInstalls=@(Read-FileInstalls)
        $filesList.Items.Clear()
        foreach($entry in $fileInstalls){
            $kind=if($entry.kind -eq 'legacy'){'旧路径副本'}else{'一键版目录'}
            $row=New-Object Windows.Forms.ListViewItem($kind)
            [void]$row.SubItems.Add(($entry.bytes/1000000).ToString('0.00')+' MB');[void]$row.SubItems.Add($entry.path)
            $row.ToolTipText=$entry.path+[Environment]::NewLine+$entry.fileCount+' 个文件'
            [void]$filesList.Items.Add($row)
        }
        $totalBytes=[long]($fileInstalls | Measure-Object bytes -Sum).Sum
        $filesLabel.Text='插件 / 素材目录：'+$fileInstalls.Count+' 处，共 '+($totalBytes/1000000).ToString('0.00')+' MB（勾选下方“同时清理”后删除）'
        $hint.Text=if(@(Get-Process obs64 -ErrorAction SilentlyContinue).Count){'OBS 正在运行。请先正常退出 OBS，再点“重新查找”后勾选卸载。'}elseif($list.Items.Count){'卸载前自动备份场景配置。旧版滤镜只移除奶蛙滤镜，保留承载它的其他源。'}elseif($fileInstalls.Count){'未发现场景中的奶蛙源或待安装请求，但发现了上述插件 / 素材目录。勾选“同时清理”可移除这些文件。'}else{'未发现奶蛙源、待安装请求或插件 / 素材目录。'}
    }
    $refresh.Add_Click({try{& $reload}catch{[void][Windows.Forms.MessageBox]::Show($dialog,$_.Exception.Message,'查找未完成')}})
    $remove.Add_Click({
        $remove.Enabled=$false
        try{
            $ids=@($list.CheckedItems | ForEach-Object {[string]$_.Tag})
            $answer=Uninstall-Targets $ids $cleanup.Checked
            $message='已卸载 '+$answer.removed.Count+' 项奶蛙。'
            if($answer.removedFolders.Count){$message+=[Environment]::NewLine+'一键版插件和素材已清理。'}
            if($answer.backupPath){$message+=[Environment]::NewLine+'场景备份：'+$answer.backupPath}
            [void][Windows.Forms.MessageBox]::Show($dialog,$message,'卸载完成');& $reload
        }catch{[void][Windows.Forms.MessageBox]::Show($dialog,$_.Exception.Message,'卸载未完成')}
        finally{$remove.Enabled=$true}
    })
    & $reload
    [void]$dialog.ShowDialog($owner)
}
function Show-Installer {
    Add-Type -AssemblyName System.Windows.Forms
    Add-Type -AssemblyName System.Drawing
    $targets=@(Read-Targets)
    $form=New-Object Windows.Forms.Form
    $form.Text='奶蛙 · OBS 安装与卸载';$form.ClientSize=New-Object Drawing.Size(520,350);$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.MaximizeBox=$false
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
    $uninstall=New-Object Windows.Forms.Button;$uninstall.Text='查找 / 卸载奶蛙';$uninstall.SetBounds(20,285,180,40);$form.Controls.Add($uninstall)
    $uninstall.Add_Click({try{Show-Uninstaller $form}catch{[void][Windows.Forms.MessageBox]::Show($form,$_.Exception.Message,'查找未完成')}})
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
    if(($TargetId -or $RemoveFiles) -and $Action -ne 'Uninstall'){throw '-TargetId 和 -RemoveFiles 仅用于 -Action Uninstall。'}
    if($ListTargets){Write-Answer ([pscustomobject]@{configRoot=$ConfigRoot;targets=@(Read-Targets)});exit 0}
    if($ListInstalled){Write-Answer ([pscustomobject]@{configRoot=$ConfigRoot;obsRunning=[bool]$running.Count;targets=@(Read-Installed);pluginFolders=@(Get-PluginFolders);fileInstalls=@(Read-FileInstalls)});exit 0}
    if($Action -eq 'Uninstall'){
        if($Collection -or $Scene -or $SourceName){throw '卸载请先运行 -ListInstalled，再用 -TargetId 精确选择；不要使用安装目标参数。'}
        Write-Answer (Uninstall-Targets $TargetId ([bool]$RemoveFiles));exit 0
    }
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
    if($Quiet -or $Json -or $ListTargets -or $ListInstalled -or $Action -eq 'Uninstall' -or $TargetId -or $RemoveFiles -or $Status -or $Collection -or $Scene -or $SourceName){Write-Answer ([pscustomobject]@{status='error';message=$_.Exception.Message});exit 1}
    Add-Type -AssemblyName System.Windows.Forms
    [void][Windows.Forms.MessageBox]::Show($_.Exception.Message,'奶蛙 OBS 安装');exit 1
}
