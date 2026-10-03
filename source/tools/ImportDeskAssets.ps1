$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$importer = Join-Path $PSScriptRoot 'ImportDeskAssets.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $importer) (Join-Path $PSScriptRoot 'ImportDeskAssets.cs')
if ($LASTEXITCODE -ne 0) { throw 'Desk importer compilation failed.' }
& $importer $repository
if ($LASTEXITCODE -ne 0) { throw 'Desk import failed.' }
