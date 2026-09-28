# Clear Sight — High Contrast Accessibility 0.3.13

This build replaces diffuse colour during world-object rendering and greyscales the final RGB of effect, particle and blood-splatter shaders. It does not run a full-screen ReShade effect. Normal maps, shadows and transparency are retained. Specular highlights and environment reflections are reduced; face tint/detail layers use neutral values to avoid overexposure.

## Install with Vortex

### Upgrading from SkyrimHighContrast

Disable the previous package before enabling Clear Sight: the DLL has been renamed to `ClearSight.dll`, so leaving both packages enabled would load two plugins. To retain custom settings, copy the contents of your old `SkyrimHighContrast.ini` into the new mod's `SKSE/Plugins/ClearSight.ini` in Vortex's staging folder, then deploy. All existing settings remain compatible. The new log is `ClearSight.log`.

1. Exit Skyrim. Disable the earlier **SkyrimHighContrast Shader Prototype** package and deploy. Ensure the `SkyrimHighContrast` ReShade technique is off. ReShade itself may remain installed for other effects.
2. Enable/install **SKSE64 2.3.1** for Steam Skyrim **1.7.104**, and **Address Library for SKSE Plugins v13**, containing `versionlib-1-7-104-0.bin`. Keep these dependencies enabled when upgrading.
3. Disable the previous native High Contrast version, if installed. Install `ClearSight-High-Contrast-Accessibility-0.3.13.zip` using **Mods → Install From File**. Use the new package's INI so the new palette, detail and highlight settings take effect.
4. Leave Mod Type as **Default**, enable it, then **Deploy Mods**. It installs `Data/SKSE/Plugins/ClearSight.dll` and its INI. There is no ESP to enable and no Papyrus compilation step.
5. Launch using **SKSE** from Vortex. The effect starts enabled when you load a game. Press **Ctrl + Shift + H** to toggle it off or on. On a MacBook over Moonlight, use the Control key, not Command. A notification confirms the mode changed.

The packaged INI sets `Enabled=1`, so the mode starts enabled each launch. Set `Enabled=0` to start disabled. It automatically bypasses treatment while a non-overlay menu is open, including inventory/map/model previews. Dialogue and HUD remain visible because their UI pixels are never processed by this mod.

## Colours and detail

### Change the three highlight colours

In Vortex's Mods page, open this mod's staging folder with **Open in File Manager**. Edit `SKSE/Plugins/ClearSight.ini` in a text editor. Under `[Palette]`, change these settings independently:

```ini
[Palette]
Ally=Blue
Enemy=Red
Important=Yellow
```

Available names: Blue, Red, Yellow, Green, Cyan, Purple, Magenta, Orange, White, Grey, Black. Names ignore case. Blue, Red and Yellow use the contrast-tested defaults below; other names and custom values are not guaranteed to meet the same contrast target. For example, use `Ally=Green`, `Enemy=Purple`, `Important=Orange`.

For a custom colour, enter a six-digit hex value such as `Enemy=#A600A6`, or a comma-separated red/green/blue triple in the range 0–1. Hex bytes are divided by 255 to set the material RGB; the colour is still affected by Skyrim's lighting. Invalid values fall back to the category default and produce a log warning. Existing decimal RGB configurations remain supported.

Save, deploy through Vortex, and restart Skyrim to apply changes. This configuration is in the INI, not an in-game menu. `World` and `Neutral` remain independent, so changing the three highlight colours does not recolour scenery or corpses.

| Objects | Treatment |
| --- | --- |
| Hostile living actors | Red |
| Living player, teammates and other non-hostile NPCs, including follow targets | Blue |
| Dead actors, regardless of former allegiance or quest status | Grey (Neutral palette) |
| Non-actor quest targets, quest objects and doors along active quest routes | Yellow |
| Neutral NPCs when `NonHostileActorsBlue=0` | Light grey |
| Buildings, terrain, ordinary objects, plants, grass, nearby and distant trees | Grey |
| Effect/particle shaders, including fire and blood-splatter output | Grey, preserving brightness and alpha |

Death takes priority over all actor colour rules. Living characters retain their character category even when marked as quest targets: a non-hostile follow target stays blue, a hostile target stays red. Yellow is reserved for non-actor quest objects. Corpses stay grey even when lootable or quest-related; any existing quest marker is unchanged. Resurrected actors are classified as living again. Arbitrary objects that are narratively important but not represented in Skyrim's quest data cannot be inferred. Quest inventory UI icons are left unchanged.

Blue means non-hostile by default, not necessarily a follower or story ally. Red depends on Skyrim reporting hostility to the player. Actors in scripted scenes such as the Helgen execution may remain non-hostile in game data; the mod does not assign allegiance based on narrative roles. `NonHostileActorsBlue=0` restores the earlier grey-neutral/blue-teammate distinction.

The current tracking doors on Skyrim's active-quest routes use the `Important` colour (yellow by default), even if the final objective is a person in another location. Only loaded door models are rendered. Tracking doors are refreshed with the regular classification snapshot, so doors lose the route highlight when Skyrim removes them from the active routes, unless they are independently quest targets/objects. Multiple active quests can highlight multiple routes. Direct target and tracking references are also checked. When an active objective points to an invisible vanilla XMarker, the nearest loaded door within 384 game units in the same worldspace (or the same interior cell) is highlighted. This handles the Helgen Keep entrance markers. This proximity fallback is a heuristic: tightly clustered doors may be ambiguous. Unmarked puzzle gates are not inferred. No doors are unlocked or activated by the mod.

The native configuration file controls colours, the toggle key, and detail. `FaceDetail=0.35` preserves some facial brightness variation, `ActorDetail=0.20` reduces clothing texture, and `WorldDetail=0.12` retains a little scenery texture for navigation. Detail can darken the palette but no longer amplify it. Set any detail value to zero for fully flat diffuse colour while retaining normal-map lighting. `HighlightScale=0.15` reduces specular and environment reflection intensity; zero removes those contributions and one retains their original intensity. Restart Skyrim after editing.

Version 0.3.8 also greyscales the final shader output for ordinary scenery. This removes colour introduced after texture replacement by foliage vertex tints, backlighting or other material calculations. It applies to grass, nearby tree trunks/canopies and distant tree billboards through their existing render hooks. Alpha tests and transparency remain unchanged. Actors and quest objects bypass this scenery step, retaining their configured colours. The `World` palette now controls underlying brightness; its hue is removed at the final scenery shading step. Unsupported shader variants are logged and remain unchanged; coverage of all modded plants and trees is not yet verified.

While high contrast is active, local point lights attached to treated draws use neutral RGB with their original luminance. This covers nearby illumination from fire, torches and magic using the same local-light path. Brightness, animation, position, radius, attenuation and shadow settings are retained. Light colours are restored immediately after shader constant upload; source lights and saved game data are not permanently edited. Character and quest palettes remain coloured under these neutral lights.

## Current limits and verification

### Dungeon shadow detail (0.3.10)

The 0.3.9 settings were confirmed active in the reported dark dungeon. Version 0.3.10 raises the default shadow floor from 0.015 to 0.035 and uses a stronger fourth-root curve. The upper lift limit remains 0.06. Use the new INI or update `SceneryShadowLift=0.035` in an existing configuration; keeping the old value retains a weaker lift. Active values are now logged at startup. This improves visibility but compresses the darkest shading range; completely absent shading detail cannot be recovered.

Dark scenery now receives a bounded shadow lift after lighting, separate from the effects shader cache. The default curve is `max(L, min(0.06, 0.035 + 0.025*pow(max(L,0)/0.06,0.25)))`, where L is the shader's greyscale output. This lifts dark tones while retaining their ordering, leaves values above 0.06 unchanged, and does not change alpha. It cannot reconstruct detail where the original shader produces identical black pixels. The new `WorldDetail=0.12` also retains more surface detail than the previous 0.03.

With a custom INI, set these under `[General]` and restart Skyrim:

```ini
WorldDetail=0.12
SceneryShadowLift=0.035
SceneryShadowRange=0.06
```

`SceneryShadowLift=0` disables the lift. The lift value is limited below the range to keep the curve monotonic. Increasing the range can reduce separation from character colours. The defaults limit the added brightness to the existing 0.06 background reference; the previous palette checks still pass, but final in-game contrast is still affected by exposure and post-processing. Scenery includes exterior shadows too; characters, quest highlights, effects and UI do not receive this shadow lift. Dungeon appearance still requires in-game checking.

### Default palette contrast (0.3.8)

Use the new INI, or set `World=0.06,0.06,0.06`, `Ally=Blue`, `Enemy=Red`, `Important=Yellow` in your existing INI. The previous lighter World value does not satisfy the new palette test. Named Blue/Red/Yellow presets are brighter in this release; explicit custom RGB values remain unchanged.

The reference-swatch test uses the [W3C contrast formula](https://www.w3.org/WAI/WCAG21/understanding/contrast-minimum.html): decode sRGB, calculate relative luminance, then `(lighter + 0.05)/(darker + 0.05)`. Against the neutral grey reference RGB `(0.06,0.06,0.06)`:

| Default highlight | RGB | Reference contrast |
| --- | --- | --- |
| Blue characters | 0.15, 0.55, 1.00 | 5.73:1 |
| Red enemies | 1.00, 0.20, 0.10 | 5.23:1 |
| Yellow quest objects | 1.00, 0.85, 0.05 | 13.80:1 |

A separate linear-material calculation includes the darkest default face-detail multiplier and downward 8-bit quantization; its minimum ratio is 3.32:1. These are palette/material checks, **not measured in-game screen contrast or a WCAG conformance claim**. Shadows, lighting, exposure, fog, translucency, colour grading and untreated backgrounds can reduce actual contrast below 3:1. A strict every-scene minimum would need additional rendering controls and validation of captured final frames. This build changes defaults; it does not enforce a per-pixel contrast floor.

This is an experimental native build. Compilation/GPU tests and offline hook-address checks are separate from in-game verification; see `NATIVE-VALIDATION.md` for the actual results. Visual appearance, stability and performance in Skyrim still need a game run.

Targets the vanilla renderer and exactly **1.7.104**; other runtimes are rejected. Community Shaders/ENB compatibility is not implemented. Detected Community Shaders/ENB modules cause the hooks to remain disabled. Other rendering injectors have not been tested.

Water and sky shaders are left intact. Effects retain their animation and shape; their final RGB becomes luminance while alpha, discard, depth and other shader outputs remain unchanged. This includes shader-generated blood colours and particle vertex tints. Unsupported shader bytecode is left unchanged and logged as `conversion unavailable`. Version 0.3.5 separately neutralizes local lights during treated draws; sunlight, weather/fog colours, colour grading and rendering paths outside the hooked shaders are not changed by that light correction. Modded shaders and every fire, blood or magic asset have not been verified in game.

For diffuse-material conversion, cube/array/multisample/dynamic/render-target textures are skipped. Derived textures default to a maximum dimension of 256 pixels; hair/foliage edges and facial detail need visual checking. Since version 0.3.1, supported textures are converted on first use instead of drawing originals when a time budget runs out. This removes that source of flicker, at the cost of possible first-use hitches. The derived texture cache is capped at 256 MB without the old 256-entry limit; source references and captured/cached shaders use additional memory. Scenes exceeding the byte limit may repeatedly rebuild textures and need performance testing.

If the toggle does nothing, inspect `ClearSight.log` in Skyrim's SKSE log directory under Documents/My Games. A successful load logs native hook installation; enabling the mode logs loaded-reference category counts and GPU initialization. Disable the mod and deploy to uninstall. The plugin does not write save data or change quest flags.

Official dependencies: [SKSE](https://skse.silverlock.org/) and [Address Library](https://www.nexusmods.com/skyrimspecialedition/mods/32444).

## Loading saves

Version 0.3.13 suspends classification and treatment during save loading. After a successful load, the loading/main menus must close and the player cell and model must be ready for 1.5 seconds before treatment resumes. A failed load keeps treatment suspended. Quest targets are resolved to handles under their map lock rather than retaining raw target pointers. The log records suspension, completion and readiness. This is defensive hardening; the intermittent reported CTD has not been reproduced or attributed by a crash dump. No save data is modified.
