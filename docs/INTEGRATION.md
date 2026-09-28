# Feedback and requirements — 26 September 2026

**Implementation update:** `native/` now implements the material-rendering route described here: diffuse texture replacement at lighting/grass/distant-tree draws, retained alpha/normals, native actor/quest classification, menu bypass and a shared toggle. The original findings below describe the superseded ReShade prototype. See [native verification](NATIVE-VALIDATION.md) for what has actually been tested; code implementation does not establish in-game acceptance.

The user's first in-game test exposed four issues:

| Observation | Cause / evidence | Current disposition |
| --- | --- | --- |
| Menus and UI disappear | The full-frame effect replaces pixels after game UI composition | Unresolved; blocks a usable release |
| All people are grey | Distributed ZIP has no game classification plugin or object mask | Unresolved; no actor-colour claim for this ZIP |
| Red clothes/yellow hair show through at edges | Colour matching cannot distinguish source materials from category markers | Colour-matching path removed entirely |
| Faces lack detail | Distance shading discards local surface shape and material detail | Added depth-normal shape shading and optional grayscale detail; untested |

## Required renderer integration

The next full version must alter world rendering before Skyrim composites menus and HUD. Either replace world materials at their rendering stage, or run a world-only composite with reliable category masks before UI composition. A screen-space rectangle mask or automatic pause detection would not protect arbitrary HUD, subtitles and menus, so neither is accepted as the final solution.

The integration must receive actual object identity from Skyrim. Resolve hostile actors before teammates; leave neutral actors separate. Important objects must come from supported quest references. Material RGB is never evidence of an object's category.

Flat colours must retain geometry lighting, silhouette and enough facial features for recognition. Prefer original geometry/material normals with texture albedo suppressed. Depth-reconstructed normals are only a prototype approximation: texture-painted eyes/lips, fine normal-map features and transparent surfaces are not represented faithfully in scene depth. Any retained texture detail must be adjustable.

Investigate the installed runtime and rendering mods before choosing hooks. No native renderer implementation, verified hook address, category-buffer interface, or UI-safe insertion point exists in this repository yet. The Papyrus scripts are a starting point for classification, not a complete renderer bridge. Adding native SKSE or ReShade add-on integration would change dependencies and must be documented when selected.

## Acceptance for a playable build

- Inventory, map, dialogue, subtitles, pause menu and HUD remain unchanged while the mode is enabled.
- Teammates are blue, hostile actors red, registered important objects yellow; ordinary clothes, hair and effects never assign categories.
- Faces remain recognisable at conversation distance without restoring full-colour textures.
- One accessible toggle controls the whole mode and restores the normal view.
- Missing masks or unavailable render integration leave the original image visible.

Until these are satisfied, the ZIP is a scenery/detail study only. It should not be presented as the requested complete accessibility mod.
