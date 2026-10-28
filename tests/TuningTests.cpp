#include "BowSpeed/Tuning.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/InventoryWeight.h"
#include "Check.h"
#include <fstream>
#include <iostream>
#include <limits>

int main(int argc, char** argv) {
    try {
        using namespace BowSpeed;
        Check(ParseConfig("[Immersion]\nArrowInventoryWeight=true\n").arrowInventoryWeight, "Immersion inventory-weight flag must enable the feature");
        Check(!ParseConfig("").arrowInventoryWeight, "Inventory weight must default off");
        const auto invalidWeight=ParseConfig("[Immersion]\nArrowInventoryWeight=true\nArrowInventoryWeight=maybe\n");
        Check(invalidWeight.arrowInventoryWeight && invalidWeight.warnings.size()==1, "Invalid inventory-weight flag retains prior valid choice");
        const auto defaults=ParseConfig("");
        Check(!defaults.mgoProfiles && !defaults.npcSpeedEnabled && !defaults.compensateRicochet, "Standalone defaults do not opt into NPC or MGO policies");
        Check(defaults.pairedPlayerSpeedMult==1 && defaults.npcSpeedMult==1, "Independent tuning defaults are neutral");
        const auto vanilla=ResolveReferenceSettings(ParseConfig("[Pairing]\nReferencePreset=Vanilla\nReferenceSpeed=9000\n"));
        Check(vanilla && Near(static_cast<float>(vanilla->referenceSpeed),2520), "Vanilla reference ignores custom baseline and calibrates to native Long+Iron");
        const auto longShot=ComputeMatchedShot(*vanilla,1,1,1.8,1);
        Check(longShot && Near(static_cast<float>(longShot->launchSpeed),2520), "Vanilla calibrated full-draw Long+Iron matches 3600 times .7");
        const auto daedricIron=ComputeMatchedShot(*vanilla,2.0408163265306123,1,1.8,1);
        Check(daedricIron && Near(static_cast<float>(daedricIron->launchSpeed),3024), "Vanilla reference preserves pairing speed differences");
        const auto scaled=ResolveReferenceSettings(ParseConfig("[Pairing]\nReferencePreset=Tuned\nReferenceSpeed=5000\nPlayerSpeedMult=1.1\n"));
        Check(scaled && Near(static_cast<float>(scaled->referenceSpeed),5500), "Player setting scales the actual reference exactly once");
        Check(BindingInScope(false,"Skyrim.esm") && BindingInScope(false,"DAWNGUARD.ESM") && BindingInScope(false,"Dragonborn.esm"), "Standalone accepts core DLC identities case-insensitively");
        Check(!BindingInScope(false,"Artificer.esp") && BindingInScope(true,"Artificer.esp"), "MGO bindings cannot enter standalone without opt-in");
        const auto good=ParseConfig("[SpeedTuning]\nNPCEnabled=true\nNPCSpeedMult=0.8\nCompensateRicochet=true\n[Compatibility]\nMGOProfiles=true\n");
        Check(good.npcSpeedEnabled && good.compensateRicochet && good.mgoProfiles && good.npcSpeedMult==.8, "MGO policy fields resolve independently");
        for (const std::string invalid:{"nan","inf","0.49","1.51","1junk",""}) {
            const auto cfg=ParseConfig("[Pairing]\nPlayerSpeedMult=1.1\nPlayerSpeedMult="+invalid+"\n[SpeedTuning]\nNPCSpeedMult=0.8\nNPCSpeedMult="+invalid+"\n");
            Check(cfg.pairedPlayerSpeedMult==1.1 && cfg.npcSpeedMult==.8 && cfg.warnings.size()==2, "Invalid duplicate tuning retains valid prior choices");
        }
        const auto invalidPreset=ParseConfig("[Pairing]\nReferencePreset=Vanilla\nReferencePreset=bad\n");
        Check(invalidPreset.referencePreset==ReferencePreset::Vanilla && invalidPreset.warnings.size()==1, "Invalid preset retains previous calibration");
        BowTuningEligibility npc{true,false,true,true,true,true,false};
        Check(Near(SelectUnpairedBowFactor(npc,.7f,.7f,1,.8,1,false,false),.7f), "Standalone leaves NPC getter unchanged");
        Check(Near(SelectUnpairedBowFactor(npc,.7f,.7f,1,1,.8,true,true),.875f), "MGO NPC default cancels existing .8 global slowdown");
        Check(Near(SelectUnpairedBowFactor(npc,.7f,.7f,1,.8,.8,true,true),.7f), "MGO NPC .8 preserves tested native getter over Ricochet .8");
        Check(Near(SelectUnpairedBowFactor(npc,.7f,.7f,1,.9,1,true,false),.63f), "NPC preset works without Ricochet");
        auto player=npc; player.player=true;
        auto completePlayer=player;completePlayer.completeCatalog=true;
        for (const char* reason:{"unknown bow", "unknown arrow", "explicit exclusion", "gravity exception"})
            Check(SelectUnpairedBowFactor(completePlayer,.7f,1.3f,1.5,.8,.8,true,true)==.7f,reason);
        auto completeNPC=npc;completeNPC.completeCatalog=true;
        Check(Near(SelectUnpairedBowFactor(completeNPC,.7f,.7f,1.5,.8,.8,true,true),.7f),"Complete catalogue retains explicit NPC tuning");
        Check(Near(SelectUnpairedBowFactor(player,.7f,.7f,1,1,.8,true,true),.875f), "Unpaired eligible player bow compensates once");
        Check(Near(SelectUnpairedBowFactor(player,.7f,.7f,1.1,.8,.8,true,true),.9625f), "Independent player choice does not inherit NPC setting");
        for (int gate=0;gate<6;++gate) {
            auto excluded=npc;
            if (gate==0) excluded.trusted=false;
            if (gate==1) excluded.observation=true;
            if (gate==2) excluded.actor=false;
            if (gate==3) excluded.bow=false;
            if (gate==4) excluded.physicalArrow=false;
            if (gate==5) excluded.ordinaryForce=false;
            Check(SelectUnpairedBowFactor(excluded,.7f,.7f,1,.8,.8,true,true)==.7f, "Copies, helpers, spells, bolts and untrusted paths cannot get NPC correction");
        }
        Check(SelectUnpairedBowFactor(npc,.7f,.7f,1,1,std::nullopt,true,true)==.7f, "Unavailable Ricochet compensation passes through");
        for (double invalid:{0.0,-1.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            Check(SelectUnpairedBowFactor(npc,.7f,.7f,1,1,invalid,true,true)==.7f, "Invalid compensation cannot amplify NPCs");
        Check(RicochetSpeedFromIni("[Global]\nfProjectileSpeedMult=0.8\n")==.8, "Global Ricochet speed reads exact section");
        Check(RicochetSpeedFromIni("[Other]\nfProjectileSpeedMult=0.8\n")==1, "Unrelated key cannot affect compensation");
        Check(!RicochetSpeedFromIni("[Global]\nfProjectileSpeedMult=nan\n"), "Malformed Ricochet setting refuses correction");
        Check(RicochetSpeedFromIni("[GLOBAL]\nfProjectileSpeedMult=0.1 ; framework clamps\n")==.3, "Compensation follows framework clamp");
        for (int i=1;i<argc;++i) {
            std::ifstream input(argv[i]); Check(input.good(), "Preset INI must exist");
            const std::string ini{std::istreambuf_iterator<char>(input),{}};
            const auto cfg=ParseConfig(ini);
            Check(cfg.warnings.empty() && cfg.enabled && !cfg.observeOnly && cfg.pairingEnabled, "Generated preset parses and enables pairing without warnings");
            const auto reference=ResolveReferenceSettings(cfg); Check(reference.has_value(), "Generated preset produces valid reference");
            Check(BuildInventoryWeightProfiles(cfg).empty()!=cfg.arrowInventoryWeight, "Generated inventory-weight flag resolves or suppresses the catalogue");
            if (!cfg.mgoProfiles) Check(!cfg.npcSpeedEnabled && !cfg.compensateRicochet && !cfg.arrowPhysics && cfg.playerSpeedMult==1 && cfg.playerCrossbowSpeedMult==1, "Main configuration leaves NPC, fallback and crossbow speed unchanged");
            std::cout << argv[i] << "|" << reference->referenceSpeed << "|" << cfg.npcSpeedMult << "|" << cfg.npcSpeedEnabled << "|" << cfg.arrowInventoryWeight << '\n';
        }
        std::cout << "Standalone/MGO tuning behavior passed\n";
        return 0;
    } catch (const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
