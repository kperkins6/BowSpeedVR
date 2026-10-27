#include "BowSpeed/FunctionGuard.h"
#include "BowSpeed/Runtime.h"
#include "Check.h"
#include <array>
void GuardTests() {
    using namespace BowSpeed;
    Check(SupportedSKSEVersion(false,0x010400F1), "Steam SKSEVR 1.4.15.1 must load");
    Check(SupportedSKSEVersion(false,0x010400F0), "Unflagged 1.4.15 version supported");
    Check(!SupportedSKSEVersion(true,0x010400F1), "Editor rejected");
    Check(!SupportedSKSEVersion(false,0x010400E1), "Older VR runtime rejected");
    Check(!SupportedSKSEVersion(false,0x010400F2), "Unknown platform flag rejected");
    Check(!SupportedSKSEVersion(false,0x01050610), "Flat Skyrim runtime rejected");
    const std::array<std::uint8_t,16> neutral={0xF3,0x0F,0x10,0x05,0x10,0,0,0,0xC3,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC,0xCC};
    Check(IsKnownNeutralFunction(neutral,1.0f), "Recognized constant-one return can be trusted");
    Check(!IsKnownNeutralFunction(neutral,1.2f), "Changed RIP constant must reject activation");
    Check(!IsKnownNeutralFunction(neutral,std::nullopt), "Unreadable constant must reject activation");
    auto detour=neutral;detour[0]=0xE9;
    Check(!IsKnownNeutralFunction(detour,1.0f), "Same-address inline detour must reject activation");
    const std::array<std::uint8_t,10> immediate={0xB8,0,0,0x80,0x3F,0x66,0x0F,0x6E,0xC0,0xC3};
    Check(IsKnownNeutralFunction(immediate,std::nullopt), "Recognized immediate-one return");
    auto altered=immediate;altered[3]=0x40;
    Check(!IsKnownNeutralFunction(altered,std::nullopt), "Changed immediate rejects activation");
    Check(!IsKnownNeutralFunction(std::span(neutral).first(5),1.0f), "Truncated function rejected");
    const std::array<std::uint8_t,34> sourceGetter={
        0x48,0x8B,0x81,0xB0,0x01,0,0,0x48,0x85,0xC0,0x74,0x0D,
        0xF3,0x0F,0x10,0x80,0x70,0x01,0,0,0xE9,0x17,0x1E,0xC7,0xFF,
        0xF3,0x0F,0x10,0x05,0xE7,0xDA,0xE3,0,0xC3};
    Check(IsKnownWeaponFunction(sourceGetter,1.0f), "Captured VR source-weapon getter recognized");
    Check(!IsKnownWeaponFunction(sourceGetter,1.2f), "Modified null-source constant rejected");
    Check(!IsKnownWeaponFunction(sourceGetter,std::nullopt), "Missing null-source constant rejected");
    auto changedSource=sourceGetter;changedSource[16]=0x74;
    Check(!IsKnownWeaponFunction(changedSource,1.0f), "Changed weapon field read rejected");
    changedSource=sourceGetter;changedSource[21]^=1;
    Check(!IsKnownWeaponFunction(changedSource,1.0f), "Redirected tail helper rejected");
    changedSource=sourceGetter;changedSource[0]=0xE9;
    Check(!IsKnownWeaponFunction(changedSource,1.0f), "Source getter detour rejected");
    Check(!IsKnownWeaponFunction(std::span(sourceGetter).first(33),1.0f), "Incomplete source getter rejected");
    Check(OverridePermitted(false,true,0.7f,true), "Observed Long Bow scalar permitted");
    Check(OverridePermitted(false,true,0.775f,true), "Existing non-neutral bow scalar preserved for scaling");
    Check(!OverridePermitted(true,true,0.7f,true), "Observed source getter still obeys observation");
    Check(OverridePermitted(false,true,1.0f,true), "Verified active override permitted");
    Check(!OverridePermitted(true,true,1.0f,true), "Observation always pass-through");
    Check(!OverridePermitted(false,false,1.0f,true), "Unknown original implementation pass-through");
    Check(!OverridePermitted(false,true,-1.0f,true), "Invalid engine scalar pass-through");
    Check(!OverridePermitted(false,true,1.0f,false), "Late code patch pass-through");
    Check(HookWriteSucceeded(10,10,20,20), "Successful installation read-back");
    Check(!HookWriteSucceeded(10,10,20,10), "Failed write must not report installed");
    Check(!HookWriteSucceeded(10,11,20,20), "Conflicted previous pointer must reject installation");
}
