#pragma once
#include <cstdint>
namespace BowSpeed {
struct DiagnosticState {
    std::uintptr_t weapon=0,ammo=0;
    float original=1,selected=1,returned=1,power=1,runtime=1,base=0,gravity=0;
    bool moving=false,player=false,bow=false,crossbow=false;
    bool operator==(const DiagnosticState&) const = default;
};
struct DiagnosticGate {
    std::uint64_t calls=0;
    std::uint32_t emitted=0;
    DiagnosticState previous;
    bool ShouldEmit(const DiagnosticState& state) {
        ++calls;
        if (emitted>=8 || (emitted && state==previous)) return false;
        previous=state;++emitted;return true;
    }
};
}
