#pragma once
#include <compare>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "BowSpeed/AmmoPhysics.h"
#include "BowSpeed/MatchedPhysics.h"
namespace BowSpeed {
enum class ReferencePreset { Tuned, Vanilla };
struct FormKey {
    std::string plugin;
    std::uint32_t localID;
    auto operator<=>(const FormKey&) const = default;
};
struct RegisteredArrow { double mass{}, capacity{}; bool gravityException{}; };
struct Config {
    bool enabled = true;
    bool observeOnly = true;
    bool diagnosticLogging = true;
    float playerSpeedMult = 1.0f;
    float playerCrossbowSpeedMult = 1.0f;
    bool arrowPhysics = false;
    float massInfluence = 0.2f;
    std::map<FormKey, float> bows;
    std::map<FormKey, AmmoClass> ammoClasses;
    bool pairingEnabled = false;
    bool pairingMomentumImpact = true;
    MatchedSettings pairingSettings;
    ReferencePreset referencePreset = ReferencePreset::Tuned;
    double pairedPlayerSpeedMult = 1.0;
    bool mgoProfiles = false;
    bool npcSpeedEnabled = false;
    double npcSpeedMult = 1.0;
    bool compensateRicochet = false;
    bool arrowInventoryWeight = false;
    std::map<FormKey, float> pairingBowPower;
    std::map<FormKey, float> pairingAmmoMass;
    std::map<FormKey, float> pairingAmmoCapacity;
    std::vector<std::string> warnings;
    std::map<FormKey,double> registeredBows;
    std::map<FormKey,RegisteredArrow> registeredArrows;
    std::set<FormKey> excludedForms;
    bool replaceCatalog=false;
};
Config ParseConfig(std::string_view ini);
std::map<FormKey, AmmoClass> DefaultAmmoClasses();
}
