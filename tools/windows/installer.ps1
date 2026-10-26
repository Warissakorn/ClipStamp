[CmdletBinding()]
param(
    [string]$PayloadDirectory,
    [string]$OutputDirectory,
    [string]$Compiler
)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $PayloadDirectory) { $PayloadDirectory = Join-Path $projectRoot 'dist/VideoTime' }
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'dist/installers' }
$payload = (Resolve-Path -LiteralPath $PayloadDirectory).Path.TrimEnd('\')
foreach ($required in @('VideoTime.exe','Qt6Core.dll','Qt6Widgets.dll','Qt6Multimedia.dll','qt.conf','plugins/platforms/qwindows.dll','tools/ffmpeg.exe','tools/ffprobe.exe','THIRD-PARTY-NOTICES.md','licenses/NotoSansThai-OFL.txt')) {
    if (-not (Test-Path -LiteralPath (Join-Path $payload $required) -PathType Leaf)) { throw "Incomplete deployment, missing: $required. Run build.ps1 -Package first." }
}
$nsisVersion = '3.12'
$nsisSha256 = '56581f90db321581c5381193d796fffcf2d24b2f8fed2160a6c6a3baa67f2c4f'
$tools = Join-Path $projectRoot '.tools/installer'
if (-not $Compiler) {
    $Compiler = Join-Path $tools "nsis-$nsisVersion/makensis.exe"
    if (-not (Test-Path -LiteralPath $Compiler)) {
        New-Item -ItemType Directory -Path $tools -Force | Out-Null
        $archive = Join-Path $tools "nsis-$nsisVersion.zip"
        if (-not (Test-Path -LiteralPath $archive)) {
            Invoke-WebRequest -UseBasicParsing -Uri "https://downloads.sourceforge.net/project/nsis/NSIS%203/$nsisVersion/nsis-$nsisVersion.zip" -OutFile $archive
        }
        if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $nsisSha256) { throw 'NSIS archive checksum mismatch.' }
        Expand-Archive -LiteralPath $archive -DestinationPath $tools -Force
    }
}
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) { throw "NSIS compiler not found: $Compiler" }
$versionMatch = [regex]::Match((Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw), 'project\(VideoTime VERSION (\d+\.\d+\.\d+)')
if (-not $versionMatch.Success) { throw 'Cannot determine VideoTime version.' }
$version = $versionMatch.Groups[1].Value
$generated = Join-Path $projectRoot 'build/installer'
New-Item -ItemType Directory -Path $generated,$OutputDirectory -Force | Out-Null
$output = Join-Path (Resolve-Path -LiteralPath $OutputDirectory).Path "VideoTime-$version-Setup-x64.exe"
function Escape-Nsis([string]$Value) { return $Value.Replace('$','$$').Replace('"','$\"') }
$files = @(Get-ChildItem -LiteralPath $payload -Recurse -File | Sort-Object FullName)
$sizeKb = [int][Math]::Ceiling(($files | Measure-Object Length -Sum).Sum / 1024)
$manifest = foreach ($file in $files) {
    [pscustomobject]@{ Path = $file.FullName.Substring($payload.Length).TrimStart('\'); Sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash }
}
ConvertTo-Json -InputObject @($manifest) -Depth 3 | Set-Content -LiteralPath (Join-Path $generated 'payload-manifest.json') -Encoding utf8
$config = @(
    ('!define PRODUCT_VERSION "' + $version + '"'),
    ('!define PRODUCT_VERSION_QUAD "' + $version + '.0"'),
    ('!define PAYLOAD_DIR "' + (Escape-Nsis $payload) + '"'),
    ('!define OUTPUT_FILE "' + (Escape-Nsis $output) + '"'),
    ('!define INSTALLED_SIZE_KB ' + $sizeKb)
)
$config | Set-Content -LiteralPath (Join-Path $generated 'config.nsh') -Encoding utf8
$deleteFiles = foreach ($file in $files) {
    $relative = $file.FullName.Substring($payload.Length).TrimStart('\')
    $escaped = Escape-Nsis $relative
    'IfFileExists "$INSTDIR\' + $escaped + '" 0 +2'
    'Delete "$INSTDIR\' + $escaped + '"'
}
$directories = @(Get-ChildItem -LiteralPath $payload -Recurse -Directory | Sort-Object { $_.FullName.Length } -Descending)
# Removing nonempty folders is deliberately allowed to fail, preserving user-owned files.
$removeDirectories = foreach ($directory in $directories) {
    $relative = $directory.FullName.Substring($payload.Length).TrimStart('\')
    'RMDir "$INSTDIR\' + (Escape-Nsis $relative) + '"'
}
$deleteFiles | Set-Content -LiteralPath (Join-Path $generated 'uninstall-files.nsh') -Encoding utf8
$removeDirectories | Set-Content -LiteralPath (Join-Path $generated 'uninstall-directories.nsh') -Encoding utf8
$script = Join-Path $PSScriptRoot 'installer.nsi'
& $Compiler '/INPUTCHARSET' 'UTF8' '/V3' "/X!addincludedir $generated" $script
if ($LASTEXITCODE -ne 0) { throw "NSIS compilation failed: $LASTEXITCODE" }
$hash = (Get-FileHash -LiteralPath $output -Algorithm SHA256).Hash.ToLowerInvariant()
$hash + '  ' + [IO.Path]::GetFileName($output) | Set-Content -LiteralPath ($output + '.sha256') -Encoding ascii
Write-Host "Installer: $output"
Write-Host "SHA256: $hash"
