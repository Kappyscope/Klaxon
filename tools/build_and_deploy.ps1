# build_and_deploy.ps1 - Klaxon build + SD payload staging (PC-side)
#
# Builds the .wuhb via the devkitPro msys2 toolchain and stages everything a
# user drags onto their SD card:
#
#   dist/Klaxon-ActionMax/Klaxon-ActionMax.wuhb
#   dist/Klaxon-ActionMax/data/media/common/*.mpg
#   dist/Klaxon-ActionMax/data/media/<game>/tape.mpg
#
# Copy the WHOLE dist/Klaxon-ActionMax folder to <SD>:/wiiu/apps/
#
# Build notes (hard-won on this machine, do not "simplify"):
#   * The Wii U toolchain requires the devkitPro msys2 cmake, launched from an
#     msys2 login shell (bash -lc).  The Windows cmake hard-fails in
#     dkp-initialize-path.cmake, and calling the msys2 cmake directly from
#     PowerShell breaks its compiler-ABI try_compile.
#   * MSYS2 path conversion mangles Windows-style path arguments; the project
#     is accessed through the space-free junction E:\klaxon-src inside bash.

param(
    [switch]$ConfigureOnly
)

$ErrorActionPreference = 'Stop'

$Root      = Split-Path -Parent $PSScriptRoot
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = $PSScriptRoot }
$Bash      = 'E:\devkitPro\msys2\usr\bin\bash.exe'

# ---------------------------------------------------------------------------
# 0. Space-free paths
#
# The devkitPro msys2 toolchain mangles Windows-style arguments and both cmake
# and make choke on spaces, so the project is reached through a junction whose
# path has no spaces.  Earlier versions simply assumed E:\klaxon-src already
# existed (it was made by hand), which meant a fresh clone could not build.
# This creates it, or falls back to %TEMP% when the project drive is not
# writable, and derives the build directory the same way.
# ---------------------------------------------------------------------------
function ConvertTo-MsysPath([string]$win) {
    return '/' + $win.Substring(0, 1).ToLower() + (($win.Substring(2)) -replace '\\', '/')
}

function Ensure-SourceJunction([string]$target) {
    $driveRoot = (Split-Path -Qualifier $target) + '\'
    $candidates = @( (Join-Path $driveRoot 'klaxon-src') )
    if ($env:TEMP -and ($env:TEMP -notmatch '\s')) { $candidates += (Join-Path $env:TEMP 'klaxon-src') }

    foreach ($path in $candidates) {
        if ($path -match '\s') { continue }
        if (Test-Path $path) {
            $item = Get-Item $path -Force
            if ($item.LinkType -eq 'Junction' -and @($item.Target)[0] -eq $target) {
                return $path                      # already correct
            }
            if ($item.LinkType -eq 'Junction') { Remove-Item $path -Force }
            elseif ((Get-ChildItem $path -Force | Measure-Object).Count -eq 0) { Remove-Item $path -Force }
            else { continue }                     # real directory with content: leave it alone
        }
        try {
            New-Item -ItemType Junction -Path $path -Target $target -ErrorAction Stop | Out-Null
            return $path
        } catch { }
    }
    return $null
}

$SrcJunction = Ensure-SourceJunction $Root
if (-not $SrcJunction) {
    Write-Host 'FATAL: could not create a space-free junction to the project folder.' -ForegroundColor Red
    Write-Host '       Move the project to a path without spaces, or create one manually, e.g.:' -ForegroundColor Red
    Write-Host ('       New-Item -ItemType Junction -Path C:\klaxon-src -Target "{0}"' -f $Root) -ForegroundColor Red
    exit 1
}

$driveRoot = (Split-Path -Qualifier $Root) + '\'
$buildCandidates = @( (Join-Path $driveRoot 'klaxon-build') )
if ($env:TEMP -and ($env:TEMP -notmatch '\s')) { $buildCandidates += (Join-Path $env:TEMP 'klaxon-build') }
$BuildWin = $buildCandidates | Where-Object { $_ -notmatch '\s' } | Select-Object -First 1
if (-not $BuildWin) { Write-Host 'FATAL: no space-free location for the build tree.' -ForegroundColor Red; exit 1 }
New-Item -ItemType Directory -Force -Path $BuildWin | Out-Null

$BuildDir  = ConvertTo-MsysPath $BuildWin
$SrcDir    = ConvertTo-MsysPath $SrcJunction
$DistApp   = Join-Path $Root 'dist\Klaxon-ActionMax'
$WuhbInBuild  = Join-Path $BuildWin 'Klaxon-ActionMax.wuhb'
$WuhbInDist   = Join-Path $DistApp 'Klaxon-ActionMax.wuhb'
$MediaDir     = Join-Path $Root 'sd_data\media'

function Log($msg) { Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg) }
function Fail($msg) {
    Write-Host ("[{0}] FATAL: {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg) -ForegroundColor Red
    exit 1
}

Log ('source junction: {0}' -f $SrcJunction)
Log ('build tree     : {0}' -f $BuildWin)

Write-Host ''
Write-Host '======================================================' -ForegroundColor Cyan
Write-Host '  KLAXON - build & deploy' -ForegroundColor Cyan
Write-Host '======================================================' -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# 1. Configure (only when the build tree is missing)
# ---------------------------------------------------------------------------
if (-not (Test-Path (Join-Path $BuildWin 'CMakeCache.txt'))) {
    Log 'configuring build tree (msys2 cmake)...'
    & $Bash -lc "cd $SrcDir && cmake -B $BuildDir -DCMAKE_TOOLCHAIN_FILE=/opt/devkitpro/cmake/WiiU.cmake 2>&1"
    if ($LASTEXITCODE -ne 0) { Fail 'cmake configure failed' }
} else {
    Log 'build tree already configured'
}
if ($ConfigureOnly) { Log 'configure-only requested, stopping.'; exit 0 }

# ---------------------------------------------------------------------------
# 2. Build
# ---------------------------------------------------------------------------
Log 'building (this compiles the MPEG decoder, takes a minute)...'
& $Bash -lc "cd $SrcDir && cmake --build $BuildDir 2>&1"
if ($LASTEXITCODE -ne 0) { Fail 'build failed - read the compiler errors above' }

if (-not (Test-Path $WuhbInBuild)) { Fail "build did not produce $WuhbInBuild" }

# ---------------------------------------------------------------------------
# 3. Stage the SD payload
# ---------------------------------------------------------------------------
Log 'staging dist/Klaxon-ActionMax ...'
if (Test-Path $DistApp) { Remove-Item -Recurse -Force $DistApp }
New-Item -ItemType Directory -Force -Path $DistApp | Out-Null

Copy-Item $WuhbInBuild $WuhbInDist -Force
$wuhbMB = (Get-Item $WuhbInDist).Length / 1MB
Log ("  .wuhb: {0:N1} MB (assets + game configs + manifest inside)" -f $wuhbMB)

$dataDir = Join-Path $DistApp 'data'
if (Test-Path $MediaDir) {
    New-Item -ItemType Directory -Force -Path $dataDir | Out-Null
    Copy-Item -Path $MediaDir -Destination (Join-Path $dataDir 'media') -Recurse -Force
    $mediaFiles = (Get-ChildItem -Recurse -File (Join-Path $dataDir 'media') | Measure-Object).Count
    $mediaMB = ((Get-ChildItem -Recurse -File (Join-Path $dataDir 'media') | Measure-Object Length -Sum).Sum) / 1MB
    Log ("  data/media: {0} files, {1:N0} MB" -f $mediaFiles, $mediaMB)
    if ($mediaFiles -lt 7) {
        Log '  WARNING: fewer than 7 media files (2 common + 5 games expected).' -ForegroundColor Yellow
        Log '  Run tools/prepare_media.ps1 first - see README.md.' -ForegroundColor Yellow
    }
} else {
    Log '  WARNING: no sd_data/media - the .wuhb alone will show NO READY GAMES.' -ForegroundColor Yellow
    Log '  Run tools/prepare_media.ps1 first - see README.md.' -ForegroundColor Yellow
}

# Assets also staged to the SD: the app reads them from data/assets FIRST
# (content-mount reads can hang on hardware once audio is up).
foreach ($sub in @('assets', 'games', 'manifest')) {
    $src = Join-Path $Root ("romfs\" + $sub)
    if (Test-Path $src) {
        Copy-Item -Path $src -Destination (Join-Path $dataDir $sub) -Recurse -Force
        Log ("  data/{0}: staged (SD-first asset reads)" -f $sub)
    }
}

# ---------------------------------------------------------------------------
# 4. Done
# ---------------------------------------------------------------------------
Write-Host ''
Write-Host '======================================================' -ForegroundColor Green
Write-Host '  deploy folder:' $DistApp
Write-Host '  copy the WHOLE folder to  <SD>:/wiiu/apps/'
Write-Host '======================================================' -ForegroundColor Green
Write-Host ''
Write-Host '  SD layout after copy:'
Write-Host '    <SD>:/wiiu/apps/Klaxon-ActionMax/Klaxon-ActionMax.wuhb'
Write-Host '    <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/common/*.mpg'
Write-Host '    <SD>:/wiiu/apps/Klaxon-ActionMax/data/media/<game>/tape.mpg'
Write-Host ''
Write-Host '  A fresh klaxon.log lands next to the .wuhb on boot.' -ForegroundColor DarkGray
