# extract_assets.ps1 — Klaxon asset pipeline (PC-side)
#
# Extracts the Singe emulator assets from actionmax.zip (downloaded from
# https://archive.org/details/singev1-actionmax) into romfs\assets, organized:
#   romfs\assets\sprites\*.png   (crosshair, light, bullets, box art, logos)
#   romfs\assets\sounds\*.wav    (gunshot, good/bad hit, game over, buzzer...)
#   romfs\assets\fonts\*.ttf     (original Singe fonts, kept for authenticity)
#
# Validates magic bytes of every extracted file and reports anything missing.

param(
    [string]$ZipPath = 'E:\ACTIONMAX RESOURCES\Remux - IA\actionmax\actionmax.zip',
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot   # script lives in tools\; project root is its parent
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = $PSScriptRoot }
$RomfsAssets = Join-Path $Root 'romfs\assets'

function Log($msg) { Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg) }
function Fail($msg) {
    Write-Host ("[{0}] FATAL: {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg) -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $ZipPath)) { Fail "actionmax.zip not found at $ZipPath" }

if (Test-Path $RomfsAssets) {
    if (-not $Force) {
        $existing = (Get-ChildItem -Recurse -File $RomfsAssets | Measure-Object).Count
        if ($existing -gt 0) {
            Log "romfs\assets already populated ($existing files); use -Force to re-extract"
            return
        }
    }
    # -Force re-extracts only what comes from the zip; fonts downloaded
    # separately (ui_display/ui_body/dseg7 + licenses) are preserved.
    foreach ($sub in @('sprites','sounds')) {
        $p = Join-Path $RomfsAssets $sub
        if (Test-Path $p) { Remove-Item -Recurse -Force $p }
    }
}

New-Item -ItemType Directory -Force -Path $RomfsAssets | Out-Null

# --- Extract to temp, then organize by type ---
$temp = Join-Path $env:TEMP ("klaxon_assets_" + [guid]::NewGuid().ToString('N').Substring(0,8))
New-Item -ItemType Directory -Force -Path $temp | Out-Null

Log "expanding $ZipPath"
Expand-Archive -Path $ZipPath -DestinationPath $temp -Force

$maps = @{
    'sprites' = @('.png')
    'sounds'  = @('.wav')
    'fonts'   = @('.ttf')
}

$count = 0
foreach ($type in $maps.Keys) {
    $dstDir = Join-Path $RomfsAssets $type
    New-Item -ItemType Directory -Force -Path $dstDir | Out-Null
    Get-ChildItem -Recurse -File $temp | Where-Object {
        $maps[$type] -contains $_.Extension.ToLower()
    } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $dstDir $_.Name) -Force
        $count++
    }
}

# Original Singe scripts + DAPHNE frame maps + borf.ini, kept verbatim for
# provenance and parsed by prepare_media.ps1
$singeDir = Join-Path $Root 'romfs\singe'
New-Item -ItemType Directory -Force -Path $singeDir | Out-Null
Get-ChildItem -Recurse -File $temp | Where-Object {
    $_.Extension.ToLower() -in @('.singe', '.txt', '.ini')
} | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $singeDir $_.Name) -Force
    $count++
}

# --- Magic byte validation ---
function Test-Magic {
    param([string]$Path)
    $b = [IO.File]::ReadAllBytes($Path)
    if ($b.Length -lt 12) { return $false }
    $ext = [IO.Path]::GetExtension($Path).ToLower()
    if ($ext -eq '.png') { return ($b[0] -eq 0x89 -and $b[1] -eq 0x50 -and $b[2] -eq 0x4E -and $b[3] -eq 0x47) }
    if ($ext -eq '.wav') { return ($b[0] -eq 0x52 -and $b[1] -eq 0x49 -and $b[2] -eq 0x46 -and $b[3] -eq 0x46) }
    if ($ext -eq '.ttf') {
        $isTtf = ($b[0] -eq 0x00 -and $b[1] -eq 0x01 -and $b[2] -eq 0x00 -and $b[3] -eq 0x00)
        $isOtf = ($b[0] -eq 0x4F -and $b[1] -eq 0x54 -and $b[2] -eq 0x54 -and $b[3] -eq 0x4F)
        return ($isTtf -or $isOtf)
    }
    return $false
}

$bad = @()
Get-ChildItem -Recurse -File $RomfsAssets | ForEach-Object {
    # Only validate media magic bytes; license text files (OFL*.txt) live in
    # the same folder legitimately.
    if ($_.Extension.ToLower() -in @('.png', '.wav', '.ttf') -and
        -not (Test-Magic $_.FullName)) { $bad += $_.FullName }
}
if ($bad.Count -gt 0) {
    Fail ("Invalid files after extraction:`n  " + ($bad -join "`n  "))
}

Remove-Item -Recurse -Force $temp

# --- Report expected Singe files ---
$expected = @(
    'sprites\sprite_crosshair.png','sprites\sprite_lighton.png','sprites\sprite_lightoff.png',
    'sprites\sprite_actionmax.png','sprites\sprite_bullet.png',
    'sprites\sprite_38ambushalley.png','sprites\sprite_bluethunder.png',
    'sprites\sprite_hydrosub2021.png','sprites\sprite_popsghostly.png','sprites\sprite_sonicfury.png',
    'sounds\sound_gunshot.wav','sounds\sound_goodhit.wav','sounds\sound_badhit.wav',
    'sounds\sound_gameover.wav','sounds\sound_actionmax.wav',
    'sounds\sound_asteadyaimiscritical.wav','sounds\sound_getreadyforaction.wav',
    'fonts\font_bluestone.ttf','fonts\font_chemrea.ttf','fonts\font_led_real.ttf'
)
$missing = @()
foreach ($e in $expected) {
    if (-not (Test-Path (Join-Path $RomfsAssets $e))) { $missing += $e }
}

Log ("extracted {0} files into romfs\assets" -f $count)
if ($missing.Count -gt 0) {
    Log ("WARNING: {0} expected Singe files are MISSING from the zip:" -f $missing.Count)
    $missing | ForEach-Object { Log "  missing: $_" }
} else {
    Log "all expected Singe asset files present"
}
Log "=== asset extraction COMPLETE ==="
