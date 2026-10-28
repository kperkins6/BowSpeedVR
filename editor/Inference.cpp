#include "Inference.h"
#include "EquipmentSafety.h"
#include "BowSpeed/PhysicsCatalog.h"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>
namespace BowSpeed::Editor {
namespace {
constexpr double MinPower = 1.0, MaxPower = 2.04081632653061224;
constexpr double MaxMass = 2.3010204081632653;
std::string Lower(std::string s) {
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c += 'a' - 'A';
    return s;
}
bool Safe(const Equipment &e) {
    return e.ordinary && !e.scripted && e.kind != ItemKind::Crossbow;
}
// Standard representatives only: named uniques and enchanted duplicates must
// not outweigh a family or teach a curve from their enchantment balance.
bool Representative(const FormKey &k, ItemKind kind) {
    static const std::set<std::uint32_t> bows{0x3B562, 0x13985, 0x13841, 0x1398D, 0x13995, 0x1399D, 0x139A5,
                                              0x139AD, 0x139B5, 0x302CA, 0x68C57, 0xCEE9B, 0x38340, 0x83167};
    static const std::set<std::uint32_t> arrows{0x1397D, 0x1397F, 0x139BB, 0x139BC, 0x139BD, 0x139BE,
                                                0x139BF, 0x139C0, 0x34182, 0x38341, 0xCEE9E, 0xEAFDF};
    if (Lower(k.plugin) == "skyrim.esm")
        return (kind == ItemKind::Bow ? bows : arrows).contains(k.localID);
    if (Lower(k.plugin) == "dawnguard.esm")
        return k.localID == (kind == ItemKind::Bow ? 0x176F1u : 0x176F4u);
    if (Lower(k.plugin) == "dragonborn.esm")
        return kind == ItemKind::Bow ? (k.localID == 0x26231 || k.localID == 0x26232)
                                     : (k.localID == 0x26239 || k.localID == 0x2623B);
    return false;
}
struct Anchor {
    double damage{}, tier{}, capacity{}, weight = 1;
    std::string label;
};
struct Evidence {
    std::vector<Anchor> curve;
    std::map<std::string, std::vector<InferredDefaults>> materials;
    bool adjusted = false;
};
bool Same(const InferredDefaults &a, const InferredDefaults &b) {
    return std::abs(a.power - b.power) < 1e-9 && std::abs(a.mass - b.mass) < 1e-9 &&
           std::abs(a.capacity - b.capacity) < 1e-9;
}
void Fit(Evidence &e) {
    auto &v = e.curve;
    std::sort(v.begin(), v.end(), [](const auto &a, const auto &b) {
        return a.damage < b.damage || (a.damage == b.damage && a.label < b.label);
    });
    std::vector<Anchor> grouped;
    for (const auto &a : v) {
        if (!grouped.empty() && grouped.back().damage == a.damage) {
            auto &g = grouped.back();
            g.tier = (g.tier * g.weight + a.tier) / (g.weight + 1);
            g.capacity = (g.capacity * g.weight + a.capacity) / (g.weight + 1);
            g.weight += 1;
            g.label += "/" + a.label;
        } else
            grouped.push_back(a);
    }
    v = std::move(grouped);
    struct Block {
        std::size_t first, last;
        double sum, weight;
    };
    std::vector<Block> blocks;
    for (std::size_t i = 0; i < v.size(); ++i) {
        blocks.push_back({i, i, v[i].tier * v[i].weight, v[i].weight});
        while (blocks.size() >= 2) {
            auto &a = blocks[blocks.size() - 2];
            const auto b = blocks.back();
            if (a.sum / a.weight <= b.sum / b.weight)
                break;
            a.last = b.last;
            a.sum += b.sum;
            a.weight += b.weight;
            blocks.pop_back();
            e.adjusted = true;
        }
    }
    for (const auto &b : blocks)
        for (std::size_t i = b.first; i <= b.last; ++i)
            v[i].tier = b.sum / b.weight;
}
} // namespace
std::map<FormKey, InferredDefaults> InferDefaults(const ScanResult &scan, const PhysicsCatalog &known) {
    std::map<FormKey, InferredDefaults> out;
    if (!scan.complete)
        return out;
    // Curves learn the supplied defaults, never saved custom overrides or newly
    // inferred items. Only their current winning damage comes from this scan.
    const auto defaults = ResolvePhysicsCatalog(ParseConfig(""));
    Evidence bows, arrows;
    for (const auto &[key, item] : scan.equipment) {
        if (!Safe(item) || !Representative(key, item.kind))
            continue;
        InferredDefaults profile;
        std::string label;
        double tier = 0;
        auto &e = item.kind == ItemKind::Bow ? bows : arrows;
        if (item.kind == ItemKind::Bow) {
            auto b = defaults.bows.find(key);
            if (b == defaults.bows.end())
                continue;
            profile.power = b->second.power;
            label = b->second.name;
            tier = (profile.power - MinPower) / (MaxPower - MinPower);
        } else {
            auto a = defaults.arrows.find(key);
            if (a == defaults.arrows.end() || a->second.gravityException)
                continue;
            profile.mass = a->second.mass;
            profile.capacity = a->second.capacity;
            label = a->second.name;
            tier = (profile.mass - 1) / (MaxMass - 1);
        }
        profile.note = label;
        for (const auto &keyword : item.materialKeywords)
            e.materials[Lower(keyword)].push_back(profile);
        if (std::isfinite(item.damage) && item.damage > 0)
            e.curve.push_back({item.damage, std::clamp(tier, 0., 1.), profile.capacity, 1, label});
    }
    Fit(bows);
    Fit(arrows);
    for (const auto &[key, item] : scan.equipment) {
        if (!Safe(item) || ReviewedInteractionHelper(key) || ReviewedGravityException(key) ||
            known.bows.contains(key) || known.arrows.contains(key))
            continue;
        if (item.kind == ItemKind::Bow) {
            auto parent = item.weaponTemplate;
            std::set<FormKey> seen{key};
            while (parent && seen.insert(*parent).second) {
                if (auto b = known.bows.find(*parent); b != known.bows.end()) {
                    InferredDefaults p;
                    p.power = b->second.power;
                    p.method = "template";
                    p.note =
                        "template (high confidence): " + b->second.name + " via " + FormatFormKey(*parent);
                    out[key] = std::move(p);
                    break;
                }
                auto p = scan.equipment.find(*parent);
                if (p == scan.equipment.end())
                    break;
                parent = p->second.weaponTemplate;
            }
            if (out.contains(key))
                continue;
        }
        const auto &e = item.kind == ItemKind::Bow ? bows : arrows;
        std::optional<InferredDefaults> material;
        bool conflict = false;
        for (const auto &keyword : item.materialKeywords) {
            auto candidates = e.materials.find(Lower(keyword));
            if (candidates == e.materials.end())
                continue;
            for (const auto &p : candidates->second) {
                if (material && !Same(*material, p))
                    conflict = true;
                if (!material) {
                    material = p;
                    material->note = "material (high confidence): " + keyword + " matches " + p.note;
                }
            }
        }
        if (material && !conflict) {
            material->method = "material";
            out[key] = *material;
            continue;
        }
        if (!std::isfinite(item.damage) || item.damage <= 0 || e.curve.empty())
            continue;
        auto high = std::lower_bound(e.curve.begin(), e.curve.end(), item.damage,
                                     [](const auto &a, double d) { return a.damage < d; });
        auto lo = high == e.curve.begin() ? high : std::prev(high);
        if (high == e.curve.end())
            high = lo;
        const double fraction =
            high->damage == lo->damage
                ? 0
                : std::clamp((item.damage - lo->damage) / (high->damage - lo->damage), 0., 1.);
        const double tier = std::clamp(std::lerp(lo->tier, high->tier, fraction), 0., 1.);
        InferredDefaults p;
        p.power = std::lerp(MinPower, MaxPower, tier);
        p.mass = 1.25 * p.power - .25;
        p.capacity = std::lerp(lo->capacity, high->capacity, fraction);
        p.method = e.curve.size() == 1 ? "nearest" : "damage";
        std::ostringstream note;
        note << p.method << " (low confidence): " << lo->label;
        if (high != lo)
            note << " to " << high->label;
        note << "; base damage " << std::setprecision(6) << item.damage;
        if (item.damage < e.curve.front().damage || item.damage > e.curve.back().damage)
            note << "; clamped to observed anchors";
        if (e.adjusted)
            note << "; monotone fit for rebalanced anchors";
        if (conflict)
            note << "; conflicting material evidence";
        p.note = note.str();
        out[key] = std::move(p);
    }
    return out;
}
} // namespace BowSpeed::Editor
