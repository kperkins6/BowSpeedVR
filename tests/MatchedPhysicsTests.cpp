#include "BowSpeed/MatchedPhysics.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
int checks=0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
bool Near(double a, double b, double tolerance=1e-10) {
    return std::abs(a-b)<=tolerance*std::max({1.0,std::abs(a),std::abs(b)});
}
using namespace BowSpeed;
void AnchorsAndLimits() {
    const auto iron=ComputeMatchedShot({},1.0,1.0,1.8,1.0);
    Check(iron && Near(iron->launchSpeed,5000.0), "Long+Iron launches at reference speed");
    Check(Near(iron->momentumRatio,1.0) && Near(iron->energyRatio,1.0) && Near(iron->forceRatio,1.0), "Iron anchor has reference momentum, energy and force");
    const auto daedric=ComputeMatchedShot({},2.0408163265306123,2.3010204081632653,2.5510204081632653,1.0);
    Check(daedric && Near(daedric->launchSpeed,5000.0), "Daedric matched pair launches at reference speed");
    Check(Near(daedric->momentumRatio,2.3010204081632653) && Near(daedric->energyRatio,2.3010204081632653), "Arrow mass stays fixed for matched Daedric impact");
    const auto weak=ComputeMatchedShot({},2.0408163265306123,1.0,1.8,1.0);
    Check(weak && Near(weak->launchSpeed,6000.0) && weak->capacityLimited && !weak->speedLimited, "Weak arrow is capacity limited to 6000");
    const auto below=ComputeMatchedShot({},2.0408163265306123,1.0,1.8,0.5);
    Check(below && Near(below->launchSpeed,3571.428571428571) && !below->capacityLimited, "Partial draw reduces requested energy BEFORE capacity");
    const auto above=ComputeMatchedShot({},2.0408163265306123,1.0,1.8,0.9);
    Check(above && Near(above->launchSpeed,6000.0) && above->capacityLimited, "Partial draw above cap retains capped launch speed");
    const auto speed=ComputeMatchedShot({},2.0408163265306123,0.55,2.5510204081632653,1);
    Check(speed && Near(speed->launchSpeed,6250.0) && speed->speedLimited && !speed->capacityLimited, "Light arrows bind speed ceiling, not capacity");
    const auto speedBelow=ComputeMatchedShot({},2.0408163265306123,.55,2.5510204081632653,.5);
    const auto speedAbove=ComputeMatchedShot({},2.0408163265306123,.55,2.5510204081632653,.9);
    Check(speedBelow && Near(speedBelow->launchSpeed,4464.285714285714) && !speedBelow->speedLimited, "Partial draw below speed cap uses reduced requested energy");
    Check(speedAbove && Near(speedAbove->launchSpeed,6250) && speedAbove->speedLimited, "Partial draw above speed cap retains ceiling launch speed");
    const auto tied=ComputeMatchedShot({},4,1,1.953125,1);
    Check(tied && tied->capacityLimited && tied->speedLimited, "Both binding caps report a tie");
    const auto equal=ComputeMatchedShot({},1,1,1.25,1);
    Check(equal && !equal->capacityLimited && !equal->speedLimited, "Equality with requested energy removes no energy");
    const auto close=ComputeMatchedShot({},1,1,1.25-1e-13,1);
    Check(close && !close->capacityLimited, "Rounding-sized energy difference does not flag a cap");
    MatchedSettings unlimited;
    unlimited.capacityLimits=false;
    const auto noCapacity=ComputeMatchedShot(unlimited,2.0408163265306123,1,1.8,1);
    Check(noCapacity && Near(noCapacity->launchSpeed,6250) && !noCapacity->capacityLimited && noCapacity->speedLimited, "Disabling capacity still enforces launch ceiling");
    MatchedSettings scaled;
    scaled.referenceSpeed=6000;
    const auto fast=ComputeMatchedShot(scaled,1,1,1.8,1);
    Check(fast && Near(fast->launchSpeed,6000) && Near(fast->momentumRatio,1.2) && Near(fast->energyRatio,1.44), "Global speed scales impact against fixed 5000 reference");
    Check(Near(fast->forceRatio,1.0954451150103321), "Force scales with square root of fixed-reference momentum");
}
void ZeroAndAdapters() {
    const auto zero=ComputeMatchedShot({},2,1,1.8,0);
    Check(zero && zero->launchSpeed==0 && zero->requestedQ==0 && zero->usedQ==0 && zero->momentumRatio==0 && zero->energyRatio==0 && zero->forceRatio==.75 && !zero->capacityLimited && !zero->speedLimited, "Zero draw is finite, zero energy and has no binding cap");
    const auto zeroPower=ComputeMatchedShot({},0,1,1.8,1);
    Check(!zeroPower, "Zero bow power rejects malformed equipment");
    const auto zeroMass=ComputeMatchedShot({},1,0,1.8,1);
    Check(!zeroMass, "Zero arrow mass rejects even with positive virtual inertia");
    auto getter=MatchedGetterFactor(6000,4442.925390625,.5,1);
    Check(getter && Near(*getter,6000.0/(4442.925390625*.5)), "Getter cancels native draw exactly once");
    getter=MatchedGetterFactor(6000,4442.925390625,.5,2);
    Check(getter && Near(*getter,6000.0/4442.925390625), "Getter includes runtime speed multiplier");
    Check(MatchedGetterFactor(0,5000,0,1)==1, "Zero launch plus zero draw uses neutral safe getter fallback");
    Check(!MatchedGetterFactor(1,5000,0,1), "Positive requested launch with zero draw rejects impossible adapter");
    Check(MatchedGetterFactor(0,5000,.5,1)==0, "Zero launch with positive denominator has zero factor");
    for (double native : {1.0,1.5,4.0}) {
        const auto force=NormalizeMatchedForce(native,native,.8291561976);
        Check(force && Near(*force,.8291561976), "Material forces normalize to a common force-1 baseline");
        const auto doubled=NormalizeMatchedForce(2*native,native,1.2);
        Check(doubled && Near(*doubled,2.4), "Force normalization preserves nonmaterial native scaling");
    }
    Check(NormalizeMatchedForce(0,1,1)==0, "Zero incoming impulse remains zero");
    Check(ComputeMatchedImpactRatio(1,5000)==1 && !ComputeMatchedImpactRatio(0,5000) && ComputeMatchedImpactRatio(1,0)==.75, "Impact adapter rejects invalid zero mass while allowing zero velocity");
    const auto floor=ComputeMatchedImpactRatio(.1,5000);
    const auto ceiling=ComputeMatchedImpactRatio(4,5000);
    Check(floor && *floor==.75 && ceiling && *ceiling==1.55, "Impact force clamps at both approved bounds");
}
void FullParameterSweep() {
    // Literal 34x21 full-array numeric fixtures from data/matched-pair-design.json.
    // Independent conservation checks catch restoring discarded capacity energy,
    // bow-dependent arrow mass, double draw reduction and speed-ceiling bypass.
    constexpr std::array<double,34> powers{{
        1.0000000000000004,
        1.110012755102041,
        1.2257653061224492,
        1.2257653061224492,
        1.3472576530612248,
        1.4744897959183676,
        1.607461734693878,
        1.607461734693878,
        1.7461734693877555,
        1.8906250000000007,
        1.8906250000000007,
        2.0408163265306123,
        1.5804081632653066,
        2.0408163265306123,
        1.3472576530612248,
        1.04,
        1.08,
        1.607461734693878,
        0.8400000000000001,
        1.4744897959183676,
        1.607461734693878,
        2.0408163265306123,
        1.8906250000000007,
        1.9600000000000002,
        1.607461734693878,
        1.607461734693878,
        1.607461734693878,
        1.3472576530612248,
        1.2257653061224492,
        1.607461734693878,
        1.3472576530612248,
        1.4744897959183676,
        1.8906250000000007,
        2.0408163265306123,
    }};
    constexpr std::array<std::array<double,2>,21> arrows{{
        {1,1.8},
        {1.137515943877551,2.05},
        {1.2822066326530615,2.2},
        {1.2822066326530615,1.8},
        {1.434072066326531,2.35},
        {1.434072066326531,2.35},
        {1.5931122448979596,2.5510204081632653},
        {1.7593271683673475,2.5510204081632653},
        {1.9327168367346945,2.5510204081632653},
        {1.7593271683673475,2.35},
        {2.113281250000001,2.5510204081632653},
        {2.113281250000001,2.5510204081632653},
        {2.3010204081632653,2.5510204081632653},
        {2.3010204081632653,2.5510204081632653},
        {1.05,1.6},
        {1.1,1.65},
        {1.1,1.3},
        {1.7255102040816332,2.5510204081632653},
        {0.55,2.5510204081632653},
        {3,2.5510204081632653},
        {4,2.5510204081632653},
    }};
    int pairs=0;
    for (double power : powers) {
        for (const auto& arrow : arrows) {
            ++pairs;
            for (double draw : {0.0,.25,.5,.9,1.0}) {
                const auto shot=ComputeMatchedShot({},power,arrow[0],arrow[1],draw);
                Check(shot.has_value(), "Every full-array pair models every draw level");
                Check(shot->launchSpeed>=0 && shot->launchSpeed<=6250+1e-8, "Every pair obeys launch ceiling");
                Check(shot->usedQ<=shot->requestedQ+1e-12 && shot->usedQ<=arrow[1]+1e-12, "Limits cannot restore or create requested energy");
                const double speed=shot->launchSpeed/5000;
                Check(Near(shot->usedQ,(arrow[0]+.25)*speed*speed), "Used energy accounts for fixed arrow and virtual bow inertia");
                Check(Near(shot->momentumRatio,arrow[0]*speed) && Near(shot->energyRatio,arrow[0]*speed*speed), "Every bow retains the arrow's fixed mass in momentum and kinetic energy");
                Check(Near(shot->requestedQ,1.25*power*draw*draw), "Requested energy is native draw squared before limits");
                Check(Near(shot->forceRatio,std::clamp(std::sqrt(shot->momentumRatio),.75,1.55)), "Force consumes conserved momentum with approved clamp");
                if (draw>0) {
                    const auto factor=MatchedGetterFactor(shot->launchSpeed,4442.925390625,draw,.8);
                    Check(factor && Near(4442.925390625*draw*.8*(*factor),shot->launchSpeed), "Adapter reconstructs desired launch across every pair");
                }
            }
            Check(!ComputeMatchedShot({},power,arrow[0],arrow[1],-0.01), "Every pair rejects invalid draw");
        }
    }
    Check(pairs==714, "All 714 fixture parameter pairs were exercised");
    std::cout << "Swept " << pairs << " full-array pairs at five draw levels\n";
}
void InvalidInputs() {
    const double nan=std::numeric_limits<double>::quiet_NaN();
    const double inf=std::numeric_limits<double>::infinity();
    const double huge=std::numeric_limits<double>::max();
    const double tiny=std::numeric_limits<double>::denorm_min();
    for (double bad : {-1.0,nan,inf,-inf}) {
        Check(!ComputeMatchedShot({},bad,1,1.8,1), "Invalid bow power rejects");
        Check(!ComputeMatchedShot({},1,bad,1.8,1), "Invalid arrow mass rejects");
        Check(!ComputeMatchedShot({},1,1,bad,1), "Invalid capacity rejects");
        Check(!ComputeMatchedShot({},1,1,1.8,bad), "Invalid draw rejects");
        Check(!ComputeMatchedImpactRatio(bad,5000) && !ComputeMatchedImpactRatio(1,bad), "Invalid impact input rejects");
        Check(!MatchedGetterFactor(bad,5000,1,1) && !MatchedGetterFactor(5000,bad,1,1) && !MatchedGetterFactor(5000,5000,bad,1) && !MatchedGetterFactor(5000,5000,1,bad), "Invalid getter input rejects");
        Check(!NormalizeMatchedForce(bad,1,1) && !NormalizeMatchedForce(1,bad,1) && !NormalizeMatchedForce(1,1,bad), "Invalid force input rejects");
    }
    for (double bad : {0.0,-1.0,nan,inf}) {
        auto settings=MatchedSettings{};
        settings.referenceSpeed=bad;
        Check(!ComputeMatchedShot(settings,1,1,1.8,1), "Invalid reference speed rejects");
        settings=MatchedSettings{};settings.maxSpeedRatio=bad;
        Check(!ComputeMatchedShot(settings,1,1,1.8,1), "Invalid speed ceiling rejects");
        Check(!ComputeMatchedShot({},1,1,bad,1), "Nonpositive capacity rejects even before limit selection");
    }
    for (double bad : {-1.0,nan,inf}) {
        auto settings=MatchedSettings{};settings.virtualBowMass=bad;
        Check(!ComputeMatchedShot(settings,1,1,1.8,1), "Invalid virtual mass rejects");
    }
    auto settings=MatchedSettings{};settings.virtualBowMass=0;
    Check(ComputeMatchedShot(settings,1,1,1.8,1).has_value() && !ComputeMatchedShot(settings,1,0,1.8,1), "Zero virtual mass requires positive arrow mass");
    Check(!ComputeMatchedShot({},1,1,1.8,1.01) && !MatchedGetterFactor(1,1,1.01,1), "Draw factors above full draw reject");
    Check(!MatchedGetterFactor(0,0,0,1) && !MatchedGetterFactor(0,1,0,0) && !MatchedGetterFactor(1,0,1,1) && !MatchedGetterFactor(1,1,1,0), "Zero base/runtime denominators reject even at zero draw");
    Check(!NormalizeMatchedForce(1,0,1), "Zero original projectile force rejects division by zero");
    Check(NormalizeMatchedForce(1,1,0)==0, "Zero momentum force multiplier is valid");
    Check(!MatchedGetterFactor(1,huge,1,huge) && !MatchedGetterFactor(1,tiny,.5,1) && !MatchedGetterFactor(huge,tiny,1,1), "Nonrepresentable getter denominator or output rejects");
    Check(!MatchedGetterFactor(tiny,huge,1,1), "Underflowed positive getter factor rejects");
    Check(!NormalizeMatchedForce(huge,tiny,1) && !NormalizeMatchedForce(huge,1,huge), "Nonrepresentable force conversion rejects");
    Check(!ComputeMatchedShot({},huge,1,1.8,1), "Overflowed requested energy rejects instead of silently capping");
    Check(!ComputeMatchedShot({},1,1,1.8,tiny), "Underflowed positive requested energy rejects");
    settings=MatchedSettings{};settings.capacityLimits=false;
    Check(!ComputeMatchedShot(settings,1,1,0,1), "Disabling capacity does not permit invalid metadata");
    settings=MatchedSettings{};settings.virtualBowMass=huge;
    Check(!ComputeMatchedShot(settings,1,huge,1.8,1), "Overflowed total mass rejects");
    settings=MatchedSettings{};settings.maxSpeedRatio=huge;
    Check(!ComputeMatchedShot(settings,1,1,1.8,1), "Overflowed ceiling energy rejects");
    settings=MatchedSettings{};settings.referenceSpeed=huge;
    Check(!ComputeMatchedShot(settings,1,1,1.8,1), "Overflowed energy/momentum output rejects");
    Check(!ComputeMatchedImpactRatio(huge,huge), "Overflowed impact momentum rejects");
}
}

int main() {
    try {
        AnchorsAndLimits();ZeroAndAdapters();InvalidInputs();FullParameterSweep();
        std::cout << "Matched physics checks passed: " << checks << '\n';
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Matched physics check failed: " << e.what() << '\n';
        return 1;
    }
}
