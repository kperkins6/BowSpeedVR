#include "BowSpeed/MatchedRuntime.h"
#include <cmath>
#include <limits>
#include "BowSpeed/MatchedPhysics.h"
namespace BowSpeed {
double ResolvePairingOverride(const std::map<FormKey,float>& values,std::string_view plugin,std::uint32_t localID,double fallback) {
    std::string normalized(plugin);
    for (auto& c:normalized) if (c>='A' && c<='Z') c=static_cast<char>(c+('a'-'A'));
    if (normalized=="skyrim.esm") normalized="Skyrim.esm";
    const auto found=values.find({std::move(normalized),localID});
    return found==values.end() ? fallback : found->second;
}
double SelectMatchedImpactForce(const std::optional<LaunchSnapshot>& snapshot,double incoming,double legacy,
    double impactSpeed,bool pairing,bool trusted) noexcept {
    if (!trusted || (pairing && (!std::isfinite(incoming) || incoming<=0 || incoming>4))) return incoming;
    if (!pairing || !snapshot) return legacy;
    if (!snapshot->eligible || !snapshot->momentumImpact) return incoming;
    const auto ratio=ComputeMatchedImpactRatio(snapshot->mass,impactSpeed,snapshot->settings ? snapshot->settings->impactStrength : 1.0);
    const auto normalized=ratio ? NormalizeMatchedForce(incoming,snapshot->baseForce,*ratio) : std::nullopt;
    return normalized && *normalized<=std::numeric_limits<float>::max() ? *normalized : incoming;
}
std::optional<float> RuntimeGetterFactor(double desired,double base,double draw,double runtime) noexcept {
    const auto factor=MatchedGetterFactor(desired,base,draw,runtime);
    if (!factor || *factor<=0 || *factor>std::numeric_limits<float>::max()) return {};
    const auto value=static_cast<float>(*factor);
    return std::isfinite(value) && value>0 ? std::optional<float>(value) : std::nullopt;
}
bool PairingLaunchTrusted(bool speedPath,bool impactPath,double nativeFactor) noexcept {
    return speedPath && impactPath && std::isfinite(nativeFactor) && nativeFactor>0;
}
bool PairingPermitted(const PairingEligibility& e) noexcept {
    return e.enabled && !e.observed && e.launchTrusted && e.impactTrusted && e.identityTrusted &&
        e.player && e.bow && e.reviewedBow && e.knownArrow && !e.gravityException && e.ordinaryForce && e.physicalArrow;
}
std::optional<std::size_t> ShotSnapshots::Record(const LaunchSnapshot& shot) noexcept {
    if (!shot.eligible || !shot.identity.pointer || !shot.identity.handle || !shot.identity.form || !shot.identity.base ||
        !shot.bow || !shot.ammo || !std::isfinite(shot.mass) || shot.mass<=0 ||
        !std::isfinite(shot.baseForce) || shot.baseForce<=0 || shot.baseForce>4 ||
        !std::isfinite(shot.launchSpeed) || shot.launchSpeed<0 ||
        !std::isfinite(shot.getterFactor) || shot.getterFactor<=0 || shot.age!=0) return {};
    for (std::size_t i=0;i<Capacity;++i) {
        if (entries[i] && entries[i]->identity.pointer==shot.identity.pointer) {
            // Repeated prelaunch getter calls must not replace original equipment.
            if (entries[i]->identity==shot.identity) return i;
            entries[i].reset();
        }
    }
    const auto slot=next;next=(next+1)%Capacity;entries[slot]=shot;return slot;
}
std::optional<LaunchSnapshot> ShotSnapshots::Find(const ShotIdentity& identity,double age,bool trusted) noexcept {
    if (!trusted) {Clear();return {};}
    for (auto& entry:entries) {
        if (!entry || entry->identity.pointer!=identity.pointer) continue;
        if (entry->identity!=identity || !std::isfinite(age) || age<entry->age || age>120) {entry.reset();return {};}
        entry->age=age;return entry;
    }
    return {};
}
void ShotSnapshots::Clear() noexcept {for (auto& entry:entries) entry.reset();next=0;}
}
