param([string]$ObsRoot='C:\Program Files\obs-studio')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$sdk=Join-Path $repo 'build\obs-sdk-source\libobs'
if(-not(Test-Path -LiteralPath (Join-Path $sdk 'obs-module.h'))) {
    $sdkRepo=Join-Path $repo 'build\obs-sdk-source'
    New-Item -ItemType Directory -Path (Join-Path $repo 'build') -Force | Out-Null
    & git clone --depth 1 --filter=blob:none --sparse --branch 32.2.2 https://github.com/obsproject/obs-studio.git $sdkRepo
    if($LASTEXITCODE -ne 0){throw 'OBS SDK checkout failed'}
    & git -C $sdkRepo sparse-checkout set libobs
    if($LASTEXITCODE -ne 0){throw 'OBS SDK headers unavailable'}
}
$build=Join-Path $repo 'build\obs-native'
$out=Join-Path $repo 'dist\input-overlay'
New-Item -ItemType Directory -Path $build,$out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msvc=Get-ChildItem -LiteralPath (Join-Path $vs 'VC\Tools\MSVC') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$bin=Join-Path $msvc.FullName 'bin\Hostx64\x64'
# Link against the exact installed OBS binary; no mismatched SDK import library.
$exports=& (Join-Path $bin 'dumpbin.exe') /exports (Join-Path $ObsRoot 'bin\64bit\obs.dll')
if($LASTEXITCODE -ne 0){throw 'OBS exports unavailable'}
$symbols=@($exports | ForEach-Object {if($_ -match '^\s+\d+\s+[0-9A-F]+\s+[0-9A-F]+\s+(\w+)(?:\s|$)'){$Matches[1]}})
if($symbols.Count -lt 500){throw 'OBS export extraction incomplete'}
@('LIBRARY obs.dll','EXPORTS') + $symbols | Set-Content -LiteralPath (Join-Path $build 'obs.def') -Encoding ascii
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
$cmd=@"
@echo off
chcp 65001 >nul
call "$vs\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /LD /Ibuild\obs-sdk-source\libobs /Ibuild\obs-native obs-native\milk-frog-native.cpp /Fobuild\obs-native\milk-frog-native.obj /link build\obs-native\obs.lib user32.lib ole32.lib windowscodecs.lib gdiplus.lib /OUT:dist\input-overlay\milk-frog-native.dll /IMPLIB:build\obs-native\milk-frog-native.lib
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc tools\VerifyNativeFountain.cpp /Fobuild\obs-native\VerifyNativeFountain.obj /Febuild\obs-native\VerifyNativeFountain.exe
if errorlevel 1 exit /b %errorlevel%
build\obs-native\VerifyNativeFountain.exe
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc tools\VerifyLaughPlayback.cpp /Fobuild\obs-native\VerifyLaughPlayback.obj /Febuild\obs-native\VerifyLaughPlayback.exe /link ole32.lib windowscodecs.lib
if errorlevel 1 exit /b %errorlevel%
build\obs-native\VerifyLaughPlayback.exe assets\milk-frog\laugh\laugh-frames.mfa
exit /b %errorlevel%
"@
$cmdPath=Join-Path $build 'build.cmd'
$cmd | Set-Content -LiteralPath $cmdPath -Encoding utf8NoBOM
Push-Location -LiteralPath $repo
try { & $env:ComSpec /d /c 'build\obs-native\build.cmd' } finally { Pop-Location }
if($LASTEXITCODE -ne 0){throw 'Native OBS plugin build failed'}
Write-Output ('Built: '+(Join-Path $out 'milk-frog-native.dll'))
if((Split-Path -Leaf $repo) -eq 'source'){
    $active=Join-Path (Split-Path -Parent $repo) 'obs-input-overlay'
    New-Item -ItemType Directory -Path $active -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $out 'milk-frog-native.dll') -Destination $active -Force
}
