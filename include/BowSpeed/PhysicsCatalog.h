#pragma once
#include <cstdint>
#include <span>
#include <string_view>

namespace BowSpeed {
struct BowPhysicsProfile { std::string_view name; double power; };
struct ArrowPhysicsProfile { std::string_view name; double mass; double capacityQ; double dragArea; };
struct BowPhysicsBinding { std::string_view plugin; std::uint32_t localID; std::string_view profile; };
struct ArrowPhysicsBinding {
    std::string_view plugin;
    std::uint32_t localID;
    std::string_view profile;
    // Runtime must exclude this binding unless player-isolated normalization is verified.
    bool requiresGravityNormalization;
};
std::span<const BowPhysicsProfile> BowPhysicsProfiles() noexcept;
std::span<const ArrowPhysicsProfile> ArrowPhysicsProfiles() noexcept;
std::span<const BowPhysicsBinding> DefaultBowPhysicsBindings() noexcept;
std::span<const ArrowPhysicsBinding> DefaultArrowPhysicsBindings() noexcept;
const BowPhysicsProfile* FindBowPhysicsProfile(std::string_view) noexcept;
const ArrowPhysicsProfile* FindArrowPhysicsProfile(std::string_view) noexcept;
}
