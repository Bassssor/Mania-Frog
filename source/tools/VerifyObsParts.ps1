param([string]$ObsRoot='D:\steam1\steamapps\common\OBS Studio')
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build\obs-native'
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$script=@"
@echo off
call "$vs\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /Ibuild\obs-sdk-source\libobs /Ibuild\obs-native tools\VerifyObsParts.cpp /Fobuild\obs-native\VerifyObsParts.obj /Febuild\obs-native\VerifyObsParts.exe /link build\obs-native\obs.lib user32.lib ole32.lib windowscodecs.lib gdiplus.lib
exit /b %errorlevel%
"@
Set-Content -LiteralPath (Join-Path $build 'verify-parts.cmd') -Value $script -Encoding utf8NoBOM
Push-Location -LiteralPath $root
$previousPath=$env:PATH
try {
    & $env:ComSpec /d /c 'build\obs-native\verify-parts.cmd'
    if($LASTEXITCODE -ne 0){throw 'OBS graphics verifier compilation failed'}
    $env:PATH=(Join-Path $ObsRoot 'bin\64bit')+';'+$env:PATH
    & (Join-Path $build 'VerifyObsParts.exe') $root $ObsRoot
    if($LASTEXITCODE -ne 0){throw 'Isolated OBS graphics verification failed'}
} finally {$env:PATH=$previousPath;Pop-Location}
