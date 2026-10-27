#pragma once
#include <optional>

namespace BowSpeed {
struct MatchedSettings {
    double referenceSpeed=5000.0;
    double virtualBowMass=0.25;
    double maxSpeedRatio=1.25;
    bool capacityLimits=true;
};
struct MatchedShot {
    double launchSpeed{}, momentumRatio{}, energyRatio{};
    double requestedQ{}, usedQ{}, forceRatio{};
    bool capacityLimited{}, speedLimited{};
};
// Power and arrow mass are positive; virtual mass is nonnegative; ceilings are positive.
// Draw is in [0,1]. Zero draw returns zero energy/speed and the .75 force floor.
// Flags identify limits that remove energy; tied binding limits both report true.
std::optional<MatchedShot> ComputeMatchedShot(
    const MatchedSettings&, double bowPower, double arrowMass,
    double capacityQ, double nativeDrawSpeedFactor) noexcept;
// Momentum reference stays fixed at 5000; force is clamp(sqrt(momentum), .75, 1.55).
// Arrow mass is positive and speed is nonnegative, both finite.
std::optional<double> ComputeMatchedImpactRatio(double arrowMass,
    double impactSpeed) noexcept;
// Base/runtime are positive finite. Draw is in [0,1]. The sole zero denominator
// fallback is a neutral factor of 1 when BOTH desired speed and draw are zero.
// Other zero, overflowed or underflowed denominators/results return nullopt.
std::optional<double> MatchedGetterFactor(double desiredLaunchSpeed,
    double baseSpeed, double nativeDrawSpeedFactor, double runtimeSpeedMult) noexcept;
// Incoming force and momentum multiplier are nonnegative finite; original force
// is positive finite. Divide out material force before applying the multiplier.
std::optional<double> NormalizeMatchedForce(double incomingForce,
    double originalProjectileForce, double momentumForceRatio) noexcept;
}
