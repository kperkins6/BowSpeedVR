#include "BowSpeed/ImpactThunk.h"
#include "BowSpeed/AtomicCodeWrite.h"
#include "Check.h"
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
namespace {
int projectileToken=0,collisionToken=0;
bool passed=false;
float ExpectedForce=0;
bool ExpectedScale=false;
float Receiver(float force,const void* collision,bool scale,const void* projectile) {
    passed=force==ExpectedForce && collision==&collisionToken && scale==ExpectedScale && projectile==&projectileToken;
    return force+0.125f;
}
struct Allocation {
    void* memory=VirtualAlloc(nullptr,4096,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    ~Allocation() {if (memory) VirtualFree(memory,0,MEM_RELEASE);}
};
}
#endif
void ImpactThunkTests() {
#if defined(_WIN32) && defined(_M_X64)
    Allocation allocation;
    Check(allocation.memory!=nullptr,"Allocate isolated executable memory for ABI acceptance");
    auto* memory=static_cast<std::uint8_t*>(allocation.memory);
    const auto thunk=BowSpeed::MakeImpactThunk(reinterpret_cast<std::uintptr_t>(&Receiver));
    std::memcpy(memory,thunk.data(),thunk.size());
    // A Windows x64 caller supplies projectile in R14, force XMM0, collision
    // RDX and bool R8, matching the inspected engine call site. R14 is saved
    // and restored, and shadow space/stack alignment are honored.
    std::array<std::uint8_t,37> caller={
        0x41,0x56,0x48,0x83,0xEC,0x20,0x4C,0x8B,0xF1,
        0x0F,0x28,0xC1,0x49,0x8B,0xD0,0x4D,0x8B,0xC1,
        0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,
        0x48,0x83,0xC4,0x20,0x41,0x5E,0xC3};
    const auto address=reinterpret_cast<std::uintptr_t>(memory);
    std::memcpy(caller.data()+20,&address,sizeof(address));
    std::memcpy(memory+64,caller.data(),caller.size());
    DWORD old=0;
    Check(VirtualProtect(memory,4096,PAGE_EXECUTE_READ,&old)!=0,"Make ABI harness executable");
    FlushInstructionCache(GetCurrentProcess(),memory,4096);
    BowSpeed::CodeBlock original{},replacement{},foreign{};
    original.fill(0x90);replacement.fill(0xCC);foreign.fill(0xC3);
    // These blocks are deliberately never executed. They test ownership of
    // the code publication/rollback used at the native call site.
    DWORD blockOld=0;
    Check(VirtualProtect(memory+128,16,PAGE_EXECUTE_READWRITE,&blockOld)!=0,"Prepare code publication test");
    std::memcpy(memory+128,original.data(),16);
    Check(VirtualProtect(memory+128,16,blockOld,&old)!=0,"Restore code publication test protection");
    const auto block=reinterpret_cast<std::uintptr_t>(memory+128);
    Check(BowSpeed::ReplaceOwnedCodeBlock(block,original,replacement),"Matching code block can be published atomically");
    Check(!BowSpeed::ReplaceOwnedCodeBlock(block,original,foreign),"Stale expectation never overwrites a changed call");
    Check(std::memcmp(memory+128,replacement.data(),16)==0,"Conflicting writer leaves the owned call intact");
    Check(BowSpeed::ReplaceOwnedCodeBlock(block,replacement,foreign),"Simulate a later foreign replacement");
    Check(!BowSpeed::ReplaceOwnedCodeBlock(block,replacement,original),"Rollback refuses to erase a foreign patch");
    Check(std::memcmp(memory+128,foreign.data(),16)==0,"Foreign call survives rejected rollback");
    Check(!BowSpeed::ReplaceOwnedCodeBlock(block+1,foreign,original),"Unaligned code publication is rejected");
    using Caller=float(const void*,float,const void*,bool);
    auto* invoke=reinterpret_cast<Caller*>(memory+64);
    for (bool scale : {false,true}) {
        ExpectedForce=scale ? 1.5f : 0.875f;ExpectedScale=scale;passed=false;
        const auto result=invoke(&projectileToken,ExpectedForce,&collisionToken,scale);
        Check(passed,"Native impact thunk preserves float, collision pointer and bool while supplying stored projectile");
        Check(result==ExpectedForce+0.125f,"Native impact thunk preserves float return and caller stack");
    }
#endif
}
