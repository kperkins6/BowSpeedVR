#pragma once
#include <algorithm>
#include <array>
#include <cstdint>
#include <span>
namespace BowSpeed {
inline constexpr std::uintptr_t ImpactContextRVA=0x77EE85;
inline constexpr std::uintptr_t ImpactCallRVA=0x77EE97;
inline constexpr std::uintptr_t NativeForceRVA=0x3B49C0;
// VR 1.4.15: R14 holds the projectile; XMM0 force, RDX collision object,
// R8B true are passed to the force scaler. R9 is free for our extra context.
inline constexpr std::array<std::uint8_t,26> ImpactContext={
    0x49,0x8B,0x46,0x40,0x41,0xB0,0x01,0x48,0x8B,0xD3,
    0xF3,0x0F,0x10,0x80,0xC8,0x00,0x00,0x00,
    0xE8,0x24,0x5B,0xC3,0xFF,0x0F,0x28,0xF8};
inline constexpr std::array<std::uint8_t,25> NativeForceEntry={
    0x48,0x83,0xEC,0x48,0x0F,0x29,0x7C,0x24,0x20,0x0F,0x28,0xF8,
    0x48,0x85,0xD2,0x0F,0x84,0xBE,0x00,0x00,0x00,0x48,0x8B,0x52,0x10};
inline bool RecognizedImpactPath(bool runtime,bool executable,std::span<const std::uint8_t> context,
    std::span<const std::uint8_t> entry) {
    return runtime && executable && context.size()==ImpactContext.size() && entry.size()==NativeForceEntry.size() &&
        std::equal(context.begin(),context.end(),ImpactContext.begin()) &&
        std::equal(entry.begin(),entry.end(),NativeForceEntry.begin());
}
}
