$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$fetchBuildDir = Join-Path $projectRoot "build-vs"
$localBuildDir = Join-Path $projectRoot "build-local"
$downloadedJuce = Join-Path $fetchBuildDir "_deps\juce-src"
$vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path $vcvars)) {
    throw "vcvars64.bat not found at $vcvars"
}

if (-not (Test-Path $cmake)) {
    throw "cmake.exe not found at $cmake"
}

$machinePath = [Environment]::GetEnvironmentVariable("Path", "Machine")
$userPath = [Environment]::GetEnvironmentVariable("Path", "User")
$cleanPath = (($machinePath, $userPath) | Where-Object { $_ }) -join ";"

[Environment]::SetEnvironmentVariable("PATH", $null, "Process")
[Environment]::SetEnvironmentVariable("Path", $cleanPath, "Process")

Set-Location $projectRoot

if (Test-Path (Join-Path $downloadedJuce "CMakeLists.txt")) {
    $buildDir = $localBuildDir
    $configure = "`"$vcvars`" && `"$cmake`" -S . -B `"$buildDir`" -G `"Visual Studio 17 2022`" -A x64 -DQQDEBREATH_FETCH_JUCE=OFF -DQQDEBREATH_JUCE_PATH=`"$downloadedJuce`""
} else {
    $buildDir = $fetchBuildDir
    $configure = "`"$vcvars`" && `"$cmake`" -S . -B `"$buildDir`" -G `"Visual Studio 17 2022`" -A x64"
}

$build = "`"$vcvars`" && `"$cmake`" --build `"$buildDir`" --config Release --target QQDeBreath_VST3"

cmd.exe /d /c $configure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

cmd.exe /d /c $build
exit $LASTEXITCODE
