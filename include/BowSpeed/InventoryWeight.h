#pragma once
#include "BowSpeed/Config.h"
#include "BowSpeed/MatchedRuntime.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/Tuning.h"
#include "BowSpeed/UserConfig.h"
#include <cmath>
#include <optional>
namespace BowSpeed {
// Inventory units are a gameplay scale, not a claim that model mass is pounds.
inline std::optional<float> InventoryWeightFromMass(double mass) noexcept {
    if (!std::isfinite(mass) || mass<.1 || mass>8) return {};
    return static_cast<float>(mass*.1);
}
inline std::map<FormKey,float> BuildInventoryWeightProfiles(const Config& config) {
    std::map<FormKey,float> result;
    if (!config.enabled || config.observeOnly || !config.arrowInventoryWeight) return result;
    for (const auto& [key,profile]:ResolvePhysicsCatalog(config).arrows) {
        if (const auto weight=InventoryWeightFromMass(profile.mass)) result.emplace(key,*weight);
    }
    return result;
}
inline float SelectArrowInventoryWeight(float native,bool active,bool trusted,bool arrow,std::optional<float> modeled) noexcept {
    if (!active || !trusted || !arrow || !modeled || !std::isfinite(*modeled) || *modeled<=0 || *modeled>.8f) return native;
    return *modeled;
}
}
