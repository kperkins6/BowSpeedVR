# Desktop Editor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Ship an offline MO2-launched Windows editor and matching generic BowSpeedVR runtime, with MGO as an optional preset.

**Architecture:** A bounded TES plugin scanner resolves active records; an editor session combines discovered equipment with data presets and a versioned user overlay. Both the desktop preview and SKSEVR plugin share bow_speed_core. The editor exports a separate settings mod.

**Tech Stack:** C++23, CMake/MSVC, Windows common controls; vendored zlib puff decoder for compressed records, ZIP store writer for settings exports.

**Spec:** ../specs/2026-10-28-desktop-editor-design.md

## Global Constraints

- Windows x64; Skyrim VR 1.4.15, SKSEVR and VR Address Library for the runtime.
- Offline editor, read-only source scan, source plugin/INI files never rewritten.
- No MGO/Archery/Ricochet requirement. MGO data is explicitly optional.
- Stable originating plugin/local FormID identities; no permanent load-order prefixes.
- Preserve installed gravity, damage, draw timing and special-projectile exclusions.
- New user overlay must be atomic on validation failure; old INIs remain supported.
- No live-game deployment, public push or publication in this implementation.

## Review Focus

- Missing/ambiguous projected load order must block complete exports rather than quietly scanning all plugins (Task 2).
- A patch overriding a master's object retains the master's canonical identity, including light plugin references (Task 2).
- Unknown or special equipment is detected without silently changing its flight (Tasks 1/3).
- Source-mod writes through MO2 must never occur; explicit output files are backed up and stale sessions diagnosed (Task 3).
- A desktop preview must equal the runtime's resolved parameters and bounded impact response (Tasks 1/3).

### Task 1: Generic runtime overlay and catalogue resolution

**Files:** include/BowSpeed/UserConfig.h, src/UserConfig.cpp, src/Plugin.cpp, include/BowSpeed/InventoryWeight.h, include/BowSpeed/MatchedPhysics.h, src/MatchedPhysics.cpp, src/MatchedRuntime.cpp, tests/UserConfigTests.cpp, CMakeLists.txt.

**Interfaces:** ParseUserConfig(string_view) -> UserConfig (valid/errors plus typed Config and binding maps); ApplyUserConfig(Config, UserConfig) -> Config; ResolvePhysicsCatalog(Config) -> owned bow/arrow maps of canonical FormKey to physics values/exclusion. New Config catalogue maps are independent of installed form pointers. MatchedSettings gains impactStrength=1.0. Existing registrations remain when no overlay exists.

- [ ] Write failing tests: arbitrary NewBow.esp|0x800 registers power 1.5; new ammo mass 2/capacity 2 registers; an invalid number rejects the whole overlay; exclusion suppresses pairing and weight; legacy no-overlay defaults match. Impact strength 0.5 scales the bounded response, strength 1 preserves old values.
- [ ] Run new test target and observe missing behavior; implement strict schema 1 overlay and shared catalogue selection, then runtime type/projectile checks.
- [ ] Run all CTest groups; commit runtime support.

### Task 2: Active-session plugin scanner

**Files:** editor/PluginScan.h/.cpp, editor/SessionPaths.h/.cpp, editor/vendor/puff.c/.h and license, tests/PluginScanTests.cpp, tests/fixtures generator, CMakeLists.txt.

**Interfaces:** ScanPlugins(dataDir, pluginsFile, optional orderFile) -> ScanResult containing equipment records, diagnostics, complete flag and fingerprint; DetectSession(gameDir, optional profileDir) -> explicit source paths. ScanResult items retain kind, key, winning file, template, projectile and native data; generated fixture helpers remain test-only.

- [ ] Write generated binary fixtures and failing checks for active-only equipment, override identity/winner, deleted forms, template cycle, master order errors, FE/light mapping, XXXX/compressed data, invalid lengths and missing projected lists.
- [ ] Implement streaming/bounded plugin parsing, canonical references, resolution and ordinary-equipment classification. Vendor pinned puff only for raw DEFLATE inside validated zlib records.
- [ ] Run scanner tests and an explicit-profile read-only MGO scan; retain scan evidence outside source. Commit scanner.

### Task 3: Editor model, presets, preview and export

**Files:** editor/EditorModel.h/.cpp, editor/Export.h/.cpp, tests/EditorModelTests.cpp, presets/MGO.ini, include/BowSpeed/UserConfig.h and shared serializer helpers.

**Interfaces:** EditorModel(ScanResult, base Config); ApplyPreset(UserConfig), ImportOverlay(UserConfig), Edit item settings, Preview(bowKey, ammoKey, draw) -> shared MatchedShot; SerializeOverlay() -> text; ExportSettings(model, destination, ZIP/directory) -> file list and diagnostics.

- [ ] Write failing round-trip tests for unknown-item opt-in, exclusions, preset resolution without MGO, exact Long/Iron 5000 preview, partial draw, cap indication, bounded impact, wrong runtime warning and source preservation.
- [ ] Implement stock defaults plus template inheritance; special items stay excluded. Move MGO bindings to a supplied data preset while retaining the old INI compatibility path.
- [ ] Implement full overlay import/export and installable ZIP; directory export only to an explicitly selected physical settings-mod path, backups and atomic file replacement; no deletion of other files.
- [ ] Run model/export/whole CTest suite; commit.

### Task 4: Desktop interface and diagnostic CLI

**Files:** editor/Main.cpp, editor/DesktopWindow.h/.cpp, editor/BowSpeedVREditor.manifest, editor/BowSpeedVREditor.rc, CMakeLists.txt, tests/editor_cli tests.

**Interfaces:** BowSpeedVREditor.exe [--game-dir PATH] [--profile-dir PATH] [--output PATH] [--scan-report PATH]; CLI scan-report exits nonzero for incomplete scans and never edits game data. GUI uses the same model/export code.

- [ ] Add CLI integration tests with a temporary real fixture tree: enabled-only report, unsupported runtime, missing list, stable overlay round trip, export ZIP consumable by runtime parser.
- [ ] Build native desktop interface with game/profile selection, scan/rescan, filterable equipment tables, profile assignment and numeric editing, globals, bow/arrow/draw preview, import, explicit MGO preset and settings export. Report projected-list source and runtime-version compatibility; disable apply/export for blocking scan errors.
- [ ] Exercise built executable CLI and inspect GUI through available computer-use tooling; commit.

### Task 5: Packaging, documentation and whole-branch review

**Files:** scripts/package-editor.py, scripts/build-editor.ps1, docs/EDITOR.md, README.md, docs/BUILDING.md, THIRD-PARTY.md, tests/test_editor_package.py.

- [ ] Package editor, matching runtime DLL/default INI, optional MGO preset, licenses and instructions. Verify ZIP contents and deterministic manifest with actual binaries.
- [ ] Run all native tests, existing Python packaging tests, editor CLI/fixture acceptance and read-only current-profile scan. Record what requires MO2/FUS/in-game acceptance instead of claiming it passed.
- [ ] Request one fresh whole-branch code review, fix material findings with regression tests, rerun affected/full checks, commit final changes locally. Keep installed game and public release untouched.

## Execution notes

User explicitly approved implementation. Execute continuously in the existing dedicated source checkout and feature branch; do not ask for another plan approval or create another worktree. Maintain a private progress ledger at work/BowSpeedVR-editor-progress.md and commit logical deliverables. Review this plan against the spec before coding; interfaces above are the shared contracts.
