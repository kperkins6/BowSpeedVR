#include "BowSpeed/Config.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "Check.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

namespace {
bool Close(double a, double b) { return std::abs(a-b)<1e-12; }
void CatalogTests() {
    using namespace BowSpeed;
    Check(BowPhysicsProfiles().size()==34 && ArrowPhysicsProfiles().size()==21, "All design profiles remain available, including opt-in profiles");
    const auto* daedric=FindBowPhysicsProfile("DAEDRIC bow");
    Check(daedric && Close(daedric->power,2.0408163265306123), "Power uses Q / 1.25 at full double precision");
    const auto* iron=FindArrowPhysicsProfile("iRoN");
    const auto* bound=FindArrowPhysicsProfile("Bound");
    Check(iron && Close(iron->mass,1.0) && Close(iron->capacityQ,1.8), "Iron retains independent mass and capacity");
    Check(bound && Close(bound->mass,.55), "Bound mass is fixed across bows");
    Check(!FindBowPhysicsProfile("unknown") && !FindArrowPhysicsProfile(""), "Unknown profiles cannot silently select defaults");
    Check(FindBowPhysicsProfile("Daedric Bow")==daedric, "Lookups return stable immutable profile pointers");
    Check(DefaultBowPhysicsBindings().size()==588, "Reviewed player/summoned bow bindings exclude six opt-in Vigilant candidates");
    std::set<std::pair<std::string_view,std::uint32_t>> bows, arrows;
    for (const auto& b:DefaultBowPhysicsBindings()) {
        Check(FindBowPhysicsProfile(b.profile)!=nullptr, "Every default bow binding resolves a profile");
        Check(bows.emplace(b.plugin,b.localID).second, "Bow bindings are unique");
        Check(b.plugin!="vigilant.esm" && b.plugin!="vrik.esp" && b.plugin!="spellwheelvr.esp" && b.plugin!="weaponthrowvr.esp", "Special and VR helper bows remain absent");
    }
    Check(!bows.contains({"skyrim.esm",0x10EC8A}) && !bows.contains({"skyrim.esm",0xA5DF0}), "Creature and illusion launchers remain absent");
    Check(bows.contains({"skyrim.esm",0x424F8}) && bows.contains({"skyrim.esm",0x58F60}), "Reviewed bound bows remain eligible for runtime player-source guard");
    Check(DefaultArrowPhysicsBindings().size()==25, "Exactly the reviewed player arrow candidates bind by default");
    std::size_t gravity=0;
    for (const auto& a:DefaultArrowPhysicsBindings()) {
        Check(FindArrowPhysicsProfile(a.profile)!=nullptr, "Every default arrow binding resolves a profile");
        Check(arrows.emplace(a.plugin,a.localID).second, "Ammo bindings are unique");
        Check(a.plugin!="vigilant.esm" && a.plugin!="weaponthrowvr.esp", "Special ammunition remains absent");
        const bool expected=(a.plugin=="dis_heavy_legion.esp" && a.localID==0x898) ||
            (a.plugin=="legacyofthedragonborn.esm" && a.localID==0x236D52) ||
            (a.plugin=="elvenia armor.esp" && a.localID==0x816);
        Check(a.requiresGravityNormalization==expected, "Only Imperial, Ancient Falmer and Elvenia need gravity normalization");
        gravity+=a.requiresGravityNormalization;
    }
    Check(gravity==3, "All three gravity exceptions are explicitly flagged");
    Check(!arrows.contains({"skyrim.esm",0x10EC8C}) && !arrows.contains({"dragonborn.esm",0x17720}), "Creature bolt and thrown spear remain absent");
    Check(!arrows.contains({"archeryv2.esp",0x19E0}) && !arrows.contains({"archeryv2.esp",0xBC07}), "Development tests remain absent");
    Check(!arrows.contains({"3dnpc.esp",0x2CFA85}) && !arrows.contains({"3dnpc.esp",0x2DC400}), "Quest effects remain absent");
}
void PairingConfigTests(const char* packagePath) {
    using namespace BowSpeed;
    const auto defaults=ParseConfig("[General]\nEnabled=true\n");
    Check(!defaults.pairingEnabled && defaults.pairingMomentumImpact, "Old configuration stays in legacy mode");
    Check(Close(defaults.pairingSettings.referenceSpeed,5000) && Close(defaults.pairingSettings.virtualBowMass,.25) && Close(defaults.pairingSettings.maxSpeedRatio,1.25) && defaults.pairingSettings.capacityLimits, "Approved numeric defaults");
    Check(defaults.pairingBowPower.empty() && defaults.pairingAmmoMass.empty() && defaults.pairingAmmoCapacity.empty(), "Missing overrides do not invent equipment bindings");
    const auto valid=ParseConfig("[PAIRING]\nEnabled=true\nReferenceSpeed=6000\nVirtualBowMass=0.5\nMaxSpeedRatio=1.1\nCapacityLimits=false\nMomentumImpact=false\n[PairingBowPower]\nSkYrIm.EsM|0x139B5=2\n[PairingAmmoMass]\nEXAMPLE.ESP|0x800=1.75\n[PairingAmmoCapacity]\nexample.esp|0x800=3.25\n");
    Check(valid.warnings.empty() && valid.pairingEnabled && !valid.pairingMomentumImpact && !valid.pairingSettings.capacityLimits, "Pairing toggles parse independently");
    Check(Close(valid.pairingSettings.referenceSpeed,6000) && Close(valid.pairingSettings.virtualBowMass,.5) && Close(valid.pairingSettings.maxSpeedRatio,1.1), "Numeric settings are stored");
    Check(valid.pairingBowPower.at({"Skyrim.esm",0x139B5})==2 && valid.pairingAmmoMass.at({"example.esp",0x800})==1.75f && valid.pairingAmmoCapacity.at({"example.esp",0x800})==3.25f, "Case-insensitive plugin identity and per-form override sections");
    struct Range { const char* section; const char* key; const char* low; const char* high; const char* below; const char* above; };
    for (const auto& r: {
        Range{"Pairing","ReferenceSpeed","1000","10000","999","10001"},
        Range{"Pairing","VirtualBowMass","0","2","-0.01","2.01"},
        Range{"Pairing","MaxSpeedRatio","0.5","1.5","0.49","1.51"},
        Range{"PairingBowPower","Skyrim.esm|0x139B5","0.1","8","0.09","8.01"},
        Range{"PairingAmmoMass","Skyrim.esm|0x1397D","0.1","8","0.09","8.01"},
        Range{"PairingAmmoCapacity","Skyrim.esm|0x1397D","0.1","20","0.09","20.01"}}) {
        const std::string prefix="["+std::string(r.section)+"]\n"+r.key+"=";
        Check(ParseConfig(prefix+r.low+"\n").warnings.empty() && ParseConfig(prefix+r.high+"\n").warnings.empty(), "Safe range endpoints are inclusive");
        for (const std::string invalid:{"nan","inf","-inf","1e1000","1junk","",r.below,r.above}) {
            const auto prior=ParseConfig(prefix+r.high+"\n");
            const auto cfg=ParseConfig(prefix+r.high+"\n"+r.key+"="+invalid+"\n");
            Check(!cfg.warnings.empty(), "Unsafe input warns");
            Check(cfg.pairingSettings.referenceSpeed==prior.pairingSettings.referenceSpeed && cfg.pairingSettings.virtualBowMass==prior.pairingSettings.virtualBowMass && cfg.pairingSettings.maxSpeedRatio==prior.pairingSettings.maxSpeedRatio && cfg.pairingBowPower==prior.pairingBowPower && cfg.pairingAmmoMass==prior.pairingAmmoMass && cfg.pairingAmmoCapacity==prior.pairingAmmoCapacity, "Invalid duplicate retains the previous valid value");
        }
    }
    for (const std::string section:{"PairingBowPower","PairingAmmoMass","PairingAmmoCapacity"}) {
        for (const std::string key:{"|0x123","Skyrim.esm|bad","Skyrim.esm|0x0","Skyrim.esm|0x1000000","Skyrim.esm|0x123junk","NoPlugin","../evil.esm|0x123"}) {
            const auto cfg=ParseConfig("["+section+"]\n"+key+"=2\n");
            Check(!cfg.warnings.empty() && cfg.pairingBowPower.empty() && cfg.pairingAmmoMass.empty() && cfg.pairingAmmoCapacity.empty(), "Malformed keys cannot bind equipment");
        }
    }
    const auto badBool=ParseConfig("[Pairing]\nEnabled=true\nEnabled=perhaps\nCapacityLimits=false\nCapacityLimits=1\nMomentumImpact=false\nMomentumImpact=yes\nUnknown=1\n");
    Check(badBool.pairingEnabled && !badBool.pairingSettings.capacityLimits && !badBool.pairingMomentumImpact && badBool.warnings.size()==4, "Invalid booleans retain prior settings and unknown keys warn");
    std::string noisy="[Pairing]\n";
    for (int i=0;i<1000;++i) noisy+="ReferenceSpeed=nan\n";
    Check(ParseConfig(noisy).warnings.size()<=64, "Warnings stay bounded for hostile input");
    std::ifstream package(packagePath);
    Check(package.good(), "Pairing package INI exists");
    const std::string ini{std::istreambuf_iterator<char>(package),{}};
    const auto packaged=ParseConfig(ini);
    Check(packaged.warnings.empty() && packaged.enabled && !packaged.observeOnly && packaged.diagnosticLogging && packaged.arrowPhysics && packaged.pairingEnabled && packaged.pairingMomentumImpact, "Shipped INI activates approved pairing mode");
    Check(packaged.playerSpeedMult==1.25f && packaged.playerCrossbowSpeedMult==1.25f, "Package retains explicit legacy multipliers");
}
}
int main(int argc,char** argv) {
    try {
        Check(argc==2,"Pass the package configuration path");
        CatalogTests();
        PairingConfigTests(argv[1]);
        std::cout<<"Physics catalogue and pairing configuration passed\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr<<e.what()<<'\n';
        return 1;
    }
}
