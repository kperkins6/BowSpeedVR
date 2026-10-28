# Hybrid equipment defaults implementation plan

> **For agentic workers:** Use superpowers:executing-plans inline with test-first verification. The user approved the hybrid design in chat and enabled inferred defaults. Continue in the dedicated desktop-editor checkout; no new approval or worktree is needed.

**Goal:** Enable unfamiliar ordinary equipment with explainable, overridable defaults while preserving reviewed profiles and explicit saved settings.

**Architecture:** Extend scanner evidence with resolved damage/material keywords. An editor-only inference module derives defaults from ordinary canonical vanilla/DLC reference equipment in the active scan. Template inheritance wins, then unambiguous material evidence, then a monotone bounded damage-to-tier curve. Export inference notes as backward-compatible INI comments.

**Tech stack:** Existing C++23 native Windows editor and shared physics model.

**Spec:** Approved chat: automatically enable inferred defaults, clearly label inferred values, allow overrides; template/material/damage hybrid; matching full-draw mass = 1.25 * bow energy - 0.25; keep known profiles fixed.

## Constraints and review focus

- Do not auto-enroll crossbows, scripted/special equipment, gravity exceptions, or explicit exclusions.
- Use current winning damage of a fixed representative base-equipment set, rather than hardcoded vanilla damage or enchantment duplicates. Never train from newly inferred/custom equipment.
- Clamp damage estimates to Long/Daedric energy and Iron/Daedric mass bounds. Nonpositive/nonfinite damage cannot support damage inference. A lone anchor supports a clearly labeled nearest fallback; no evidence leaves an item unchanged.
- Group tied damage and use weighted isotonic regression for tier, preventing reversed/rebalanced anchor damage from producing a decreasing curve. Capacity is interpolated independently within observed bounds.
- Preserve manual values and exclusions on import/reopen. Old runtimes must ignore provenance comments and still accept exported schema 1.
- Installation updates only the editor, after backup and verification. Preserve live runtime/INI/Ricochet and do not install an inferred overlay automatically.

### Task 1: Scanner evidence

Files: editor/PluginScan.h/.cpp, tests/PluginScanTests.cpp.

- [x] Write failing binary fixtures for KWDA identity resolution through masters/overrides, template-resolved damage/keywords, and malformed keyword arrays.
- [x] Add Equipment.materialKeywords; resolve keyword EditorIDs after winning records load, inherit absent child keywords from templates, and report resolved damage. Propagate template scripts into exclusion checks.
- [x] Run scanner and full native suite; commit.

### Task 2: Inference and provenance

Files: editor/Inference.h/.cpp, EditorModel.h/.cpp, DesktopWindow.cpp, include/BowSpeed/UserConfig.h, src/UserConfig.cpp, CMakeLists.txt, tests/InferenceTests.cpp, EditorModelTests.cpp, UserConfigTests.cpp.

Interfaces: InferDefaults(ScanResult, PhysicsCatalog) -> map<FormKey, InferredDefaults> with power/mass/capacity, method and explanation; EditableItem.inferenceNote retains user-visible provenance. UserConfig.inferenceNotes carries optional comment metadata only.

- [x] Tests first: template/material precedence, damage midpoint and clamping, same-tier 1.0 speed, active-list damage scaling, ties/inversions, ambiguity and invalid evidence, safe exclusions, manual override/export/reimport and source-label persistence.
- [x] Implement bounded curves using canonical representative IDs, explicit high/low confidence explanations, enabled-by-default ordinary inference and safe override precedence. Keep curated anchors untouched.
- [x] Show inferred labels in the table/filter and explanations alongside numeric controls; include comment provenance in exported diagnostic overlays.
- [x] Run full native/Python suite; commit.

### Task 3: Acceptance and delivery

Files: docs/EDITOR.md, packaging/version metadata as needed; private workspace evidence.

- [x] Document inference limits and export workflow. Build/package actual binaries; obtain one independent final code review and fix material findings with regression tests.
- [x] Run full native/Python suite and active MGO scan; review inference distribution and sample mappings, verifying no excluded equipment becomes enabled.
- [x] Back up installed editor, update it in MO2, verify live runtime detection/scanning, and close it for playtesting. Live runtime tuning remains unchanged. Commit final source changes locally; no public push.

Final evidence: native 13/13, Python 11/11, deterministic catalogue check, actual package validation, and read-only active MGO winning-record scan (2012 plugins, 1278 equipment). Five additional ordinary items receive inferred defaults after preserving the complete prior exclusion policy. The MO2-launched UI was inspected for enabled labels, Inferred filtering, details/controls and runtime compatibility; the final safety-table contents were rechecked via the scanner/model probe. MO2 and the editor are closed at completion. Installed editor matches final 1.1.1 package; runtime 1.1.0, base INI and Ricochet are unchanged. No inferred settings mod was automatically installed. Physical/gameplay acceptance and FUS testing remain user playtests.

Self-review: scanner, model, import and UI interfaces are all covered. Deliberate new behavior is tested at the model/runtime-parser boundary; keyword extraction has real binary fixtures. Current native runtime already consumes numeric registrations, so inference requires no new in-game hook or schema version.
