[CmdletBinding()]
param(
    [switch]$Setup,
    [switch]$Package,
    [switch]$Installer,
    [string]$QtRoot,
    [string]$CompilerDirectory,
    [string]$FFmpegDirectory
)
$ErrorActionPreference = 'Stop'
if ($Installer) { $Package = $true }
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$toolsRoot = Join-Path $projectRoot '.tools'
$pythonModules = Join-Path $toolsRoot 'python'
if (-not $QtRoot) { $QtRoot = Join-Path $toolsRoot 'Qt/6.8.3/mingw_64' }
if (-not $CompilerDirectory) { $CompilerDirectory = Join-Path $toolsRoot 'Qt/Tools/mingw1310_64/bin' }
$cmake = Join-Path $pythonModules 'cmake/data/bin/cmake.exe'
function Invoke-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "Command failed ($LASTEXITCODE): $Program" }
}
if ($Setup) {
    New-Item -ItemType Directory -Path $toolsRoot -Force | Out-Null
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    $pythonArguments = @()
    if (-not $pythonCommand) { $pythonCommand = Get-Command py -ErrorAction SilentlyContinue; $pythonArguments = @('-3') }
    if (-not $pythonCommand) { throw 'Install Python 3.11 or newer, then run this script with -Setup.' }
    Invoke-Checked $pythonCommand.Source ($pythonArguments + @('-m', 'pip', 'install', '--target', $pythonModules, 'aqtinstall==3.3.0', 'cmake==4.4.4'))
    $env:PYTHONPATH = $pythonModules
    $state = Join-Path $toolsRoot 'state'
    New-Item -ItemType Directory -Path $state -Force | Out-Null
    $previousAppData = $env:APPDATA
    $previousLocalAppData = $env:LOCALAPPDATA
    try {
        $env:APPDATA = $state
        $env:LOCALAPPDATA = $state
        Push-Location $toolsRoot
        if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin/qmake.exe'))) {
            Invoke-Checked $pythonCommand.Source ($pythonArguments + @('-m','aqt','install-qt','windows','desktop','6.8.3','win64_mingw','-m','qtmultimedia','-O',(Join-Path $toolsRoot 'Qt')))
        }
        if (-not (Test-Path -LiteralPath (Join-Path $CompilerDirectory 'g++.exe'))) {
            Invoke-Checked $pythonCommand.Source ($pythonArguments + @('-m','aqt','install-tool','windows','desktop','tools_mingw1310','qt.tools.win64_mingw1310','-O',(Join-Path $toolsRoot 'Qt')))
        }
    } finally {
        Pop-Location
        $env:APPDATA = $previousAppData
        $env:LOCALAPPDATA = $previousLocalAppData
    }
}
if (-not (Test-Path -LiteralPath $cmake)) {
    $installedCmake = Get-Command cmake -ErrorAction SilentlyContinue
    if ($installedCmake) { $cmake = $installedCmake.Source }
    else { throw 'CMake is missing. Run tools/windows/build.ps1 -Setup first.' }
}
if (-not (Test-Path -LiteralPath (Join-Path $QtRoot 'bin/qmake.exe'))) { throw "Qt SDK not found: $QtRoot" }
if (-not (Test-Path -LiteralPath (Join-Path $CompilerDirectory 'g++.exe'))) { throw "Compiler not found: $CompilerDirectory" }
$env:PATH = "$(Join-Path $QtRoot 'bin');$CompilerDirectory;$(Split-Path $cmake);$env:PATH"
if (-not $FFmpegDirectory) {
    $ffmpegCommand = Get-Command ffmpeg -ErrorAction SilentlyContinue
    if ($ffmpegCommand) { $FFmpegDirectory = Split-Path $ffmpegCommand.Source }
    elseif (Test-Path -LiteralPath 'C:/Program Files/Shotcut/ffmpeg.exe') { $FFmpegDirectory = 'C:/Program Files/Shotcut' }
}
if ($FFmpegDirectory) { $env:CLIPSTAMP_FFMPEG_DIR = $FFmpegDirectory }
$buildDirectory = Join-Path $projectRoot 'build'
Invoke-Checked $cmake @('-S',$projectRoot,'-B',$buildDirectory,'-G','MinGW Makefiles','-DCMAKE_BUILD_TYPE=Release',"-DCMAKE_PREFIX_PATH=$QtRoot", "-DCMAKE_CXX_COMPILER=$(Join-Path $CompilerDirectory 'g++.exe')")
Invoke-Checked $cmake @('--build',$buildDirectory,'--parallel','4')
$ctest = Join-Path (Split-Path $cmake) 'ctest.exe'
Invoke-Checked $ctest @('--test-dir',$buildDirectory,'--output-on-failure')
if ($Package) {
    if (-not $FFmpegDirectory -or -not (Test-Path -LiteralPath (Join-Path $FFmpegDirectory 'ffprobe.exe'))) { throw 'Packaging requires -FFmpegDirectory containing ffmpeg.exe and ffprobe.exe.' }
    $runtimeDirectory = Join-Path $buildDirectory 'ffmpeg-runtime'
    New-Item -ItemType Directory -Path $runtimeDirectory -Force | Out-Null
    $objdump = Join-Path $CompilerDirectory 'objdump.exe'
    $pending = [Collections.Generic.Queue[string]]::new()
    $pending.Enqueue('ffmpeg.exe'); $pending.Enqueue('ffprobe.exe')
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    while ($pending.Count -gt 0) {
        $name = $pending.Dequeue()
        if (-not $seen.Add($name)) { continue }
        $source = Join-Path $FFmpegDirectory $name
        if (-not (Test-Path -LiteralPath $source)) { continue }
        Copy-Item -LiteralPath $source -Destination (Join-Path $runtimeDirectory $name) -Force
        $headers = & $objdump '-p' $source
        if ($LASTEXITCODE -ne 0) { throw "Cannot inspect dependencies: $source" }
        foreach ($line in $headers) {
            if ($line -match 'DLL Name:\s*(.+)$') {
                $dll = $Matches[1].Trim()
                if (Test-Path -LiteralPath (Join-Path $FFmpegDirectory $dll)) { $pending.Enqueue($dll) }
            }
        }
    }
    Get-ChildItem -LiteralPath $FFmpegDirectory -File | Where-Object { $_.Name -match 'LICENSE|COPYING' } | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $runtimeDirectory -Force
    }
    $buildInfo = & (Join-Path $FFmpegDirectory 'ffmpeg.exe') '-version'
    $buildInfo += ''; $buildInfo += 'Included local runtime files:'
    $buildInfo += (Get-ChildItem -LiteralPath $runtimeDirectory -File | Select-Object -ExpandProperty Name)
    $buildInfo | Set-Content -LiteralPath (Join-Path $runtimeDirectory 'BUILD-INFO.txt') -Encoding utf8
    Invoke-Checked $cmake @('-S',$projectRoot,'-B',$buildDirectory,"-DCLIPSTAMP_FFMPEG_DIR=$runtimeDirectory")
    $distDirectory = Join-Path $projectRoot 'dist/ClipStamp'
    Invoke-Checked $cmake @('--install',$buildDirectory,'--prefix',$distDirectory)
    Write-Host "Ready to run: $(Join-Path $distDirectory 'ClipStamp.exe')"
}

if ($Installer) {
    & (Join-Path $PSScriptRoot 'installer.ps1')
}
