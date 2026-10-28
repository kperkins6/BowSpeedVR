# BowSpeedVR desktop editor

Date: 2026-10-28
Status: Design for review; no product implementation authorized by this document alone.

## Purpose and agreed direction

Make BowSpeedVR practical to configure on different Skyrim VR mod lists. A player launches a Windows desktop editor through their own Mod Organizer 2 instance, discovers the bows and arrows enabled in that session, tunes them, and exports settings for the BowSpeedVR runtime. Mad God's Overhaul is an optional preset rather than a requirement or a separate engine.

The user chose a desktop editor, ideally launched through MO2 to detect active equipment, and explicitly retained MGO as an optional preset. Existing preferences remain: player pairing physics, separate optional NPC speed tuning, constant installed gravity, bow energy and arrow mass/capacity, bounded physical impact, and optional inventory weight. Damage and draw-time editing are future work.

The decisions below marked "design choice" are recommendations, not additional user requirements.

## What exists today

Standalone 1.0 already runs without MGO, Archery, or Ricochet. It targets Skyrim VR 1.4.15 with SKSEVR and VR Address Library. Its pure C++ physics library can be shared with an editor.

Pairing currently resolves a compiled catalogue of reviewed identities. Existing per-form overrides change values for those identities but do not register arbitrary new equipment. MGO catalogue scope is a Boolean setting. Generic discovery therefore needs a corresponding runtime registration path; a graphical INI editor alone would not meet this goal.

The public 1.0 release and the locally installed game are separate copies. Development of the editor must not silently deploy to the user's game or alter the published release.

## First release scope

- Windows x64 editor that runs offline as an MO2 executable.
- Discover enabled plugin files, resolve relevant record overrides and references, and show ordinary bows and arrows plus items excluded from pairing.
- Register additional ordinary modded bows and arrows in runtime configuration.
- Edit bow energy, arrow mass/capacity, global player speed/reference, optional NPC speed, bounded impact strength, and optional arrow inventory weight.
- Preview combinations using exactly the runtime's physics implementation.
- Import/export settings and optional presets, including MGO.
- Produce a separate settings mod without rewriting source plugins or other mods' INIs.

Desktop Skyrim SE/AE, crossbow pairing, damage changes, draw-time changes, drag, durability, live in-game reloading, and automatic tuning of arbitrary magical/helper projectiles are outside this release. Discovery can list excluded crossbows and special ammunition with a reason.

## Technology choices

Design choice: keep the scanner, profile model, preview and runtime changes in Windows x64 C++23/CMake, sharing the existing pure physics library. The desktop UI must ship with its own required components and must not depend on MO2's private Qt ABI, Python installation, a browser service, or the user's development tools. The implementation plan may select the UI toolkit; toolkit selection does not change the product contract.

Use normal file APIs inside the process launched by MO2. Do not implement another overlay of all installed mod folders and assume it reproduces MO2. Files visible through the launch's virtual Data directory are authoritative for file conflicts.

## Launch and active-session discovery

The ordinary setup adds BowSpeedVREditor.exe to MO2's executable list and uses the Skyrim VR directory as its Start in directory. Optional explicit arguments identify the game directory, profile directory and output directory for installations where automatic discovery is ambiguous.

On each launch:

1. Resolve and display the game directory. Reject a desktop Skyrim executable as an unsupported runtime target.
2. Read the enabled plugin list projected into Skyrim VR's application-data location by that MO2 session, together with its ordering information and required base masters. Resolve each enabled filename through virtual Data.
3. If the projected list is missing, contradictory or unresolvable, request the profile directory for this launch. Do not infer an active profile from the last saved selected_profile value or scan every installed plugin as though enabled.
4. Treat the exact automatic projection of plugins/order as a development validation item. The implementation must prove it with two MO2 profiles having different enabled items and overrides before claiming automatic active-profile detection. If projection is insufficient, pass the profile path explicitly; a small MO2 launch bridge is an allowed fallback, not a required additional runtime dependency.
5. Display the source of the active list and a scan fingerprint. Show a human-readable profile name only when obtained reliably. USVFS DLL presence alone does not establish which profile was selected.

MO2 file conflict resolution and plugin record override resolution are different steps. The editor must handle both. A plugin visible in Data can still be disabled. The final record can originate in a master and be overridden by a patch.

## Read-only equipment scanner

Read TES4 headers/master lists and the relevant WEAP, AMMO, PROJ and KYWD data from enabled plugins in the effective game order. Resolve references to canonical keys consisting of originating plugin filename plus local form ID. Preserve the winning plugin separately as provenance. Never treat a record's on-disk file-local high byte as its current game load-order index.

Support ordinary ESM/ESP records, ESL/light references when supported by the target VR setup, compressed records, extended subrecord sizes, nested groups, deleted records and weapon template chains. Missing masters, invalid lengths, unsupported encodings, unresolved references and template cycles must yield explicit diagnostics, not invented defaults. A scan with unresolved ordering or missing enabled masters cannot be applied as a complete active catalogue.

Collect identity, name/EditorID, winning plugin, template, weapon type/speed, ammunition type, projectile speed/gravity/force and flags needed for classification. These are record values, not promises of observed in-game velocity. Localized names should resolve when supported string resources are available; otherwise show EditorID and canonical identity rather than blocking the scan or inventing a label. The builder may choose a bounded string/BSA reader or defer optional localized labels.

The scanner must not execute scripts, load third-party SKSE DLLs or run the game to inspect equipment. Runtime changes from other native plugins cannot be inferred completely by scanning ESPs and INIs.

## Classification and enabling items

Separate "detected" from "enabled for pairing". Exclude bolts/crossbows, deleted forms, non-arrow projectiles, unresolved forms and known helper/trap/scripted forms from automatic pairing. Display exclusions so a user can understand why an item was skipped.

Design choice: known vanilla/DLC identities use existing reviewed profiles by default. Valid ordinary weapon-template descendants may inherit a profile when the entire template chain resolves to a reviewed base and no special flags require review. Suggestions based only on material/name/damage are visibly suggestions and remain disabled until the user assigns a profile or explicit values. Health damage is not treated as measured bow energy.

An ordinary new bow/arrow can be assigned a reviewed archetype or explicit valid physics values and then registered. The runtime resolves its canonical key and rechecks type/eligibility independently. The editor does not offer an unrestricted override for helper, trap or clearly special forms in this release.

Existing gravity exceptions remain excluded from pairing unless separately validated. Bound Arrows retain the reviewed extra-light profile. Future magic-arrow integration must preserve its special effects intentionally rather than being classified by its visible name alone.

## Tuning and preview semantics

Bow tuning changes its relative energy index; it is not a draw-speed rating. Arrow tuning changes relative mass and shaft capacity. The current model remains:

    requested energy = 1.25 * bow power * draw^2
    usable energy = min(requested energy, applicable shaft capacity,
                        (arrow mass + virtual bow mass) * speed ceiling ratio^2)
    launch speed = resolved reference speed * sqrt(usable energy /
                                                   (arrow mass + virtual bow mass))

The editor calls the shared library for all limits and calculations rather than reimplementing this formula. Its physical-impact preview uses the same bounded momentum response as the runtime and clearly states the assumed incoming speed. Impact speed at an actual target is not known from the launch preview alone.

Keep relative gameplay mass, optional inventory weight and Havok rigid-body mass distinct. Values must not be presented as calibrated kilograms, joules, newtons or FPS without an independently validated conversion.

Design choice: expose an overall physical-impact strength with a neutral default of 1.0, implemented in the runtime as part of the same bounded response. It must not introduce a second application of native impulse or change health damage. Preview it as a relative physical-impact factor, not guaranteed cart movement or target force. The unresolved cart-flipping investigation is separate and is not solved by this editor.

Player pairing controls remain separate from optional NPC launch-speed controls. NPC default is unchanged (1.0); crossbows, traps, spells and reflected/helper copies retain the current exclusions. Preserve installed gravity, draw timing, perks, enchantments and health damage.

Show the available Tuned reference (5000), Vanilla-calibrated reference (2520 for full-draw Long Bow + Iron) and a custom reference. Vanilla calibration is only one anchor. A proposed "this list's baseline" mode must be labeled uncalibrated until a reference has been established from a suitable observed shot. For this release the user can enter a measured custom reference; automated runtime baseline capture is deferred. Never claim a record-only scan has measured all runtime modifiers.

Provide bow/arrow selection, full/partial-draw previews, comparisons with a selected reference combination, cap indicators and validation errors. Draft edits affect previews but do not affect a running game.

## Presets and configuration ownership

Design choice: versioned, data-only presets contain physics profiles, form bindings, exclusions and optional global settings. MGO is one supplied preset, not a runtime code branch. Applying it is explicit. It applies bindings only to items that resolve in the active session and reports missing entries. It must not carry machine paths, overwrite Ricochet's files or enable absent mods.

User edits are stored separately from shipped presets. A preset update offers a comparison; it does not replace edited values silently. The editor can export a complete portable user configuration, including explicit per-item exclusions. User configuration takes precedence over shipped defaults, and its canonical identities do not depend on plugin load order. Plugin renaming or form compaction can invalidate identities and must produce unresolved-entry diagnostics.

Extend the runtime with a versioned user overlay read after the existing BowSpeedVR.ini. Design choice: use a dedicated BowSpeedVR.user.ini for explicit new registrations, value overrides, exclusions and global user choices. Field naming and parser structure are left to the builder; registration and override semantics must remain distinct. Reject an invalid overlay atomically with clear logging rather than applying half of it. Existing 1.0 configurations retain their documented behavior when no overlay is installed.

The editor must recognize the installed runtime version and show when its export needs a newer DLL. A user overlay must never appear successfully applied to the current 1.0 DLL, which does not understand it. Export for a planned runtime upgrade is allowed when labeled accordingly.

## Saving and applying

Default output is an installable settings-mod ZIP, named independently from the base runtime and MGO preset. The user installs/enables it in the same MO2 profile and lets its user overlay win. An optional saved physical output directory permits repeat exports into a dedicated user-selected settings mod.

Do not write through virtual Data to an existing INI: that could edit whichever source mod supplies it. Directory output must be an explicitly selected physical settings-mod directory, checked against the game Data and source installation paths. Scope writes to editor-owned settings/metadata files, make a recoverable backup before replacing them, and use temporary-file replacement. Do not delete other files in the destination. Preserve unknown configuration fields where practical or report an incompatible schema before export.

Before export, show the destination, changed items and blocking validation errors. Export carries a scan fingerprint. A later launch rescans; changed/missing items are reported and require review before claiming the configuration matches the new active list. Changing MO2 profiles while the editor is running requires relaunch so the process receives the new virtual filesystem.

Settings take effect on the next Skyrim VR launch. A shared physical settings mod affects every profile in which it is enabled; recommend a separate settings mod per profile when different tuning is desired.

## Validation needed before release

- Synthetic plugin fixtures prove master/reference mapping, override order, disabled plugins, deletion, templates/cycles, compressed/extended data and supported light-plugin identities. Include conflicting patches and duplicate filenames resolved through different MO2 profiles.
- Runtime tests prove that ordinary new forms can be registered, invalid/special forms are skipped, exclusions remove pairing, old configurations retain behavior, and malformed overlays do not partially apply.
- Editor/export/runtime round trips produce identical resolved physics, including capacity and speed caps, partial draws, extra-light Bound Arrows and user impact strength.
- Save/export tests prove source plugins/INIs are unchanged, output is confined to the chosen destination, backup/atomic replacement works, and absent/wrong runtime versions are reported.
- Exercise real MO2 launches using a small controlled list and the existing local MGO installation without applying changes. Verify two distinct profiles resolve different active equipment and winning patches.
- FUS support is not labeled tested until a consenting FUS user or a local FUS installation completes the same scan/preview/export and in-game representative shots. Documentation alone establishes a target, not tested compatibility.
- Existing flight/impact hooks and optional inventory weight still require the relevant in-game acceptance checks; the editor does not remove those requirements.

## Implementation order

1. Prove active-session discovery and read-only winning-record scanning with diagnostic output.
2. Define versioned generic bindings/user overlay and add guarded runtime registration.
3. Convert MGO-specific catalogue selection into an optional data preset while preserving old INI compatibility.
4. Build desktop editing and shared-library preview, then settings-mod import/export.
5. Validate profile isolation, runtime round trips and representative VR behavior; package the editor and matching runtime.

Do not deploy to the live game, push GitHub changes or publish another release as part of design review. Those actions follow the completed, tested implementation and the user's authorization for that stage.

## Sources and remaining uncertainty

- Local source: src/Plugin.cpp, include/BowSpeed/Config.h, src/PhysicsCatalog.cpp, docs/INSTALLATION.md.
- MO2's official USVFS README describes the per-process file overlay: https://github.com/ModOrganizer2/usvfs#readme
- Official MO2 launch source: https://github.com/ModOrganizer2/modorganizer/blob/master/src/spawn.cpp
- FUS's documented MO2 profiles and archery interaction mods: https://github.com/Kvitekvist/FUS

Automatic projection of the exact active plugin order, supported VR light-plugin variants, and UI packaging must be validated during implementation rather than assumed from the word "MO2". File scanning cannot fully detect native hook conflicts or predict secondary Havok contacts.
