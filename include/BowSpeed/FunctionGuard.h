#pragma once
#include <cstdint>
#include <optional>
#include <span>
#include <array>
#include <algorithm>
#include <cmath>
namespace BowSpeed {
inline bool IsKnownNeutralFunction(std::span<const std::uint8_t> code,std::optional<float> ripConstant) {
    // x64: movss xmm0,[rip+disp32]; ret. No calls, branches or side effects.
    constexpr std::array<std::uint8_t,4> prefix={0xF3,0x0F,0x10,0x05};
    if (code.size()>=9 && std::equal(prefix.begin(),prefix.end(),code.begin()) && code[8]==0xC3)
        return ripConstant && *ripConstant==1.0f;
    // x64: mov eax,3F800000h; movd xmm0,eax; ret.
    constexpr std::array<std::uint8_t,10> immediate={0xB8,0,0,0x80,0x3F,0x66,0x0F,0x6E,0xC0,0xC3};
    return code.size()>=immediate.size() && std::equal(immediate.begin(),immediate.end(),code.begin());
}
inline bool IsKnownWeaponFunction(std::span<const std::uint8_t> code,std::optional<float> ripConstant) {
    if (IsKnownNeutralFunction(code,ripConstant)) return true;
    // Captured from the verified 1.4.15 VR entry at RVA 0x75CB20.
    // Read weaponSource +0x1B0, then record speed +0x170, and tail-call
    // RVA 0x3CE950. Null weapon returns the RIP constant at RVA 0x159A628.
    constexpr std::array<std::uint8_t,34> sourceGetter={
        0x48,0x8B,0x81,0xB0,0x01,0,0,0x48,0x85,0xC0,0x74,0x0D,
        0xF3,0x0F,0x10,0x80,0x70,0x01,0,0,0xE9,0x17,0x1E,0xC7,0xFF,
        0xF3,0x0F,0x10,0x05,0xE7,0xDA,0xE3,0,0xC3};
    return code.size()>=sourceGetter.size() && ripConstant && *ripConstant==1.0f &&
        std::equal(sourceGetter.begin(),sourceGetter.end(),code.begin());
}
inline bool OverridePermitted(bool observation,bool trusted,float original,bool unchanged) {
    // Scale the original contribution, preserving non-neutral bow differences.
    return !observation && trusted && std::isfinite(original) && original>0.0f && unchanged;
}
inline bool HookWriteSucceeded(std::uintptr_t expectedOriginal,std::uintptr_t previous,std::uintptr_t hook,std::uintptr_t readBack) {
    return previous==expectedOriginal && readBack==hook;
}
}
