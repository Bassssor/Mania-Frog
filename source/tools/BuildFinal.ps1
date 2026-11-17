$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot

function Assert-Exit([int]$code,[string]$step){
    if($code -ne 0){throw "$step failed (exit code $code)"}
}

& python (Join-Path $PSScriptRoot 'PrepareLaughPlayback.py')
Assert-Exit $LASTEXITCODE 'Laughter playback assets'
& (Join-Path $root 'build.ps1')
& (Join-Path $PSScriptRoot 'VerifyAssets.ps1')
& python (Join-Path $PSScriptRoot 'VerifyLaughBody.py')
Assert-Exit $LASTEXITCODE 'Laughing body validation'
& python (Join-Path $PSScriptRoot 'BuildPortableExe.py')
Assert-Exit $LASTEXITCODE 'Single executable build'
$executable=if((Split-Path -Leaf $root) -eq 'source'){Join-Path (Split-Path -Parent $root) 'MilkFrog.exe'}else{Join-Path $root 'dist\MilkFrog.exe'}
& $executable --verify-labels (Join-Path $root 'build\label-verification') --mode 4
Assert-Exit $LASTEXITCODE 'Keycap label bounds validation'
& $executable --verify-controls (Join-Path $root 'build\controls-verification') --mode 4
Assert-Exit $LASTEXITCODE 'Tray and keybinding validation'
& $executable --verify-laugh (Join-Path $root 'build\laugh-verification') --default-keys --mode 4
Assert-Exit $LASTEXITCODE 'Native laughter playback validation'
& $executable --verify-laugh-hotkey (Join-Path $root 'build\laugh-hotkey-verification') --mode 4
Assert-Exit $LASTEXITCODE 'Laughter hotkey settings validation'
& (Join-Path $PSScriptRoot 'BuildObsKeySetup.ps1')
& python (Join-Path $PSScriptRoot 'ExportInputOverlay.py')
Assert-Exit $LASTEXITCODE 'OBS preset export'
Write-Output 'Built the final standalone EXE and native OBS preset. No ZIP archive was created.'
