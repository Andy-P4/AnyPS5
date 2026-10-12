# Diagnostic runner for GPU console errors
param([int]$Seconds = 25)
Set-Location -Path $PSScriptRoot
if (-not $env:VK_LOADER_DRIVERS_DISABLE) { $env:VK_LOADER_DRIVERS_DISABLE = '*dzn*' }
$env:APS5_NO_DRAW_PRECHECK = '1'
# $env:APS5_IGNORE_DEPTH_TEST = '1'
$env:APS5_STORE_AT_FLIP = '1'

$log = Join-Path $PSScriptRoot 'gpu_console.log'
$outLog = Join-Path $PSScriptRoot 'gpu_console_out.log'
if (Test-Path $log) { Remove-Item $log -Force }
if (Test-Path $outLog) { Remove-Item $outLog -Force }

Write-Host "Launching eboot.exe (tracking for $Seconds seconds)..." -ForegroundColor Cyan
$p = Start-Process -FilePath '.\eboot.exe' -RedirectStandardError $log -RedirectStandardOutput $outLog -PassThru -NoNewWindow

# Also ensure window is moved to second monitor if detected
$moved = $false
for ($i = 0; $i -lt 100; $i++) {
    Start-Sleep -Milliseconds 100
    if ($p.HasExited) { break }
    if (-not $moved) {
        $hwnd = $p.MainWindowHandle
        if ($hwnd -ne [IntPtr]::Zero) {
            Add-Type @"
                using System;
                using System.Runtime.InteropServices;
                public class WinApi {
                    [DllImport("user32.dll")]
                    public static extern bool SetWindowPos(IntPtr hWnd, IntPtr hWndInsertAfter, int X, int Y, int cx, int cy, uint uFlags);
                }
"@ -ErrorAction SilentlyContinue
            [WinApi]::SetWindowPos($hwnd, [IntPtr]::Zero, 2560, 0, 0, 0, 0x0001 -bor 0x0004 -bor 0x0040)
            Write-Host "Window positioned on second monitor (X=2560)" -ForegroundColor Green
            $moved = $true
        }
    }
}

$remaining = $Seconds - 10
if ($remaining -gt 0 -and -not $p.HasExited) {
    Start-Sleep -Seconds $remaining
}

if (-not $p.HasExited) {
    Write-Host "Stopping process after $Seconds seconds..."
    Stop-Process -Id $p.Id -Force
}
Write-Host "Done. Logs written to $log and $outLog"
