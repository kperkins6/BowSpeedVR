# Testing and problem reports

The automated suite covers configuration, pairing calculations, item eligibility, native-code guards, impact thunk behavior, inventory-weight fallback and installer output. Build and test commands are in [BUILDING.md](BUILDING.md). Automated checks do not substitute for headset tests of menu display, carrying totals, physics or mod-manager navigation.

## In-game checks

1. On a minimal VR profile with SKSEVR and VR Address Library, check the log for active flight/impact hooks and the vanilla/DLC catalogue. Inventory weight defaults OFF. Test full and partial draws with Long Bow + Iron, Daedric + Iron, Long Bow + Daedric and Daedric + Daedric. Default full-draw targets are 5,000, 6,000, 3,500 and 5,000 game units/s. Check Bound arrows and NPC archers too. Standalone leaves NPC/crossbow speeds and installed gravity alone.
2. Set `ReferencePreset=Vanilla` and `PlayerSpeedMult=1.0`; full-draw Long Bow + Iron should launch at 2,520. Other combinations retain the pairing model.
3. In MGO, test NPC 1.0/player 1.0 and NPC 0.8/player 1.0. With Ricochet global speed 0.8, ordinary NPC native speed is multiplied by 1.25 and 1 respectively before Ricochet applies. Check player and NPC choices independently, including Vanilla player speed. Restart after changing Ricochet settings.
4. Check clutter and ragdoll impacts against incoming-velocity diagnostics. Compare ricochets, broken/replaced ammo and copied projectiles. NPC health damage and force should retain their installed behavior.
5. Check save/load, equipment switching, rapid firing, long-lived arrows, missing optional mods and unknown equipment. Hook conflicts should refuse changes rather than applying stale shot data.
6. Check both installers in MO2/Vortex, including defaults and Back/Next navigation. Standalone has two weight choices; MGO has 36 NPC/player/weight combinations. Confirm the MGO INI wins, and disabling that patch restores the standalone selection.
7. Enable inventory weight and confirm its hook in the startup log. Add, remove, fire, recover, drop, stash and trade stacks of 100 Iron, Glass and Daedric arrows separately. Default carry changes are approximately 10.00, 19.33 and 23.01 units. Check menu rounding and stack quantities. Bolts, unknown ammo and non-ammo item weights should remain native.
8. Check Bound arrow conjuration/despawn, followers and special arrows. Per-arrow mass overrides should affect carrying weight at the same 0.10 scale. MGO gravity-excluded arrows can receive inventory mass while retaining fallback flight.
9. Save with weight ON, restart with it OFF and reload. Confirm carrying totals recalculate without manual item changes. Repeat with ON and check a new game and repeated loads. This feature's menu/carry/save behavior remains an open in-game validation item.

Isolated native tests cover later replacement of the original weight getter and allocation/query failures. Do not test hook conflicts by patching arbitrary game memory.

## Report a problem

Enable `[General] DiagnosticLogging=true` to collect additional information in `BowSpeedVR.log`. Include game/SKSE versions, the mod version, winning INI, affected bow/ammunition and relevant startup/launch/impact lines in a GitHub Issue. Logs are local and never uploaded automatically.
