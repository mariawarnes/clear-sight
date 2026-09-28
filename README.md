# Clear Sight — High Contrast Accessibility

The current implementation is the **native SKSE plugin in `native/`**, targeting Steam Skyrim **1.7.104**. It treats world materials during geometry rendering, preserves normal maps and alpha, and uses game data to distinguish hostile actors, teammates and quest targets. Menus bypass treatment; HUD pixels are not processed.

Use [native installation instructions](docs/NATIVE-INSTALL.md) and [validation results](docs/NATIVE-VALIDATION.md). Build with `./tools/Build-Native.ps1`, then package with `./tools/Package-Native.ps1`. The resulting `build/ClearSight-High-Contrast-Accessibility-0.3.13.zip` installs through Vortex as a **Default** mod. Toggle with **Ctrl + Shift + H**.

Dependencies: SKSE64 **2.3.1** and Address Library **v13** for Skyrim **1.7.104**. The native implementation does **not** need ReShade, Creation Kit, an ESP, or the scripts below. Disable the old ReShade prototype before using it. In-game appearance and stability still need verification; the build is experimental.

<details>
<summary>Legacy ReShade/Papyrus prototype notes — superseded by the native implementation</summary>

A starting point for a low-detail accessibility mode inspired by The Last of Us Part II and God of War Ragnarök. Targets Windows Skyrim Special Edition / Anniversary Edition. No assets from either reference game are used.

**Status: uncompiled, not yet playable as a complete mod.** Papyrus sources, a ReShade effect, and a Creation Kit assembly guide are included. There is no ESP or PEX release yet. No files have been installed into Skyrim.

| Category | Default appearance |
| --- | --- |
| Environment | Muted slate, depth-normal shading and edges; optional grayscale detail |
| Allies (player teammates) | Blue |
| Enemies (currently hostile to player) | Red |
| Registered important objects | Yellow |
| Neutral actors | Environment colour |

This table describes the target mode. The downloadable shader alone renders **all geometry in the environment colour**, including characters. It does not currently display the actor/object categories.

## Implementation

`Data/Scripts/Source` contains a toggle controller, player alias, nearby-actor effect, and toggle power script. A non-hostile cloak delivers the actor effect; classification updates once per second. Important objects come from a configurable reference-alias array. Dedicated effect shaders supply colour markers.

`reshade-shaders/Shaders/SkyrimHighContrast.fx` replaces scenery with flat colour and depth-normal shading. Colour-key detection was removed after testing showed coloured fragments on clothing/hair. Existing `PreserveMarkers` preset values no longer have any effect. The Skyrim effect-shader colours will also be flattened until a real category mask is implemented.

Geometry shading attempts to retain noses, cheeks and other modeled features. It cannot recover painted eyes/lips or normal-map details missing from the depth buffer. The optional **Original lighting / texture detail** slider adds grayscale brightness detail across the entire frame, including some texture; its default is zero. Neither this revision's appearance nor its compilation has been tested in-game.

The shader requires a working ReShade depth buffer and the standard `ReShade.fxh`. It affects the final image, including menus/HUD. Disable it in menus for now. Skyrim's toggle power and ReShade's technique toggle are separate. A release needs an engine-integrated object mask and UI-safe render stage to resolve these limitations.

## Build and try

### Shader-only installation with Vortex

Run `./tools/Package-Shader.ps1` to create `build/SkyrimHighContrast-Shader-Prototype-0.2.zip`. Install that archive using Vortex's **Install From File**, check its Mod Type is **Engine Injector**, then enable and deploy. Disable the previous shader package first if installed. Included Vortex override instructions set the game-root destination on supported versions (Vortex 1.14+). The archive contains only our shader and installation metadata, with no ReShade runtime, settings, or Skyrim plugin. Vortex deployment has not been tested here.

Keep your existing ReShade installation with Standard effects. In-game, press Home, reload effects and enable SkyrimHighContrast. Set an effect-toggle shortcut in ReShade Settings before enabling it, because menus are still affected. Actor and quest highlighting require both the plugin build below and a replacement for the removed colour bridge. If you previously copied the same shader manually, resolve Vortex's existing-file prompt before deployment. Disable and deploy in Vortex to remove the managed shader.

### Full plugin development

1. Follow [the Creation Kit guide](docs/CREATION-KIT.md) to create the missing game records and fill properties.
2. Compile with `tools/Compile-Papyrus.ps1`, supplying your Creation Kit compiler and vanilla source directory. Compile before attaching the scripts in the Kit.
3. Install the resulting ESP and compiled scripts through a separate mod-manager test profile.
4. Install ReShade for the game's DirectX 11 renderer. Copy the `.fx` file into its shader search directory alongside the standard includes. Enable `SkyrimHighContrast` only after checking depth with ReShade's `DisplayDepth`.
5. Use the **High Contrast** lesser power to toggle game highlights; bind a ReShade technique key to toggle environment flattening. Complete [the in-game checks](docs/TESTING.md) before regular use.

Skyrim was found locally at `D:\SteamLibrary\steamapps\common\Skyrim Special Edition`, with Papyrus API sources but no Papyrus compiler found under that installation. The installed game has not been modified.

## Next milestones

- Assemble ESP and compile against the installed game's Creation Kit sources.
- Validate opaque colour fills on skin, armour, creatures, and important objects.
- Supply an engine category mask; process the world before HUD/menus are drawn.
- Add active-quest objective discovery, unified toggle, configurable palettes, and compatibility testing.

ReShade syntax and depth integration follow the upstream [FX reference](https://github.com/crosire/reshade-shaders/blob/slim/REFERENCE.md) and [standard include](https://github.com/crosire/reshade-shaders/blob/slim/Shaders/ReShade.fxh). Papyrus native signatures were checked against the local Skyrim script sources.

See [the feedback and integration requirements](docs/INTEGRATION.md) for the remaining blockers.

</details>
