param(
    [Parameter(Mandatory=$true)][string]$ObsRoot,
    [string]$SdkRoot
)
$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
if(-not $SdkRoot){$SdkRoot=Join-Path $root 'build/obs-sdk-source/libobs'}
if(-not(Test-Path -LiteralPath (Join-Path $SdkRoot 'obs-module.h'))){throw 'Specify -SdkRoot containing the OBS libobs headers.'}
$build=Join-Path $root 'build/obs-easy';$out=Join-Path (Split-Path -Parent $root) 'obs-input-overlay'
New-Item -ItemType Directory -Path $build,$out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio C++ Build Tools are required'}
$msvc=Get-ChildItem -LiteralPath (Join-Path $vs 'VC/Tools/MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$bin=Join-Path $msvc.FullName 'bin/Hostx64/x64'
$exports=& (Join-Path $bin 'dumpbin.exe') /exports (Join-Path $ObsRoot 'bin/64bit/obs.dll')
if($LASTEXITCODE -ne 0){throw 'OBS exports unavailable'}
$symbols=@($exports | ForEach-Object {if($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)(?:\s|$)'){$Matches[1]}})
if($symbols.Count -lt 500){throw 'OBS export extraction incomplete'}
@('LIBRARY obs.dll','EXPORTS')+$symbols | Set-Content -LiteralPath (Join-Path $build 'obs.def') -Encoding ascii
& (Join-Path $bin 'lib.exe') /nologo /machine:x64 ('/def:'+(Join-Path $build 'obs.def')) ('/out:'+(Join-Path $build 'obs.lib'))
if($LASTEXITCODE -ne 0){throw 'OBS import library generation failed'}
@'
#pragma once
#define OBS_DATA_PATH "data"
#define OBS_PLUGIN_PATH "obs-plugins"
#define OBS_PLUGIN_DESTINATION "obs-plugins"
#define OBS_RELEASE_CANDIDATE 0
#define OBS_BETA 0
'@ | Set-Content -LiteralPath (Join-Path $build 'obsconfig.h') -Encoding ascii
'101 ICON "SFML/SFML/milk-frog.ico"' | Set-Content -LiteralPath (Join-Path $build 'launcher.rc') -Encoding ascii
$commands=@"
@echo off
chcp 65001 >nul
call "$vs\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /LD /DMILK_FROG_EASY /I"$SdkRoot" /Ibuild\obs-easy obs-native\milk-frog-native.cpp /Fobuild\obs-easy\plugin.obj /link build\obs-easy\obs.lib user32.lib shell32.lib ole32.lib windowscodecs.lib gdiplus.lib /OUT:build\obs-easy\milk-frog-easy.dll /IMPLIB:build\obs-easy\milk-frog-easy.lib
if errorlevel 1 exit /b %errorlevel%
rc /nologo /fobuild\obs-easy\launcher.res build\obs-easy\launcher.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE obs-native\EasyLauncher.cpp build\obs-easy\launcher.res /Fobuild\obs-easy\launcher.obj /Febuild\obs-easy\MilkFrogOBS.exe /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib
exit /b %errorlevel%
"@
[IO.File]::WriteAllText((Join-Path $build 'build.cmd'),($commands -replace "`r?`n","`r`n"),(New-Object Text.UTF8Encoding($false)))
Push-Location -LiteralPath $root
try{& $env:ComSpec /d /c 'build\obs-easy\build.cmd'}finally{Pop-Location}
if($LASTEXITCODE -ne 0){throw 'OBS easy import build failed'}
Copy-Item -LiteralPath (Join-Path $build 'milk-frog-easy.dll') -Destination $out -Force
# Windows PowerShell 5.1 requires a BOM for Chinese text in scripts.
$script=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'InstallObsEasy.ps1') -Raw -Encoding UTF8
[IO.File]::WriteAllText((Join-Path $out 'InstallObsEasy.ps1'),$script,(New-Object Text.UTF8Encoding($true)))
Copy-Item -LiteralPath (Join-Path $build 'MilkFrogOBS.exe') -Destination (Split-Path -Parent $root) -Force
Write-Output 'Built MilkFrogOBS.exe, the standalone OBS source and silent installer.'
