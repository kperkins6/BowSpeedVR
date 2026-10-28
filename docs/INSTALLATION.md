# Install and configure BowSpeedVR

Requires Skyrim VR 1.4.15, SKSEVR 2.0.12, VR Address Library for SKSEVR and the Microsoft Visual C++ x64 runtime on Windows x64. Archery, Ricochet and Mad God's Overhaul are optional.

For the desktop workflow, install the matching Runtime ZIP and follow the [editor guide](EDITOR.md). Runtime 1.1.0 reads `BowSpeedVR.user.ini` after the base INI. This separate user overlay can register equipment from other mods; it is distinct from the legacy per-form override sections below. A complete editor overlay suppresses legacy bow/ammo modifiers to prevent stacking. Invalid overlays are rejected as a whole.

## Runtime and editor

Install `BowSpeedVR-1.1.1-Runtime.zip` with MO2 or Vortex. It contains the DLL, default INI, documentation and licenses. Enable one BowSpeedVR DLL; replace an existing installation rather than enabling two copies. Inventory weight defaults OFF and can be enabled in the editor or INI.

Extract `BowSpeedVR-1.1.1-Editor.zip` outside the game Data directory, retaining its `presets` folder. Add its executable to MO2 and set Start in to the Skyrim VR directory. Run through MO2 to scan that profile, review/tune equipment, export a settings mod and enable it in the same profile. Let its `SKSE/Plugins/BowSpeedVR.user.ini` win conflicts. Close the editor before launching the game through SKSEVR. See the [editor guide](EDITOR.md) for details. The editor includes its C++ runtime and needs no Python, .NET or network connection.

For manual runtime installation, copy the Runtime ZIP's `SKSE` folder into the game's Data folder. The `BowSpeedVR` folder contains documentation and licenses. The MO2-launched editor is recommended for discovering modded equipment.

Runtime defaults pair supported vanilla/DLC bows and arrows, preserve installed gravity and leave NPC/crossbow flight unchanged. The editor supplies enabled, labeled estimates for unfamiliar ordinary equipment using templates, material evidence and bounded active-list damage curves. Known profiles and reviewed exclusions remain protected; estimates can be overridden.

## Optional MGO preset and NPC tuning

The editor's MGO button loads the supplied data preset; it does not install or enable other mods. Save any existing draft settings first. Missing preset equipment is skipped and reported. Export the resulting settings as a separate mod.

Player and NPC speeds are independent controls. NPC tuning and arrow inventory weights default disabled. To tune NPC bow arrows, enable NPC speed tuning and choose its multiplier. Ricochet compensation is optional and is enabled by the supplied MGO preset. Crossbows, spells, helpers and unsupported special projectiles are outside NPC tuning.

With compensation configured, NPC 0.8× and player 1.0× gives slower enemy arrows with the default player physics. Player multipliers scale the 5,000 reference; Vanilla calibrated uses 2,520. NPC multipliers refer to installed native bow/arrow speed before Ricochet's global modifier, which can differ from vanilla.

Imperial, Ancient Falmer and Elvenia arrows with reviewed gravity exceptions retain native flight. A complete editor overlay keeps other unassigned/excluded player shots native. An additive overlay (`ReplaceCatalog=false`) retains the current base catalogue and legacy fallback settings, which is useful for adding registrations while preserving an existing tune.

Ricochet's DLL presence and global speed setting are read at startup for compensation. Its INI is never changed. Without its DLL, compensation uses 1.0. An invalid speed disables unpaired correction. Keep Ricochet speed fixed during a game session and restart after changing it.

## Global and per-item settings

Use the desktop editor, or edit the winning `SKSE/Plugins/BowSpeedVR.ini` and restart. There is no in-game MCM. Desktop user overlays take precedence over the base settings they specify.

```ini
[Pairing]
ReferencePreset=Tuned
ReferenceSpeed=5000
PlayerSpeedMult=1.0
MaxSpeedRatio=1.25
CapacityLimits=true
MomentumImpact=true
```

`ReferencePreset=Vanilla` uses 2,520 regardless of `ReferenceSpeed`, then applies `PlayerSpeedMult`. Keep the multiplier at 1.0 to match the unmodified full-draw Long Bow + Iron launch speed. This changes a speed anchor, not gravity, damage or draw timing.

Per-form tuning uses plugin names and local hexadecimal form IDs. These examples keep the Long Bow's default power and double the Iron Arrow's mass:

```ini
[PairingBowPower]
Skyrim.esm|0x0003B562=1.0

[PairingAmmoMass]
Skyrim.esm|0x0001397D=2.0
```

Bow power is an energy index, not a direct speed multiplier. Arrow mass affects flight, physical impact and optional inventory weight. `[PairingAmmoCapacity]` uses the same key format to limit the energy an arrow can receive. Consult `data/bow-records.csv` and `data/ammo-records.csv` in the source for reviewed identities. Unrecognized items are not made eligible merely by adding an override.

Gravity, health damage, native draw timing, perks, enchantments and poison retain their installed behavior. All speed values are game units, not feet per second.

## Arrow inventory weight

Enable this option in the editor or winning INI; it defaults OFF. The manual equivalent is:

```ini
[Immersion]
ArrowInventoryWeight=true
```

Inventory weight per arrow equals resolved physics mass × 0.10. Iron weighs 0.100, Steel approximately 0.114, Glass 0.193, Daedric/Dragonbone 0.230 and Bound 0.055 game inventory units each. Supported arrow items receive weight in all inventories. Bolts and unknown items retain native weight. MGO arrows excluded from pairing because of gravity can still receive their known inventory mass.

Restart after changing the flag. Player carry totals are invalidated once after loading so ON/OFF changes can recalculate through the native inventory system. No weight field is added to VR ammo records, and no permanent carry penalty is written. Avoid a second mod that independently adds scripted arrow carry weight.

Native-hook tests cover weight selection and guarded fallback. Actual VR inventory display, carrying totals and ON/OFF save/restart behavior still require in-game validation. See the [testing guide](TESTING.md).

## Troubleshooting and removal

- Check `BowSpeedVR.log` in the SKSE log directory for version, resolved forms and hook startup status. `DiagnosticLogging=true` in `[General]` adds launch and impact information.
- A conflicting item-weight getter or allocation failure disables that optional hook while flight setup can continue. Unsupported runtime or native hook bytes cause the relevant feature to refuse installation.
- When settings have no effect, confirm which INI wins the mod-manager file conflict and restart the game.
- Back up custom INIs before replacing a runtime or configuration mod. Newly supplied defaults can overwrite edits when the mod manager replaces files.
- Disable the separate settings mod to restore the base configuration. Disable the runtime mod as well to remove BowSpeedVR's effects. Restart Skyrim after either change.
