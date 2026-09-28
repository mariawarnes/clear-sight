[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$stage = Join-Path $root 'build/clear-sight-package'
$dll = Join-Path $stage 'SKSE/Plugins/ClearSight.dll'
if (-not (Test-Path -LiteralPath $dll)) { throw 'Run Build-Native.ps1 successfully before packaging.' }
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
Compress-Archive -Path (Join-Path $stage 'SKSE'), $docs -DestinationPath $archivePath -CompressionLevel NoCompression -Force
Write-Output $archivePath
