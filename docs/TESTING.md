# Verification checklist

Current verification: actor classification, spell and lifecycle API names checked against installed Papyrus sources; PowerShell helper parsed and missing-tool failure checked. User testing of the first shader reported unreadable menus, uniformly grey actors, red/yellow fragments and insufficient facial detail. The revised depth-normal shader has not been compiled or tested in-game. The full plugin cases below remain untested.

1. Confirm ReShade DisplayDepth shows real world geometry in interiors and exteriors; set reversed/upside-down depth flags as needed. A blank depth image is a blocker for flattening.
2. Confirm the power appears on an existing save and new game; highlights start disabled. Use the power twice and confirm effects clear within 1.5 seconds and the scan ability is removed.
3. Recruit a follower: blue. Approach a hostile bandit: red. Neutral NPC: no category fill. A teammate becoming hostile: red on the next update. Dead actor: fill cleared.
4. Register an item and door alias: yellow when loaded. Empty/refill aliases, pick up the item, leave the cell, and return; no lasting ghost colour or Papyrus errors.
5. Save with the mode on, reload, toggle off; repeat with mode off. No duplicated power or accumulating update loops. Fast travel and load screens must recover.
6. Inspect face, hair, armour, weapons, creature bodies, foliage, water and transparent objects. Record gaps in shader coverage. Check overlap with magic effects and cloak spells.
7. Enable flattening: check geometry shading on nearby faces, sloped terrain and silhouettes. Tune Geometry shading and approximate Vertical field of view. Test dark caves and bright snow. Compare Original lighting / texture detail at 0 and 0.25; confirm the tradeoff between facial readability and surface texture.
8. With the revised shader and a preset saved by the original version, verify that red clothes and yellow hair no longer create category-colour fragments. Colour matching has been removed. All actors remain slate until the category-mask integration exists.
9. Check map, books, inventory, subtitles and HUD. The shader currently affects these too: disable its technique when using menus. This is a known release blocker.
10. Compare frame time and Papyrus logs with the mode off/on in a crowded town. Test long sessions and quick repeated toggles. Record game runtime, ReShade version, GPU, mod list and failures.

Do not call this a complete accessibility mode until reliable category masks, UI preservation and unified toggling are implemented and tested.
