# Copy-ready Nexus fields

## Page fields

| Field | Value |
| --- | --- |
| Game | Skyrim Special Edition |
| Mod name | Clear Sight - High Contrast Accessibility |
| Version | 0.3.13 |
| Author | mlw05 |
| Category | Visuals and Graphics |
| Adult content | No; no adult assets are added |
| Language | English |
| Description | Paste NEXUS-DESCRIPTION.txt; it uses Nexus BBCode |
| Tags | AI-Generated Content; AI Media; Skyrim Script Extender (SKSE); Utilities for Players; Accessibility if offered |

Do not add VR, performance optimisation or compatibility tags that have not been established. Tag labels outside the two verified AI labels may depend on the game's available list.

## Short description

Experimental high-contrast accessibility mode: greyscale scenery, blue non-hostile characters, red enemies and yellow quest objects. Configurable colours; menus and HUD stay unchanged. Requires Steam Skyrim 1.7.104.0, SKSE and Address Library.

## Requirements

- Nexus requirement: Address Library for SKSE Plugins — https://www.nexusmods.com/skyrimspecialedition/mods/32444 — note: Main Files, Address Library All in One (1.7.104.0) v13.
- External requirement: Skyrim Script Extender (SKSE64) — https://skse.silverlock.org/ — note: 2.3.1 for Steam Skyrim 1.7.104.0.
- No ReShade, SkyUI, MCM or Creation Kit requirement. No ESP/ESL.

## Main file

Filename: ClearSight-0.3.13.zip

Display name: Clear Sight 0.3.13 - Experimental

Version: 0.3.13

File description:

Native SKSE plugin for Steam Skyrim 1.7.104.0 only. Requires SKSE64 2.3.1 and Address Library v13. Vanilla renderer; no ENB/Community Shaders. Enabled by default; Ctrl+Shift+H toggles. Includes save-loading safeguards, but the reported intermittent CTD is not confirmed fixed. Install through Vortex as Default. No ESP. Read the known limitations before use.

## Miscellaneous file

Filename: ClearSight-0.3.13-Source.zip

Display name: Clear Sight 0.3.13 - Corresponding Source

Version: 0.3.13

File description:

Matching native plugin source, tests, build instructions and third-party dependency sources for 0.3.13. GPL-3.0-or-later with included exceptions. For developers and licence compliance; not required to play. Keep this download available alongside the compiled main file. Windows x64 C++ build tools and Windows SDK required.

## Image

File: images/ClearSight-Nexus-thumbnail.png

Title: Clear Sight - High Contrast Accessibility

Caption: Promotional illustration showing the default blue character, red enemy and yellow objective palette. AI-generated artwork; not an in-game screenshot.

## Permissions and credits

Third-party content question: Yes, used with permission under the included licences. Do not select “all assets are mine”.

Credits text:

CommonLibSSE-NG / alandtse CommonLibVR and its contributors; Microsoft DirectXShaderCompiler, DirectXTK and DirectXMath; fmt, spdlog and rapidcsv contributors; Valve OpenVR headers. All required notices are included. Code and documentation were developed using OpenAI Codex; promotional thumbnail generated with OpenAI image generation. No Bethesda or inspiration-game assets are redistributed.

Custom permissions text:

Clear Sight's native code is GPL-3.0-or-later with the Modding Exception and GPL-3.0 Linking Exception supplied in the downloads. Use, modification and redistribution are permitted under those terms, including their source and notice obligations. Third-party material retains its original licences. The separate promotional thumbnail is not covered by the code licence. Please read LICENSING.md, COPYING and EXCEPTIONS.md.

Choose reuse/upload/modification/conversion options consistent with GPL for the code, rather than requiring extra permission. Do not claim exclusive ownership of dependencies. Donation/reward-program participation is your choice and is not set by this preparation. No console conversion is supplied.

## Changelog for 0.3.13

- Suspend world classification and treatment during save loading.
- Wait for successful load, closed loading/main menus and stable player-world readiness before resuming.
- Resolve quest targets to handles while holding the target-map lock; remove retained raw quest-target pointers and direct cached-path traversal.
- Add load-stage diagnostics and regression tests for consecutive/interrupted loading.
- Correct release licence notices and include corresponding source for distribution.
- Known issue: intermittent older-save CTD still needs in-game confirmation; no stability guarantee.

## Upload order

1. Create the page with the fields above and paste the description.
2. Upload the main ZIP as Main Files and source ZIP as Miscellaneous Files.
3. Add the thumbnail as the main listing image, requirements, tags, credits and permissions.
4. Keep the page in draft until you have reviewed it. The files are prepared locally; nothing has been published by Codex.

Do not upload the whole Nexus folder as a ZIP, old prototypes, build directories, local save files or photographs of your desktop. If Nexus quarantines a file, retain it for review and follow https://help.nexusmods.com/article/117-why-has-my-mod-been-quarantined ; don't repackage to evade scanning.

References checked 27 September 2026: https://help.nexusmods.com/article/28-file-submission-guidelines ; https://skse.silverlock.org/ ; https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files ; CommonLib source at the pinned revision in SOURCE-BUILD.md.
