# prepare_media.ps1 - Klaxon media pipeline (PC-side, run once)
#
# Converts the original Action Max rips into the single runtime format Klaxon
# uses: one muxed MPEG-1 System stream (.mpg) per tape segment - video MPEG-1
# 720x480 at AUTHORITATIVE 29.97 fps, audio MPEG-1 Layer II 48 kHz stereo.
# pl_mpeg decodes video + audio from that one file with intrinsic sync.
#
# Gates (the pipeline fails loudly instead of producing bad media):
#   1. Source hash verification against media/source_hashes.sha256
#   2. Source frame counts vs the Singe-authored lengths (from the .txt files)
#   3. Output codec/fps/frame-count validation with ffprobe
#
# Outputs:
#   sd_data/media/common/intro.mpg, menu.mpg
#   sd_data/media/<game>/tape.mpg            (5 games)
#   sd_data/media/manifest.txt               (hashes of the produced files)
#   romfs/games/<game>/game.cfg              (generated from the Singe .txt)
#   romfs/assets/backgrounds/<game>.png      (backgroundFrame title stills)
#   romfs/manifest/prepared_hashes.sha256    (4-col runtime manifest)

param(
    [string]$SourceRoot = 'E:\ACTIONMAX RESOURCES',
    [string]$Tree = 'Singe - IA',          # Singe - IA = original Singe 1 media (matches authored frames); Remux - IA has +/-1-3 frame drift
    [string]$VideoBitrate = '4000k',
    [switch]$SkipHashCheck,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

$Root = Split-Path -Parent $PSScriptRoot   # script lives in tools\; project root is its parent
if (-not (Test-Path (Join-Path $Root 'CMakeLists.txt'))) { $Root = $PSScriptRoot }
$SrcDir        = Join-Path $SourceRoot $Tree
$CommonSrc     = Join-Path $SrcDir 'actionmax'
$MediaDir      = Join-Path $Root 'sd_data\media'
$MediaCommon   = Join-Path $MediaDir 'common'
$RomfsGames    = Join-Path $Root 'romfs\games'
$RomfsBgDir    = Join-Path $Root 'romfs\assets\backgrounds'
$RomfsManifest = Join-Path $Root 'romfs\manifest'
$HashManifest  = Join-Path $Root 'media\source_hashes.sha256'

$Games = @('38ambushalley','bluethunder','hydrosub2021','popsghostly','sonicfury')

$Common = @{
    'intro' = 'video_actionmaxintro'
    'menu'  = 'video_menu'
}

$GameTitles = @{
    '38ambushalley' = '38 Ambush Alley'
    'bluethunder'   = 'Blue Thunder'
    'hydrosub2021'  = 'Hydro Sub 2021'
    'popsghostly'   = "The Rescue of Pops Ghostly"
    'sonicfury'     = 'Sonic Fury'
}

function Log($msg) {
    Write-Host ("[{0}] {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg)
}
function Fail($msg) {
    Write-Host ("[{0}] FATAL: {1}" -f (Get-Date -Format 'HH:mm:ss'), $msg) -ForegroundColor Red
    exit 1
}

# ---------------------------------------------------------------------------
# 0. Sanity
# ---------------------------------------------------------------------------
if (-not (Test-Path $CommonSrc)) {
    Fail "Source tree not found: $CommonSrc"
}
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) { Fail "ffmpeg not in PATH" }
if (-not (Get-Command ffprobe -ErrorAction SilentlyContinue)) { Fail "ffprobe not in PATH" }

New-Item -ItemType Directory -Force -Path $MediaCommon, $RomfsGames, $RomfsBgDir, $RomfsManifest | Out-Null

# ---------------------------------------------------------------------------
# 1. Hash verification of the source files
# ---------------------------------------------------------------------------
$srcHashes = @{}
if (Test-Path $HashManifest) {
    Get-Content $HashManifest | ForEach-Object {
        if ($_ -match '^([0-9a-fA-F]{64})\s+\*(.+)$') { $srcHashes[$Matches[2].Replace('/','\')] = $Matches[1].ToLower() }
        elseif ($_ -match '^([0-9a-fA-F]{64})\s+(.+)$') { $srcHashes[$Matches[2].Replace('/','\')] = $Matches[1].ToLower() }
    }
}

$computedNew = 0
function Verify-SourceHash {
    param([string]$Path)
    if ($SkipHashCheck) { return }
    if (-not (Test-Path $Path)) { Fail "Missing source file: $Path" }
    $rel = $Path.Substring($SourceRoot.Length + 1)
    $actual = (Get-FileHash -Algorithm SHA256 $Path).Hash.ToLower()
    if ($srcHashes.ContainsKey($rel)) {
        if ($srcHashes[$rel] -ne $actual) {
            Fail "HASH MISMATCH for $rel`n  expected $($srcHashes[$rel])`n  actual   $actual"
        }
        Log ("hash OK  {0}" -f $rel)
    } else {
        Log ("hash NEW {0}  (recorded as {1})" -f $rel, $actual)
        $script:computedNew++
    }
}

# ---------------------------------------------------------------------------
# 2. Parse the Singe .txt game scripts (the authoritative game data)
# ---------------------------------------------------------------------------
$cfgKeys = @('gameID','lengthIntro','lengthGame','lengthMenu','backgroundFrame',
             'highThreshold','lowThreshold','sensorTop','sensorLeft','sensorX','sensorY')

function Parse-SingeConfig {
    # Parses the ORIGINAL .singe scripts (extracted from actionmax.zip by
    # extract_assets.ps1 into romfs/singe/).  gameID is optional (the file
    # name is the id); every playback-critical key is required.
    param([string]$Path)
    $cfg = @{}
    foreach ($line in Get-Content $Path) {
        if ($line -match '^\s*(\w+)\s*=\s*"?([^"\r\n]+)"?\s*$') {
            $k = $Matches[1]; $v = $Matches[2].Trim()
            if ($cfgKeys -contains $k) { $cfg[$k] = $v }
        }
    }
    foreach ($k in $cfgKeys) {
        if ($k -ne 'gameID' -and -not $cfg.ContainsKey($k)) {
            Fail "Singe script $Path is missing key '$k'"
        }
    }
    return $cfg
}

function Parse-DaphneFrameMap {
    # The distribution's .txt files are DAPHNE frame maps:
    #   <startFrame>  <file>
    # Returns a map of segment start frames for cross-validation.
    param([string]$Path, [string]$Game)
    $map = @{}
    foreach ($line in Get-Content $Path) {
        if ($line -match '^\s*(\d+)\s+(.+)$') {
            $frame = [int]$Matches[1]
            $file  = $Matches[2].Trim()
            if ($file -match 'actionmaxintro') { $map['intro'] = $frame }
            elseif ($file -match 'video_menu') { $map['menu'] = $frame }
            elseif ($file -match $Game)        { $map['game'] = $frame }
        }
    }
    return $map
}

# ---------------------------------------------------------------------------
# 3. Helpers
# ---------------------------------------------------------------------------
function Get-VideoFrames {
    param([string]$Path)
    $n = ffprobe -v error -count_frames -select_streams v:0 `
         -show_entries stream=nb_read_frames -of csv=p=0 $Path
    return [int]($n.Trim().TrimEnd(','))
}

function Get-FfprobeCsv {
    # ffprobe CSV output carries a trailing comma; strip it.
    param([string[]]$ArgList)
    $out = & ffprobe @ArgList
    return ($out -join '').Trim().TrimEnd(',')
}

function Get-Mpeg1FrameRateCode {
    # Reads the frame_rate_code from the first MPEG-1 sequence header
    # (start code 00 00 01 B3).  Code 4 = 30000/1001 (29.97).
    # NOTE: the bundled ffprobe nightly misreports r_frame_rate for MPEG-1
    # streams (a synthetic 29.97 encode probes as 60000/1001), so the gate
    # below trusts the sequence header itself - the same thing pl_mpeg reads.
    param([string]$Path)
    $bytes = [IO.File]::ReadAllBytes($Path)
    for ($i = 0; $i -lt ($bytes.Length - 12); $i++) {
        if ($bytes[$i] -eq 0 -and $bytes[$i+1] -eq 0 -and $bytes[$i+2] -eq 1 -and $bytes[$i+3] -eq 0xB3) {
            return [int]($bytes[$i+7] -band 0x0F)
        }
    }
    return 0
}

function Get-QuickHash {
    # SHA-256 of (first 256 KiB of the file) + (8-byte LE file size).
    # Must match Sha256::quickFile() in the app byte for byte.
    param([string]$Path)
    $sha = [System.Security.Cryptography.SHA256]::Create()
    $fs  = [IO.File]::OpenRead($Path)
    try {
        $buf = New-Object byte[] 262144
        $n = $fs.Read($buf, 0, $buf.Length)
        if ($n -gt 0) { [void]$sha.TransformBlock($buf, 0, $n, $null, 0) }
        $sizeBytes = [BitConverter]::GetBytes([uint64]$fs.Length)
        [void]$sha.TransformFinalBlock($sizeBytes, 0, 8)
        return ([BitConverter]::ToString($sha.Hash)).Replace('-','').ToLower()
    } finally {
        $fs.Close()
        $sha.Dispose()
    }
}

# ---------------------------------------------------------------------------
# 4. Convert one segment
# ---------------------------------------------------------------------------
function Convert-Segment {
    param([string]$Name, [string]$M2v, [string]$Ogg, [int]$AuthoredLen, [string]$OutPath)

    Verify-SourceHash $M2v
    Verify-SourceHash $Ogg

    if ((Test-Path $OutPath) -and -not $Force) {
        Log "skip (exists): $OutPath  (use -Force to re-encode)"
        return
    }

    $srcFrames = Get-VideoFrames $M2v
    if ($AuthoredLen -gt 0) {
        if ($srcFrames -lt $AuthoredLen -or $srcFrames -gt ($AuthoredLen + 12)) {
            Fail "$Name : source has $srcFrames frames but Singe authored length is $AuthoredLen (+12 max)"
        }
        Log ("{0}: {1} source frames (authored {2})" -f $Name, $srcFrames, $AuthoredLen)
    }

    $tmp = "$OutPath.tmp"
    New-Item -ItemType Directory -Force -Path (Split-Path -Parent $OutPath) | Out-Null
    Log ("encoding {0} -> {1}" -f $Name, $OutPath)
    & ffmpeg -y -hide_banner -loglevel warning -stats `
        -i $M2v -i $Ogg `
        -map 0:v:0 -map 1:a:0 `
        -c:v mpeg1video -b:v $VideoBitrate -maxrate 6000k -bufsize 1835k -pix_fmt yuv420p `
        -r 30000/1001 -fps_mode cfr `
        -c:a mp2 -b:a 192k -ar 48000 -ac 2 `
        -f mpeg `
        $tmp
    if ($LASTEXITCODE -ne 0) { Fail "$Name : ffmpeg failed" }

    # --- output validation gate ---
    $vcodec = Get-FfprobeCsv @('-v','error','-select_streams','v:0','-show_entries','stream=codec_name','-of','csv=p=0',$tmp)
    $acodec = Get-FfprobeCsv @('-v','error','-select_streams','a:0','-show_entries','stream=codec_name','-of','csv=p=0',$tmp)
    $outFrames = Get-VideoFrames $tmp
    $rateCode = Get-Mpeg1FrameRateCode $tmp

    if ($vcodec.Trim() -ne 'mpeg1video') { Fail "$Name : output video codec is '$vcodec', must be mpeg1video" }
    if ($acodec.Trim() -ne 'mp2')        { Fail "$Name : output audio codec is '$acodec', must be mp2" }
    if ($rateCode -ne 4) {
        Fail "$Name : sequence header frame_rate_code is $rateCode, must be 4 (30000/1001)"
    }
    if ([Math]::Abs($outFrames - $srcFrames) -gt 2) {
        Fail "$Name : output has $outFrames frames, source had $srcFrames"
    }

    Move-Item -LiteralPath $tmp -Destination $OutPath -Force
    $mb = (Get-Item $OutPath).Length / 1MB
    Log ("OK {0}: {1} frames validated, {2:N1} MB" -f $Name, $outFrames, $mb)
}

# ---------------------------------------------------------------------------
# 5. Run everything
# ---------------------------------------------------------------------------
Log "=== Klaxon media pipeline (source tree: $Tree) ==="

# Common segments
foreach ($name in $Common.Keys) {
    $stem = $Common[$name]
    Convert-Segment -Name $name `
        -M2v (Join-Path $CommonSrc "$stem.m2v") `
        -Ogg (Join-Path $CommonSrc "$stem.ogg") `
        -AuthoredLen 0 `
        -OutPath (Join-Path $MediaCommon "$name.mpg")
}

# Game segments + game.cfg + title background stills
foreach ($game in $Games) {
    $singeScript = Join-Path $Root "romfs\singe\$game.singe"
    if (-not (Test-Path $singeScript)) {
        Fail "Missing Singe script: $singeScript (run tools/extract_assets.ps1 first)"
    }
    Verify-SourceHash (Join-Path $CommonSrc "$game.txt")
    $cfg = Parse-SingeConfig $singeScript

    # Cross-validate the Singe numbers against the DAPHNE frame map, which is
    # frame-accurate segment boundaries straight from the distribution.
    $frameMap = Parse-DaphneFrameMap (Join-Path $CommonSrc "$game.txt") $game
    if ($frameMap.ContainsKey('intro') -and $frameMap.ContainsKey('game')) {
        $mapIntro = $frameMap['game'] - $frameMap['intro']
        if ([Math]::Abs($mapIntro - [int]$cfg['lengthIntro']) -gt 2) {
            Fail "$game : DAPHNE map intro=$mapIntro but Singe lengthIntro=$($cfg['lengthIntro'])"
        }
        Log ("{0}: map intro={1} matches Singe lengthIntro={2}" -f $game, $mapIntro, $cfg['lengthIntro'])
    }
    if ($frameMap.ContainsKey('game') -and $frameMap.ContainsKey('menu')) {
        $mapGame = $frameMap['menu'] - $frameMap['game']
        if ([Math]::Abs($mapGame - [int]$cfg['lengthGame']) -gt 3) {
            Fail "$game : DAPHNE map game=$mapGame but Singe lengthGame=$($cfg['lengthGame'])"
        }
        Log ("{0}: map game={1} matches Singe lengthGame={2}" -f $game, $mapGame, $cfg['lengthGame'])
    }

    Convert-Segment -Name $game `
        -M2v (Join-Path $CommonSrc "$game\video_$game.m2v") `
        -Ogg (Join-Path $CommonSrc "$game\video_$game.ogg") `
        -AuthoredLen ([int]$cfg['lengthGame']) `
        -OutPath (Join-Path $MediaDir "$game\tape.mpg")

    # Title background still (backgroundFrame, 1-based -> select n >= bg-1)
    $bgFrame = [int]$cfg['backgroundFrame']
    if ($bgFrame -gt 0) {
        $bgOut = Join-Path $RomfsBgDir "$game.png"
        if (-not (Test-Path $bgOut) -or $Force) {
            & ffmpeg -y -hide_banner -loglevel error `
                -i (Join-Path $CommonSrc "$game\video_$game.m2v") `
                -vf ("select=gte(n\,{0})" -f ($bgFrame - 1)) -frames:v 1 `
                $bgOut
            if ($LASTEXITCODE -ne 0 -or -not (Test-Path $bgOut)) {
                Log ("WARNING: background still for {0} failed (bg={1})" -f $game, $bgFrame)
            } else {
                Log ("bg still: {0} (frame {1})" -f $bgOut, $bgFrame)
            }
        }
    }

    # game.cfg for the app - generated from the Singe script, single source of truth
    $gameDir = Join-Path $RomfsGames $game
    New-Item -ItemType Directory -Force -Path $gameDir | Out-Null
    $title = if ($GameTitles.ContainsKey($game)) { $GameTitles[$game] } else { $cfg['gameID'] }
    $lines = @(
        "# Klaxon game config - generated by tools/prepare_media.ps1"
        "# Source: $Tree/actionmax/$game.txt (original Singe script by ScottD)"
        "gameID=$($cfg['gameID'])"
        "title=$title"
        "highThreshold=$($cfg['highThreshold'])"
        "lowThreshold=$($cfg['lowThreshold'])"
        "sensorX=$($cfg['sensorX'])"
        "sensorY=$($cfg['sensorY'])"
        "sensorLeft=$($cfg['sensorLeft'])"
        "sensorTop=$($cfg['sensorTop'])"
        "lengthIntro=$($cfg['lengthIntro'])"
        "lengthGame=$($cfg['lengthGame'])"
        "lengthMenu=$($cfg['lengthMenu'])"
        "backgroundFrame=$($cfg['backgroundFrame'])"
        "media=tape.mpg"
    )
    [IO.File]::WriteAllLines((Join-Path $gameDir 'game.cfg'), $lines)
    Log ("cfg written: romfs\games\{0}\game.cfg" -f $game)
}

# ---------------------------------------------------------------------------
# 6. Runtime manifest: <full-sha256>  <quick-sha256>  <size>  <relpath>
# ---------------------------------------------------------------------------
$prepared = @()
Get-ChildItem -Recurse -File $MediaDir -Include *.mpg | ForEach-Object {
    $rel   = $_.FullName.Substring($MediaDir.Length + 1).Replace('\','/')
    $full  = (Get-FileHash -Algorithm SHA256 $_.FullName).Hash.ToLower()
    $quick = Get-QuickHash $_.FullName
    $prepared += "$full  $quick  $($_.Length)  $rel"
}
[IO.File]::WriteAllLines((Join-Path $Root 'media\prepared_hashes.sha256'), $prepared)
[IO.File]::WriteAllLines((Join-Path $RomfsManifest 'prepared_hashes.sha256'), $prepared)
Log ("runtime manifest written: {0} media files" -f $prepared.Count)

if ($computedNew -gt 0) {
    Log "NOTE: $computedNew source files were not in media/source_hashes.sha256 - update it (tools/update_source_hashes.ps1)"
}

Log "=== media pipeline COMPLETE ==="
