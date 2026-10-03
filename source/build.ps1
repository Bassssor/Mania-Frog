$ErrorActionPreference = 'Stop'
$repository = $PSScriptRoot
$sfmlRoot = Join-Path $repository 'vendor\SFML-2.6.2'
if (-not (Test-Path -LiteralPath (Join-Path $sfmlRoot 'include\SFML\Graphics.hpp'))) {
    $vendorDir = Join-Path $repository 'vendor'
    New-Item -ItemType Directory -Path $vendorDir -Force | Out-Null
    $archive = Join-Path $vendorDir 'SFML.zip'
    Invoke-WebRequest -Uri 'https://www.sfml-dev.org/files/SFML-2.6.2-windows-vc17-32-bit.zip' -OutFile $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $vendorDir -Force
    Remove-Item -LiteralPath $archive
}
if (-not (Test-Path -LiteralPath (Join-Path $repository 'SFML\SFML\milk-frog.ico'))) {
    & (Join-Path $repository 'tools\PackAssets.ps1')
}
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio C++ build tools are required.' }
$installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $installation) { throw 'Visual Studio C++ build tools were not found.' }
& (Join-Path $installation 'MSBuild\Current\Bin\MSBuild.exe') (Join-Path $repository 'SFML\SFML\SFML.vcxproj') /m /v:minimal /p:Configuration=Release /p:Platform=Win32
if ($LASTEXITCODE -ne 0) { throw 'C++ build failed.' }
$output = Join-Path $repository 'dist\milk-frog-4k'
$crt = Get-ChildItem -LiteralPath (Join-Path $installation 'VC\Redist\MSVC') -Filter 'Microsoft.VC*.CRT' -Directory -Recurse |
    Where-Object { $_.Parent.Name -eq 'x86' } | Sort-Object FullName -Descending | Select-Object -First 1
if ($crt) { Copy-Item -Path (Join-Path $crt.FullName '*.dll') -Destination $output -Force }
Copy-Item -LiteralPath (Join-Path $repository 'USAGE.zh.txt') -Destination $output -Force
$tableMask = Join-Path $repository 'build\tabletop-top-mask.png'
$tableExport = Start-Process -FilePath (Join-Path $output 'MilkFrog.exe') -ArgumentList '--export-tabletop','tabletop.png',('"' + $tableMask + '"') -WindowStyle Hidden -Wait -PassThru
if($tableExport.ExitCode -ne 0) { throw 'Native rounded tabletop export failed.' }
Copy-Item -LiteralPath (Join-Path $output 'tabletop.png') -Destination (Join-Path $repository 'SFML\SFML\tabletop.png') -Force
Copy-Item -LiteralPath (Join-Path $output 'tabletop.png') -Destination (Join-Path $repository 'assets\milk-frog\sprites\tabletop.png') -Force
$fixture=Join-Path $repository 'assets\milk-frog\legacy-seam-fixture'
if(Test-Path -LiteralPath $fixture){
    $target=Join-Path $output 'legacy-seam-fixture'
    New-Item -ItemType Directory -Path $target -Force | Out-Null
    Copy-Item -Path (Join-Path $fixture 'base*.png') -Destination $target -Force
}
Write-Output ('Built: ' + (Join-Path $output 'MilkFrog.exe'))
