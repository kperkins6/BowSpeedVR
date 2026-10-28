#include "BowSpeed/Config.h"
#include "BowSpeed/InventoryWeight.h"
#include "BowSpeed/ProjectilePolicy.h"
#include "BowSpeed/UserConfig.h"
#include "Check.h"
#include <iostream>
int main() {
    try {
        const auto config = BowSpeed::ParseConfig("[Pairing]\nImpactStrength=0.5\n");
        Check(config.warnings.empty(), "Editor impact-strength setting must be recognized");
        using namespace BowSpeed;
        const auto user = ParseUserConfig(
            "[UserConfig]\nSchemaVersion=1\n[Pairing]\nImpactStrength=0.5\n[CatalogBow]\nNewBow.esp|0x800=1."
            "5\n[CatalogArrow]\nNewArrow.esl|0x801=2,2,false\n[Exclude]\nSkyrim.esm|0x1397D=true\n");
        Check(user.valid, "Valid generic overlay registers unknown equipment");
        const auto active = ApplyUserConfig(
            ParseConfig("[General]\nObserveOnly=false\n[Immersion]\nArrowInventoryWeight=true\n"), user);
        const auto cat = ResolvePhysicsCatalog(active);
        Check(cat.bows.at(CanonicalFormKey("NEWBOW.ESP", 0x800)).power == 1.5,
              "Generic bow is eligible without compiled binding");
        Check(cat.arrows.at(CanonicalFormKey("NewArrow.esl", 0x801)).mass == 2,
              "Generic arrow retains explicit mass");
        Check(!cat.arrows.contains(CanonicalFormKey("Skyrim.esm", 0x1397D)),
              "Exclusions remove built-in pairing");
        Check(!BuildInventoryWeightProfiles(active).contains(CanonicalFormKey("Skyrim.esm", 0x1397D)),
              "Excluded ammo retains native inventory weight");
        Check(Near(BuildInventoryWeightProfiles(active).at(CanonicalFormKey("NewArrow.esl", 0x801)), .2f),
              "New arrow inventory weight follows model mass");
        const auto invalid =
            ParseUserConfig("[UserConfig]\nSchemaVersion=1\n[CatalogBow]\nNewBow.esp|0x800=nan\n");
        Check(!invalid.valid && ApplyUserConfig(active, invalid).registeredBows == active.registeredBows,
              "Invalid overlay applies nothing");
        Check(!ParseUserConfig("[UserConfig]\nSchemaVersion=2\n").valid, "Unknown schema rejected");
        const auto complete = ParseUserConfig(
            "[UserConfig]\nSchemaVersion=1\nReplaceCatalog=true\n[CatalogBow]\nNewBow.esp|0x800=1\n");
        const auto legacy = ParseConfig("[General]\nPlayerSpeedMult=1.3\nPlayerCrossbowSpeedMult=1.3\n["
                                        "ArrowPhysics]\nEnabled=true\n[Compatibility]\nMGOProfiles=true\n");
        const auto replaced = ApplyUserConfig(legacy, complete);
        Check(!replaced.arrowPhysics && replaced.playerSpeedMult == 1 &&
                  replaced.playerCrossbowSpeedMult == 1 && !replaced.mgoProfiles,
              "Complete editor overlay prevents legacy speed/ammo stacking");
        Check(!ParseUserConfig("[UserConfig]\nSchemaVersion=1\n[Typo]\nA=1\n").valid,
              "Unknown fields are diagnosed");
        Check(!ParseUserConfig("[UserConfig]\nSchemaVersion=1\n[CatalogBow]\nx.esp|0x800=1\nX.ESP|0x800=2\n")
                   .valid,
              "Case-insensitive duplicate identity rejected");
        const auto round = ParseUserConfig(SerializeUserConfig(active, "fixture"));
        Check(round.valid && round.fingerprint == "fixture", "Export is accepted by runtime parser");
        Check(ResolvePhysicsCatalog(ApplyUserConfig(config, round))
                      .bows.at(CanonicalFormKey("newbow.esp", 0x800))
                      .power == 1.5,
              "Registration survives export/import");
        Check(Near(static_cast<float>(*ComputeMatchedImpactRatio(1, 5000, .5)), .5f),
              "Impact strength reduces physical response");
        Check(*ComputeMatchedImpactRatio(8, 10000, 2) == 1.55, "Impact strength respects upper bound");
        Check(!ComputeMatchedImpactRatio(1, 5000, -1), "Invalid strength rejected");
        Check(ResolvePhysicsCatalog(ParseConfig("")).bows.contains(CanonicalFormKey("Skyrim.esm", 0x3B562)),
              "No overlay preserves standalone default catalogue");
        Check(OrdinaryArrowPhysics(64, 0, 1, 3600, 1), "Ordinary native arrow is eligible");
        for (unsigned flag : {1u, 2u, 4u})
            Check(!OrdinaryArrowPhysics(64, static_cast<std::uint16_t>(flag), 1, 3600, 1),
                  "Hitscan/explosion/alternate explosion must be excluded at runtime");
        for (double invalidPhysics :
             {0., -1., std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
            Check(!OrdinaryArrowPhysics(64, 0, invalidPhysics, 3600, 1),
                  "Invalid native gravity is excluded");
            Check(!OrdinaryArrowPhysics(64, 0, 1, invalidPhysics, 1), "Invalid native speed is excluded");
        }
        Check(!OrdinaryArrowPhysics(65, 0, 1, 3600, 1), "Mixed missile/arrow projectile types are excluded");
        std::cout << "User overlay and generic catalogue tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
