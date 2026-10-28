# BowSpeedVR

**Bow and arrow pairing physics for Skyrim VR.** Give your choice of bow and ammunition a different feel: heavy arrows need more bow energy, strong bows can drive them faster, and light shafts have a capacity limit that keeps them from becoming laser beams. Physical impact follows momentum, with optional inventory weight drawn from the same arrow mass model.

BowSpeedVR changes launch speed and physical impact for supported player bow shots. Your installed gravity, health damage, draw timing, perks, enchantments and poisons retain their existing behavior.

## Downloads

Get published ZIPs from [GitHub Releases](https://github.com/kperkins6/BowSpeedVR/releases). The desktop editor and matching runtime can also be built using [the build guide](docs/BUILDING.md).

| Download | Use |
|---|---|
| **Runtime** | Main SKSEVR plugin and default INI. Works without Archery, Ricochet or Mad God's Overhaul. |
| **Editor** | Offline Windows application, optional MGO preset and editor guide. Launch through MO2 to tune the active profile. |
| **Source** | Repository/source archives; see the build guide for pinned dependencies and license notices. |

The desktop packages are separate: **Editor** is an offline Windows application; **Runtime** is its matching SKSEVR plugin. Editor settings require runtime **1.1.0 or newer**.

## Desktop editor

Add `BowSpeedVREditor.exe` to MO2's executable list and set **Start in** to your Skyrim VR directory. Launch it from the profile you want to tune: it scans enabled plugins, resolves winning overrides and weapon templates, and discovers bows and arrows from your list. MGO is an explicitly optional preset.

Assign archetypes or edit bow energy, arrow mass and shaft capacity, preview combinations and partial draws, and adjust player/NPC speed, physical impact and optional inventory weight. Unfamiliar ordinary equipment receives enabled, editable defaults from known templates, unambiguous material matches or bounded damage curves fitted to the active list. Inferred values are clearly labeled with their method and confidence; known profiles and reviewed safety exclusions are preserved. Export a separate settings mod, enable it in the same MO2 profile, and restart Skyrim VR. Source ESPs and other mods' configurations are not edited.

See the [desktop editor guide](docs/EDITOR.md) for installation, controls and compatibility limits. The editor needs no Python, .NET, separate UI runtime or Internet access.

## Requirements

- Skyrim VR **1.4.15** on Windows x64.
- **SKSEVR 2.0.12**.
- **VR Address Library for SKSEVR**.
- Microsoft Visual C++ x64 runtime for the SKSE plugin. The desktop editor includes its own C++ runtime.

This plugin supports Skyrim VR, including ordinary vanilla and DLC bows and arrows. Desktop Skyrim SE/AE is unsupported.

## Install

1. Install **BowSpeedVR-1.1.1-Runtime.zip** through Mod Organizer 2 or Vortex. Enable only one BowSpeedVR DLL.
2. Extract **BowSpeedVR-1.1.1-Editor.zip** to a tools directory, retaining its `presets` folder.
3. Add `BowSpeedVREditor.exe` to MO2 with **Start in** set to the Skyrim VR directory, then run it through the profile you want to tune.
4. Review the profiles and inferred defaults. Export a settings ZIP, install and enable it in that profile, and let `BowSpeedVR.user.ini` win conflicts.
5. Close the editor and launch Skyrim VR through SKSEVR. Restart the game after changing settings.

For MGO, the editor includes an optional **MGO preset**. Player and NPC speed can be adjusted independently; NPC speed tuning and inventory weight are opt-in. With Ricochet compensation configured, **NPC 0.8× + player 1.0×** gives more time to dodge enemy arrows while retaining the default player model. Loading a preset replaces draft settings, so save settings you want to keep first.

See the [installation and configuration guide](docs/INSTALLATION.md) for manual settings and troubleshooting. There is no MCM; use the desktop editor or edit the INI, then restart to change settings.

## How the physics works

Each bow has an energy budget. Each arrow has a fixed mass and shaft capacity. The launch model uses the relationship behind **E = ½mv²**, with a small virtual bow inertia term:

```text
usable energy = min(bow energy × draw², shaft capacity, speed-ceiling energy)
launch speed  = reference speed × sqrt(usable energy / (arrow mass + bow inertia))
momentum      = arrow mass × incoming speed
```

These are relative gameplay values, not calibrated kilograms, joules or feet per second. Capacity limits model an unsuitable shaft by limiting usable energy; the mod does not actually break arrows. Drag, durability, health damage and draw-time changes are outside the model.

With the default **Tuned** reference and full draw:

| Bow | Arrow | Launch speed (game units/s) |
|---|---|---:|
| Long Bow | Iron | 5,000 |
| Daedric | Iron | 6,000 |
| Long Bow | Daedric | 3,500 |
| Daedric | Daedric | 5,000 |

Matched base bow/arrow pairs share a consistent reference feel, while mixed combinations trade speed against momentum. The default speed ceiling is 6,250 game units/s. Physical impact uses a bounded multiplier derived from incoming momentum; it does not change health damage.

## Tuning

Edit `SKSE/Plugins/BowSpeedVR.ini` and restart:

| Setting | Default | Effect |
|---|---|---|
| `[Pairing] PlayerSpeedMult` | `1.0` | Scale all supported player pairing speeds. |
| `[Pairing] ReferencePreset` | `Tuned` | Use the tuned reference; `Vanilla` anchors full-draw Long Bow + Iron at 2,520 game units/s. |
| `[Pairing] ReferenceSpeed` | `5000` | Reference speed when using `Tuned`. |
| `[Pairing] MaxSpeedRatio` | `1.25` | Maximum speed relative to the selected reference. |
| `[Pairing] CapacityLimits` | `true` | Limit usable energy for low-capacity shafts. |
| `[Pairing] MomentumImpact` | `true` | Adjust physical impact using arrow mass and incoming speed. |

Per-bow energy and per-arrow mass/capacity overrides use plugin names and local form IDs. Details are in the [configuration guide](docs/INSTALLATION.md). The Vanilla preset calibrates one launch-speed anchor while preserving the pairing model for other combinations; it does not restore every vanilla mechanic.

## Optional arrow inventory weight

Enable arrow inventory weight in the editor or INI to give supported arrows inventory and carrying weight using **physics mass × 0.10**. It defaults **OFF**.

| Arrow | Weight each | Weight of 100 |
|---|---:|---:|
| Iron | 0.100 | 10.00 |
| Steel | 0.114 | 11.38 |
| Glass | 0.193 | 19.33 |
| Daedric / Dragonbone | 0.230 | 23.01 |
| Bound | 0.055 | 5.50 |

Weights are game inventory units. Custom physics-mass overrides also change carrying weight. Supported arrow items receive their weight in all inventories; bolts and unknown ammunition retain their native behavior. Avoid combining this option with a separate scripted arrow carry-weight penalty.

The option has automated native-hook coverage. VR menu display, encumbrance and save/restart transitions still need in-game validation; it remains OFF by default.

## Compatibility and scope

- Runtime defaults leave NPC and crossbow flight unchanged. Ordinary NPC bow-arrow speed can be enabled separately.
- Optional compensation accounts for Ricochet's configured global speed without rewriting its INI. NPC multipliers refer to installed bow/arrow speed before that global modifier. Restart after changing Ricochet speed.
- The editor estimates unfamiliar ordinary equipment; unusable evidence, reviewed helpers/traps, scripted equipment and unsupported special effects stay excluded. Damage-derived estimates are labeled low confidence and remain adjustable.
- A complete editor overlay keeps unassigned/excluded player shots native and suppresses legacy bow/ammo modifiers. Additive overlays retain the existing catalogue and legacy fallback policy. Without an overlay, ordinary fallback shots can still receive configured legacy adjustments.
- Imperial, Ancient Falmer and Elvenia arrows with reviewed gravity exceptions retain native flight. Their known inventory masses remain available where enrolled.
- Native runtime and ownership checks refuse unsupported or conflicting hook paths. Check `BowSpeedVR.log` if a feature does not activate.
- No ESP, Papyrus script, controller binding or third-party DLL is installed.

## Support and development

For a problem report, use [GitHub Issues](https://github.com/kperkins6/BowSpeedVR/issues) and include the game/SKSE versions, winning `BowSpeedVR.ini`, affected bow and arrow, and relevant lines from `BowSpeedVR.log`. Logs stay on your machine unless you choose to share them.

[Build instructions](docs/BUILDING.md) · [Testing guide](docs/TESTING.md) · [Dependency notices](THIRD-PARTY.md)

Licensed under [GPL-3.0-or-later](LICENSE) with the supplied [linking/modding exceptions](EXCEPTIONS.md). The implementation was developed with AI assistance under human-directed design and reviewed with automated tests and code review.
