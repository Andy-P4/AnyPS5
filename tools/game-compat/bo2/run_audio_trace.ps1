# Diagnostic runner: traces AJM (audio decode) and AudioOut, stops the game after N seconds.
param([int]$Seconds = 25)
Set-Location -Path $PSScriptRoot
if (-not $env:VK_LOADER_DRIVERS_DISABLE) { $env:VK_LOADER_DRIVERS_DISABLE = '*dzn*' }
$env:APS5_NO_DRAW_PRECHECK = '1'
$env:APS5_IGNORE_DEPTH_TEST = '1'
$env:APS5_STORE_AT_FLIP = '1'
$env:APS5_TRACE_AUDIOOUT = '1'
$log = Join-Path $PSScriptRoot 'audio_trace.log'
$p = Start-Process -FilePath '.\eboot.exe' -RedirectStandardError $log -RedirectStandardOutput (Join-Path $PSScriptRoot 'audio_trace_out.log') -PassThru -NoNewWindow
Start-Sleep -Seconds $Seconds
if (-not $p.HasExited) { Stop-Process -Id $p.Id -Force }
Write-Host "Done. Exit: $($p.HasExited)"
