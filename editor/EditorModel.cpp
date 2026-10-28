#include "EditorModel.h"
#include "Inference.h"
#include "EquipmentSafety.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/Tuning.h"
#include <cmath>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#include <winver.h>
#endif
namespace BowSpeed::Editor {
namespace {
void Range(double n, double lo, double hi) {
    if (!std::isfinite(n) || n < lo || n > hi)
        throw std::runtime_error("Value is outside the supported range");
}
void Allowed(const EditableItem &i) {
    if (!i.equipment.ordinary || i.gravityException)
        throw std::runtime_error("This item is excluded: " + (i.gravityException
                                                                  ? std::string("gravity exception")
                                                                  : i.equipment.reason));
}
} // namespace
EditorModel::EditorModel(ScanResult s, Config c) : scan(std::move(s)), settings(std::move(c)) {
    const auto cat = ResolvePhysicsCatalog(settings);
    for (const auto &[key, e] : scan.equipment) {
        EditableItem i;
        i.equipment = e;
        if (const auto reason = ReviewedExclusionReason(key); !reason.empty()) {
            i.equipment.ordinary = false;
            i.equipment.reason = reason;
            i.explicitlyExcluded = true;
            items.emplace(key, std::move(i));
            continue;
        }
        i.gravityException = ReviewedGravityException(key);
        i.explicitlyExcluded = settings.excludedForms.contains(key);
        if (auto b = cat.bows.find(key); e.kind == ItemKind::Bow && b != cat.bows.end()) {
            i.power = b->second.power;
            i.profile = b->second.name;
            i.enabled = e.ordinary;
        }
        if (auto a = cat.arrows.find(key); e.kind == ItemKind::Arrow && a != cat.arrows.end()) {
            i.mass = a->second.mass;
            i.capacity = a->second.capacity;
            i.profile = a->second.name;
            i.gravityException = a->second.gravityException || ReviewedGravityException(key);
            i.enabled = e.ordinary && !i.gravityException;
        }
        items.emplace(key, std::move(i));
    }
    std::size_t inferred = 0;
    for (const auto &[key, p] : InferDefaults(scan, cat)) {
        auto &i = items.at(key);
        if (i.profile != "Unassigned" || i.explicitlyExcluded || settings.replaceCatalog)
            continue;
        i.power = p.power;
        i.mass = p.mass;
        i.capacity = p.capacity;
        i.profile = "Inferred: " + p.method;
        i.inferenceNote = p.note;
        i.enabled = true;
        ++inferred;
    }
    if (inferred)
        notices.push_back(
            std::to_string(inferred) +
            " inferred defaults enabled in this draft; filter Inferred to review or override them.");
}
void EditorModel::SetBow(const FormKey &k, double power) {
    Range(power, .1, 8);
    auto &i = items.at(k);
    Allowed(i);
    if (i.equipment.kind != ItemKind::Bow)
        throw std::runtime_error("Select a bow");
    i.power = power;
    i.explicitlyExcluded = false;
    i.profile = "Custom";
    i.inferenceNote.clear();
    i.enabled = true;
}
void EditorModel::SetArrow(const FormKey &k, double mass, double capacity) {
    Range(mass, .1, 8);
    Range(capacity, .1, 20);
    auto &i = items.at(k);
    Allowed(i);
    if (i.equipment.kind != ItemKind::Arrow)
        throw std::runtime_error("Select an arrow");
    i.mass = mass;
    i.explicitlyExcluded = false;
    i.capacity = capacity;
    i.profile = "Custom";
    i.inferenceNote.clear();
    i.enabled = true;
}
void EditorModel::SetEnabled(const FormKey &k, bool enable) {
    auto &i = items.at(k);
    if (enable) {
        Allowed(i);
        if (i.profile == "Unassigned")
            throw std::runtime_error("Assign a profile or explicit values first");
    }
    i.enabled = enable;
    i.explicitlyExcluded = !enable;
}
void EditorModel::AssignProfile(const FormKey &k, std::string_view name) {
    auto &i = items.at(k);
    if (i.equipment.kind == ItemKind::Bow) {
        auto p = FindBowPhysicsProfile(name);
        if (!p)
            throw std::runtime_error("Unknown bow profile");
        SetBow(k, p->power);
    } else {
        auto p = FindArrowPhysicsProfile(name);
        if (!p)
            throw std::runtime_error("Unknown arrow profile");
        SetArrow(k, p->mass, p->capacityQ);
    }
    i.profile = name;
}
void EditorModel::Import(const UserConfig &user, bool preset) {
    if (!user.valid)
        throw std::runtime_error(user.errors.empty() ? "Invalid user configuration" : user.errors.front());
    auto next = ApplyUserConfig(BuildConfig(), user);
    auto cat = ResolvePhysicsCatalog(next);
    notices.clear();
    if (user.values.replaceCatalog)
        for (auto &[key, i] : items) {
            i.enabled = false;
            i.explicitlyExcluded = true;
        }
    for (auto &[key, i] : items) {
        if (auto b = cat.bows.find(key); i.equipment.kind == ItemKind::Bow && b != cat.bows.end()) {
            i.power = b->second.power;
            i.explicitlyExcluded = false;
            if (user.values.registeredBows.contains(key)) {
                auto note = user.inferenceNotes.find(key);
                i.inferenceNote = note == user.inferenceNotes.end() ? "" : note->second;
                i.profile = i.inferenceNote.empty() ? (preset ? "Preset" : "Imported") : "Inferred (saved)";
            }
            i.enabled = i.equipment.ordinary;
        }
        if (auto a = cat.arrows.find(key); i.equipment.kind == ItemKind::Arrow && a != cat.arrows.end()) {
            i.mass = a->second.mass;
            i.explicitlyExcluded = false;
            i.capacity = a->second.capacity;
            i.gravityException = a->second.gravityException || ReviewedGravityException(key);
            if (user.values.registeredArrows.contains(key)) {
                auto note = user.inferenceNotes.find(key);
                i.inferenceNote = note == user.inferenceNotes.end() ? "" : note->second;
                i.profile = i.inferenceNote.empty() ? (preset ? "Preset" : "Imported") : "Inferred (saved)";
            }
            i.enabled = i.equipment.ordinary && !i.gravityException;
        }
        if (next.excludedForms.contains(key)) {
            i.enabled = false;
            i.explicitlyExcluded = true;
        }
    }
    std::size_t missing = 0;
    for (const auto &[k, p] : user.values.registeredBows)
        if (!items.contains(k))
            ++missing;
    for (const auto &[k, p] : user.values.registeredArrows)
        if (!items.contains(k))
            ++missing;
    if (missing)
        notices.push_back(std::to_string(missing) +
                          " preset/configuration entries are absent from this active list");
    needsReview =
        needsReview || (!preset && !user.fingerprint.empty() && user.fingerprint != scan.fingerprint);
    settings = std::move(next);
}
Config EditorModel::BuildConfig() const {
    auto c = settings;
    c.replaceCatalog = true;
    c.registeredBows.clear();
    c.registeredArrows.clear();
    c.excludedForms.clear();
    for (const auto &[key, i] : items) {
        if (!i.enabled) {
            if (i.gravityException && !i.explicitlyExcluded && i.equipment.ordinary &&
                i.profile != "Unassigned") {
                c.registeredArrows[key] = {i.mass, i.capacity, true};
                continue;
            }
            c.excludedForms.insert(key);
            continue;
        }
        Allowed(i);
        if (i.equipment.kind == ItemKind::Bow)
            c.registeredBows[key] = i.power;
        else if (i.equipment.kind == ItemKind::Arrow)
            c.registeredArrows[key] = {i.mass, i.capacity, i.gravityException};
    }
    return c;
}
std::string EditorModel::Overlay() const {
    if (!scan.complete)
        throw std::runtime_error("Complete a valid active-profile scan before exporting");
    if (needsReview)
        throw std::runtime_error("Review and accept the changed active scan before exporting");
    auto text = SerializeUserConfig(BuildConfig(), scan.fingerprint);
    for (const auto &[key, item] : items) {
        if (item.inferenceNote.empty())
            continue;
        auto note = item.inferenceNote;
        for (auto &c : note)
            if (static_cast<unsigned char>(c) < 32)
                c = ' ';
        note.resize(std::min<std::size_t>(note.size(), 512));
        text += ";@inferred " + FormatFormKey(key) + "\t" + note + "\n";
    }
    auto parsed = ParseUserConfig(text);
    if (!parsed.valid)
        throw std::runtime_error(parsed.errors.front());
    return text;
}
std::optional<MatchedShot> EditorModel::Preview(const FormKey &b, const FormKey &a, double draw) const {
    if (!settings.enabled || settings.observeOnly || !settings.pairingEnabled)
        return {};
    auto bow = items.find(b), arrow = items.find(a);
    if (bow == items.end() || arrow == items.end() || !bow->second.enabled || !arrow->second.enabled ||
        bow->second.equipment.kind != ItemKind::Bow || arrow->second.equipment.kind != ItemKind::Arrow)
        return {};
    auto ref = ResolveReferenceSettings(settings);
    if (!ref)
        return {};
    auto shot = ComputeMatchedShot(*ref, bow->second.power, arrow->second.mass, arrow->second.capacity, draw);
    if (shot && !settings.pairingMomentumImpact)
        shot->forceRatio = 1;
    return shot;
}
std::string MgoPreset() {
    auto c = ParseConfig("[General]\nObserveOnly=false\n[Pairing]\nEnabled=true\n[Compatibility]"
                         "\nMGOProfiles=true\n[SpeedTuning]\nCompensateRicochet=true\n");
    auto cat = ResolvePhysicsCatalog(c);
    c.replaceCatalog = true;
    c.mgoProfiles = false;
    for (const auto &[k, b] : cat.bows)
        c.registeredBows[k] = b.power;
    for (const auto &[k, a] : cat.arrows)
        c.registeredArrows[k] = {a.mass, a.capacity, a.gravityException};
    return SerializeUserConfig(c);
}
std::optional<RuntimeVersion> ReadRuntimeVersion(const std::filesystem::path &path) {
#ifdef _WIN32
    DWORD handle = 0;
    auto size = GetFileVersionInfoSizeW(path.c_str(), &handle);
    if (!size || size > 1024 * 1024)
        return {};
    std::vector<unsigned char> data(size);
    if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data()))
        return {};
    VS_FIXEDFILEINFO *info = nullptr;
    UINT bytes = 0;
    if (!VerQueryValueW(data.data(), L"\\", reinterpret_cast<void **>(&info), &bytes) ||
        bytes < sizeof(*info) || info->dwSignature != 0xFEEF04BD)
        return {};
    return RuntimeVersion{HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
                          HIWORD(info->dwFileVersionLS)};
#else
    (void)path;
    return {};
#endif
}
} // namespace BowSpeed::Editor
