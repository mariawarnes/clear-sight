[CmdletBinding()]
param(
    [switch]$TestsOnly,
    # Each runtime gets its own DLL; by default both are built for the installer.
    [ValidateSet('1.7.104', '1.6.1170')][string[]]$Runtime = @('1.7.104', '1.6.1170'),
    [string]$BuildName
)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$vswhere = 'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ build tools and Windows SDK are required.' }
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (-not (Test-Path -LiteralPath $cmake)) { throw 'Install the Visual Studio CMake component.' }
$baseArgs = @('-S', (Join-Path $root 'native'), '-G', 'Visual Studio 18 2026', '-A', 'x64')
function Invoke-NativeBuild([string]$buildDir, [string[]]$configureArgs, [string]$stageDir) {
    & $cmake @baseArgs -B $buildDir @configureArgs
    if ($LASTEXITCODE) { throw 'CMake configuration failed.' }
    & $cmake --build $buildDir --config Release --parallel 4
    if ($LASTEXITCODE) { throw 'Native compilation failed.' }
    & (Join-Path $buildDir 'Release/ClearSightTests.exe')
    if ($LASTEXITCODE) { throw 'Native GPU tests failed.' }
    if ($stageDir) {
        if (Test-Path -LiteralPath $stageDir) { Remove-Item -LiteralPath $stageDir -Recurse -Force }
        & $cmake --install $buildDir --config Release --component Native --prefix $stageDir
        if ($LASTEXITCODE) { throw 'Package staging failed.' }
    }
}
if ($TestsOnly) {
    Invoke-NativeBuild (Join-Path $root 'build/gpu-tests') @('-DHC_BUILD_PLUGIN=OFF') $null
} else {
    if ($BuildName -and $Runtime.Count -ne 1) { throw '-BuildName needs exactly one -Runtime.' }
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
    $toolchain = Join-Path $vcpkgRoot 'scripts/buildsystems/vcpkg.cmake'
    foreach ($target in $Runtime) {
        # 1.7.104 keeps the original build/native directory so existing caches are reused.
        $name = if ($BuildName) { $BuildName } elseif ($target -eq '1.7.104') { 'native' } else { "native-$target" }
        # Package-Native.ps1 turns build/clear-sight-package/Runtime-* into installer options.
        $stageDir = if ($BuildName) { Join-Path $root "build/$BuildName-package" } else { Join-Path $root "build/clear-sight-package/Runtime-$target" }
        Write-Output "Building ClearSight.dll for Skyrim $target"
        Invoke-NativeBuild (Join-Path $root "build/$name") @('-DHC_BUILD_PLUGIN=ON', "-DHC_TARGET_RUNTIME=$target",
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain", '-DVCPKG_TARGET_TRIPLET=x64-windows-static', '-DVCPKG_MANIFEST_MODE=OFF') $stageDir
    }
}
