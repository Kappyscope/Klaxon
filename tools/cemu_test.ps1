# cemu_test.ps1 - run Klaxon in Cemu and grab a screenshot + log tail.
#
# Launches Cemu with the .wuhb, waits for the app to boot, captures the Cemu
# window to a PNG, then prints the tail of the app's klaxon.log.  This is the
# fast loop: Cemu emulates the AX audio path, so boot/audio regressions show up
# here without touching the console.
#
#   .\tools\cemu_test.ps1
#   .\tools\cemu_test.ps1 -Wait 60 -KeepRunning
param(
    [int]$Wait = 30,
    [string]$Out = "",
    [switch]$KeepRunning
)

$ErrorActionPreference = 'Stop'

$Root     = Split-Path -Parent $PSScriptRoot
$Cemu     = 'E:\cemu\Cemu_2.6\Cemu.exe'
$CemuSd   = 'C:\Users\mrsco\AppData\Roaming\Cemu\sdcard'
$AppDir   = Join-Path $CemuSd 'wiiu\apps\Klaxon-ActionMax'
$Wuhb     = Join-Path $Root 'dist\Klaxon-ActionMax\Klaxon-ActionMax.wuhb'
$Log      = Join-Path $AppDir 'klaxon.log'
$OutDir   = Join-Path $PSScriptRoot 'out'

if (-not $Out) {
    $Out = Join-Path $OutDir ("cemu-" + (Get-Date -Format 'HHmmss') + ".png")
}
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null

# --- stop leftovers ---------------------------------------------------------
Get-Process Cemu -ErrorAction SilentlyContinue | Stop-Process -Force

# --- deploy current build ---------------------------------------------------
Copy-Item $Wuhb (Join-Path $AppDir 'Klaxon-ActionMax.wuhb') -Force
Remove-Item $Log -Force -ErrorAction SilentlyContinue

# Media (2.4 GB) is linked, not copied: Cemu's sdcard points at the project.
$mediaLink = Join-Path $AppDir 'data\media'
$mediaSrc  = Join-Path $Root 'sd_data\media'
if (-not (Test-Path $mediaLink) -and (Test-Path $mediaSrc)) {
    New-Item -ItemType Directory -Force -Path (Split-Path $mediaLink) | Out-Null
    New-Item -ItemType Junction -Path $mediaLink -Target $mediaSrc | Out-Null
    Write-Host "linked media to $mediaSrc"
}

# --- launch -----------------------------------------------------------------
Write-Host "launching Cemu..."
Start-Process -FilePath $Cemu -ArgumentList @('-g', ('"' + $Wuhb + '"')) | Out-Null

Start-Sleep -Seconds $Wait

# --- screenshot -------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class W {
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@

$target = Get-Process Cemu -ErrorAction SilentlyContinue |
          Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if ($target) {
    $r = New-Object W+RECT
    [void][W]::GetWindowRect($target.MainWindowHandle, [ref]$r)
    $w = $r.R - $r.L
    $h = $r.B - $r.T
    if ($w -gt 0 -and $h -gt 0) {
        $bmp = New-Object System.Drawing.Bitmap $w, $h
        $g   = [System.Drawing.Graphics]::FromImage($bmp)
        $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
        $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
        $g.Dispose()
        $bmp.Dispose()
        Write-Host ("screenshot: {0} ({1}x{2})" -f $Out, $w, $h)
    }
} else {
    # Fallback: capture the whole primary screen (Cemu is foreground).
    $bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
    $bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.CopyFromScreen($bounds.X, $bounds.Y, 0, 0, $bmp.Size)
    $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "no Cemu window handle - captured full screen: $Out"
}

# --- log tail ---------------------------------------------------------------
if (Test-Path $Log) {
    $lines = Get-Content $Log
    Write-Host ""
    Write-Host ("=== klaxon.log ({0} lines) - last 25 ===" -f $lines.Count)
    $lines | Select-Object -Last 25
} else {
    Write-Host ""
    Write-Host "no klaxon.log written"
}

if (-not $KeepRunning) {
    Get-Process Cemu -ErrorAction SilentlyContinue | Stop-Process -Force
    Write-Host ""
    Write-Host "Cemu closed."
}
