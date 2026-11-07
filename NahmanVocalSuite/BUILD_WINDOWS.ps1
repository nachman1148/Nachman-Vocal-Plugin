$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$JuceDir = Join-Path $Root 'JUCE'
$BuildDir = Join-Path $Root 'build-windows'

Write-Host 'NAHMAN VOCAL SUITE - Windows build' -ForegroundColor Cyan
Write-Host 'This script builds the VST3 and Standalone targets. It does not install them automatically.'

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'CMake 3.22+ not found. Install CMake and Visual Studio 2022 with Desktop development with C++.'
}
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git not found. Install Git for Windows.'
}

if (-not (Test-Path (Join-Path $JuceDir 'CMakeLists.txt'))) {
    Write-Host 'Downloading JUCE 8.0.6...'
    git clone --depth 1 --branch 8.0.6 https://github.com/juce-framework/JUCE.git $JuceDir
    if ($LASTEXITCODE -ne 0) { throw 'JUCE download failed. Check internet access and try again.' }
}

cmake -S $Root -B $BuildDir -A x64 "-DFETCHCONTENT_SOURCE_DIR_JUCE=$JuceDir" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed.' }

cmake --build $BuildDir --config Release --target NahmanVocalSuite_VST3 NahmanVocalSuite_Standalone
if ($LASTEXITCODE -ne 0) { throw 'Compilation failed. Review the compiler output above.' }

Write-Host ''
Write-Host 'Build completed. Search these folders for the VST3 and Standalone app:' -ForegroundColor Green
Write-Host (Join-Path $BuildDir 'NahmanVocalSuite_artefacts\Release\VST3')
Write-Host (Join-Path $BuildDir 'NahmanVocalSuite_artefacts\Release\Standalone')
