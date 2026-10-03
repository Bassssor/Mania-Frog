$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$executable = Join-Path $repository 'dist\milk-frog-4k\MilkFrog.exe'
$directory = Join-Path $repository 'dist\milk-frog-4k\verification'
$process = Start-Process -FilePath $executable -ArgumentList '--verify-assets','verification','--default-keys' -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw ('State export failed: ' + $process.ExitCode) }
Add-Type -AssemblyName System.Drawing
$baseline = [System.Drawing.Bitmap]::new((Join-Path $directory 'state-0.png'))
try {
    for ($mask = 0; $mask -lt 16; $mask++) {
        $bitmap = [System.Drawing.Bitmap]::new((Join-Path $directory ('state-' + $mask + '.png')))
        try {
            # Face now deliberately changes only while a key is held.
            # Every nonzero mask shares one registered closed-eye laugh.
            if($mask -gt 0) {
                $pressed=[System.Drawing.Bitmap]::new((Join-Path $directory 'state-1.png'))
                try {
                    for($y=30;$y -lt 230;$y+=3){
                        for($x=390;$x -lt 620;$x+=3){
                            if($bitmap.GetPixel($x,$y).ToArgb() -ne $pressed.GetPixel($x,$y).ToArgb()){
                                throw ('Pressed expression differs between held-key combinations: '+$mask)
                            }
                        }
                    }
                } finally {$pressed.Dispose()}
            }
            foreach ($point in @(@(0,0),@(879,0),@(0,879),@(879,879))) {
                if ($bitmap.GetPixel($point[0],$point[1]).A -ne 0) { throw 'Opaque background corner' }
            }
        } finally { $bitmap.Dispose() }
    }
} finally { $baseline.Dispose() }
$compiler = Join-Path $env:WINDIR 'Microsoft.NET\Framework64\v4.0.30319\csc.exe'
$keyboardVerifier = Join-Path $PSScriptRoot 'VerifyKeyboard.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $keyboardVerifier) (Join-Path $PSScriptRoot 'VerifyKeyboard.cs')
if ($LASTEXITCODE -ne 0) { throw 'Keyboard verifier compilation failed.' }
& $keyboardVerifier $directory
if ($LASTEXITCODE -ne 0) { throw 'Keyboard geometry validation failed.' }
$deskVerifier = Join-Path $PSScriptRoot 'VerifyDesk.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $deskVerifier) (Join-Path $PSScriptRoot 'VerifyDesk.cs')
if ($LASTEXITCODE -ne 0) { throw 'Desk verifier compilation failed.' }
& $deskVerifier $repository
if ($LASTEXITCODE -ne 0) { throw 'Desk/corner verification failed.' }
$rearVerifier = Join-Path $PSScriptRoot 'VerifyKeyboardRear.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $rearVerifier) (Join-Path $PSScriptRoot 'VerifyKeyboardRear.cs')
if ($LASTEXITCODE -ne 0) { throw 'Keyboard rear verifier compilation failed.' }
& $rearVerifier $repository
if ($LASTEXITCODE -ne 0) { throw 'Keyboard rear slot verification failed.' }
$armVerifier = Join-Path $PSScriptRoot 'VerifyArmShadow.exe'
& $compiler /nologo /r:System.Drawing.dll ('/out:' + $armVerifier) (Join-Path $PSScriptRoot 'VerifyArmShadow.cs')
if ($LASTEXITCODE -ne 0) { throw 'Arm shadow verifier compilation failed.' }
& $armVerifier $repository
if ($LASTEXITCODE -ne 0) { throw 'Arm shadow verification failed.' }
$process = Start-Process -FilePath $executable -ArgumentList '--smoke-test' -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw ('Layered window test failed: ' + $process.ExitCode) }
Get-Content -LiteralPath (Join-Path $directory 'particle-verification.txt')
$process = Start-Process -FilePath $executable -ArgumentList '--verify-scales','verification' -WindowStyle Hidden -Wait -PassThru
if ($process.ExitCode -ne 0) { throw ('Scale verification failed: ' + $process.ExitCode) }
Get-Content -LiteralPath (Join-Path $directory 'scale-verification.txt')
Write-Output 'PASS: 16 combinations, held-only laughing face, fixed keyboard, transparent corners, particle lifecycle, layered window and resize.'


