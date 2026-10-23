[CmdletBinding()]
param([switch]$TestsOnly, [string]$BuildName = 'native')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools and Windows SDK are required.' }
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Install the Visual Studio CMake component.' }
$argsList = @('-S', (Join-Path $root 'native'), '-G', 'Visual Studio 18 2026', '-A', 'x64')
if ($TestsOnly) {
    $buildDir = Join-Path $root 'build/gpu-tests'
    $argsList += @('-B', $buildDir, '-DHC_BUILD_PLUGIN=OFF')
} else {
    $lib = Join-Path $root 'build/CommonLibSSE-NG'
    $pinned = '70c1acd5261210982bd52f6d4468a082fe04d798'
    if (-not (Test-Path -LiteralPath $lib)) {
        & git clone https://github.com/alandtse/CommonLibVR.git $lib
        if ($LASTEXITCODE) { throw 'CommonLib download failed.' }
        & git -C $lib checkout $pinned
    }
    $revision = & git -C $lib rev-parse HEAD
    if ($revision -ne $pinned) { throw "Expected CommonLib revision $pinned, found $revision" }
    & git -C $lib submodule update --init --depth 1
    if ($LASTEXITCODE) { throw 'CommonLib dependency download failed.' }
    $vcpkgRoot = Join-Path $root 'build/vcpkg'
    if (-not (Test-Path -LiteralPath $vcpkgRoot)) {
        & git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
        if ($LASTEXITCODE) { throw 'vcpkg download failed.' }
    }
    $vcpkg = Join-Path $vcpkgRoot 'vcpkg.exe'
    if (-not (Test-Path -LiteralPath $vcpkg)) {
        & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
        if ($LASTEXITCODE) { throw 'vcpkg bootstrap failed.' }
    }
    & $vcpkg install spdlog:x64-windows-static rapidcsv:x64-windows-static directxtk:x64-windows-static
    if ($LASTEXITCODE) { throw 'C++ dependency build failed.' }
    $buildDir = Join-Path $root "build/$BuildName"
    $toolchain = Join-Path $vcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
    $argsList += @('-B', $buildDir, '-DHC_BUILD_PLUGIN=ON', "-DCMAKE_TOOLCHAIN_FILE=$toolchain", '-DVCPKG_TARGET_TRIPLET=x64-windows-static', '-DVCPKG_MANIFEST_MODE=OFF')
}
& $cmake @argsList
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmake --build $buildDir --config Release --parallel 4
if ($LASTEXITCODE) { throw 'Native compilation failed.' }
& (Join-Path $buildDir 'Release/ClearSightTests.exe')
if ($LASTEXITCODE) { throw 'Native GPU tests failed.' }
if (-not $TestsOnly) {
    & $cmake --install $buildDir --config Release --component Native --prefix (Join-Path $root $(if ($BuildName -eq 'native') { 'build/clear-sight-package' } else { "build/$BuildName-package" }))
    if ($LASTEXITCODE) { throw 'Package staging failed.' }
}
