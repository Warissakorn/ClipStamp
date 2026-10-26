[CmdletBinding()]
param([string]$Installer)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
if (-not $Installer) {
    $Installer = (Get-ChildItem -LiteralPath (Join-Path $projectRoot 'dist/installers') -Filter '*-Setup-x64.exe' | Sort-Object LastWriteTime -Descending | Select-Object -First 1).FullName
}
if (-not $Installer -or -not (Test-Path -LiteralPath $Installer)) { throw 'Build an installer first.' }
$testRoot = [IO.Path]::GetFullPath((Join-Path $projectRoot ('build/installer-smoke-' + [guid]::NewGuid().ToString('N'))))
if (-not $testRoot.StartsWith($projectRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Test path escaped the workspace.' }
$installed = Join-Path $testRoot 'ClipStamp test'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$log = Join-Path $testRoot 'results.txt'
function Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
    $Message | Tee-Object -FilePath $log -Append
}
function Run-Checked([string]$Program, [string]$Arguments) {
    $process = Start-Process -FilePath $Program -ArgumentList $Arguments -WindowStyle Hidden -PassThru -Wait
    if ($process.ExitCode -ne 0) { throw "Process failed ($($process.ExitCode)): $Program" }
}
$uninstallKey = 'HKCU:/Software/Microsoft/Windows/CurrentVersion/Uninstall/ClipStamp'
$registrationBefore = if (Test-Path $uninstallKey) { Get-ItemProperty $uninstallKey | ConvertTo-Json -Compress } else { '' }
Run-Checked $Installer "/S /NOINTEGRATION /D=$installed"
Check (Test-Path -LiteralPath (Join-Path $installed 'ClipStamp.exe')) 'PASS: silent installation to a path containing spaces'
$manifestPath = Join-Path $projectRoot 'build/installer/payload-manifest.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
foreach ($entry in $manifest) {
    $target = Join-Path $installed $entry.Path
    if (-not (Test-Path -LiteralPath $target)) { throw "Missing installed file: $($entry.Path)" }
    if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $entry.Sha256) { throw "Installed file differs: $($entry.Path)" }
}
Check $true 'PASS: all installed files match deployment SHA256 hashes'
$preserved = Join-Path $installed 'user-project-test.txt'
'User data must survive upgrades and uninstall.' | Set-Content -LiteralPath $preserved -Encoding utf8
Run-Checked $Installer "/S /NOINTEGRATION /D=$installed"
Check (Test-Path -LiteralPath $preserved) 'PASS: reinstall preserves additional user files'
$previousPath = $env:PATH
$previousBackend = $env:QT_QPA_PLATFORM
$previousToolDirectory = $env:CLIPSTAMP_FFMPEG_DIR
try {
    $env:PATH = Join-Path $env:SystemRoot 'System32'
    $env:QT_QPA_PLATFORM = 'windows'
    $env:CLIPSTAMP_FFMPEG_DIR = Join-Path $installed 'tools'
    $sample = Join-Path $testRoot 'sample.mp4'
    $screenshot = Join-Path $testRoot 'installed-ui.png'
    Run-Checked (Join-Path $installed 'tools/ffmpeg.exe') ('-v error -y -f lavfi -i color=c=navy:s=320x180:r=24 -t 1 -c:v libx264 -pix_fmt yuv420p "' + $sample + '"')
    Check ((Get-Item -LiteralPath $sample).Length -gt 1000) 'PASS: bundled FFmpeg works without development tools on PATH'
    Run-Checked (Join-Path $installed 'ClipStamp.exe') ('--screenshot "' + $screenshot + '" "' + $sample + '"')
    Check ((Get-Item -LiteralPath $screenshot).Length -gt 1000) 'PASS: installed application opens a video without Qt SDK on PATH'
} finally {
    $env:PATH = $previousPath
    $env:QT_QPA_PLATFORM = $previousBackend
    $env:CLIPSTAMP_FFMPEG_DIR = $previousToolDirectory
}
Run-Checked (Join-Path $installed 'Uninstall.exe') '/S'
$deadline = (Get-Date).AddSeconds(30)
while ((Test-Path -LiteralPath (Join-Path $installed 'Uninstall.exe')) -and (Get-Date) -lt $deadline) { Start-Sleep -Milliseconds 200 }
Check (-not (Test-Path -LiteralPath (Join-Path $installed 'ClipStamp.exe'))) 'PASS: uninstall removes application files'
Check (-not (Test-Path -LiteralPath (Join-Path $installed 'Uninstall.exe'))) 'PASS: uninstaller removes itself'
Check (Test-Path -LiteralPath $preserved) 'PASS: uninstall preserves user data'
$registrationAfter = if (Test-Path $uninstallKey) { Get-ItemProperty $uninstallKey | ConvertTo-Json -Compress } else { '' }
Check ($registrationBefore -eq $registrationAfter) 'PASS: isolated test leaves existing Windows registration unchanged'
Write-Host "Installer verification complete: $testRoot"
