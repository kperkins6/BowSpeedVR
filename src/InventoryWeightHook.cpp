#include "BowSpeed/PCH.h"
#include "BowSpeed/InventoryWeightHook.h"
#include "BowSpeed/InventoryWeight.h"
#include "BowSpeed/InventoryWeightGuard.h"
#include "BowSpeed/AtomicCodeWrite.h"
#include "BowSpeed/NearCodeMemory.h"
#include <atomic>
#include <limits>
#include <memory>

namespace BowSpeed {
namespace {
using WeightFunction=float(const RE::TESForm*);
std::atomic<WeightFunction*> nativeWeight=nullptr;
InventoryWeightForms weights;
struct CodeDeleter {void operator()(void* memory) const noexcept {if (memory) VirtualFree(memory,0,MEM_RELEASE);}};
std::unique_ptr<void,CodeDeleter> weightCode;
std::uintptr_t entry=0,defaultAddress=0;
std::array<std::uint8_t,NativeWeightGetter.size()> installedBytes{};
std::atomic_bool installed=false,warnedChanged=false;
bool manageInventoryCache=false;
bool MemoryAvailable(std::uintptr_t address,std::size_t bytes,bool executable=false) {
    REX::W32::MEMORY_BASIC_INFORMATION info{};
    if (!REX::W32::VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info))) return false;
    const auto protection=info.protect & 0xFF;
    const bool readable=protection==0x02 || protection==0x04 || protection==0x08 || protection==0x20 || protection==0x40 || protection==0x80;
    const bool code=protection==0x10 || protection==0x20 || protection==0x40 || protection==0x80;
    const auto start=reinterpret_cast<std::uintptr_t>(info.baseAddress);
    return info.state==0x1000 && !(info.protect & 0x100) && readable && (!executable || code) &&
        address>=start && bytes<=info.regionSize && address-start<=info.regionSize-bytes;
}
bool PathUnchanged() {
    if (!installed.load(std::memory_order_acquire)) return false;
    const bool unchanged=std::memcmp(installedBytes.data(),reinterpret_cast<const void*>(entry),installedBytes.size())==0 &&
        *reinterpret_cast<const float*>(defaultAddress)==-1;
    if (!unchanged && !warnedChanged.exchange(true))
        spdlog::warn("Inventory weight native path changed; modeled arrow weights are passing through");
    return unchanged;
}
float GetArrowWeight(const RE::TESForm* form) {
    const auto fallback=nativeWeight.load(std::memory_order_acquire);
    if (!fallback) return -1;
    // This independent getter remains callable if a later entry detour chains
    // us after overwriting the game's body. Refuse modeled values first.
    if (!PathUnchanged()) return fallback(form);
    const float native=fallback(form);
    const bool arrow=form && form->GetFormType()==RE::FormType::Ammo;
    std::optional<float> modeled;
    if (arrow) {
        const auto found=weights.find(form->GetFormID());
        if (found!=weights.end()) modeled=found->second;
    }
    return SelectArrowInventoryWeight(native,true,true,arrow,modeled);
}
}

bool InstallInventoryWeightHook(const Config& config,InventoryWeightForms resolved) {
    manageInventoryCache=config.enabled && !config.observeOnly;
    if (!config.enabled || config.observeOnly || !config.arrowInventoryWeight) {
        spdlog::info("Arrow inventory weight disabled; existing inventory behavior retained");return false;
    }
    if (resolved.empty() || installed.load()) {
        spdlog::warn("Arrow inventory weight has no eligible bindings or is already installed");return false;
    }
    const auto image=REL::Module::get().base();
    entry=image+WeightGetterRVA;defaultAddress=image+NativeWeightDefaultRVA;
    const bool runtime=REL::Module::IsVR() && REL::Module::get().version()==SKSE::RUNTIME_VR_1_4_15;
    const bool readable=MemoryAvailable(entry,NativeWeightGetter.size(),true) && MemoryAvailable(defaultAddress,sizeof(float));
    if (!readable || !WeightGetterMatches(runtime,true,{reinterpret_cast<const std::uint8_t*>(entry),NativeWeightGetter.size()},
                                         *reinterpret_cast<const float*>(defaultAddress))) {
        spdlog::error("Arrow inventory weight hook refused: native getter differs from inspected VR 1.4.15");return false;
    }
    // Own a separate, recoverable near allocation; leave the impact trampoline
    // untouched. Allocation refusal must not abort the independent core hooks.
    std::unique_ptr<void,CodeDeleter> prepared(AllocateNearCode(entry,4096));
    if (!prepared) {
        spdlog::error("Arrow inventory weight near allocation refused; feature disabled");return false;
    }
    auto* stub=static_cast<std::uint8_t*>(prepared.get());
    const auto clone=MakeWeightGetterClone(reinterpret_cast<std::uintptr_t>(stub),defaultAddress);
    if (!clone) {
        spdlog::error("Arrow inventory weight getter relocation refused; feature disabled");return false;
    }
    std::memcpy(stub,clone->data(),clone->size());
    std::array<std::uint8_t,14> relay={0xFF,0x25,0,0,0,0};
    const auto callback=reinterpret_cast<std::uintptr_t>(&GetArrowWeight);
    std::memcpy(relay.data()+6,&callback,sizeof(callback));
    auto* relayMemory=stub+224;
    std::memcpy(relayMemory,relay.data(),relay.size());
    if (!FlushInstructionCache(GetCurrentProcess(),stub,clone->size()) ||
        !FlushInstructionCache(GetCurrentProcess(),relayMemory,relay.size())) return false;
    const auto distance=reinterpret_cast<std::intptr_t>(relayMemory)-static_cast<std::intptr_t>(entry+5);
    if (distance<std::numeric_limits<std::int32_t>::min() || distance>std::numeric_limits<std::int32_t>::max()) return false;
    CodeBlock before{};
    std::memcpy(before.data(),NativeWeightGetter.data(),before.size());
    auto replacement=before;
    replacement[0]=0xE9;
    const auto relative=static_cast<std::int32_t>(distance);
    std::memcpy(replacement.data()+1,&relative,sizeof(relative));
    std::fill(replacement.begin()+5,replacement.begin()+8,0x90);
    installedBytes=NativeWeightGetter;
    std::copy(replacement.begin(),replacement.end(),installedBytes.begin());
    weights=std::move(resolved);
    // Keep code alive even if publication succeeds but a subsequent read-back
    // or conditional rollback refuses ownership of the now-reachable entry.
    weightCode=std::move(prepared);
    nativeWeight.store(reinterpret_cast<WeightFunction*>(stub),std::memory_order_release);
    if (!WeightGetterMatches(runtime,true,{reinterpret_cast<const std::uint8_t*>(entry),NativeWeightGetter.size()},
                             *reinterpret_cast<const float*>(defaultAddress)) ||
        !ReplaceOwnedCodeBlock(entry,before,replacement)) {
        ReplaceOwnedCodeBlock(entry,replacement,before);
        spdlog::error("Arrow inventory weight compare/write refused; feature disabled");return false;
    }
    if (std::memcmp(installedBytes.data(),reinterpret_cast<const void*>(entry),installedBytes.size())!=0 ||
        std::memcmp(relay.data(),relayMemory,relay.size())!=0 || std::memcmp(clone->data(),stub,clone->size())!=0) {
        ReplaceOwnedCodeBlock(entry,replacement,before);
        spdlog::error("Arrow inventory weight read-back failed; feature disabled");return false;
    }
    installed.store(true,std::memory_order_release);
    spdlog::info("Arrow inventory weight hook installed; getterRVA=0x{:X} bindings={} unitsPerMass=0.1",WeightGetterRVA,weights.size());
    return true;
}

void InvalidatePlayerInventoryWeight() {
    // Recompute after loading in either ON or OFF mode so a carried total from
    // an earlier configuration cannot survive a change to the installer flag.
    if (!manageInventoryCache) return;
    auto* player=RE::PlayerCharacter::GetSingleton();
    if (!player) return;
    const auto* extra=player->extraList.GetByType<RE::ExtraContainerChanges>();
    if (extra && extra->changes) {
        // Native inventory total uses -1 as its cache-invalid marker, not zero.
        extra->changes->totalWeight=-1;
        spdlog::info("Player inventory weight cache invalidated after game load");
    }
}
}
