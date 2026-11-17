param([Parameter(Mandatory)][string]$Executable,[Parameter(Mandatory)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$output=[System.IO.Path]::GetFullPath($OutputDirectory)
New-Item -ItemType Directory -Path $output -Force | Out-Null
$process=Start-Process -FilePath ([System.IO.Path]::GetFullPath($Executable)) -ArgumentList '--benchmark-particles',('"'+$output+'"'),'--mode','4' -WindowStyle Hidden -PassThru
$rows=[System.Collections.Generic.List[object]]::new()
$timer=[System.Diagnostics.Stopwatch]::StartNew()
while(-not $process.HasExited) {
    $process.Refresh()
    $rows.Add([pscustomobject]@{seconds=$timer.Elapsed.TotalSeconds;working_set=$process.WorkingSet64;private_bytes=$process.PrivateMemorySize64;peak_working_set=$process.PeakWorkingSet64})
    Start-Sleep -Milliseconds 100
}
if($process.ExitCode -ne 0){throw ('Benchmark failed: '+$process.ExitCode)}
$rows | Export-Csv -LiteralPath (Join-Path $output 'memory.csv') -NoTypeInformation
$last=$rows | Select-Object -Last 20
$summary=[pscustomobject]@{
    samples=$rows.Count
    peak_working_set=($rows.peak_working_set | Measure-Object -Maximum).Maximum
    peak_private=($rows.private_bytes | Measure-Object -Maximum).Maximum
    render_working_set=($last.working_set | Measure-Object -Average).Average
    render_private=($last.private_bytes | Measure-Object -Average).Average
}
$summary | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'memory-summary.json')
$summary
