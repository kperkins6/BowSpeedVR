#pragma once
#include <cmath>
#include <cstdint>
namespace BowSpeed {
inline bool OrdinaryArrowPhysics(std::uint16_t type, std::uint16_t flags, double gravity, double speed,
                                 double force) noexcept {
    return type == 64 && (flags & 7) == 0 && std::isfinite(gravity) && gravity > 0 && std::isfinite(speed) &&
           speed > 0 && std::isfinite(force) && force > 0 && force <= 4;
}
} // namespace BowSpeed
