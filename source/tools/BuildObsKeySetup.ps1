$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$build=Join-Path $root 'build\obs-key-setup'
$out=Join-Path (Split-Path -Parent $root) 'obs-input-overlay'
New-Item -ItemType Directory -Path $build,$out -Force | Out-Null
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if(-not $vs){throw 'Visual Studio C++ Build Tools are required'}
'101 ICON "SFML/SFML/milk-frog.ico"' | Set-Content -LiteralPath (Join-Path $build 'key-setup.rc') -Encoding ascii
$script=@"
@echo off
call "$vs\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul
rc /nologo /fobuild\obs-key-setup\key-setup.res build\obs-key-setup\key-setup.rc
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /O2 /MT /EHsc /utf-8 /DUNICODE /D_UNICODE obs-native\KeySetup.cpp build\obs-key-setup\key-setup.res /Fobuild\obs-key-setup\KeySetup.obj /Febuild\obs-key-setup\MilkFrogKeySetup.exe /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib gdi32.lib gdiplus.lib ole32.lib
exit /b %errorlevel%
"@
$script | Set-Content -LiteralPath (Join-Path $build 'build.cmd') -Encoding utf8NoBOM
Push-Location -LiteralPath $root
try{& $env:ComSpec /d /c 'build\obs-key-setup\build.cmd'}finally{Pop-Location}
if($LASTEXITCODE -ne 0){throw 'OBS key setup build failed'}
Copy-Item -LiteralPath (Join-Path $build 'MilkFrogKeySetup.exe') -Destination $out -Force
Write-Output ('Built: '+(Join-Path $out 'MilkFrogKeySetup.exe'))
