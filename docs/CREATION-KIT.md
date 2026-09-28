# Creation Kit assembly

This is a record recipe, not an included plugin. Use the Skyrim Special Edition Creation Kit and its complete Papyrus sources. The local SKSE source folder is incomplete (for example, EffectShader.psc is absent), so do not use that folder alone as the compiler's import library.

Load Skyrim.esm and create a new `SkyrimHighContrast.esp`. Do not overwrite vanilla records. Compile the four scripts first and make their PEX files available in Data/Scripts in your development profile. Put PSC files in the Kit's configured source directory so the script editor can see their properties.

## 1. Colour shaders (EFSH)

Create three new Effect Shader records:

| Editor ID | Fill RGB |
| --- | --- |
| HC_EnemyShader | 255, 0, 0 |
| HC_AllyShader | 0, 0, 255 |
| HC_ImportantShader | 255, 255, 0 |

Use a constant white fill texture, full fill alpha, no animation, no particles, no edge falloff and no holes. All colour keys should have the same RGB. Start with source blend One, destination blend Zero, blend operation Add, and depth test Equal where available. Use zero fade-in/out and no pulsation. These settings are an experimental starting point: inspect the Kit preview and then test opaque coverage in-game. Do not reuse a vanilla shader ID: the scripts stop only these owned effects.

If the Kit requires a texture asset, create a plain opaque white DDS with your texture editor under `Textures/HighContrast/white.dds`. This asset is not supplied. Effect shaders may not completely replace lighting or cover all geometry; coverage is a milestone to validate, not an asserted capability.

## 2. Actor detection

Create `HC_ActorMarkEffect` (MGEF): Script archetype, Fire and Forget, Aimed delivery, non-hostile, non-detrimental, no hit event, no FX/sounds and no cost. Attach `HC_ActorEffect`. Set No Recast to prevent overlapping instances. Condition out the player (`GetIsID Player == 0`, subject).

Create `HC_ActorMarkSpell` (SPEL): ordinary spell, Fire and Forget, Aimed, zero cost. Add the above effect, duration 3 seconds, magnitude 0, area 0.

Create `HC_ScanCloakEffect` (MGEF): Cloak archetype, Constant Effect, Self, non-hostile and non-detrimental, no FX/sounds. Set its associated spell to `HC_ActorMarkSpell`.

Create `HC_ScanAbility` (SPEL): Ability, Constant Effect, Self. Add `HC_ScanCloakEffect`. Start with magnitude 15 (cloak radius uses feet); expand only after testing performance and detection range. No conditions based on combat: allies must be detected too. Check in-game that this cloak does not displace other cloak spells; compatibility is unverified.

## 3. Controller and power

Create `HC_ControllerQuest` (QUST): Start Game Enabled, Run Once, no stages required. Attach `HC_Controller`. Fill its three shader properties and `ScanAbility`.

Create `HC_ToggleMagicEffect` (MGEF): Script, Fire and Forget, Self, no cost, non-hostile, no FX. Attach `HC_ToggleEffect` and point `Controller` at `HC_ControllerQuest`.

Create `HC_TogglePower` (SPEL), display name **High Contrast**, type Lesser Power, Fire and Forget, Self, zero cost. Add `HC_ToggleMagicEffect` with duration 1. Set the controller's `TogglePower` to this record.

Set `HC_ActorEffect.Controller` on `HC_ActorMarkEffect` to the same quest.

Add a player reference alias to `HC_ControllerQuest`, forced reference PlayerRef. Attach `HC_PlayerAlias`, setting `Controller` to the quest. This restores timers and the power after loading a save. Generate the plugin's SEQ file using the Kit/xEdit workflow for start-enabled quests when preparing distribution; validate both an existing save and a new game.

## 4. Important objects

Add optional reference aliases to `HC_ControllerQuest` for a small test set of **non-actor world objects**. Fill them with specific references, or with external aliases from a quest integration you intentionally support. Populate the controller's `ImportantObjects` property with those aliases. Do not set the Quest Object flag merely to highlight an item; that would change gameplay.

Empty aliases are ignored. Unloaded objects are skipped. Expiring effects bound stale colour to about 1.5 seconds after an alias changes. For an initial test, use one visible item and one door. Do not register actors here: two shaders could compete. Vanilla quest aliases are not automatically enumerated. Inventory UI highlighting is not implemented.

## 5. Compile

From the repository root, run with actual compiler/source locations:

```powershell
./tools/Compile-Papyrus.ps1 -Compiler 'D:\path\PapyrusCompiler.exe' -Imports 'D:\path\complete\Source\Scripts'
```

The default flags file is `TESV_Papyrus_Flags.flg` in the import directory; pass `-Flags` if it is elsewhere. PEX files go to `build/Data/Scripts`. Assemble these with your ESP and SEQ in a mod-manager test profile. The ReShade file belongs beside the game's ReShade shaders, not inside the mod's Data folder.
