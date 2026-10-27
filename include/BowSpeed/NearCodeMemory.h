#pragma once
#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <limits>
namespace BowSpeed {
// Optional hooks must be able to refuse an allocation without CommonLib's
// fatal trampoline-allocation path. Keep the query/allocation calls injectable
// so exhaustion can be exercised without exhausting the game process.
inline void* AllocateNearCode(std::uintptr_t anchor,std::size_t bytes,
                             decltype(&VirtualAlloc) allocate=&VirtualAlloc,
                             decltype(&VirtualQuery) query=&VirtualQuery) noexcept {
    constexpr std::uintptr_t radius=0x40000000;
    if (!anchor || !bytes || bytes>radius || !allocate || !query) return nullptr;
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto granularity=static_cast<std::uintptr_t>(system.dwAllocationGranularity);
    if (!granularity) return nullptr;
    const auto minimum=reinterpret_cast<std::uintptr_t>(system.lpMinimumApplicationAddress);
    const auto maximum=reinterpret_cast<std::uintptr_t>(system.lpMaximumApplicationAddress);
    if (anchor<minimum || anchor>maximum) return nullptr;
    const auto lower=(std::max)(minimum,anchor>radius ? anchor-radius : minimum);
    const auto upper=(std::min)(maximum,anchor<=maximum-radius ? anchor+radius : maximum);
    const auto align=[granularity](std::uintptr_t value) {
        return value+(granularity-value%granularity)%granularity;
    };
    auto cursor=align(lower);
    while (cursor<upper) {
        MEMORY_BASIC_INFORMATION info{};
        if (!query(reinterpret_cast<const void*>(cursor),&info,sizeof(info))) return nullptr;
        const auto base=reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        if (!info.RegionSize || base>(std::numeric_limits<std::uintptr_t>::max)()-info.RegionSize) return nullptr;
        const auto end=base+info.RegionSize;
        if (end<=cursor) return nullptr;
        if (info.State==MEM_FREE) {
            auto candidate=align((std::max)(cursor,base));
            const auto limit=(std::min)(end,upper);
            while (candidate<limit && bytes<=limit-candidate) {
                if (auto* memory=allocate(reinterpret_cast<void*>(candidate),bytes,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE))
                    return memory;
                if (granularity>limit-candidate) break;
                candidate+=granularity;
            }
        }
        cursor=align(end);
    }
    return nullptr;
}
}
