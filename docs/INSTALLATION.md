# Install and configure BowSpeedVR

Requires Skyrim VR 1.4.15, SKSEVR 2.0.12 and VR Address Library for SKSEVR on Windows x64. Archery, Ricochet and Mad God's Overhaul are optional for Standalone.

## Standalone

Install `BowSpeedVR-1.0.0-Standalone.zip` with MO2 or Vortex. Its installer offers arrow inventory weight, default OFF. Enable one BowSpeedVR DLL and launch through SKSEVR. Replace a duplicate BowSpeedVR installation rather than enabling two copies.

For manual installation, copy the ZIP's `SKSE` folder into the game's Data folder. This installs the default INI with inventory weight OFF. The `BowSpeedVR` folder contains documentation and licenses. Files in `fomod` and `presets` are for the mod-manager wizard and do not need to be copied manually.

Standalone pairs supported vanilla/DLC bows and arrows and preserves installed gravity. Its defaults leave NPC and crossbow flight unchanged. Unknown equipment skips pairing; ordinary fallback bow shots can still receive configured speed multipliers and compensation when those are enabled.

## MGO configuration patch

Install Standalone first, then `BowSpeedVR-1.0.0-MGO-FOMOD.zip` as a separate mod. In MO2, place the patch after Standalone so its `SKSE/Plugins/BowSpeedVR.ini` wins. In Vortex, resolve that conflict in favor of the patch. The patch contains configuration only and requires the matching main DLL.

The wizard has three independent choices:

| Choice | Options | Default |
|---|---|---|
| NPC arrows | 0.8×, 0.9×, 1.0× | 1.0× |
| Player arrows | 0.8×, 0.9×, 1.0×, 1.1×, 1.2×, Vanilla calibrated | 1.0× |
| Arrow inventory weight | ON, OFF | OFF |

Select NPC 0.8× and player 1.0× for slower enemy arrows with the default player physics. Player multipliers scale the 5,000 reference; Vanilla calibrated uses 2,520. NPC multipliers refer to the installed native bow/arrow speed before Ricochet's configured global modifier, which can differ from vanilla.

The patch enables the reviewed MGO equipment catalogue. It does not install Archery or Ricochet. Imperial, Ancient Falmer and Elvenia arrows skip pairing because their gravity differs. Unknown bows/arrows also skip pairing, but ordinary fallback shots can still receive selected speed multipliers, Ricochet compensation and eligible legacy ammo adjustments. NPC tuning excludes crossbows, spells, traps, helpers and reflected copies.

Ricochet's DLL presence and global speed setting are read at startup for compensation. Its INI is never changed. Without its DLL, compensation uses 1.0. An invalid speed disables unpaired correction. Keep Ricochet speed fixed during a game session and restart after changing it.

## Global and per-item settings

Edit the winning `SKSE/Plugins/BowSpeedVR.ini` and restart. There is no in-game MCM.

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

Both installers offer this option, default OFF. The MGO patch replaces the complete standalone INI, including the weight choice; select it on the MGO immersion page when using the patch. The manual equivalent is:

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
- Reinstalling either wizard selects a complete INI and replaces manual edits. Save custom settings before reinstalling.
- Disable the MGO patch to expose the standalone configuration. Disable Standalone too to remove BowSpeedVR's effects. Other mods retain their own changes.
