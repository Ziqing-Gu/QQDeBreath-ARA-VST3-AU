param(
    [string]$Configuration = "Release"
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$juce = Resolve-Path -LiteralPath (Join-Path $root "build-vs\_deps\juce-src")
$ara = Resolve-Path -LiteralPath (Join-Path $root "external\ARA_SDK")
$buildDir = Join-Path $root "ara-demo-build"

$vsRoot = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools"
$vcvars = Join-Path $vsRoot "VC\Auxiliary\Build\vcvars64.bat"
$cmake = Join-Path $vsRoot "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

if (-not (Test-Path -LiteralPath $vcvars)) {
    throw "Visual Studio vcvars64.bat not found: $vcvars"
}

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "Visual Studio bundled CMake not found: $cmake"
}

$configure = @(
    "`"$vcvars`"",
    "&&",
    "`"$cmake`"",
    "-S", "`"$juce`"",
    "-B", "`"$buildDir`"",
    "-G", "`"Visual Studio 17 2022`"",
    "-A", "x64",
    "-DJUCE_BUILD_EXAMPLES=ON",
    "-DJUCE_BUILD_EXTRAS=OFF",
    "-DJUCE_COPY_PLUGIN_AFTER_BUILD=OFF",
    "-DJUCE_GLOBAL_ARA_SDK_PATH=`"$ara`""
) -join " "

$build = @(
    "`"$vcvars`"",
    "&&",
    "`"$cmake`"",
    "--build", "`"$buildDir`"",
    "--config", $Configuration,
    "--target", "ARAPluginDemo_VST3"
) -join " "

Write-Host "JUCE: $juce"
Write-Host "ARA SDK: $ara"
Write-Host "Build dir: $buildDir"
Write-Host "Configuring ARAPluginDemo..."
cmd /c $configure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host "Building ARAPluginDemo_VST3..."
cmd /c $build
exit $LASTEXITCODE
