#pragma once
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/UserConfig.h"
namespace BowSpeed::Editor {
// Safety metadata is independent of the user's optional MGO catalogue scope.
inline bool ReviewedGravityException(const FormKey &key) {
    for (const auto &binding : DefaultArrowPhysicsBindings())
        if (binding.requiresGravityNormalization &&
            CanonicalFormKey(binding.plugin, binding.localID) == CanonicalFormKey(key.plugin, key.localID))
            return true;
    return false;
}
inline std::string_view ReviewedExclusionReason(const FormKey &key) {
    const auto canonical = CanonicalFormKey(key.plugin, key.localID);
    for (const auto &entry : ReviewedEquipmentExclusions())
        if (entry.localID == canonical.localID && entry.plugin == canonical.plugin)
            return entry.reason;
    return {};
}
} // namespace BowSpeed::Editor
