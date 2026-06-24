<#
.SYNOPSIS
    Assemble the APAngband release artifacts.

.DESCRIPTION
    Produces two files in dist/ at the repo root:

      * APAngband-win64.zip - the runnable Windows bundle (angband.exe, the
        Archipelago runtime DLLs, angband.INI, and a scrubbed copy of lib/).
      * angband.apworld     - the Archipelago generation world (the angband/
        source folder, packaged under the internal package name "angband").
        Filename matches the package dir so AP's installer accepts it.

    angband.exe and the DLLs are build outputs (gitignored), so build first
    from the MSYS2 MinGW64 shell:  cd src && mingw32-make -f Makefile.win MINGW=yes

    Run from anywhere:  powershell -ExecutionPolicy Bypass -File scripts\make-release.ps1
#>

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

# Repo root is the parent of this script's directory.
$Root  = Split-Path -Parent $PSScriptRoot
$Dist  = Join-Path $Root 'dist'

# The 11 runtime DLLs the AP client needs (mirrors RUNTIME_DLLS in Makefile.win).
$Dlls = @(
    'libwebsockets.dll', 'libjansson-4.dll', 'libglib-2.0-0.dll', 'libintl-8.dll',
    'libcrypto-3-x64.dll', 'libssl-3-x64.dll', 'libpcre2-8-0.dll', 'libiconv-2.dll',
    'libwinpthread-1.dll', 'libpng16-16.dll', 'zlib1.dll'
)

Add-Type -AssemblyName System.IO.Compression           # ZipArchive, ZipArchiveMode
Add-Type -AssemblyName System.IO.Compression.FileSystem # ZipFile, ZipFileExtensions

function New-Zip([string]$FolderToZip, [string]$ZipPath) {
    # Build the archive entry-by-entry rather than with CreateFromDirectory, for
    # two reasons that both matter for a distributable release:
    #   * forward-slash separators - PS 5.1's .NET Framework CreateFromDirectory
    #     writes backslashes, which some extractors treat as literal filename
    #     characters (producing files called "lib\gamedata\...");
    #   * empty directories - Compress-Archive drops them, but the game quits if
    #     lib\user\{save,scores,panic,archive} are missing, so we emit explicit
    #     directory entries for any empty folder.
    # Relative paths are taken from the PARENT of $FolderToZip, so its own name
    # becomes the top-level entry (e.g. "APAngband-win64/..." or "apangband/...").
    if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
    $FolderToZip = (Resolve-Path $FolderToZip).Path.TrimEnd('\')
    $baseLen = (Split-Path -Parent $FolderToZip).Length + 1
    $opt = [System.IO.Compression.CompressionLevel]::Optimal

    $zip = [System.IO.Compression.ZipFile]::Open(
        $ZipPath, [System.IO.Compression.ZipArchiveMode]::Create)
    try {
        foreach ($item in Get-ChildItem -LiteralPath $FolderToZip -Recurse -Force) {
            $rel = $item.FullName.Substring($baseLen) -replace '\\', '/'
            if ($item.PSIsContainer) {
                # Only empty dirs need their own entry; non-empty ones are implied.
                if (-not (Get-ChildItem -LiteralPath $item.FullName -Force)) {
                    $null = $zip.CreateEntry("$rel/")
                }
            } else {
                $null = [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
                    $zip, $item.FullName, $rel, $opt)
            }
        }
    } finally {
        $zip.Dispose()
    }
}

# Fresh dist/.
if (Test-Path $Dist) { Remove-Item $Dist -Recurse -Force }
New-Item -ItemType Directory -Path $Dist | Out-Null

# ---------------------------------------------------------------------------
# 1. APAngband-win64.zip
# ---------------------------------------------------------------------------
Write-Host '== Building APAngband-win64.zip =='

$exe = Join-Path $Root 'angband.exe'
if (-not (Test-Path $exe)) {
    throw "angband.exe not found at repo root. Build it first from the MSYS2 " +
          "MinGW64 shell: cd src && mingw32-make -f Makefile.win MINGW=yes"
}

$missing = $Dlls | Where-Object { -not (Test-Path (Join-Path $Root $_)) }
if ($missing) {
    throw "Missing runtime DLL(s) at repo root: $($missing -join ', '). " +
          "The Makefile.win build copies these next to the exe; re-run it."
}

$stageWin = Join-Path $Dist 'APAngband-win64'
New-Item -ItemType Directory -Path $stageWin | Out-Null

# Top-level runtime files.
Copy-Item $exe $stageWin
Copy-Item (Join-Path $Root 'angband.INI') $stageWin
foreach ($d in $Dlls) { Copy-Item (Join-Path $Root $d) $stageWin }

# The lib/ data tree.
Copy-Item (Join-Path $Root 'lib') (Join-Path $stageWin 'lib') -Recurse

$libStage = Join-Path $stageWin 'lib'

# Drop build files that have no runtime purpose.
Get-ChildItem $libStage -Recurse -File -Force |
    Where-Object { $_.Name -in @('Makefile', '.gitignore') } |
    Remove-Item -Force

# Wipe per-user runtime data but keep the (now empty) directory skeleton -
# the game refuses to start if lib\user\{save,scores,panic,archive} are absent.
$userStage = Join-Path $libStage 'user'
if (Test-Path $userStage) {
    Get-ChildItem $userStage -Recurse -File -Force | Remove-Item -Force
}

# Stray Python caches, just in case any tool dropped one under lib/.
Get-ChildItem $libStage -Recurse -Directory -Force |
    Where-Object { $_.Name -eq '__pycache__' } |
    Remove-Item -Recurse -Force

$winZip = Join-Path $Dist 'APAngband-win64.zip'
New-Zip $stageWin $winZip
Remove-Item $stageWin -Recurse -Force

# ---------------------------------------------------------------------------
# 2. angband.apworld
# ---------------------------------------------------------------------------
Write-Host '== Building angband.apworld =='

$worldSrc = Join-Path $Root 'angband'
if (-not (Test-Path (Join-Path $worldSrc 'world.py'))) {
    throw "apworld source not found at $worldSrc (expected world.py)."
}

# AP imports the world by the package-folder name inside the .apworld. We stage
# it under "angband" (the "Angband" game name itself comes from archipelago.json,
# independent of this folder name).
$stageApw = Join-Path $Dist 'apworld-stage'
$pkgStage = Join-Path $stageApw 'angband'
New-Item -ItemType Directory -Path $pkgStage | Out-Null

# Copy everything except Python caches (version-specific, not for distribution).
Get-ChildItem $worldSrc -Force |
    Where-Object { $_.Name -ne '__pycache__' } |
    ForEach-Object { Copy-Item $_.FullName $pkgStage -Recurse -Force }

Get-ChildItem $pkgStage -Recurse -Directory -Force |
    Where-Object { $_.Name -eq '__pycache__' } |
    Remove-Item -Recurse -Force

# Build as .zip first (New-Zip's top-level entry is the folder name "angband"),
# then rename to the .apworld extension Archipelago expects.
#
# The filename stem MUST contain the internal package-folder name: AP's installer
# checks `directories[0] in apworld_path.stem` (LauncherComponents._install_apworld)
# and otherwise aborts with the misleading "expected a single directory" error.
# Package dir and filename are both "angband", so the check passes.
$apwZip = Join-Path $Dist 'angband.apworld.zip'
New-Zip $pkgStage $apwZip
$apw = Join-Path $Dist 'angband.apworld'
if (Test-Path $apw) { Remove-Item $apw -Force }
Rename-Item $apwZip 'angband.apworld'
Remove-Item $stageApw -Recurse -Force

# ---------------------------------------------------------------------------
Write-Host ''
Write-Host 'Release artifacts written to dist/:'
Get-ChildItem $Dist | ForEach-Object {
    '  {0,-24} {1,10:N0} bytes' -f $_.Name, $_.Length | Write-Host
}
