# BowSpeedVR desktop editor

Tune player bow-and-arrow pairing physics for your active Skyrim VR mod list. The editor runs offline on Windows x64. MGO, Archery and Ricochet are optional. The editor does not edit your ESPs, damage, draw timings or gravity.

## Install and launch through MO2

1. Install the matching **Runtime ZIP** as a mod. It requires Skyrim VR 1.4.15, SKSEVR 2.0.12, VR Address Library and the Microsoft Visual C++ x64 runtime used by SKSE plugins.
2. Extract the **Editor ZIP** into a tools directory, retaining its `presets` folder. The desktop executable includes its C++ runtime and needs no Python, .NET or separate UI installation.
3. In MO2, open **Executables**, add `BowSpeedVREditor.exe`, and set **Start in** to the Skyrim VR directory containing `SkyrimVR.exe`.
4. Select the profile you want to tune and run the editor through MO2. It automatically scans the virtual Data directory and the active plugin list projected into Skyrim VR's application-data directory.

Keep MO2's virtual filesystem active while editing. Close the editor before changing profiles and relaunch it afterward. If automatic discovery cannot locate the active list, select the profile directory containing `plugins.txt`. Selecting a profile directory does not replace the need to launch through MO2.

The scanner supports current MO2 starred active-plugin lists. Ambiguous plain legacy lists are reported rather than assumed active. It resolves plugin masters, winning overrides, weapon templates and light-plugin references. Localized names fall back to EditorIDs when string-table names are unavailable.

## Tune equipment

Use **Find** and the equipment filter to select an item. Known vanilla/DLC equipment starts with reviewed values. Ordinary bows derived from reviewed weapon templates can inherit a profile. Other equipment stays unchanged until assigned.

Choose an archetype and click **Assign**, or enter explicit bow energy / arrow mass and shaft capacity values, check **Enable pairing**, and click **Apply item values**. Uncheck pairing and apply to leave that item out of the model. Crossbows, non-playable helpers, unresolved templates and special projectiles cannot be enrolled through the ordinary editor.

Bow energy is not the weapon's draw-speed rating. Arrow mass is a relative physics value; it is not Havok rigid-body mass or a measured number of kilograms. Optional inventory weight uses mass times 0.10 in game carrying units. Bound Arrows use the extra-light profile.

Global controls offer player pairing, a reference speed, player speed multiplier, optional independent NPC speed, physical-impact strength, shaft capacity limits, momentum-based impact and inventory weight. Click **Apply global values** to update the draft. NPC tuning retains the runtime's crossbow/helper exclusions. Inventory weights apply to supported ammunition in all inventories.

**Vanilla reference** anchors full-draw Long Bow + Iron at 2520 game units/s. Tuned uses 5000 by default; a custom reference can be 1000–10000. These are game units, not FPS. A record scan cannot measure every runtime modifier from other SKSE plugins; no automatic claim of matching the list's observed launch speed is made.

Select an enabled bow and arrow in the combination preview and vary draw percentage. Launch speed, relative momentum and physical-impact factor come from the same C++ model used by the runtime. Cap indicators show whether shaft capacity or the speed ceiling removed energy. Impact preview assumes incoming speed equals launch speed; it does not predict cart movement or health damage. Impact strength is 0–2, with a final factor bounded to 0–1.55.

## Optional MGO preset

**MGO preset (optional)** loads `presets/MGO.ini` into the draft. It is data for the same runtime. Missing equipment is skipped and reported; absent mods are not enabled or installed. Reviewed gravity exceptions retain native flight. You can change player and NPC speed separately after applying it.

The editor never writes Ricochet's configuration. **Compensate Ricochet speed** uses the runtime's optional detection of Ricochet's DLL and configured global speed. Restart Skyrim after changing either mod's configuration. The preset can replace draft values, so export settings you want to keep before loading a different preset. Reviewed gravity exceptions can retain optional inventory mass while their flight stays native; an explicit item exclusion removes both pairing and modeled weight.

## Export and apply

**Export settings ZIP** creates a separate, installable settings mod. Install it in the same MO2 profile, enable it, and let its `SKSE/Plugins/BowSpeedVR.user.ini` win conflicts. Restart Skyrim VR for changes to take effect. The editor reports whether the installed runtime supports this overlay; it needs **1.1.0 or newer**. It can export ahead of a runtime upgrade, but the older DLL cannot apply those settings.

Alternatively, **Export folder** writes to a dedicated physical settings-mod folder you select. Use an empty folder on the first export (MO2's `meta.ini` is allowed). The editor marks its output folder for repeat exports, rejects existing source-mod folders and the game directory, backs up an existing overlay as `.bak`, and does not delete other files. Use a separate settings mod for each profile when tuning differs; any shared mod affects every profile where enabled. Archive exports require a `.zip` destination; reports require a `.json` destination outside the game directory.

Importing settings made from a different scan requires reviewing the equipment and clicking **Accept current scan**. A changed active file set detected before export requires a fresh session. Missing identities after plugin renaming or form compaction are reported.

User settings are separate from supplied presets and the base INI. A complete editor overlay registers its explicit equipment catalogue and suppresses older per-bow/ammo modifiers to prevent stacking. Without a user overlay, existing INI behavior remains available. A malformed overlay is rejected as a whole and the base configuration remains active; inspect `BowSpeedVR.log` for errors.

To remove editor settings, disable their separate settings mod. To remove all BowSpeedVR behavior, disable the runtime mod as well. No ESP patch is generated.

## Diagnostics and testing

The editor includes a headless scanner for reproducible reports:

```text
BowSpeedVREditor.exe --game-dir "C:\Games\SkyrimVR" --profile-dir "C:\MO2\profiles\My Profile" --scan-report "C:\Reports\bows.json"
```

Launch that command through MO2 for its modded Data view. Other diagnostic options include `--validate-overlay FILE`, `--runtime-version DLL`, `--export-zip FILE`, `--export-dir FOLDER`, `--import FILE`, `--preset FILE`, and `--accept-scan` after reviewing a changed list. Reports are local files; nothing is uploaded.

Active-session discovery was exercised against the local MGO setup. Synthetic fixtures cover override identity, disabled plugins, deletion, compressed/extended records, template cycles, light-plugin references, configuration round trips and source-file preservation. FUS remains a target for user acceptance testing; this is not a claim that every FUS profile or native-plugin combination has been tested. In-game flight, impacts and optional inventory-weight acceptance still require VR playtesting.
