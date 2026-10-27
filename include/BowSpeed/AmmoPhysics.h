#pragma once
#include <cmath>
#include <cstdint>
#include <optional>
#include <unordered_map>
namespace BowSpeed {
enum class AmmoClass { ExtraLight, Light, Medium, Heavy, Massive };
using AmmoClasses = std::unordered_map<const void*, AmmoClass>;
struct AmmoModifiers { float speed=1.0f; float impact=1.0f; };
inline AmmoModifiers CalculateAmmoModifiers(std::optional<AmmoClass> category,float influence) {
    if (!category || !std::isfinite(influence) || influence<0.0f || influence>0.4f) return {};
    float ratio=1.0f;
    switch (*category) {
        case AmmoClass::ExtraLight: ratio=1.0f/3.0f;break;
        case AmmoClass::Light: ratio=0.5f;break;
        case AmmoClass::Medium: break;
        case AmmoClass::Heavy: ratio=2.0f;break;
        case AmmoClass::Massive: ratio=3.0f;break;
        default: return {};
    }
    const float impact=std::pow(ratio,influence*0.5f);
    return {1.0f/impact,impact};
}
inline AmmoModifiers ResolveAmmoModifiers(bool enabled,bool playerShot,bool usableBow,const void* ammo,
    const AmmoClasses& classes,float influence) {
    if (!enabled || !playerShot || !usableBow || !ammo) return {};
    const auto entry=classes.find(ammo);
    return entry==classes.end() ? AmmoModifiers{} : CalculateAmmoModifiers(entry->second,influence);
}
inline float ScalePhysicalImpact(float original,float factor) {
    if (!std::isfinite(original) || original<=0.0f || !std::isfinite(factor) || factor<0.75f || factor>1.25f) return original;
    const float result=original*factor;
    return std::isfinite(result) ? result : original;
}
}
