#include "BowSpeed/Policy.h"
#include "BowSpeed/Eligibility.h"
#include "Check.h"
#include <iostream>
#include <limits>
void ConfigTests();
void GuardTests();
void DiagnosticTests();
void AmmoPhysicsTests();
int main(int argc, char** argv) {
    try {
        if (argc==2) {
            if (std::string(argv[1])=="--guards") GuardTests();
            else if (std::string(argv[1])=="--diagnostics") DiagnosticTests();
            else if (std::string(argv[1])=="--ammo") AmmoPhysicsTests();
            else throw std::runtime_error("Unknown test group");
            std::cout << "Behavior checks passed\n";return 0;
        }
        using BowSpeed::SelectWeaponFactor;
        const float longBow=SelectWeaponFactor(true,true,std::nullopt,0.7f,1.3f);
        const float daedric=SelectWeaponFactor(true,true,std::nullopt,1.0f,1.3f);
        Check(Near(longBow,0.91f), "Long Bow baseline must be multiplied by 1.3");
        Check(Near(daedric,1.3f), "Daedric baseline must be multiplied by 1.3");
        Check(Near(daedric/longBow,1.0f/0.7f), "Existing bow-speed ratio must remain unchanged");
        Check(SelectWeaponFactor(false, true, 1.3f, 0.8f,1.3f) == 0.8f, "NPC must retain original");
        Check(SelectWeaponFactor(true, false, 1.3f, 0.8f,1.3f) == 0.8f, "Excluded weapon must retain original");
        using BowSpeed::SelectPlayerWeaponFactor;
        const float crossbow=SelectPlayerWeaponFactor(true,false,true,1.3f,1.0f,1.3f,1.25f);
        Check(Near(crossbow,1.25f), "Player crossbow uses its separate multiplier and ignores bow overrides");
        Check(Near(8100.0f*0.8f*crossbow,8100.0f), "Player crossbow compensation cancels Ricochet slowdown");
        Check(SelectPlayerWeaponFactor(false,false,true,1.3f,0.9f,1.3f,1.25f)==0.9f, "NPC crossbow retains native contribution");
        Check(SelectPlayerWeaponFactor(false,true,false,1.3f,0.7f,1.3f,1.25f)==0.7f, "NPC bow retains native contribution");
        Check(Near(SelectPlayerWeaponFactor(true,true,false,1.1f,0.7f,1.3f,1.25f),0.77f), "Per-bow overrides remain independent of crossbow setting");
        Check(SelectPlayerWeaponFactor(true,false,false,1.3f,0.8f,1.3f,1.25f)==0.8f, "Unrelated player projectile remains unchanged");
        Check(SelectPlayerWeaponFactor(true,false,true,std::nullopt,0.8f,1.3f,1.31f)==0.8f, "Invalid crossbow multiplier passes through");
        Check(SelectPlayerWeaponFactor(true,false,true,std::nullopt,0.0f,1.3f,1.25f)==0.0f, "Zero original crossbow contribution passes through");
        Check(Near(SelectWeaponFactor(true,true,std::nullopt,0.775f,1.3f),1.0075f), "Unmapped player bows use the player default");
        Check(Near(SelectWeaponFactor(true,true,1.1f,0.7f,1.3f),0.77f), "Per-bow setting replaces default multiplier without stacking");
        Check(SelectWeaponFactor(true,true,1.0f,0.7f,1.3f)==0.7f, "Per-bow neutral override preserves baseline");
        for (float invalid : {0.9f, 1.31f, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()})
            Check(SelectWeaponFactor(true, true, invalid, 0.8f,1.3f) == 0.8f, "Invalid mapped multiplier must preserve original");
        Check(SelectWeaponFactor(true,true,std::nullopt,0.8f,1.31f)==0.8f, "Invalid default multiplier preserves original");
        float result=0;
        for (int i=0; i<1000; ++i) result=SelectWeaponFactor(true,true,std::nullopt,0.7f,1.3f);
        Check(Near(result,0.91f), "Repeated original-function calculations must not accumulate");
        Check(Near(5553.6567f * 0.5f * result * 0.5f,1263.4569f), "Partial draw and restitution must remain separate");
        Check(SelectWeaponFactor(true,true,std::nullopt,-1.0f,1.3f)==-1.0f, "Invalid engine scalar passes through");
        Check(BowSpeed::CanInstallHook(true,true,true,0x75CB20), "Expected VR target should pass hook guard");
        Check(!BowSpeed::CanInstallHook(false,true,true,0x75CB20), "Foreign runtime rejected");
        Check(!BowSpeed::CanInstallHook(true,false,true,0x75CB20), "Unreadable table rejected");
        Check(!BowSpeed::CanInstallHook(true,true,false,0x75CB20), "Nonexecutable function rejected");
        Check(!BowSpeed::CanInstallHook(true,true,true,0x75CB21), "Changed target rejected");
        int sourceToken=0, unmappedToken=0;
        Check(BowSpeed::UsableRangedSource(true,1.0f), "Long Bow source is usable");
        Check(BowSpeed::UsableRangedSource(true,0.5f), "Daedric or crossbow source with valid speed is usable");
        Check(!BowSpeed::UsableRangedSource(true,0.0f), "Synthetic ranged weapon with zero record speed must pass through");
        Check(!BowSpeed::UsableRangedSource(false,1.0f), "Unrelated weapon source is unusable");
        Check(!BowSpeed::UsableRangedSource(true,std::numeric_limits<float>::quiet_NaN()), "Invalid ranged record speed is unusable");
        const BowSpeed::Factors factors{{&sourceToken,1.3f}};
        Check(BowSpeed::ResolveEligibility(true,true,&sourceToken,factors).factor==1.3f, "Source bow selects its factor");
        Check(!BowSpeed::ResolveEligibility(true,false,&sourceToken,factors).factor, "Non-bow form rejected");
        Check(!BowSpeed::ResolveEligibility(false,true,&sourceToken,factors).factor, "NPC eligibility rejected");
        Check(!BowSpeed::ResolveEligibility(true,true,nullptr,factors).factor, "Missing source rejected");
        Check(!BowSpeed::ResolveEligibility(true,true,&unmappedToken,factors).factor, "Unmapped source rejected");
        if (argc == 1) ConfigTests();
        std::cout << "Behavior checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; return 1;
    }
}
