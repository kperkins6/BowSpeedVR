#include "BowSpeed/PCH.h"
#include "BowSpeed/ImpactHook.h"
#include "BowSpeed/ImpactGuard.h"
#include "BowSpeed/Eligibility.h"
#include "BowSpeed/ImpactThunk.h"
#include "BowSpeed/SpeedHook.h"
#include "BowSpeed/AtomicCodeWrite.h"
#include "BowSpeed/MatchedPhysics.h"
#include <atomic>
#include <cstring>
#include <limits>
namespace BowSpeed {
namespace {
using ForceFunction=float(float,RE::bhkCollisionObject*,bool);
REL::Relocation<ForceFunction*> nativeForce;
Config settings;
AmmoClasses ammoClasses;
std::uintptr_t contextAddress=0;
std::array<std::uint8_t,ImpactContext.size()> installedContext{};
bool installed=false;
std::atomic_bool warnedChanged=false;
std::atomic_uint32_t impactSamples=0;
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
float GetPlayerImpactForce(float force,RE::bhkCollisionObject* collision,bool scaleByMass,const RE::Projectile* projectile) {
    // The extra R9 argument comes from the verified caller's R14 projectile.
    // Native force remains a function of target mass/layer and the PROJ force.
    AmmoModifiers modifiers;
    bool player=false,bow=false;
    const RE::TESObjectWEAP* weapon=nullptr;
    const RE::TESAmmo* ammo=nullptr;
    if (projectile) {
        const auto& shot=projectile->GetProjectileRuntimeData();
        const auto shooter=shot.shooter.get();
        player=shooter && shooter.get()==RE::PlayerCharacter::GetSingleton();
        weapon=shot.weaponSource;ammo=shot.ammoSource;
        bow=weapon && UsableRangedSource(weapon->IsBow(),weapon->GetSpeed());
        modifiers=ResolveAmmoModifiers(settings.arrowPhysics,player,bow,ammo,ammoClasses,settings.massInfluence);
    }
    const bool unchanged=ImpactPathUnchanged();
    const float selected=ScalePhysicalImpact(force,modifiers.impact);
    const auto snapshot=settings.pairingEnabled ? FindMatchedSnapshot(projectile,unchanged) : std::nullopt;
    const double incomingSpeed=projectile ? projectile->GetProjectileRuntimeData().linearVelocity.Length() : 0;
    const float input=static_cast<float>(SelectMatchedImpactForce(snapshot,force,selected,incomingSpeed,
        settings.pairingEnabled,!settings.observeOnly && unchanged));
    if (snapshot && settings.diagnosticLogging && impactSamples.fetch_add(1)<64) {
        // Ricochet may have replaced ammoSource; retain mass from this object's original launch.
        const auto ratio=ComputeMatchedImpactRatio(snapshot->mass,incomingSpeed);
        spdlog::info("paired impact projectile=0x{:08X} originalBow=0x{:08X} originalAmmo=0x{:08X} currentAmmo=0x{:08X} mass={} launchVelocity={} impactVelocity={} oldForce={} baseForce={} newForce={} momentum={} fallback={}",
            projectile->GetFormID(),snapshot->bow,snapshot->ammo,ammo ? ammo->GetFormID() : 0,snapshot->mass,snapshot->launchSpeed,incomingSpeed,force,snapshot->baseForce,input,snapshot->momentumImpact,
            !snapshot->momentumImpact ? "MomentumImpact disabled" : force>4 ? "special force" : ratio ? "none" : "invalid impact speed");
    }
    const float result=nativeForce(input,collision,scaleByMass);
    if (settings.diagnosticLogging && player && bow && impactSamples.fetch_add(1)<64)
        spdlog::info("impact projectile=0x{:08X} bow=0x{:08X} ammo=0x{:08X} player={} ammoImpact={} recordForce={} appliedForce={} nativeScaledForce={} observation={} trusted={}",
            projectile->GetFormID(),weapon ? weapon->GetFormID() : 0,ammo ? ammo->GetFormID() : 0,player,modifiers.impact,force,input,result,settings.observeOnly,unchanged);
    return result;
}
}
bool ImpactPathUnchanged() {
    const bool unchanged=installed && SpeedPathUnchanged() && (!settings.pairingEnabled || PairingDrawPathUnchanged()) &&
        std::memcmp(installedContext.data(),reinterpret_cast<const void*>(contextAddress),installedContext.size())==0 &&
        std::memcmp(NativeForceEntry.data(),reinterpret_cast<const void*>(nativeForce.address()),NativeForceEntry.size())==0;
    if (installed && !unchanged && !warnedChanged.exchange(true))
        spdlog::warn("Native impact path changed after installation; passing through ammo speed and force adjustments");
    return unchanged;
}
bool InstallImpactHook(const Config& config,const AmmoClasses& classes) {
    const auto image=REL::Module::get().base();
    const bool runtime=REL::Module::IsVR() && REL::Module::get().version()==SKSE::RUNTIME_VR_1_4_15;
    contextAddress=image+ImpactContextRVA;
    const auto call=image+ImpactCallRVA;
    const auto target=image+NativeForceRVA;
    if (!runtime || !MemoryAvailable(contextAddress,ImpactContext.size(),true) || !MemoryAvailable(target,NativeForceEntry.size(),true) ||
        !RecognizedImpactPath(runtime,true,{reinterpret_cast<const std::uint8_t*>(contextAddress),ImpactContext.size()},
            {reinterpret_cast<const std::uint8_t*>(target),NativeForceEntry.size()})) {
        spdlog::error("Native impact hook refused: code differs from inspected VR 1.4.15; ammo physics disabled");return false;
    }
    settings=config;ammoClasses=classes;nativeForce=target;
    // Keep the original call ABI and return address. The small thunk adds R14
    // as the fourth argument, then tail-jumps via a RIP-relative pointer.
    // It changes no stack state, XMM registers or original argument registers.
    const auto wrapper=reinterpret_cast<std::uintptr_t>(&GetPlayerImpactForce);
    const auto thunk=MakeImpactThunk(wrapper);
    SKSE::AllocTrampoline(64);
    auto& trampoline=SKSE::GetTrampoline();
    auto* stub=static_cast<std::uint8_t*>(trampoline.allocate(thunk.size()));
    std::memcpy(stub,thunk.data(),thunk.size());
    std::array<std::uint8_t,14> jumpRelay={0xFF,0x25,0,0,0,0};
    const auto stubAddress=reinterpret_cast<std::uintptr_t>(stub);
    std::memcpy(jumpRelay.data()+6,&stubAddress,sizeof(stubAddress));
    auto* relayMemory=static_cast<std::uint8_t*>(trampoline.allocate(jumpRelay.size()));
    std::memcpy(relayMemory,jumpRelay.data(),jumpRelay.size());
    if (!FlushInstructionCache(GetCurrentProcess(),stub,thunk.size()) ||
        !FlushInstructionCache(GetCurrentProcess(),relayMemory,jumpRelay.size())) {
        spdlog::error("Impact generated-code cache flush failed; ammo physics disabled");return false;
    }
    const auto distance=reinterpret_cast<std::intptr_t>(relayMemory)-static_cast<std::intptr_t>(call+5);
    if (distance<std::numeric_limits<std::int32_t>::min() || distance>std::numeric_limits<std::int32_t>::max()) {
        spdlog::error("Impact relay out of call range; ammo physics disabled");return false;
    }
    const auto blockAddress=call & ~std::uintptr_t{15};
    const auto offset=static_cast<std::size_t>(call-blockAddress);
    CodeBlock before{};
    std::memcpy(before.data(),reinterpret_cast<const void*>(blockAddress),before.size());
    if (offset+5>before.size() ||
        std::memcmp(before.data()+offset,ImpactContext.data()+18,5)!=0 ||
        !RecognizedImpactPath(runtime,true,{reinterpret_cast<const std::uint8_t*>(contextAddress),ImpactContext.size()},
            {reinterpret_cast<const std::uint8_t*>(target),NativeForceEntry.size()})) {
        spdlog::error("Impact code changed during preparation; ammo physics disabled");return false;
    }
    auto replacement=before;
    const auto relative=static_cast<std::int32_t>(distance);
    replacement[offset]=0xE8;
    std::memcpy(replacement.data()+offset+1,&relative,sizeof(relative));
    installedContext=ImpactContext;
    std::memcpy(installedContext.data()+18,replacement.data()+offset,5);
    // Publish the complete aligned block only if the original code still
    // matches. A concurrent foreign call is never displaced or overwritten.
    if (!ReplaceOwnedCodeBlock(blockAddress,before,replacement)) {
        ReplaceOwnedCodeBlock(blockAddress,replacement,before);
        spdlog::error("Impact compare/write refused; ammo physics disabled");return false;
    }
    std::int32_t displacement=0;
    std::memcpy(&displacement,reinterpret_cast<const void*>(call+1),sizeof(displacement));
    const auto relay=static_cast<std::uintptr_t>(static_cast<std::intptr_t>(call+5)+displacement);
    bool relayMatches=false;
    if (MemoryAvailable(relay,14,true)) {
        const std::array<std::uint8_t,6> jump={0xFF,0x25,0,0,0,0};
        std::uintptr_t destination=0;
        std::memcpy(&destination,reinterpret_cast<const void*>(relay+6),sizeof(destination));
        relayMatches=std::memcmp(jump.data(),reinterpret_cast<const void*>(relay),jump.size())==0 && destination==reinterpret_cast<std::uintptr_t>(stub);
    }
    if (*reinterpret_cast<const std::uint8_t*>(call)!=0xE8 || !relayMatches ||
        std::memcmp(replacement.data(),reinterpret_cast<const void*>(blockAddress),replacement.size())!=0 ||
        std::memcmp(installedContext.data(),reinterpret_cast<const void*>(contextAddress),installedContext.size())!=0 ||
        std::memcmp(NativeForceEntry.data(),reinterpret_cast<const void*>(target),NativeForceEntry.size())!=0) {
        ReplaceOwnedCodeBlock(blockAddress,replacement,before);
        spdlog::error("Native impact hook read-back failed; ammo physics disabled");return false;
    }
    installed=true;
    spdlog::info("Native player-arrow physical-force hook installed; callRVA=0x{:X} observation={} influence={}",ImpactCallRVA,settings.observeOnly,settings.massInfluence);
    return true;
}
}
