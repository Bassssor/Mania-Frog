$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$packer = Join-Path $PSScriptRoot 'PackAssets.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $packer) (Join-Path $PSScriptRoot 'PackAssets.cs')
if ($LASTEXITCODE -ne 0) { throw 'Asset packer compilation failed.' }
$sprites = Join-Path $repository 'assets\milk-frog\sprites'
& $packer (Join-Path $repository 'assets\milk-frog\atlas-raised-arms.png') (Join-Path $repository 'assets\milk-frog\atlas-body-fitted.png') (Join-Path $repository 'assets\milk-frog\atlas-arm-mask.png') $sprites
if ($LASTEXITCODE -ne 0) { throw 'Sprite packaging failed.' }
Copy-Item -Path (Join-Path $sprites 'base*.png') -Destination (Join-Path $repository 'SFML\SFML') -Force
Copy-Item -LiteralPath (Join-Path $sprites 'milk-frog.ico') -Destination (Join-Path $repository 'SFML\SFML\milk-frog.ico') -Force
$legacyFixture = Join-Path $repository 'assets\milk-frog\legacy-seam-fixture'
if (Test-Path -LiteralPath $legacyFixture) {
    $destination = Join-Path $repository 'dist\milk-frog-4k\legacy-seam-fixture'
    New-Item -ItemType Directory -Path $destination -Force | Out-Null
    Copy-Item -Path (Join-Path $legacyFixture 'base*.png') -Destination $destination -Force
}
