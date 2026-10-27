#pragma once
#include <array>
#include <cstdint>
#include <cstring>
namespace BowSpeed {
inline std::array<std::uint8_t,17> MakeImpactThunk(std::uintptr_t wrapper) {
    // mov r9,r14; jmp qword ptr [rip]; wrapper address.
    std::array<std::uint8_t,17> thunk={0x4D,0x8B,0xCE,0xFF,0x25,0,0,0,0};
    std::memcpy(thunk.data()+9,&wrapper,sizeof(wrapper));
    return thunk;
}
}
