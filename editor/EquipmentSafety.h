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
inline bool ReviewedInteractionHelper(const FormKey &key) {
    const auto canonical = CanonicalFormKey(key.plugin, key.localID);
    // Existing reviewed exclusions in data/bow-records.csv. These launchers
    // serve VR menus, lockpicking and reload mechanics, not player archery.
    if (canonical.plugin == "crossbowreloadvr.esp")
        return canonical.localID == 0x800;
    if (canonical.plugin == "spellwheelvr.esp")
        return canonical.localID == 0x807 || canonical.localID == 0x808 || canonical.localID == 0x17D56;
    return false;
}
} // namespace BowSpeed::Editor
