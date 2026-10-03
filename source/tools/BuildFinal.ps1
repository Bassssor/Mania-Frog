$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
& python (Join-Path $PSScriptRoot 'PrepareLaughPlayback.py')
if($LASTEXITCODE -ne 0){throw 'Laughter playback assets failed'}
& (Join-Path $root 'build.ps1')
& (Join-Path $PSScriptRoot 'VerifyAssets.ps1')
& python (Join-Path $PSScriptRoot 'VerifyLaughBody.py')
if($LASTEXITCODE -ne 0){throw 'Laughing body validation failed'}
& python (Join-Path $PSScriptRoot 'BuildPortableExe.py')
if($LASTEXITCODE -ne 0){throw 'Single executable build failed'}
$executable=if((Split-Path -Leaf $root) -eq 'source'){Join-Path (Split-Path -Parent $root) 'MilkFrog.exe'}else{Join-Path $root 'dist\MilkFrog.exe'}
& $executable --verify-labels (Join-Path $root 'build\label-verification')
if($LASTEXITCODE -ne 0){throw 'Keycap label bounds validation failed'}
& $executable --verify-controls (Join-Path $root 'build\controls-verification')
if($LASTEXITCODE -ne 0){throw 'Tray and keybinding validation failed'}
& $executable --verify-laugh (Join-Path $root 'build\laugh-verification') --default-keys
if($LASTEXITCODE -ne 0){throw 'Native laughter playback validation failed'}
& $executable --verify-laugh-hotkey (Join-Path $root 'build\laugh-hotkey-verification')
if($LASTEXITCODE -ne 0){throw 'Laughter hotkey settings validation failed'}
& python (Join-Path $PSScriptRoot 'ExportInputOverlay.py')
if($LASTEXITCODE -ne 0){throw 'OBS preset export failed'}
Write-Output 'Built the final standalone EXE and native OBS preset. No ZIP archive was created.'
