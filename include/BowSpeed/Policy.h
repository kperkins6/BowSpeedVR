#pragma once
#include <optional>
#include <cstdint>
#include <cmath>
namespace BowSpeed {
inline constexpr std::uintptr_t ExpectedWeaponSpeedRVA = 0x75CB20;
inline bool CanInstallHook(bool validRuntime, bool tableReadable, bool targetExecutable, std::uintptr_t observedRVA) {
    return validRuntime && tableReadable && targetExecutable && observedRVA == ExpectedWeaponSpeedRVA;
}
inline float SelectWeaponFactor(bool playerShot, bool bowShot, std::optional<float> configured, float original,float playerDefault=1.0f) {
    if (!playerShot || !bowShot || !std::isfinite(original) || original<=0.0f) return original;
    const float factor=configured.value_or(playerDefault);
    return std::isfinite(factor) && factor>=1.0f && factor<=1.3f ? original*factor : original;
}
inline float SelectPlayerWeaponFactor(bool playerShot, bool bowShot, bool crossbowShot,
    std::optional<float> configuredBow, float original, float bowDefault, float crossbowDefault) {
    return SelectWeaponFactor(playerShot,bowShot || crossbowShot,
        bowShot ? configuredBow : std::nullopt,original,
        bowShot ? bowDefault : crossbowDefault);
}
}
