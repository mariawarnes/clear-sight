# Clear Sight — Validation

Target: Steam SkyrimSE.exe 1.7.104.0, SKSE64 2.3.1, Address Library format 5/v13.

## Completed checks

- Version 0.3.13: inspected the 0.3.12 log, which ends after the first reference snapshot of a different save; no Skyrim crash dump or matching Windows crash event was found. Added successful-load and stable world-readiness gating (1.5 seconds), failed-load suspension, load-generation checks before publishing snapshots, and pending-task cleanup on completion. Removed raw target pointers outside the target-map lock and direct traversal of cached raw teleport references; uses the engine tracking handle for the current route door. Tests cover startup/failure, warmup boundary, consecutive loads and interrupted readiness, alongside the existing GPU/classification suite. The actual intermittent CTD remains unconfirmed; these tests do not simulate Skyrim loading a save. SKSE message success encoding verified against https://github.com/ianpatt/skse64/blob/master/skse64/Hooks_SaveLoad.cpp .

- Version 0.3.12: the deployed 0.3.11 log reported zero route doors. Read-only inspection of Skyrim.esm found MQ101 entrance targets are XMarkers 000E24C8/000E24C7 (base 0000003B), not door references, and neither has a linked door. Added native target/tracking resolution plus a nearest-loaded-door fallback for active XMarkers, bounded to 384 units and the same spatial context. Regression cases use measured vanilla distances: Hadvar marker 117.27 units from door 00091A00; Ralof marker 313.47 units from door 0005DF01. Tests reject other spaces and distant doors. Sets rebuild each snapshot; actor rules are unchanged. Address Library resolver entries were verified in the installed executable and their instructions inspected. Build and GPU tests pass. Actual Helgen highlighting and marker removal still require an in-game check; proximity matching is heuristic.

- Version 0.3.11: copies door reference IDs from the active quest targets' `TeleportPath.teleportRefs` under the existing quest-target lock, filters by Door form type, and classifies loaded route doors as Important. The set is rebuilt each snapshot rather than retaining stale routes. NPCs remain subject to the existing actor-colour rules. Runtime route calculation, door highlighting and route removal still require an in-game check. The log reports the number of current route door IDs.

- Version 0.3.10: inspected the deployed 0.3.9 INI and game log, confirming the previous settings and shader conversions were active. Increased default shadow floor to 0.035 and changed the lift to a fourth-root curve, retaining the 0.06 upper limit. WARP tests compare dark output against both the new curve and 0.3.9, check brighter shadows, retained gradation and the unchanged upper limit/bright pixel/alpha. The actual dungeon still needs visual verification; this is not a final-frame contrast guarantee.

- Version 0.3.9: scenery-only shadow-lift shader variants use a separate cache from plain effect greyscale. WARP readback checks dark inputs 0.004, 0.012 and 0.03 against the bounded curve, preserves their ordering and alpha, and confirms 0.12 stays unchanged. Restoring the original shared shader restores the original pixels. All existing palette, classification, foliage/alpha and GPU tests pass. Default WorldDetail increases to 0.12. The reported dungeon has not been visually retested in game; no final-frame contrast guarantee is asserted.

- Version 0.3.8: darker neutral World default and brighter Blue/Red/Yellow presets share constants between fallback configuration and the named parser. Reference sRGB contrast tests require at least 3:1 and measure 5.73193, 5.22804, 13.8052 respectively. A separate linear-material test after default face-detail darkening and downward UNORM quantization measures 4.41483, 3.32324, 7.01754. All prior GPU/classification tests pass. These numerical checks do not establish final framebuffer contrast under Skyrim lighting or post-processing. No in-game 3:1 guarantee is made.

- Version 0.3.7: ordinary World-category draws use final-output greyscale across lighting, grass and distant-tree hooks, removing late foliage vertex/material tints. Category tests ensure actor and quest-object palettes bypass this step. A WARP draw test uses a textured leaf shader with green vertex tint, added green backlighting and alpha clipping; readback verifies grey output with unchanged alpha/discard and restoration of coloured output when reusing the original shader. These are simulated shader tests, not visual validation of actual Skyrim trees. The existing game log confirms pixel-shader capture and effect conversion ran in 0.3.6; the new scenery coverage still needs an in-game check.

- Version 0.3.6 renames the package to Clear Sight — High Contrast Accessibility. Plugin metadata, DLL/INI/log names, toggle notifications, documentation directory and build/package outputs use the new identity. Rendering and settings behaviour are unchanged. Packaging uses a new staging directory to exclude the old DLL. Old names remain only where needed to identify previous versions or explain migration.

- Version 0.3.5: local-light diffuse and ambient RGB are converted to luminance for the duration of vanilla geometry setup, then restored. The filter applies to point lights regardless of whether they originate from fire, torches or spells. Tests cover orange fire, blue HDR magic and purple magic inputs, luminance preservation, unchanged palette channel ratios under neutral light and exact restoration when a light is referenced twice. The installed headers and reverse-engineered lighting setup were checked for per-pass light access and upload timing. No in-game fire/spell cast was run to validate coverage.

- Version 0.3.4: palette entries accept case-insensitive colour names, `#RRGGBB` and existing decimal RGB triples. Parser tests cover legacy equivalence, case/whitespace, hex conversion and rejection of malformed/out-of-range values. Runtime loads each category independently and retains defaults on invalid input. Packaged defaults remain Blue/Red/Yellow and Enabled=1. Colour settings require a restart; no in-game menu or live reload was added.

- Version 0.3.3: dead actors take the Neutral (grey) category before all other flags. Tested all 16 combinations of hostility, teammate, quest and non-hostile-blue flags for a dead actor. Follow/quest-target actors retain their living category; tests cover blue non-hostile targets, grey neutrals when blue is disabled, red hostile targets, and yellow non-actor quest objects. The 0.3.2 rule making neutral quest NPCs yellow is superseded.

- Version 0.3.2: corrected FaceGen tint to half-grey and its detail map to approximately quarter-grey, matching the face shader's nonlinear composition. Real GPU map readback composed through the face equation stays within 0.005 of the input albedo. FaceGen RGB tint uses half-grey; hair uses white. Diffuse detail no longer brightens the selected palette.
- Reduced default palette intensity and material specular/environment scale. Non-hostile blue is configurable; tests preserve hostile priority and yellow neutral quest targets. Logs now identify actors when their classification changes.
- Added final-output greyscale variants for effect, particle and blood-splatter shaders. D3D11 WARP validates the transformed DXBC and executes a shader with hardcoded red/blue, branching and discard. Readback confirms equal RGB, preserved luminance and unchanged alpha/discard. Shader creation also passes for constant shaders with no temporary registers, RGB-only output and multiple render targets.
- Verified effect/particle/blood vtable entries and the renderer-init capture call against the installed 1.7.104 executable. Actual game execution of these new hooks remains unverified. Successful effect conversions log `grey output ready`; unsupported/missing captures log `conversion unavailable`.

- Version 0.3.0 user screenshots show partial treatment varying between images; the accompanying game log confirms hooks and GPU initialization ran. UI protection was reported working. The four-conversions-per-16-ms original-texture fallback and 256-entry eviction limit were identified as a source of repeated missing treatment.
- Version 0.3.1 removes that conversion throttle and entry-count limit, retaining the byte limit and reducing default output dimension to 256. WARP regression checks cover 300 distinct materials, three repeated passes retaining all outputs, and forced byte-budget eviction with correct treated pixels on every request.

- Compiled the actual diffuse compute shader using Microsoft's D3DCompiler and executed it using a D3D11 WARP device.
- Read back GPU pixels: red/blue/yellow source pixels all become the selected category colour; original source textures are unchanged.
- Verified opaque, half-transparent and fully transparent source alpha is preserved.
- Verified adjustable grayscale detail retains brightness differences.
- Verified different actor categories can share one source texture without sharing the wrong treated output; cache hits reuse output and clearing releases cache entries.
- Verified compute SRV, shader, sampler and constant-buffer bindings are restored after transformation, and the output UAV is unbound.
- Checked classification priorities: hostile before teammate/quest, important non-actors, neutral actors, and irrelevant actor flags on world objects.
- Read the actual Skyrim executable and the locally downloaded Address Library without installing or launching anything. All lighting/grass/distant-tree setup/restore hook entries resolve to executable sections of the matching PE file.

- Built `ClearSight.dll` successfully with MSVC 19.50, Windows SDK 10.0.26100 and CommonLib 6.7.1. Re-ran the GPU tests successfully against the final build.
- Inspected DLL exports: `SKSEPlugin_Load` and `SKSEPlugin_Version` are present. Imports require Windows system libraries including D3DCompiler_47; there are no extra fmt/spdlog/OpenVR runtime DLL dependencies.
- Native Vortex package contains the DLL, configuration, documentation and third-party notices. It does not contain engine headers, build libraries, ReShade, or the legacy Papyrus sources.

## Not yet verified in Skyrim

The complete 0.3.6 build in Skyrim: local fire/magic light neutralization, death/resurrection colour transitions and follow targets, skin/hair balance under sunlight, reduced highlights, actor colour coverage, shader capture and restoration for fire/blood/particles, menu/model-preview transitions, hair and foliage silhouettes, quest coverage, toggle responsiveness, save/load behavior, long-session memory and frame time. These require an actual SKSE game run and visual inspection. Offline address checks validate locations, not function signatures or runtime correctness. Tests establish the transformations, not coverage of every Skyrim shader or asset.

## Reproduce

```powershell
./tools/Build-Native.ps1 -TestsOnly
./tools/Build-Native.ps1
node ./tools/Verify-Runtime.mjs 'D:\SteamLibrary\steamapps\common\Skyrim Special Edition\SkyrimSE.exe' './build/versionlib-1-7-104-0.bin'
```

The address-library test input is extracted locally from the user's downloaded archive and is not redistributed. Run the game checks in `NATIVE-INSTALL.md` after enabling the native package.

Engine library: alandtse/CommonLibVR commit `70c1acd5261210982bd52f6d4468a082fe04d798` (6.7.1), the revision pinned by Community Shaders commit `b4e7b08ec456084b5f73f42627eb3a915f31c7f1`. Vcpkg baseline used locally: `11ace808cc8a3a941f386e33726b992b22ba9e5a`. C++ libraries are linked statically. Third-party notices are included in the package.

DXBC token definitions and container checksum code are vendored from [Microsoft DirectXShaderCompiler](https://github.com/microsoft/DirectXShaderCompiler) in `native/third_party/dxc` with its license. Shader transformation preserves the original signatures and code except output-register redirection and final RGB luminance conversion. The original shader is restored after each treated draw.
