# Clear Sight 0.3.13 corresponding source

Author: mlw05. Native plugin: GPL-3.0-or-later with the exceptions in EXCEPTIONS.md. See COPYING and LICENSING.md. The source is supplied alongside the compiled plugin; it is not a playable mod download.

## Build

Use Windows x64, Git, Visual Studio 2026 Community with Desktop development with C++, the CMake component and Windows SDK. The release uses MSVC 14.50.35717 (compiler 19.50) and Windows SDK 10.0.26100. Minimum CMake declared by the project: 3.25; use the bundled VS version for the Visual Studio 18 2026 generator.

Extract the source ZIP into a new folder. From that folder in PowerShell run:

```powershell
./tools/Build-ReleaseSource.ps1
```

The script builds the bundled CommonLib source. It downloads the pinned vcpkg tool and builds the pinned dependency ports; internet access is required for this documented build. It does not download game assets, SKSE or Address Library and does not deploy files to a game installation. An offline bootstrap/toolchain is not provided. Source copies for the dependency versions used are included under vendor/dependencies; these are supplied for inspection and modification, not automatically substituted into vcpkg by the build script. To modify a dependency, use a vcpkg overlay port referencing the corresponding edited source tree.

Outputs:

- build/native/Release/ClearSight.dll
- build/native/Release/ClearSightTests.exe
- build/clear-sight-package/SKSE/Plugins/ClearSight.dll and ClearSight.ini

The tests compile and execute Direct3D shaders using WARP. They do not launch Skyrim. No game installation is needed to compile or run those tests. The plugin itself targets only Steam Skyrim 1.7.104.0 with SKSE64 2.3.1 and Address Library v13; SE/VR support paths are disabled.

## Exact dependencies

- CommonLibSSE-NG, alandtse/CommonLibVR: 70c1acd5261210982bd52f6d4468a082fe04d798 (6.7.1). Full tracked repository source is included, without Git history or prebuilt binaries. OpenVR headers and licence are included; OpenVR is not linked in this AE-only build.
- vcpkg ports/tool checkout: 11ace808cc8a3a941f386e33726b992b22ba9e5a.
- fmt 12.2.0, spdlog 1.17.0, DirectXTK 2026-05-07, DirectXMath 2026-06-12, rapidcsv 9.07: the extracted, port-patched source trees from the release build are bundled under vendor/dependencies. The relevant port files and vcpkg-cmake helpers are under vendor/vcpkg-ports. See DEPENDENCIES.txt for the installed package metadata.
- Microsoft DirectXShaderCompiler excerpts used by this plugin are in native/third_party/dxc with their original notices. The source files themselves are the exact copies compiled into this release.
- C++/Windows SDK/Direct3D system tools and libraries must be installed separately. They are not part of this archive.

The source archive excludes personal attachments, game saves/logs, game assets, old ReShade/Papyrus prototypes, generated promotional artwork, executables and nested archives. Embedded third-party sample content in dependency source trees is retained only where distributed as source with its upstream notices.

## Validation

The release DLL was built and its automated tests passed in the maintainer workspace. A separate full clean rebuild of this extracted source package has not been completed; do not claim a byte-identical/reproducible binary. Paths/timestamps and toolchain versions can change the output. Source archive integrity and binary/source file hashes are recorded in the release verification report. NATIVE-VALIDATION.md describes runtime and visual testing limits, including the unresolved intermittent save-loading CTD.
