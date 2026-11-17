$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repository 'dist\milk-frog-4k\MilkFrog.exe'
$directory = Join-Path $repository 'build\lane-verification'
$relative = [System.IO.Path]::GetRelativePath((Split-Path -Parent $executable),$directory)
$process = Start-Process -FilePath $executable -ArgumentList '--verify-lanes',$relative,'--mode','4' -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw ('Lane and contact verification failed: '+$process.ExitCode)}
Add-Type -AssemblyName System.Drawing
foreach($mode in 4,5,6,7,8) {
    foreach($file in Get-ChildItem -LiteralPath (Join-Path $directory $mode) -Filter 'state-*.png') {
        $bitmap = [System.Drawing.Bitmap]::new($file.FullName)
        try {
            if($bitmap.Width -ne 880 -or $bitmap.Height -ne 880){throw 'Incorrect scene size'}
            foreach($point in @(@(0,0),@(879,0),@(0,879),@(879,879))) {
                if($bitmap.GetPixel($point[0],$point[1]).A -ne 0){throw 'Opaque background corner'}
            }
        } finally {$bitmap.Dispose()}
    }
}
foreach($check in '--verify-labels','--verify-controls','--verify-scales') {
    $process = Start-Process -FilePath $executable -ArgumentList $check,(Join-Path $relative $check.TrimStart('-')),'--mode','4' -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw ($check+' failed: '+$process.ExitCode)}
}
foreach($mode in 5,6,7,8) {
    $process=Start-Process -FilePath $executable -ArgumentList '--verify-scales',(Join-Path $relative ('scales-'+$mode)),'--mode',$mode -WindowStyle Hidden -Wait -PassThru
    if($process.ExitCode -ne 0){throw ('Scaling '+$mode+'K failed: '+$process.ExitCode)}
}
$process = Start-Process -FilePath $executable -ArgumentList '--smoke-test','--mode','4' -WindowStyle Hidden -Wait -PassThru
if($process.ExitCode -ne 0){throw ('Layered window test failed: '+$process.ExitCode)}
Get-Content -LiteralPath (Join-Path $directory 'checks.txt')
Write-Output 'PASS: 496 combinations, 1776 held and 1776 released contacts, labels, key capture/persistence, transparent corners, window and scaling.'
