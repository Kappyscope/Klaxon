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
    [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int cmd);
    [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
}
"@

$target = Get-Process Cemu -ErrorAction SilentlyContinue |
          Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if ($target) {
    # Pin the window TOPMOST before capturing.  Screen-rect capture of a window
    # that something else overlaps grabs the overlapping pixels instead - that
    # is how a browser window once ended up in a screenshot.
    $h = $target.MainWindowHandle
    [void][W]::ShowWindow($h, 9)              # SW_RESTORE
    [void][W]::SetWindowPos($h, [IntPtr](-1), 0, 0, 0, 0, 0x0001 -bor 0x0002 -bor 0x0040)  # HWND_TOPMOST, NOSIZE|NOMOVE|SHOWWINDOW
    [void][W]::BringWindowToTop($h)
    [void][W]::SetForegroundWindow($h)
    Start-Sleep -Milliseconds 900

    # Refuse to capture unless Cemu is genuinely the focused window.  A screen
    # rectangle grab of an occluded/closed window photographs whatever is behind
    # it - that is how a desktop once ended up in a file.
    $focused = ([W]::GetForegroundWindow() -eq $h)
    if (-not $focused) {
        Write-Host "Cemu is not the focused window - capture skipped (never photograph other windows)"
        [void][W]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002)
    }

    if ($focused) {
        $r = New-Object W+RECT
        [void][W]::GetWindowRect($h, [ref]$r)
        $w = $r.R - $r.L
        $hgt = $r.B - $r.T
        if ($w -gt 0 -and $hgt -gt 0) {
            $bmp = New-Object System.Drawing.Bitmap $w, $hgt
            $g   = [System.Drawing.Graphics]::FromImage($bmp)
            $g.CopyFromScreen($r.L, $r.T, 0, 0, $bmp.Size)
            $bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
            $g.Dispose()
            $bmp.Dispose()
            Write-Host ("screenshot: {0} ({1}x{2})" -f $Out, $w, $hgt)
        }
        [void][W]::SetWindowPos($h, [IntPtr](-2), 0, 0, 0, 0, 0x0001 -bor 0x0002)
    }  # back to NOTOPMOST
} else {
    # Never fall back to the desktop: an earlier version captured the whole
    # screen here and a browser window ended up in a commit.  Window only.
    Write-Host "no Cemu window found - skipping capture (no desktop fallback)"
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
