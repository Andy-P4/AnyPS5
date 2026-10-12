# S:\AnyPS5\extracted\take_screenshot.ps1
param(
    [string]$OutName = "latest_screenshot.png"
)

Add-Type -AssemblyName System.Drawing

$ebootProc = Get-Process -Name eboot -ErrorAction SilentlyContinue
if (-not $ebootProc) {
    Write-Host "eboot.exe process not found." -ForegroundColor Yellow
    exit 1
}

$procId = $ebootProc[0].Id
$bmpPath = "S:\AnyPS5\extracted\screenshot.bmp"
$pngPath = Join-Path "S:\AnyPS5\extracted" $OutName

& S:\AnyPS5\extracted\screenshot.exe $procId $bmpPath | Out-Null

if (Test-Path $bmpPath) {
    $img = [System.Drawing.Image]::FromFile($bmpPath)
    $w = $img.Width
    $h = $img.Height
    $img.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $img.Dispose()
    Remove-Item -Force $bmpPath -ErrorAction SilentlyContinue
    Write-Host "Captured screenshot (${w}x${h}) to $pngPath" -ForegroundColor Green
} else {
    Write-Host "Failed to capture bitmap." -ForegroundColor Red
}
