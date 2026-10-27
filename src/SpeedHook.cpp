#include "BowSpeed/PCH.h"
#include "BowSpeed/SpeedHook.h"
#include "BowSpeed/Policy.h"
#include "BowSpeed/FunctionGuard.h"
#include "BowSpeed/DiagnosticGate.h"
#include "BowSpeed/ImpactHook.h"
#include "BowSpeed/MatchedPhysics.h"
#include "BowSpeed/PairingNativeGuard.h"
#include "BowSpeed/Tuning.h"
#include <chrono>
#include <limits>
#include <array>
#include <mutex>
#include <atomic>
#include <fstream>
#include <charconv>

namespace BowSpeed {
namespace {
Config settings;
Factors factors;
AmmoClasses ammoClasses;
MatchedProfiles matchedProfiles;
std::optional<double> mountedRicochetSpeed=1.0;
ShotSnapshots matchedSnapshots;
struct RetainedShot {RE::NiPointer<RE::Projectile> owner;std::chrono::steady_clock::time_point recorded;};
std::array<RetainedShot,ShotSnapshots::Capacity> retainedShots{};
std::mutex matchedMutex;
bool handlesTrusted=false;
std::atomic_uint32_t pairedSamples=0;
std::atomic_uint32_t fallbackReasons=0;
void PairingFallback(unsigned reason,const char* description) {
    const auto mask=1u<<reason;
    if (!(fallbackReasons.fetch_or(mask)&mask)) spdlog::warn("Pairing fallback: {}",description);
}
void ExpireRetainedShots() {
    const auto now=std::chrono::steady_clock::now();
    for (std::size_t i=0;i<retainedShots.size();++i) {
        auto& retained=retainedShots[i];
        if (retained.owner && now-retained.recorded>std::chrono::seconds(120)) {
            matchedSnapshots.Forget(i);retained.owner.reset();
        }
    }
}
ShotIdentity Identity(const RE::Projectile* projectile,const RE::ProjectileHandle& handle) {
    const auto* base=projectile->GetBaseObject();
    return {reinterpret_cast<std::uintptr_t>(projectile),handle.native_handle(),projectile->GetFormID(),base ? base->GetFormID() : 0};
}
using SpeedFunction = float(const RE::Projectile*);
REL::Relocation<SpeedFunction*> original;
// The first live capture showed a source-weapon getter, rather than a
// constant-one function. Capture its full body for the next diagnosis.
std::array<std::uint8_t,64> entrySnapshot{};
bool trustedFunction=false;
std::uintptr_t installedSpeedSlot=0;
bool velocityMeasurements=false;
std::atomic_bool warnedLateConflict=false;
constexpr std::size_t WeaponSpeedSlot=0xB1;
constexpr std::uintptr_t ExpectedTableRVA=0x16F93A8;
constexpr std::uint32_t CommittedMemory=0x1000;

bool Readable(std::uintptr_t address,std::size_t bytes) {
    REX::W32::MEMORY_BASIC_INFORMATION info{};
    if (!REX::W32::VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info))) return false;
    const auto protection=info.protect & 0xFF;
    const bool readable=protection==0x02 || protection==0x04 || protection==0x08 || protection==0x20 || protection==0x40 || protection==0x80;
    const auto start=reinterpret_cast<std::uintptr_t>(info.baseAddress);
    return info.state==CommittedMemory && !(info.protect & 0x100) && readable &&
        address>=start && bytes<=info.regionSize && address-start<=info.regionSize-bytes;
}
bool Executable(std::uintptr_t address) {
    REX::W32::MEMORY_BASIC_INFORMATION info{};
    if (!REX::W32::VirtualQuery(reinterpret_cast<const void*>(address),&info,sizeof(info))) return false;
    const auto protection=info.protect & 0xFF;
    return info.state==CommittedMemory && !(info.protect & 0x100) &&
        (protection==0x10 || protection==0x20 || protection==0x40 || protection==0x80);
}
std::optional<float> RipConstant(std::uintptr_t target,const std::array<std::uint8_t,64>& code) {
    constexpr std::array<std::uint8_t,4> pattern={0xF3,0x0F,0x10,0x05};
    std::size_t offset=0;
    if (!std::equal(pattern.begin(),pattern.end(),code.begin())) {
        offset=25;
        if (!std::equal(pattern.begin(),pattern.end(),code.begin()+offset)) return {};
    }
    std::int32_t displacement=0;
    std::memcpy(&displacement,code.data()+offset+4,sizeof(displacement));
    const auto address=static_cast<std::uintptr_t>(static_cast<std::intptr_t>(target)+offset+8+displacement);
    if (!Readable(address,sizeof(float))) return {};
    float value=0;
    std::memcpy(&value,reinterpret_cast<const void*>(address),sizeof(value));
    return value;
}
// Bounded records retain factor changes without logging every simulation update.
struct Sample { const RE::Projectile* pointer=nullptr; RE::FormID id=0; DiagnosticGate gate; };
std::array<Sample,256> samples{};
std::size_t nextSample=0;
std::mutex sampleMutex;
std::uint32_t npcSamples=0;
bool LogSample(const RE::Projectile* projectile,float game,float selected,float returned,bool player,bool bow,bool crossbow,AmmoModifiers ammoModifiers) {
    const auto& shot=projectile->GetProjectileRuntimeData();
    const auto magnitude=shot.linearVelocity.Length();
    const bool moving=std::isfinite(magnitude) && magnitude>0.01f;
    const auto* object=projectile->GetBaseObject();
    const auto* base=object ? object->As<RE::BGSProjectile>() : nullptr;
    const float speed=base ? base->data.speed : 0.0f;
    const float gravity=base ? base->data.gravity : 0.0f;
    const float power=projectile->GetPowerSpeedMult();
    const auto* weapon=shot.weaponSource;
    const auto* ammo=shot.ammoSource;
    const DiagnosticState state{reinterpret_cast<std::uintptr_t>(weapon),reinterpret_cast<std::uintptr_t>(ammo),game,selected,returned,power,shot.speedMult,speed,gravity,moving,player,bow,crossbow};
    std::uint64_t sequence=0;
    std::uint32_t sampleNumber=0;
    {
        std::lock_guard lock(sampleMutex);
        if (!player && npcSamples>=32) return false;
        auto found=std::find_if(samples.begin(),samples.end(),[&](const Sample& item){return item.pointer==projectile && item.id==projectile->GetFormID();});
        if (found==samples.end()) {found=samples.begin()+nextSample;nextSample=(nextSample+1)%samples.size();*found={projectile,projectile->GetFormID(),{}};}
        if (!found->gate.ShouldEmit(state)) return false;
        sequence=found->gate.calls;sampleNumber=found->gate.emitted;
        if (!player) ++npcSamples;
    }
    spdlog::info("sample stage={} sample={} call={} age={} projectile=0x{:08X} bow=0x{:08X} ammo=0x{:08X} player={} isBow={} isCrossbow={} original={} selected={} returned={} power={} runtime={} base={} gravity={} expected={} measured={} recordSpeed={} ammoSpeed={} ammoImpact={}",
        moving ? "moving" : "prelaunch",sampleNumber,sequence,shot.livingTime,projectile->GetFormID(),weapon ? weapon->GetFormID() : 0,ammo ? ammo->GetFormID() : 0,
        player,bow,crossbow,game,selected,returned,power,shot.speedMult,speed,gravity,speed*power*returned*shot.speedMult,magnitude,weapon ? weapon->GetSpeed() : 0.0f,ammoModifiers.speed,ammoModifiers.impact);
    return player && (bow || crossbow) && sampleNumber==1 && !moving && shot.livingTime==0.0f;
}
void QueueVelocitySample(const RE::Projectile* projectile,float returned) {
    if (!velocityMeasurements) return;
    const auto& launch=projectile->GetProjectileRuntimeData();
    const auto* object=projectile->GetBaseObject();
    const auto* base=object ? object->As<RE::BGSProjectile>() : nullptr;
    if (!base) return;
    const auto projectileID=projectile->GetFormID();
    const auto weaponID=launch.weaponSource ? launch.weaponSource->GetFormID() : 0;
    const auto ammoID=launch.ammoSource ? launch.ammoSource->GetFormID() : 0;
    const float expected=base->data.speed*projectile->GetPowerSpeedMult()*returned*launch.speedMult;
    const RE::ProjectileHandle handle{const_cast<RE::Projectile*>(projectile)};
    SKSE::GetTaskInterface()->AddTask([handle,projectileID,weaponID,ammoID,expected] {
        const auto reference=handle.get();
        if (!reference || reference->GetFormID()!=projectileID) {
            spdlog::info("velocity projectile=0x{:08X} bow=0x{:08X} expired before queued observation",projectileID,weaponID);
            return;
        }
        const auto& shot=reference->GetProjectileRuntimeData();
        const auto measured=shot.linearVelocity.Length();
        spdlog::info("velocity projectile=0x{:08X} bow=0x{:08X} ammo=0x{:08X} age={} launchExpected={} measured={} runtime={} sourceChanged={}",
            projectileID,weaponID,ammoID,shot.livingTime,expected,measured,shot.speedMult,
            !shot.weaponSource || shot.weaponSource->GetFormID()!=weaponID || !shot.ammoSource || shot.ammoSource->GetFormID()!=ammoID);
    });
}
std::optional<float> PairingFactor(const RE::Projectile* projectile,bool player,bool bow,bool trusted) {
    if (!settings.pairingEnabled) return {};
    if (!trusted || !handlesTrusted) {PairingFallback(0,"launch/impact/identity path unavailable or changed");ClearMatchedSnapshots();return {};}
    if (auto saved=FindMatchedSnapshot(projectile,trusted)) return static_cast<float>(saved->getterFactor);
    const auto& runtime=projectile->GetProjectileRuntimeData();
    const auto* weapon=runtime.weaponSource;
    auto* ammo=runtime.ammoSource;
    const auto* object=projectile->GetBaseObject();
    const auto* base=object ? object->As<RE::BGSProjectile>() : nullptr;
    const auto bowIt=matchedProfiles.bows.find(weapon ? weapon->GetFormID() : 0);
    const auto arrowIt=matchedProfiles.arrows.find(ammo ? ammo->GetFormID() : 0);
    const bool knownBow=bowIt!=matchedProfiles.bows.end(),knownArrow=arrowIt!=matchedProfiles.arrows.end();
    const bool gravity=knownArrow && arrowIt->second.gravityException;
    const bool physical=base && ammo && !ammo->IsBolt() && !runtime.spell &&
        base==ammo->GetRuntimeData().data.projectile && base->data.types.all(RE::BGSProjectileData::Type::kArrow);
    const bool ordinary=base && std::isfinite(base->data.force) && base->data.force>0 && base->data.force<=4;
    if (!PairingPermitted({settings.pairingEnabled,settings.observeOnly,trusted,trusted,handlesTrusted,player,bow,knownBow,knownArrow,gravity,ordinary,physical})) {
        if (player && bow && (!knownBow || !knownArrow)) PairingFallback(1,"unknown/unreviewed bow or arrow; legacy behavior");
        else if (player && gravity) PairingFallback(2,"gravity exception; legacy behavior");
        else if (player && bow && (!ordinary || !physical)) PairingFallback(3,"special force, spell or replaced projectile base; legacy behavior");
        return {};
    }
    const auto speed=runtime.linearVelocity.Length();
    if (runtime.livingTime!=0 || !std::isfinite(speed) || speed>0.01f || runtime.flags.any(RE::Projectile::Flags::kDestroyed,RE::Projectile::Flags::kProcessedImpacts)) {
        PairingFallback(4,"no trusted stationary prelaunch snapshot; legacy behavior");return {};
    }
    const double draw=projectile->GetPowerSpeedMult();
    const auto model=ComputeMatchedShot(*matchedProfiles.settings,bowIt->second.power,arrowIt->second.mass,arrowIt->second.capacity,draw);
    const auto factor=model ? RuntimeGetterFactor(model->launchSpeed,base->data.speed,draw,runtime.speedMult) : std::nullopt;
    if (!factor) {PairingFallback(5,"invalid matched calculation; legacy behavior");return {};}
    const RE::ProjectileHandle handle{const_cast<RE::Projectile*>(projectile)};
    const auto owner=handle.get();
    if (!owner || owner.get()!=projectile) {PairingFallback(6,"launch handle did not resolve to projectile");return {};}
    LaunchSnapshot snapshot{Identity(projectile,handle),weapon->GetFormID(),ammo->GetFormID(),arrowIt->second.mass,
        base->data.force,model->launchSpeed,*factor,0,true,settings.pairingMomentumImpact,matchedProfiles.settings};
    {
        std::lock_guard lock(matchedMutex);
        ExpireRetainedShots();
        const auto slot=matchedSnapshots.Record(snapshot);
        if (!slot) return {};
        retainedShots[*slot]={owner,std::chrono::steady_clock::now()};
    }
    if (settings.diagnosticLogging && pairedSamples.fetch_add(1)<64)
        spdlog::info("paired launch projectile=0x{:08X} bowProfile={} arrowProfile={} mass={} requestedQ={} usedQ={} capacityLimited={} speedLimited={} draw={} launchVelocity={} getter={}",
            projectile->GetFormID(),bowIt->second.name,arrowIt->second.name,snapshot.mass,model->requestedQ,model->usedQ,model->capacityLimited,model->speedLimited,draw,model->launchSpeed,*factor);
    return static_cast<float>(*factor);
}
float GetWeaponFactor(const RE::Projectile* projectile) {
    const float game=original(projectile);
    if (!projectile) return game;
    const auto& shot=projectile->GetProjectileRuntimeData();
    const auto shooter=shot.shooter.get();
    const auto* weapon=shot.weaponSource;
    const bool player=shooter && shooter.get()==RE::PlayerCharacter::GetSingleton();
    // The live sessions also contain synthetic source bows with record speed
    // zero and copied arrow velocities. Leave those helper projectiles alone.
    const bool bow=weapon && UsableRangedSource(weapon->IsBow(),weapon->GetSpeed());
    const bool crossbow=weapon && UsableRangedSource(weapon->IsCrossbow(),weapon->GetSpeed());
    const auto eligible=ResolveEligibility(player,bow,weapon,factors);
    const auto ammoModifiers=ResolveAmmoModifiers(settings.arrowPhysics && ImpactPathUnchanged(),player,bow,shot.ammoSource,ammoClasses,settings.massInfluence);
    const float selected=SelectPlayerWeaponFactor(eligible.playerShot,eligible.bowShot,crossbow,eligible.factor,game,settings.playerSpeedMult,settings.playerCrossbowSpeedMult)*ammoModifiers.speed;
    const bool unchanged=SpeedPathUnchanged();
    if (!settings.observeOnly && player && (bow || crossbow) && !unchanged && !warnedLateConflict.exchange(true))
        spdlog::warn("Player speed hook changed after installation; passing through affected shots");
    const bool pairedTrust=PairingLaunchTrusted(unchanged,ImpactPathUnchanged(),game);
    const auto paired=PairingFactor(projectile,player,bow,pairedTrust);
    // A paired-path conflict passes through both halves; never stack legacy tuning.
    const bool allow=OverridePermitted(settings.observeOnly,trustedFunction,game,unchanged) &&
        (!settings.pairingEnabled || pairedTrust);
    const auto* object=projectile->GetBaseObject();
    const auto* base=object ? object->As<RE::BGSProjectile>() : nullptr;
    const bool physical=base && shot.ammoSource && !shot.ammoSource->IsBolt() && !shot.spell &&
        base==shot.ammoSource->GetRuntimeData().data.projectile && base->data.types.all(RE::BGSProjectileData::Type::kArrow);
    const bool ordinary=base && std::isfinite(base->data.force) && base->data.force>0 && base->data.force<=4;
    const BowTuningEligibility tuning{allow,settings.observeOnly,shooter && shooter->As<RE::Actor>(),bow,physical,ordinary,player};
    const float fallback=bow ? SelectUnpairedBowFactor(tuning,game,selected,settings.pairedPlayerSpeedMult,
        settings.npcSpeedMult,mountedRicochetSpeed,settings.npcSpeedEnabled,settings.compensateRicochet) : selected;
    const float returned=allow ? paired.value_or(fallback) : game;
    if (settings.diagnosticLogging && LogSample(projectile,game,selected,returned,player,bow,crossbow,ammoModifiers)) QueueVelocitySample(projectile,returned);
    return returned;
}
}
void ClearMatchedSnapshots() {
    std::lock_guard lock(matchedMutex);
    matchedSnapshots.Clear();
    for (auto& retained:retainedShots) retained.owner.reset();
}
std::optional<LaunchSnapshot> FindMatchedSnapshot(const RE::Projectile* projectile,bool trusted) {
    if (!projectile || !trusted || !handlesTrusted) {ClearMatchedSnapshots();return {};}
    const RE::ProjectileHandle handle{const_cast<RE::Projectile*>(projectile)};
    const auto resolved=handle.get();
    if (!resolved || resolved.get()!=projectile) return {};
    std::lock_guard lock(matchedMutex);
    ExpireRetainedShots();
    if (std::none_of(retainedShots.begin(),retainedShots.end(),[&](const auto& item){return item.owner.get()==projectile;})) return {};
    const auto& runtime=projectile->GetProjectileRuntimeData();
    const auto shooter=runtime.shooter.get();
    if (!shooter || shooter.get()!=RE::PlayerCharacter::GetSingleton() || runtime.spell) return {};
    return matchedSnapshots.Find(Identity(projectile,handle),runtime.livingTime,trusted);
}
bool PairingDrawPathUnchanged() {
    const auto image=REL::Module::get().base();
    const auto slot=image+ExpectedTableRVA+0xB0*sizeof(std::uintptr_t);
    if (!Readable(slot,sizeof(std::uintptr_t)) ||
        !Readable(image+DrawGetterRVA,DrawGetterBytes.size()) ||
        !Readable(image+DrawPowerRVA,DrawPowerBytes.size()) ||
        !Readable(image+DrawScaleRVA,DrawScaleBytes.size())) return false;
    const auto target=*reinterpret_cast<const std::uintptr_t*>(slot);
    return target>=image && RecognizedPairingDraw(target-image,
        {reinterpret_cast<const std::uint8_t*>(image+DrawGetterRVA),DrawGetterBytes.size()},
        {reinterpret_cast<const std::uint8_t*>(image+DrawPowerRVA),DrawPowerBytes.size()},
        {reinterpret_cast<const std::uint8_t*>(image+DrawScaleRVA),DrawScaleBytes.size()});
}
bool SpeedPathUnchanged() {
    return installedSpeedSlot && trustedFunction &&
        *reinterpret_cast<const std::uintptr_t*>(installedSpeedSlot)==reinterpret_cast<std::uintptr_t>(&GetWeaponFactor) &&
        std::memcmp(entrySnapshot.data(),reinterpret_cast<const void*>(original.address()),entrySnapshot.size())==0;
}
bool InstallSpeedHook(Config config,Factors resolved,AmmoClasses resolvedAmmo,MatchedProfiles profiles,std::optional<double> ricochetSpeed) {
    const bool runtime=REL::Module::IsVR() && REL::Module::get().version()==SKSE::RUNTIME_VR_1_4_15;
    if (!runtime) {spdlog::error("Unsupported runtime; hook refused");return false;}
    REL::Relocation<std::uintptr_t> table{RE::VTABLE_ArrowProjectile[0]};
    const auto image=REL::Module::get().base();
    const auto address=table.address();
    const auto slot=address+WeaponSpeedSlot*sizeof(std::uintptr_t);
    const bool readable=address>=image && address-image==ExpectedTableRVA && Readable(slot,sizeof(std::uintptr_t));
    std::uintptr_t target=0;
    if (readable) target=*reinterpret_cast<const std::uintptr_t*>(slot);
    const bool executable=target && Executable(target);
    const auto rva=target>=image ? target-image : 0;
    if (!CanInstallHook(runtime,readable,executable,rva)) {
        spdlog::error("Hook refused: tableRVA=0x{:X} readable={} targetRVA=0x{:X} executable={}; expected=0x{:X}. A foreign hook or runtime change may be present.",address-image,readable,rva,executable,ExpectedWeaponSpeedRVA);
        return false;
    }
    if (Readable(target,entrySnapshot.size())) {
        std::memcpy(entrySnapshot.data(),reinterpret_cast<const void*>(target),entrySnapshot.size());
        trustedFunction=IsKnownWeaponFunction(entrySnapshot,RipConstant(target,entrySnapshot));
        std::string bytes;
        for (auto byte:entrySnapshot) bytes+=fmt::format("{:02X}",byte);
        spdlog::info("Original entry bytes={} recognizedWeaponFunction={}",bytes,trustedFunction);
    }
    if (!trustedFunction) {
        spdlog::warn("Original implementation is not recognized; active override refused. Observation only.");
        config.observeOnly=true;
    }
    settings=std::move(config); factors=std::move(resolved);ammoClasses=std::move(resolvedAmmo);matchedProfiles=std::move(profiles);
    mountedRicochetSpeed=ricochetSpeed;
    // Verify the library's canonical handle mappings before queued diagnostics.
    // Missing mappings disable this diagnostic before any fatal ID lookup.
    std::ifstream csv("Data/SKSE/Plugins/version-1-4-15-0.csv");
    bool createHandle=false,resolveHandle=false;
    std::string line;
    while (std::getline(csv,line)) {
        const auto comma=line.find(',');
        if (comma==line.npos) continue;
        const auto id=line.substr(0,comma);
        if (id!="15967" && id!="12204") continue;
        auto text=std::string_view(line).substr(comma+1);
        if (text.starts_with("0x")) text.remove_prefix(2);
        std::uintptr_t offset=0;
        const auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),offset,16);
        if (error!=std::errc{} || end==text.data() || !offset || !Executable(image+offset)) continue;
        if (id=="15967" && offset==0x1FF150) createHandle=true;
        if (id=="12204" && offset==0x143180) resolveHandle=true;
    }
    handlesTrusted=createHandle && resolveHandle;
    velocityMeasurements=settings.diagnosticLogging && SKSE::GetTaskInterface() && createHandle && resolveHandle;
    spdlog::info("Queued velocity observation enabled={}",velocityMeasurements);
    original=target;
    // This speed hook writes only its vtable slot, never shared projectile data.
    const auto previous=table.write_vfunc(WeaponSpeedSlot,GetWeaponFactor);
    const auto hookAddress=reinterpret_cast<std::uintptr_t>(&GetWeaponFactor);
    const auto readBack=*reinterpret_cast<const std::uintptr_t*>(slot);
    if (!HookWriteSucceeded(target,previous,hookAddress,readBack)) {
        settings.observeOnly=true;
        if (readBack==hookAddress && previous!=target) {
            original=previous;
            table.write_vfunc(WeaponSpeedSlot,previous);
        }
        spdlog::error("Hook installation failed/conflicted: previous=0x{:X} expected=0x{:X} readBack=0x{:X}. No active override.",previous,target,readBack);
        return false;
    }
    spdlog::info("Arrow weapon-speed hook installed; observation={}",settings.observeOnly);
    installedSpeedSlot=slot;
    if (settings.pairingEnabled && !PairingDrawPathUnchanged()) {
        settings.pairingEnabled=false;spdlog::warn("Paired physics disabled: native draw path differs from inspected VR executable");
    }
    if ((settings.arrowPhysics || settings.pairingEnabled) && !InstallImpactHook(settings,ammoClasses)) {
        settings.arrowPhysics=false;settings.pairingEnabled=false;
        spdlog::warn("Paired physics disabled consistently: impact guard refused; legacy bow mode");
    }
    if (settings.pairingEnabled && !handlesTrusted) {settings.pairingEnabled=false;spdlog::warn("Paired physics disabled: trusted identity mappings unavailable");}
    spdlog::info("Active mode={} MomentumImpact={} observation={}",settings.pairingEnabled ? "matched bow-arrow" : "legacy",settings.pairingEnabled && settings.pairingMomentumImpact,settings.observeOnly);
    return true;
}
}
