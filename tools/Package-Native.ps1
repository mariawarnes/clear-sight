[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $root 'build/clear-sight-package'
# One installer option per runtime; Build-Native.ps1 stages each build here.
$runtimes = @('Runtime-1.7.104', 'Runtime-1.6.1170')
foreach ($runtime in $runtimes) {
    foreach ($file in @('ClearSight.dll', 'ClearSight.ini')) {
        if (-not (Test-Path -LiteralPath (Join-Path $stage "$runtime/SKSE/Plugins/$file"))) {
            throw "Missing $runtime/SKSE/Plugins/$file. Run Build-Native.ps1 (both runtimes) successfully before packaging."
        }
    }
}
# Remove the pre-installer layout left by older builds so it cannot be packaged.
$legacy = Join-Path $stage 'SKSE'
if (Test-Path -LiteralPath $legacy) { Remove-Item -LiteralPath $legacy -Recurse -Force }
$fomod = Join-Path $stage 'fomod'
New-Item -ItemType Directory -Path $fomod -Force | Out-Null
foreach ($file in @('info.xml', 'ModuleConfig.xml')) {
    Copy-Item -LiteralPath (Join-Path $root "packaging/fomod/$file") -Destination $fomod
}
$docs = Join-Path $stage 'ClearSight-Documentation'
$licenses = Join-Path $docs 'licenses'
New-Item -ItemType Directory -Path $licenses -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'docs/NATIVE-INSTALL.md') -Destination $docs
Copy-Item -LiteralPath (Join-Path $root 'docs/NATIVE-VALIDATION.md') -Destination $docs
Copy-Item -LiteralPath (Join-Path $root 'build/CommonLibSSE-NG/licenses/LICENSE-MIT') -Destination (Join-Path $licenses 'CommonLibSSE-MIT.txt')
Copy-Item -LiteralPath (Join-Path $root 'build/CommonLibSSE-NG/COPYING') -Destination (Join-Path $licenses 'CommonLibSSE-GPL-3.0.txt')
Copy-Item -LiteralPath (Join-Path $root 'build/CommonLibSSE-NG/EXCEPTIONS.md') -Destination (Join-Path $licenses 'CommonLibSSE-EXCEPTIONS.md')
Copy-Item -LiteralPath (Join-Path $root 'build/CommonLibSSE-NG/licenses/README.md') -Destination (Join-Path $licenses 'CommonLibSSE-LICENSE-NOTE.md')
Copy-Item -LiteralPath (Join-Path $root 'build/CommonLibSSE-NG/extern/openvr/LICENSE') -Destination (Join-Path $licenses 'OpenVR.txt')
Copy-Item -LiteralPath (Join-Path $root 'COPYING') -Destination $docs
Copy-Item -LiteralPath (Join-Path $root 'EXCEPTIONS.md') -Destination $docs
Copy-Item -LiteralPath (Join-Path $root 'docs/release/LICENSING.md') -Destination $docs
Copy-Item -LiteralPath (Join-Path $root 'native/third_party/dxc/LICENSE.TXT') -Destination (Join-Path $licenses 'DirectXShaderCompiler.txt')
foreach ($library in @('fmt', 'spdlog', 'directxtk', 'directxmath', 'rapidcsv')) {
    Copy-Item -LiteralPath (Join-Path $root "build/vcpkg/installed/x64-windows-static/share/$library/copyright") -Destination (Join-Path $licenses "$library.txt")
}
$archivePath = Join-Path $root 'build/ClearSight-High-Contrast-Accessibility-0.3.13.zip'
$contents = @($fomod, $docs) + ($runtimes | ForEach-Object { Join-Path $stage $_ })
Compress-Archive -Path $contents -DestinationPath $archivePath -CompressionLevel NoCompression -Force
Write-Output $archivePath
