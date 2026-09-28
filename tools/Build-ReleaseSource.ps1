[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio 2026 C++ tools and Windows SDK are required.' }
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
$vendor = Join-Path $root 'vendor/CommonLibSSE-NG'
if (-not (Test-Path -LiteralPath (Join-Path $vendor 'CMakeLists.txt'))) { throw 'Run this from the extracted corresponding-source package.' }
$vcpkgRoot = Join-Path $root 'build/vcpkg'
$revision = '11ace808cc8a3a941f386e33726b992b22ba9e5a'
if (-not (Test-Path -LiteralPath $vcpkgRoot)) {
    & git clone https://github.com/microsoft/vcpkg.git $vcpkgRoot
    if ($LASTEXITCODE) { throw 'vcpkg clone failed.' }
    & git -C $vcpkgRoot checkout $revision
    if ($LASTEXITCODE) { throw 'vcpkg checkout failed.' }
}
if ((& git -C $vcpkgRoot rev-parse HEAD) -ne $revision) { throw 'vcpkg revision does not match this release.' }
$vcpkg = Join-Path $vcpkgRoot 'vcpkg.exe'
if (-not (Test-Path -LiteralPath $vcpkg)) {
    & (Join-Path $vcpkgRoot 'bootstrap-vcpkg.bat') -disableMetrics
    if ($LASTEXITCODE) { throw 'vcpkg bootstrap failed.' }
}
& $vcpkg install spdlog:x64-windows-static rapidcsv:x64-windows-static directxtk:x64-windows-static
if ($LASTEXITCODE) { throw 'Dependency build failed.' }
$buildDir = Join-Path $root 'build/native'
& $cmake -S (Join-Path $root 'native') -B $buildDir -G 'Visual Studio 18 2026' -A x64 `
    '-DHC_BUILD_PLUGIN=ON' "-DCOMMONLIB_SOURCE=$vendor" '-DCOMMONLIB_PREBUILT=OFF' `
    "-DCMAKE_TOOLCHAIN_FILE=$vcpkgRoot/scripts/buildsystems/vcpkg.cmake" `
    '-DVCPKG_TARGET_TRIPLET=x64-windows-static' '-DVCPKG_MANIFEST_MODE=OFF'
if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
& $cmake --build $buildDir --config Release --parallel 4
if ($LASTEXITCODE) { throw 'Compilation failed.' }
& (Join-Path $buildDir 'Release/ClearSightTests.exe')
if ($LASTEXITCODE) { throw 'Tests failed.' }
& $cmake --install $buildDir --config Release --component Native --prefix (Join-Path $root 'build/clear-sight-package')
if ($LASTEXITCODE) { throw 'Staging failed.' }
