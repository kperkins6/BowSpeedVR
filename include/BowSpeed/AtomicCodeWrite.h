#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include <intrin.h>
#endif
namespace BowSpeed {
using CodeBlock=std::array<std::uint8_t,16>;
inline bool ReplaceOwnedCodeBlock(std::uintptr_t address,const CodeBlock& expected,const CodeBlock& desired) {
#if defined(_WIN32) && defined(_M_X64)
    if (address%16!=0) return false;
    DWORD old=0;
    if (!VirtualProtect(reinterpret_cast<void*>(address),16,PAGE_EXECUTE_READWRITE,&old)) return false;
    alignas(16) std::array<__int64,2> compare{},replacement{};
    std::memcpy(compare.data(),expected.data(),16);
    std::memcpy(replacement.data(),desired.data(),16);
    const bool changed=_InterlockedCompareExchange128(reinterpret_cast<volatile __int64*>(address),replacement[1],replacement[0],compare.data())!=0;
    DWORD previous=0;
    const bool restored=VirtualProtect(reinterpret_cast<void*>(address),16,old,&previous)!=0;
    const bool coherent=!changed || FlushInstructionCache(GetCurrentProcess(),reinterpret_cast<const void*>(address),16)!=0;
    return changed && restored && coherent;
#else
    (void)address;(void)expected;(void)desired;return false;
#endif
}
}
