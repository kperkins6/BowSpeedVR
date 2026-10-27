#include "BowSpeed/Config.h"
#include "Check.h"
void ConfigTests() {
    const auto releaseSettings=BowSpeed::ParseConfig("[Pairing]\nReferencePreset=Vanilla\nPlayerSpeedMult=1.1\n[Compatibility]\nMGOProfiles=true\n[SpeedTuning]\nNPCEnabled=true\nNPCSpeedMult=0.8\nCompensateRicochet=true\n");
    Check(releaseSettings.warnings.empty(), "Standalone and MGO speed settings must be recognized");
    using namespace BowSpeed;
    Check(!ParseConfig("[ArrowPhysics]\nMassInfluence=nan\n").warnings.empty(), "Invalid arrow mass influence must warn instead of silently enabling an unsafe calculation");
    const FormKey daedric{"Skyrim.esm", 0x139B5};
    const FormKey longbow{"Skyrim.esm", 0x3B562};
    const auto defaults = ParseConfig("");
    Check(!defaults.arrowPhysics, "Missing INI never activates ammo physics");
    const auto mass=ParseConfig("[ArrowPhysics]\nEnabled=true\nMassInfluence=0.2\n[AmmoClasses]\nSkyrim.esm|0x0010B0A7=ExtraLight\nSkyrim.esm|0x000139BB=Heavy\nExample.esp|0x800=Light\n");
    Check(mass.arrowPhysics && mass.massInfluence==0.2f && mass.warnings.empty(), "Approved arrow config parses cleanly");
    Check(mass.ammoClasses.at({"Skyrim.esm",0x10B0A7})==AmmoClass::ExtraLight, "ExtraLight is a supported Bound class");
    Check(mass.ammoClasses.at({"example.esp",0x800})==AmmoClass::Light, "Explicit mod ammo overrides are supported");
    for (const std::string value : {"nan","inf","-0.1","0.41","0.2junk",""}) {
        const auto invalid=ParseConfig("[ArrowPhysics]\nMassInfluence="+value+"\n");
        Check(invalid.massInfluence==0 && !invalid.warnings.empty(), "Invalid influence disables mass differences safely");
    }
    const auto invalidClass=ParseConfig("[AmmoClasses]\nSkyrim.esm|0x10B0A7=invalid\n");
    Check(invalidClass.ammoClasses.at({"Skyrim.esm",0x10B0A7})==AmmoClass::Medium && !invalidClass.warnings.empty(), "Invalid class neutralizes a built-in mapping");
    Check(defaults.bows.empty(), "Missing file must not inject asymmetric bow overrides");
    Check(defaults.playerSpeedMult==1.0f, "Missing file preserves player baseline");
    Check(defaults.playerCrossbowSpeedMult==1.0f, "Missing file preserves player crossbow baseline");
    Check(defaults.enabled && defaults.observeOnly && defaults.diagnosticLogging, "Observation default");
    Check(ParseConfig("[Unrelated]\nfoo=bar\n").bows==defaults.bows, "Missing sections keep defaults");
    for (const std::string value : {"nan", "inf", "1.3junk", "0.9", "1.31", "", "1e1000"}) {
        const auto invalid=ParseConfig("[Bows]\nSkyrim.esm|0x000139B5="+value+"\n");
        Check(invalid.bows.at(daedric)==1.0f && !invalid.warnings.empty(), "Bad factor must warn and use neutral");
    }
    const auto duplicate=ParseConfig("[Bows]\nSkyrim.esm|0x000139B5=1.1\nSkyrim.esm|0x000139B5=1.2\n");
    Check(duplicate.bows.at(daedric)==1.2f, "Last valid duplicate wins");
    for (const std::string key : {"|0x123", "Skyrim.esm|bad", "Skyrim.esm|0x1000000", "Skyrim.esm|0x123junk", "NoPlugin", "../evil.esm|0x123"}) {
        const auto invalid=ParseConfig("[Bows]\n"+key+"=1.2\n");
        Check(invalid.bows==defaults.bows && !invalid.warnings.empty(), "Malformed form key must warn and skip");
    }
    const auto settings=ParseConfig("[General]\nEnabled=false\nObserveOnly=false\nDiagnosticLogging=false\n");
    Check(!settings.enabled && !settings.observeOnly && !settings.diagnosticLogging, "Boolean settings");
    const auto invalidBool=ParseConfig("[General]\nObserveOnly=perhaps\n");
    Check(invalidBool.observeOnly && !invalidBool.warnings.empty(), "Bad boolean retains observation");
    Check(ParseConfig("[General]\nPlayerSpeedMult=1.3\n").playerSpeedMult==1.3f, "Player speed default is configurable");
    const auto crossbowSettings=ParseConfig("[General]\nPlayerSpeedMult=1.3\nPlayerCrossbowSpeedMult=1.25\n");
    Check(crossbowSettings.warnings.empty(), "Independent player crossbow speed setting must be accepted");
    Check(crossbowSettings.playerSpeedMult==1.3f && crossbowSettings.playerCrossbowSpeedMult==1.25f, "Bow and crossbow defaults are stored independently");
    for (const std::string value : {"nan","inf","1.3junk","0.9","1.31","","1e1000"}) {
        const auto invalid=ParseConfig("[General]\nPlayerSpeedMult="+value+"\n");
        Check(invalid.playerSpeedMult==1.0f && !invalid.warnings.empty(), "Invalid player multiplier warns and preserves baseline");
        const auto invalidCrossbow=ParseConfig("[General]\nPlayerSpeedMult=1.3\nPlayerCrossbowSpeedMult="+value+"\n");
        Check(invalidCrossbow.playerCrossbowSpeedMult==1.0f && invalidCrossbow.playerSpeedMult==1.3f && !invalidCrossbow.warnings.empty(), "Invalid crossbow multiplier is neutral without changing bow default");
    }
    const auto spaced=ParseConfig("[Bows]\n Skyrim.esm | 0x000139B5 = 1.2 ; note\n");
    Check(spaced.bows.at(daedric)==1.2f, "Whitespace and comments");
}
