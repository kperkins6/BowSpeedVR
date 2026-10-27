#pragma once
#include <array>
#include "BowSpeed/Config.h"
#include <cstdint>
#include <optional>
#include <memory>
#include <string>
#include <unordered_map>
namespace BowSpeed {
double ResolvePairingOverride(const std::map<FormKey,float>& values,std::string_view plugin,std::uint32_t localID,double fallback);
struct ResolvedBowPhysics {std::string name;double power{};};
struct ResolvedArrowPhysics {std::string name;double mass{},capacity{};bool gravityException{};};
struct MatchedProfiles {
    std::unordered_map<std::uint32_t,ResolvedBowPhysics> bows;
    std::unordered_map<std::uint32_t,ResolvedArrowPhysics> arrows;
    std::shared_ptr<const MatchedSettings> settings;
};
struct PairingEligibility {
    bool enabled{}, observed{}, launchTrusted{}, impactTrusted{}, identityTrusted{};
    bool player{}, bow{}, reviewedBow{}, knownArrow{}, gravityException{}, ordinaryForce{}, physicalArrow{};
};
bool PairingPermitted(const PairingEligibility&) noexcept;
bool PairingLaunchTrusted(bool speedPath,bool impactPath,double nativeFactor) noexcept;
std::optional<float> RuntimeGetterFactor(double desired,double base,double draw,double runtime) noexcept;
struct ShotIdentity {
    std::uintptr_t pointer{};
    std::uint32_t handle{}, form{}, base{};
    bool operator==(const ShotIdentity&) const = default;
};
struct LaunchSnapshot {
    ShotIdentity identity;
    std::uint32_t bow{}, ammo{};
    double mass{}, baseForce{}, launchSpeed{}, getterFactor{}, age{};
    bool eligible{}, momentumImpact{};
    std::shared_ptr<const MatchedSettings> settings;
};
double SelectMatchedImpactForce(const std::optional<LaunchSnapshot>& snapshot,double incoming,double legacy,
    double impactSpeed,bool pairing,bool trusted) noexcept;
class ShotSnapshots {
public:
    static constexpr std::size_t Capacity=256;
    std::optional<std::size_t> Record(const LaunchSnapshot&) noexcept;
    std::optional<LaunchSnapshot> Find(const ShotIdentity&,double age,bool trusted) noexcept;
    void Clear() noexcept;
    void Forget(std::size_t slot) noexcept {if (slot<Capacity) entries[slot].reset();}
private:
    std::array<std::optional<LaunchSnapshot>,Capacity> entries{};
    std::size_t next{};
};
}
