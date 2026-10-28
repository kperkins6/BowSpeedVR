#include "Check.h"
#include "EditorModel.h"
#include "Export.h"
#include "BowSpeed/InventoryWeight.h"
#include <fstream>
#include <iostream>
int main() {
    try {
        using namespace BowSpeed;
        using namespace BowSpeed::Editor;
        ScanResult scan;
        scan.complete = true;
        scan.fingerprint = "test-one";
        const auto bow = CanonicalFormKey("Skyrim.esm", 0x3B562),
                   arrow = CanonicalFormKey("Skyrim.esm", 0x1397D),
                   custom = CanonicalFormKey("MyMod.esp", 0x800);
        scan.equipment[bow] = {bow, ItemKind::Bow, "Long Bow", "LongBow", "Skyrim.esm", "", {}, {}, 1, 0, 0,
                               7,   true,          false};
        scan.equipment[arrow] = {
            arrow, ItemKind::Arrow, "Iron Arrow", "IronArrow", "Skyrim.esm", "", {}, {}, 3600, 1, 1, 8, true,
            false};
        scan.equipment[custom] = {
            custom, ItemKind::Bow, "Custom Bow", "CustomBow", "MyMod.esp", "", {}, {}, 1, 0, 0,
            7,      true,          false};
        EditorModel model(scan);
        Check(model.items.size() == 3, "Discovered equipment appears in editor");
        Check(!model.items.at(custom).enabled, "Unknown bow remains unchanged until assigned");
        auto shot = model.Preview(bow, arrow, 1);
        Check(shot && Near(static_cast<float>(shot->launchSpeed), 5000),
              "Shared default preview matches 5000 anchor");
    model.settings.pairingEnabled = false;
    Check(!model.Preview(bow, arrow, 1), "Disabled player physics does not show an active pairing preview");
    model.settings.pairingEnabled = true;
        shot = model.Preview(bow, arrow, .5);
        Check(shot && Near(static_cast<float>(shot->launchSpeed), 2500),
              "Partial draw uses shared energy calculation");
        model.AssignProfile(custom, "Daedric Bow");
        Check(model.items.at(custom).enabled, "Assigning an archetype enables ordinary new bow");
        model.SetBow(custom, 2);
        model.SetArrow(arrow, 2, 2);
        shot = model.Preview(custom, arrow, 1);
        Check(shot && shot->capacityLimited, "Capacity cap is exposed in preview");
    model.Import(ParseUserConfig("[UserConfig]\nSchemaVersion=1\n[Pairing]\nImpactStrength=0.5\n"));
    Check(model.items.at(arrow).mass==2 && model.items.at(custom).power==2,
          "Importing partial global settings preserves edited equipment");
        model.settings.pairingSettings.impactStrength = .5;
        shot = model.Preview(bow, arrow, 1);
        Check(shot && shot->forceRatio <= .775, "Preview applies bounded physical impact strength");
        auto user = ParseUserConfig(model.Overlay());
        Check(user.valid, "Export parses through actual runtime parser");
        auto resolved = ResolvePhysicsCatalog(ApplyUserConfig(ParseConfig(""), user));
        Check(resolved.bows.at(custom).power == 2, "Registered unknown bow round-trips to runtime");
        model.SetEnabled(arrow, false);
        Check(!model.Preview(bow, arrow, 1), "Excluded combination cannot preview paired behavior");
        user = ParseUserConfig(model.Overlay());
        Check(!ResolvePhysicsCatalog(ApplyUserConfig(ParseConfig(""), user)).arrows.contains(arrow),
              "Export exclusion suppresses stock pairing");
        auto stale = user;
        stale.fingerprint = "old-list";
        model.Import(stale);
        Check(model.needsReview, "Changed active scan requires explicit review");
        auto root = std::filesystem::temp_directory_path() /
                    std::filesystem::path(
                        "bowspeed-export-" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root);
        bool blocked = false;
        try {
            ExportSettings(model, root / "settings.zip");
        } catch (...) {
            blocked = true;
        }
        Check(blocked, "Stale review prevents misleading exports");
        model.needsReview = false;
        ExportSettings(model, root / "settings.zip");
        auto bytes = ReadText(root / "settings.zip");
        Check(bytes.starts_with("PK\x03\x04"), "Installable ZIP is generated");
        ExportSettings(model, root / "output", false);
        auto path = root / "output/SKSE/Plugins/BowSpeedVR.user.ini";
        auto old = ReadText(path);
        model.SetEnabled(arrow, true);
        ExportSettings(model, root / "output", false);
        Check(ReadText(path.string() + ".bak") == old, "Repeat exports back up the previous overlay");
        model.scan.dataDir = root / "Data";
        std::filesystem::create_directories(model.scan.dataDir);
        blocked = false;
        try {
            ExportSettings(model, model.scan.dataDir / "bad", false);
        } catch (...) {
            blocked = true;
        }
        Check(blocked, "Game Data cannot be an output mod");
        Check(ParseUserConfig(MgoPreset()).valid, "Optional MGO preset is data accepted by runtime");
    auto gravityScan=scan;
    const auto gravityArrow=CanonicalFormKey("dis_heavy_legion.esp",0x898);
    gravityScan.equipment[gravityArrow]={gravityArrow,ItemKind::Arrow,"Imperial Arrow","ImperialArrow","dis_heavy_legion.esp","",{},{},5000,.981,1,10,true,false};
    EditorModel gravityModel(gravityScan,ParseConfig("[General]\nObserveOnly=false\n[Compatibility]\nMGOProfiles=true\n[Immersion]\nArrowInventoryWeight=true\n"));
    Check(!gravityModel.items.at(gravityArrow).enabled && BuildInventoryWeightProfiles(gravityModel.BuildConfig()).contains(gravityArrow),
          "Reviewed gravity exceptions retain optional inventory mass while flight stays native");
        Check(!RuntimeVersion{1, 0, 0}.compatible() && RuntimeVersion{1, 1, 0}.compatible(),
              "Old runtime is never marked overlay-compatible");
        std::filesystem::remove_all(root);
        std::cout << "Editor model tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
