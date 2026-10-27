#pragma once
#include <optional>
#include <unordered_map>
#include <cmath>
namespace BowSpeed {
inline bool UsableRangedSource(bool isRanged,float recordSpeed) { return isRanged && std::isfinite(recordSpeed) && recordSpeed>0.0f; }
using Factors = std::unordered_map<const void*, float>;
struct Eligibility { bool playerShot; bool bowShot; std::optional<float> factor; };
inline Eligibility ResolveEligibility(bool playerShot, bool bowShot, const void* source, const Factors& factors) {
    std::optional<float> factor;
    if (playerShot && bowShot && source) {
        if (const auto entry=factors.find(source); entry!=factors.end()) factor=entry->second;
    }
    return {playerShot, bowShot, factor};
}
}
