#include "BowSpeed/InventoryWeight.h"
#include "BowSpeed/InventoryWeightGuard.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "Check.h"
#include <iostream>
#include <limits>
#if defined(_WIN32) && defined(_M_X64)
#include <Windows.h>
#include "BowSpeed/NearCodeMemory.h"
namespace {
void* WINAPI RefuseAllocation(void*,SIZE_T,DWORD,DWORD) {return nullptr;}
SIZE_T WINAPI RefuseQuery(const void*,MEMORY_BASIC_INFORMATION*,SIZE_T) {return 0;}
}
#endif

int main() {
    try {
        using namespace BowSpeed;
        Check(InventoryWeightFromMass(1)==.1f, "Iron mass must weigh 0.10 inventory units");
        Check(Near(*InventoryWeightFromMass(.55),.055f), "Bound arrows remain extra light");
        Check(Near(*InventoryWeightFromMass(2.3010204081632653),.23010204f), "Daedric weight must follow the current physics mass");
        for (double invalid:{0.0,-1.0,9.0,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
            Check(!InventoryWeightFromMass(invalid), "Invalid mass cannot become inventory weight");
        for (const auto& profile:ArrowPhysicsProfiles()) {
            const auto weight=InventoryWeightFromMass(profile.mass);
            Check(weight && Near(*weight,static_cast<float>(.1*profile.mass)), "Every catalogue mass maps to the same fixed unit scale");
        }
        auto cfg=ParseConfig("[General]\nObserveOnly=false\n[Immersion]\nArrowInventoryWeight=true\n");
        auto weights=BuildInventoryWeightProfiles(cfg);
        Check(weights.at({"skyrim.esm",0x1397D})==.1f, "Enabled weights resolve the base Iron binding");
        Check(!weights.contains({"archeryv2.esp",0x10D25}), "Standalone weighting excludes MGO-only forms");
        cfg.pairingAmmoMass[{"Skyrim.esm",0x1397D}]=2;
        Check(BuildInventoryWeightProfiles(cfg).at({"skyrim.esm",0x1397D})==.2f, "Per-arrow mass tuning changes flight and inventory weight together");
        cfg.mgoProfiles=true;
        Check(BuildInventoryWeightProfiles(cfg).contains({"archeryv2.esp",0x10D25}), "MGO weighting resolves expanded arrow identities");
        Check(BuildInventoryWeightProfiles(cfg).contains({"dis_heavy_legion.esp",0x898}), "Flight gravity exclusions do not prevent inventory weight");
        cfg.observeOnly=true;Check(BuildInventoryWeightProfiles(cfg).empty(), "Observation mode applies no inventory weights");
        cfg.observeOnly=false;cfg.enabled=false;Check(BuildInventoryWeightProfiles(cfg).empty(), "Disabled plugin applies no inventory weights");
        cfg.enabled=true;cfg.arrowInventoryWeight=false;Check(BuildInventoryWeightProfiles(cfg).empty(), "Installer default retains existing inventory behavior");
        Check(SelectArrowInventoryWeight(-1,true,true,true,.1f)==.1f, "Trusted mapped arrow replaces the native no-weight sentinel");
        for (unsigned gate=0;gate<4;++gate)
            Check(SelectArrowInventoryWeight(-1,gate!=0,gate!=1,gate!=2,gate==3 ? std::nullopt : std::optional<float>{.1f})==-1,
                  "Disabled, untrusted, non-arrow and unknown items preserve native weight");
        Check(SelectArrowInventoryWeight(3,true,true,false,.1f)==3, "Other item weights remain unchanged");
        Check(WeightGetterMatches(true,true,NativeWeightGetter,-1), "Inspected VR getter recognized");
        Check(!WeightGetterMatches(false,true,NativeWeightGetter,-1), "Other runtimes cannot install weight hook");
        Check(!WeightGetterMatches(true,false,NativeWeightGetter,-1), "Unreadable native code refuses weight hook");
        Check(!WeightGetterMatches(true,true,NativeWeightGetter,0), "Changed native default sentinel refuses weight hook");
        for (std::size_t i=0;i<NativeWeightGetter.size();++i) {
            auto changed=NativeWeightGetter;changed[i]^=1;
            Check(!WeightGetterMatches(true,true,changed,-1), "Modified native body or type dispatch must refuse the hook");
        }
        Check(!WeightGetterMatches(true,true,std::span(NativeWeightGetter).first(8),-1), "Truncated native getter refuses hook");
#if defined(_WIN32) && defined(_M_X64)
        auto* executable=static_cast<std::uint8_t*>(VirtualAlloc(nullptr,2048,MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
        Check(executable!=nullptr,"Allocate isolated native getter test");
        std::memcpy(executable,NativeWeightGetter.data(),NativeWeightGetter.size());
        auto* nativeDefault=reinterpret_cast<float*>(executable+768);
        *nativeDefault=-1;
        const auto clone=MakeWeightGetterClone(reinterpret_cast<std::uintptr_t>(executable+256),reinterpret_cast<std::uintptr_t>(nativeDefault));
        Check(clone.has_value(),"Relocate isolated native getter and internal dispatch tables");
        std::memcpy(executable+256,clone->data(),clone->size());
        Check(FlushInstructionCache(GetCurrentProcess(),executable,2048)!=0,"Flush native test code");
        using Getter=float(const void*);
        const auto getter=reinterpret_cast<Getter*>(executable+256);
        Check(getter(nullptr)==-1,"Getter clone preserves native -1 sentinel in XMM0");
        *nativeDefault=2.5f;
        Check(getter(nullptr)==*nativeDefault,"Getter clone reads the current native default");
        std::array<std::uint8_t,512> form{};
        form[0x1A]=41; // Native weapon dispatch reads TESWeightForm at +0xB8.
        const float sample=7.25f;
        std::memcpy(form.data()+0xB8,&sample,sizeof(sample));
        Check(getter(form.data())==sample,"Getter clone preserves stock weapon weights and original RCX");
        // A later chained hook can replace any original instruction or table;
        // our fallback must never execute or index that overwritten body.
        std::fill_n(executable,NativeWeightGetter.size(),std::uint8_t{0xCC});
        Check(FlushInstructionCache(GetCurrentProcess(),executable,2048)!=0,"Flush simulated native takeover");
        Check(getter(form.data())==sample,"Fallback remains safe after complete original getter takeover");
        form[0x1A]=42;
        Check(getter(form.data())==*nativeDefault,"Unknown ammo retains stock default after takeover");
        Check(!MakeWeightGetterClone(0x10000,0xFFFFFFFFFFFF),"Out-of-range native data relocation is refused");
        const auto anchor=reinterpret_cast<std::uintptr_t>(executable);
        Check(AllocateNearCode(anchor,4096,&RefuseAllocation)==nullptr,"Exhausted optional allocation returns without terminating the process");
        Check(AllocateNearCode(anchor,4096,&VirtualAlloc,&RefuseQuery)==nullptr,"Failed memory query refuses the optional allocation");
        auto* nearCode=AllocateNearCode(anchor,4096);
        Check(nearCode!=nullptr,"Other hook setup can allocate after the optional allocator refuses");
        Check(VirtualFree(nearCode,0,MEM_RELEASE)!=0,"Release independent test allocation");
        VirtualFree(executable,0,MEM_RELEASE);
#endif
        std::cout<<"Inventory mass, native guards, isolated getter takeover and recoverable allocation passed\n";
        return 0;
    } catch (const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
