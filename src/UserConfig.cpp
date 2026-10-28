#include "BowSpeed/UserConfig.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/Tuning.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
namespace BowSpeed {
namespace {
std::string Lower(std::string text) {
    for (auto &c : text)
        if (c >= 'A' && c <= 'Z')
            c += ('a' - 'A');
    return text;
}
std::string Trim(std::string_view s) {
    auto a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? "" : std::string(s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1));
}
std::optional<double> Number(std::string_view s, double lo, double hi) {
    double n{};
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{} || end != s.data() + s.size() || !std::isfinite(n) || n < lo || n > hi)
        return {};
    return n;
}
const std::set<std::string> globalNames = {"general/enabled",
                                           "general/observeonly",
                                           "general/diagnosticlogging",
                                           "pairing/enabled",
                                           "pairing/referencepreset",
                                           "pairing/referencespeed",
                                           "pairing/playerspeedmult",
                                           "pairing/virtualbowmass",
                                           "pairing/maxspeedratio",
                                           "pairing/capacitylimits",
                                           "pairing/momentumimpact",
                                           "pairing/impactstrength",
                                           "speedtuning/npcenabled",
                                           "speedtuning/npcspeedmult",
                                           "speedtuning/compensatericochet",
                                           "immersion/arrowinventoryweight"};
void SetGlobal(Config &dst, const Config &src, const std::string &name) {
    if (name == "general/enabled")
        dst.enabled = src.enabled;
    else if (name == "general/observeonly")
        dst.observeOnly = src.observeOnly;
    else if (name == "general/diagnosticlogging")
        dst.diagnosticLogging = src.diagnosticLogging;
    else if (name == "pairing/enabled")
        dst.pairingEnabled = src.pairingEnabled;
    else if (name == "pairing/referencepreset")
        dst.referencePreset = src.referencePreset;
    else if (name == "pairing/referencespeed")
        dst.pairingSettings.referenceSpeed = src.pairingSettings.referenceSpeed;
    else if (name == "pairing/playerspeedmult")
        dst.pairedPlayerSpeedMult = src.pairedPlayerSpeedMult;
    else if (name == "pairing/virtualbowmass")
        dst.pairingSettings.virtualBowMass = src.pairingSettings.virtualBowMass;
    else if (name == "pairing/maxspeedratio")
        dst.pairingSettings.maxSpeedRatio = src.pairingSettings.maxSpeedRatio;
    else if (name == "pairing/capacitylimits")
        dst.pairingSettings.capacityLimits = src.pairingSettings.capacityLimits;
    else if (name == "pairing/momentumimpact")
        dst.pairingMomentumImpact = src.pairingMomentumImpact;
    else if (name == "pairing/impactstrength")
        dst.pairingSettings.impactStrength = src.pairingSettings.impactStrength;
    else if (name == "speedtuning/npcenabled")
        dst.npcSpeedEnabled = src.npcSpeedEnabled;
    else if (name == "speedtuning/npcspeedmult")
        dst.npcSpeedMult = src.npcSpeedMult;
    else if (name == "speedtuning/compensatericochet")
        dst.compensateRicochet = src.compensateRicochet;
    else if (name == "immersion/arrowinventoryweight")
        dst.arrowInventoryWeight = src.arrowInventoryWeight;
}
} // namespace
FormKey CanonicalFormKey(std::string_view plugin, std::uint32_t id) {
    return {Lower(std::string(plugin)), id};
}
std::optional<FormKey> ParseFormKey(std::string_view text) {
    auto bar = text.find('|');
    if (bar == text.npos)
        return {};
    auto p = Trim(text.substr(0, bar));
    auto s = Trim(text.substr(bar + 1));
    auto lower = Lower(p);
    if (p.empty() || p.find_first_of("\\/:*?<>|\";#\r\n") != p.npos ||
        !(lower.ends_with(".esm") || lower.ends_with(".esp") || lower.ends_with(".esl")) ||
        !(s.starts_with("0x") || s.starts_with("0X")))
        return {};
    std::uint32_t id{};
    auto [end, ec] = std::from_chars(s.data() + 2, s.data() + s.size(), id, 16);
    if (ec != std::errc{} || end != s.data() + s.size() || !id || id > 0xFFFFFF)
        return {};
    return CanonicalFormKey(p, id);
}
std::string FormatFormKey(const FormKey &key) {
    std::ostringstream out;
    out << CanonicalFormKey(key.plugin, key.localID).plugin << "|0x" << std::uppercase << std::hex
        << std::setw(6) << std::setfill('0') << key.localID;
    return out.str();
}
UserConfig ParseUserConfig(std::string_view text) {
    UserConfig out;
    if (text.size() > 4 * 1024 * 1024) {
        out.errors.push_back("User overlay exceeds 4 MiB");
        return out;
    }
    if (text.starts_with("\xEF\xBB\xBF"))
        text.remove_prefix(3);
    std::istringstream input{std::string(text)};
    std::string line, section;
    std::set<std::string> seen;
    bool schema = false;
    std::size_t lineno{};
    while (std::getline(input, line)) {
        ++lineno;
        const auto comment = Trim(line);
        constexpr std::string_view marker = ";@inferred ";
        if (comment.starts_with(marker)) {
            const auto tab = comment.find('\t', marker.size());
            if (tab != std::string::npos) {
                auto key = ParseFormKey(std::string_view(comment).substr(marker.size(), tab - marker.size()));
                auto note = Trim(std::string_view(comment).substr(tab + 1));
                if (key && !note.empty() && note.size() <= 512)
                    out.inferenceNotes[*key] = std::move(note);
            }
            continue;
        }
        auto t = Trim(std::string_view(line).substr(0, line.find_first_of(";#")));
        if (t.empty())
            continue;
        auto error = [&](const std::string &message) {
            if (out.errors.size() < 64)
                out.errors.push_back("Line " + std::to_string(lineno) + ": " + message);
        };
        if (t.front() == '[' && t.back() == ']') {
            section = Lower(Trim(std::string_view(t).substr(1, t.size() - 2)));
            continue;
        }
        auto equal = t.find('=');
        if (equal == t.npos) {
            error("Expected key=value");
            continue;
        }
        auto key = Trim(std::string_view(t).substr(0, equal)),
             value = Trim(std::string_view(t).substr(equal + 1));
        auto name = section + "/" + Lower(key);
        if (!seen.insert(name).second) {
            error("Duplicate setting or form identity");
            continue;
        }
        if (name == "userconfig/schemaversion") {
            schema = value == "1";
            if (!schema)
                error("Unsupported schema; expected 1");
            continue;
        }
        if (name == "userconfig/replacecatalog") {
            auto v = Lower(value);
            if (v != "true" && v != "false")
                error("ReplaceCatalog must be true or false");
            else
                out.values.replaceCatalog = v == "true";
            continue;
        }
        if (name == "editor/scanfingerprint") {
            if (value.find_first_of("\r\n[]") != value.npos)
                error("Invalid fingerprint");
            else
                out.fingerprint = value;
            continue;
        }
        if (globalNames.contains(name)) {
            auto setting = ParseConfig("[" + section + "]\n" + key + "=" + value + "\n");
            if (!setting.warnings.empty()) {
                error(setting.warnings.front());
                continue;
            }
            SetGlobal(out.values, setting, name);
            out.globals[name] = value;
            continue;
        }
        const auto form = ParseFormKey(key);
        if (!form) {
            error("Invalid plugin|local FormID");
            continue;
        }
        if (section == "catalogbow") {
            auto n = Number(value, .1, 8);
            if (!n)
                error("Bow power must be 0.1 to 8");
            else
                out.values.registeredBows[*form] = *n;
        } else if (section == "catalogarrow") {
            std::istringstream parts(value);
            std::string m, c, g, extra;
            std::getline(parts, m, ',');
            std::getline(parts, c, ',');
            std::getline(parts, g, ',');
            auto mass = Number(Trim(m), .1, 8), capacity = Number(Trim(c), .1, 20);
            auto gravity = Lower(Trim(g));
            if (!mass || !capacity || (gravity != "true" && gravity != "false") ||
                std::getline(parts, extra, ','))
                error("Arrow requires mass 0.1..8, capacity 0.1..20, gravity exception true/false");
            else
                out.values.registeredArrows[*form] = {*mass, *capacity, gravity == "true"};
        } else if (section == "exclude") {
            if (Lower(value) != "true")
                error("Exclusion must be true");
            else
                out.values.excludedForms.insert(*form);
        } else
            error("Unknown section or setting");
    }
    if (!schema)
        out.errors.push_back("Missing supported [UserConfig] SchemaVersion=1");
    out.valid = out.errors.empty();
    return out;
}
Config ApplyUserConfig(const Config &base, const UserConfig &user) {
    if (!user.valid)
        return base;
    auto result = base;
    if (user.values.replaceCatalog) {
        result.replaceCatalog = true;
        result.registeredBows.clear();
        result.registeredArrows.clear();
        result.excludedForms.clear();
        result.bows.clear();
        result.ammoClasses.clear();
        result.arrowPhysics = false;
        result.playerSpeedMult = 1;
        result.playerCrossbowSpeedMult = 1;
        result.mgoProfiles = false;
    }
    for (const auto &[name, value] : user.globals) {
        (void)value;
        SetGlobal(result, user.values, name);
    }
    for (const auto &[key, power] : user.values.registeredBows)
        result.registeredBows[key] = power;
    for (const auto &[key, arrow] : user.values.registeredArrows)
        result.registeredArrows[key] = arrow;
    result.excludedForms.insert(user.values.excludedForms.begin(), user.values.excludedForms.end());
    return result;
}
PhysicsCatalog ResolvePhysicsCatalog(const Config &config) {
    PhysicsCatalog out;
    if (!config.replaceCatalog)
        for (const auto &b : DefaultBowPhysicsBindings()) {
            if (!BindingInScope(config.mgoProfiles, b.plugin))
                continue;
            auto *p = FindBowPhysicsProfile(b.profile);
            if (!p)
                continue;
            out.bows[CanonicalFormKey(b.plugin, b.localID)] = {
                std::string(p->name),
                ResolvePairingOverride(config.pairingBowPower, b.plugin, b.localID, p->power)};
        }
    if (!config.replaceCatalog)
        for (const auto &b : DefaultArrowPhysicsBindings()) {
            if (!BindingInScope(config.mgoProfiles, b.plugin))
                continue;
            auto *p = FindArrowPhysicsProfile(b.profile);
            if (!p)
                continue;
            out.arrows[CanonicalFormKey(b.plugin, b.localID)] = {
                std::string(p->name),
                ResolvePairingOverride(config.pairingAmmoMass, b.plugin, b.localID, p->mass),
                ResolvePairingOverride(config.pairingAmmoCapacity, b.plugin, b.localID, p->capacityQ),
                b.requiresGravityNormalization};
        }
    for (const auto &[key, power] : config.registeredBows)
        out.bows[CanonicalFormKey(key.plugin, key.localID)] = {"Custom", power};
    for (const auto &[key, a] : config.registeredArrows)
        out.arrows[CanonicalFormKey(key.plugin, key.localID)] = {"Custom", a.mass, a.capacity,
                                                                 a.gravityException};
    for (const auto &key : config.excludedForms) {
        out.bows.erase(CanonicalFormKey(key.plugin, key.localID));
        out.arrows.erase(CanonicalFormKey(key.plugin, key.localID));
    }
    return out;
}
std::string SerializeUserConfig(const Config &c, std::string_view fingerprint) {
    std::ostringstream s;
    s << std::setprecision(15) << std::boolalpha;
    s << "; BowSpeedVR desktop settings. Requires runtime " << EditorRuntimeVersion
      << " or newer.\n[UserConfig]\nSchemaVersion=1\nReplaceCatalog=" << c.replaceCatalog
      << "\n[Editor]\nScanFingerprint=" << fingerprint << '\n';
    s << "[General]\nEnabled=" << c.enabled << "\nObserveOnly=" << c.observeOnly
      << "\nDiagnosticLogging=" << c.diagnosticLogging;
    s << "\n[Pairing]\nEnabled=" << c.pairingEnabled
      << "\nReferencePreset=" << (c.referencePreset == ReferencePreset::Vanilla ? "Vanilla" : "Tuned")
      << "\nReferenceSpeed=" << c.pairingSettings.referenceSpeed
      << "\nPlayerSpeedMult=" << c.pairedPlayerSpeedMult
      << "\nVirtualBowMass=" << c.pairingSettings.virtualBowMass
      << "\nMaxSpeedRatio=" << c.pairingSettings.maxSpeedRatio
      << "\nCapacityLimits=" << c.pairingSettings.capacityLimits
      << "\nMomentumImpact=" << c.pairingMomentumImpact
      << "\nImpactStrength=" << c.pairingSettings.impactStrength;
    s << "\n[SpeedTuning]\nNPCEnabled=" << c.npcSpeedEnabled << "\nNPCSpeedMult=" << c.npcSpeedMult
      << "\nCompensateRicochet=" << c.compensateRicochet
      << "\n[Immersion]\nArrowInventoryWeight=" << c.arrowInventoryWeight << '\n';
    const auto catalog = ResolvePhysicsCatalog(c);
    s << "[CatalogBow]\n";
    for (const auto &[key, b] : catalog.bows)
        s << FormatFormKey(key) << '=' << b.power << '\n';
    s << "[CatalogArrow]\n";
    for (const auto &[key, a] : catalog.arrows)
        s << FormatFormKey(key) << '=' << a.mass << ',' << a.capacity << ',' << a.gravityException << '\n';
    s << "[Exclude]\n";
    for (const auto &key : c.excludedForms)
        s << FormatFormKey(key) << "=true\n";
    return s.str();
}
} // namespace BowSpeed
