#include "BowSpeed/MatchedPhysics.h"
#include <algorithm>
#include <cmath>

namespace BowSpeed {
namespace {
constexpr double ImpactReferenceSpeed=5000.0;
bool Positive(double value) noexcept { return std::isfinite(value) && value>0.0; }
bool Nonnegative(double value) noexcept { return std::isfinite(value) && value>=0.0; }
bool ValidDraw(double value) noexcept { return Nonnegative(value) && value<=1.0; }
bool Close(double a, double b) noexcept {
    return std::abs(a-b)<=1e-12*std::max(std::abs(a),std::abs(b));
}
}

std::optional<MatchedShot> ComputeMatchedShot(const MatchedSettings& settings,
    double bowPower, double arrowMass, double capacityQ, double draw) noexcept {
    if (!Positive(settings.referenceSpeed) || !Nonnegative(settings.virtualBowMass) ||
        !Positive(settings.maxSpeedRatio) || !Positive(bowPower) || !Positive(arrowMass) ||
        !Positive(capacityQ) || !ValidDraw(draw) || !Nonnegative(settings.impactStrength) || settings.impactStrength>2) return std::nullopt;

    const double totalMass=arrowMass+settings.virtualBowMass;
    const double speedCapQ=totalMass*settings.maxSpeedRatio*settings.maxSpeedRatio;
    const double requestedQ=1.25*bowPower*draw*draw;
    if (!Positive(totalMass) || !Positive(speedCapQ) || !Nonnegative(requestedQ)) return std::nullopt;
    if (draw>0.0 && requestedQ==0.0) return std::nullopt;

    double usedQ=std::min(requestedQ,speedCapQ);
    if (settings.capacityLimits) usedQ=std::min(usedQ,capacityQ);
    const double speedSquared=usedQ/totalMass;
    if (!Nonnegative(speedSquared) || (usedQ>0.0 && speedSquared==0.0)) return std::nullopt;
    const double launchSpeed=settings.referenceSpeed*std::sqrt(speedSquared);
    const double referenceRatio=launchSpeed/ImpactReferenceSpeed;
    const double momentumRatio=arrowMass*referenceRatio;
    const double energyRatio=momentumRatio*referenceRatio;
    if (!Nonnegative(launchSpeed) || !Nonnegative(referenceRatio) || !Nonnegative(momentumRatio) ||
        !Nonnegative(energyRatio) || (usedQ>0.0 && launchSpeed==0.0)) return std::nullopt;

    const bool removedEnergy=usedQ<requestedQ && !Close(usedQ,requestedQ);
    return MatchedShot{launchSpeed,momentumRatio,energyRatio,requestedQ,usedQ,
        std::clamp(std::clamp(std::sqrt(momentumRatio),.75,1.55)*settings.impactStrength,0.0,1.55),
        removedEnergy && settings.capacityLimits && Close(usedQ,capacityQ),
        removedEnergy && Close(usedQ,speedCapQ)};
}

std::optional<double> ComputeMatchedImpactRatio(double arrowMass, double impactSpeed, double strength) noexcept {
    if (!Positive(arrowMass) || !Nonnegative(impactSpeed) || !Nonnegative(strength) || strength>2) return std::nullopt;
    const double momentum=arrowMass*(impactSpeed/ImpactReferenceSpeed);
    if (!Nonnegative(momentum)) return std::nullopt;
    return std::clamp(std::clamp(std::sqrt(momentum),.75,1.55)*strength,0.0,1.55);
}

std::optional<double> MatchedGetterFactor(double desiredLaunchSpeed,
    double baseSpeed, double draw, double runtimeSpeedMult) noexcept {
    if (!Nonnegative(desiredLaunchSpeed) || !Positive(baseSpeed) || !ValidDraw(draw) ||
        !Positive(runtimeSpeedMult)) return std::nullopt;
    if (draw==0.0) return desiredLaunchSpeed==0.0 ? std::optional<double>{1.0} : std::nullopt;
    const double denominator=baseSpeed*draw*runtimeSpeedMult;
    if (!Positive(denominator)) return std::nullopt;
    const double factor=desiredLaunchSpeed/denominator;
    if (!Nonnegative(factor) || (desiredLaunchSpeed>0.0 && factor==0.0)) return std::nullopt;
    return factor;
}

std::optional<double> NormalizeMatchedForce(double incomingForce,
    double originalProjectileForce, double momentumForceRatio) noexcept {
    if (!Nonnegative(incomingForce) || !Positive(originalProjectileForce) ||
        !Nonnegative(momentumForceRatio)) return std::nullopt;
    const double baseline=incomingForce/originalProjectileForce;
    const double force=baseline*momentumForceRatio;
    if (!Nonnegative(baseline) || !Nonnegative(force)) return std::nullopt;
    return force;
}
}
