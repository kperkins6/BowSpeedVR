#include "Check.h"
#include "EditorModel.h"
#include "Inference.h"
#include <iostream>
#include <limits>
using namespace BowSpeed;
using namespace BowSpeed::Editor;
const auto longBow = CanonicalFormKey("Skyrim.esm", 0x3B562);
const auto daedricBow = CanonicalFormKey("Skyrim.esm", 0x139B5);
const auto iron = CanonicalFormKey("Skyrim.esm", 0x1397D);
const auto daedric = CanonicalFormKey("Skyrim.esm", 0x139C0);
const auto newBow = CanonicalFormKey("New.esp", 0x800);
const auto newArrow = CanonicalFormKey("New.esp", 0x801);
Equipment Item(FormKey key, ItemKind kind, double damage, std::string material = {}) {
    Equipment e;
    e.key = key;
    e.kind = kind;
    e.damage = damage;
    e.ordinary = true;
    e.name = FormatFormKey(key);
    e.nativeSpeed = kind == ItemKind::Bow ? 1 : 3600;
    if (!material.empty())
        e.materialKeywords.push_back(material);
    return e;
}
ScanResult Fixture() {
    ScanResult s;
    s.complete = true;
    s.fingerprint = "inference-test";
    s.equipment[longBow] = Item(longBow, ItemKind::Bow, 10, "WeapMaterialWood");
    s.equipment[daedricBow] = Item(daedricBow, ItemKind::Bow, 20, "WeapMaterialDaedric");
    s.equipment[iron] = Item(iron, ItemKind::Arrow, 8, "WeapMaterialIron");
    s.equipment[daedric] = Item(daedric, ItemKind::Arrow, 18, "WeapMaterialDaedric");
    s.equipment[newBow] = Item(newBow, ItemKind::Bow, 15);
    s.equipment[newArrow] = Item(newArrow, ItemKind::Arrow, 13);
    return s;
}
int main() {
    try {
        auto known = ResolvePhysicsCatalog(ParseConfig(""));
        auto s = Fixture();
        auto estimates = InferDefaults(s, known);
        Check(estimates.contains(newBow) && estimates.contains(newArrow),
              "Ordinary unfamiliar bows and arrows receive inferred defaults");
        Check(Near(static_cast<float>(estimates.at(newBow).power), 1.52040816f) &&
                  Near(static_cast<float>(estimates.at(newArrow).mass), 1.65051020f),
              "Damage midpoints interpolate calibrated tier instead of snapping to a neighbor");
        EditorModel model(s);
        Check(model.items.at(newBow).enabled && model.items.at(newArrow).enabled,
              "Inferred ordinary equipment starts enabled");
        auto preview = model.Preview(newBow, newArrow, 1);
        Check(preview && Near(static_cast<float>(preview->launchSpeed), 5000),
              "Same inferred tier gives a 1.0 matched full-draw pair");
        Check(model.items.at(newBow).profile.starts_with("Inferred") &&
                  !model.items.at(newBow).inferenceNote.empty(),
              "Automatically enabled values carry visible provenance");
        Check(model.items.at(longBow).power == known.bows.at(longBow).power &&
                  model.items.at(iron).mass == known.arrows.at(iron).mass,
              "Curated profiles remain unchanged");
        auto scaled = s;
        for (auto &[key, item] : scaled.equipment)
            item.damage *= 10;
        Check(Near(static_cast<float>(InferDefaults(scaled, known).at(newBow).power), 1.52040816f),
              "A mod-list-wide damage rescale does not rescale physical energy");
        s.equipment[newBow].damage = 999;
        s.equipment[newArrow].damage = .1;
        estimates = InferDefaults(s, known);
        Check(Near(static_cast<float>(estimates.at(newBow).power), 2.04081633f) &&
                  Near(static_cast<float>(estimates.at(newArrow).mass), 1),
              "Outliers clamp at calibrated bounds instead of extrapolating lasers");
        s.equipment[newBow].materialKeywords = {"weapmaterialWOOD"};
        estimates = InferDefaults(s, known);
        Check(Near(static_cast<float>(estimates.at(newBow).power), 1) &&
                  estimates.at(newBow).method == "material",
              "Material evidence takes precedence over misleading damage");
        s.equipment[newBow].weaponTemplate = daedricBow;
        estimates = InferDefaults(s, known);
        Check(estimates.at(newBow).method == "template" &&
                  Near(static_cast<float>(estimates.at(newBow).power), 2.04081633f),
              "Known template takes precedence over material and damage");
        s = Fixture();
        s.equipment[newBow].materialKeywords = {"WeapMaterialWood", "WeapMaterialDaedric"};
        Check(InferDefaults(s, known).at(newBow).method == "damage",
              "Conflicting material evidence falls back to a labeled damage estimate");
        s.equipment[daedricBow].materialKeywords = {"WeapMaterialWood"};
        s.equipment[newBow].materialKeywords = {"WeapMaterialWood"};
        Check(InferDefaults(s, known).at(newBow).method == "damage",
              "A material shared by unlike reference bows is not treated as an exact match");
        s.equipment[daedricBow].damage = 10;
        s.equipment[newBow].damage = 10;
        Check(Near(static_cast<float>(InferDefaults(s, known).at(newBow).power), 1.52040816f),
              "Tied anchor damage is averaged deterministically");
        s.equipment[daedricBow].damage = 5;
        s.equipment[newBow].damage = 7;
        Check(Near(static_cast<float>(InferDefaults(s, known).at(newBow).power), 1.52040816f),
              "Reversed damage anchors produce a bounded monotone fit");
        s = Fixture();
        s.equipment.erase(daedricBow);
        Check(InferDefaults(s, known).at(newBow).method == "nearest",
              "A lone usable anchor gets a clearly identified sparse fallback");
        s.equipment.erase(longBow);
        Check(!InferDefaults(s, known).contains(newBow), "No bow evidence leaves equipment unchanged");
        s = Fixture();
        s.equipment[newBow].damage = std::numeric_limits<double>::quiet_NaN();
        s.equipment[newArrow].damage = 0;
        Check(InferDefaults(s, known).empty(), "Unusable damage alone cannot enroll equipment");
        s = Fixture();
        s.equipment[newBow].ordinary = false;
        s.equipment[newArrow].kind = ItemKind::Crossbow;
        Check(InferDefaults(s, known).empty(), "Special equipment and crossbows are never inferred");
        model = EditorModel(Fixture());
        auto overlay = model.Overlay();
        auto parsed = ParseUserConfig(overlay);
        Check(parsed.valid && parsed.inferenceNotes.contains(newBow),
              "Runtime-compatible overlay retains inference explanations");
        EditorModel restored(Fixture());
        restored.Import(parsed);
        Check(restored.items.at(newBow).profile.starts_with("Inferred") &&
                  restored.items.at(newBow).inferenceNote == model.items.at(newBow).inferenceNote,
              "Saved inference remains labeled after reopening");
        restored.Import(ParseUserConfig("[UserConfig]\nSchemaVersion=1\n[Pairing]\nImpactStrength=0.5\n"));
        Check(restored.items.at(newBow).profile.starts_with("Inferred") &&
                  !restored.items.at(newBow).inferenceNote.empty(),
              "A partial global import does not erase inference provenance");
        restored.SetBow(newBow, 2.7);
        Check(restored.items.at(newBow).inferenceNote.empty(),
              "Manual numeric overrides replace inference provenance");
        restored.SetEnabled(newArrow, false);
        auto manual = ParseUserConfig(restored.Overlay());
        EditorModel reopened(Fixture());
        reopened.Import(manual);
        Check(reopened.items.at(newBow).power == 2.7 && !reopened.items.at(newArrow).enabled,
              "Reopening preserves manual values and explicit exclusions");
        auto excluded = ParseConfig("");
        excluded.excludedForms.insert(newBow);
        Check(!EditorModel(Fixture(), excluded).items.at(newBow).enabled,
              "Explicit saved exclusions win over automatic inference");
        s = Fixture();
        s.equipment[newBow].scripted = true;
        Check(!InferDefaults(s, known).contains(newBow), "Scripted items cannot bypass inference safety");
        s = Fixture();
        const std::vector<FormKey> exceptions{CanonicalFormKey("elvenia armor.esp", 0x816),
                                              CanonicalFormKey("dis_heavy_legion.esp", 0x898),
                                              CanonicalFormKey("legacyofthedragonborn.esm", 0x236D52)};
        for (const auto &key : exceptions) {
            s.equipment[key] = Item(key, ItemKind::Arrow, 20);
            s.equipment[key].gravity = .2;
        }
        EditorModel noPreset(s);
        auto exported = noPreset.BuildConfig();
        for (const auto &key : exceptions) {
            Check(!noPreset.items.at(key).enabled && noPreset.items.at(key).gravityException,
                  "Reviewed gravity exceptions remain excluded with MGO preset disabled");
            Check(!exported.registeredArrows.contains(key) ||
                      exported.registeredArrows.at(key).gravityException,
                  "Default-scope export cannot turn a gravity exception into an ordinary registration");
        }
        s = Fixture();
        const std::vector<FormKey> helpers{
            CanonicalFormKey("crossbowreloadvr.esp", 0x800), CanonicalFormKey("spellwheelvr.esp", 0x807),
            CanonicalFormKey("spellwheelvr.esp", 0x808),     CanonicalFormKey("spellwheelvr.esp", 0x17D56),
            CanonicalFormKey("vrik.esp", 0x12AB1),           CanonicalFormKey("weaponthrowvr.esp", 0xDA1)};
        for (const auto &key : helpers)
            s.equipment[key] = Item(key, ItemKind::Bow, 10);
        EditorModel helpersModel(s);
        for (const auto &key : helpers) {
            Check(!helpersModel.items.at(key).enabled && !helpersModel.items.at(key).equipment.ordinary,
                  "Reviewed VR interaction launchers never receive automatic archery tuning");
            Check(!helpersModel.BuildConfig().registeredBows.contains(key),
                  "VR helper exclusions survive runtime overlay export");
        }
        const auto developmentArrow = CanonicalFormKey("archeryv2.esp", 0x19E0);
        s.equipment[developmentArrow] = Item(developmentArrow, ItemKind::Arrow, 15);
        Check(!EditorModel(s).items.at(developmentArrow).enabled,
              "Reviewed development arrows stay outside automatically inferred equipment");
        std::cout << "Hybrid inference tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
