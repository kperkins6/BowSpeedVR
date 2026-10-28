#include "BowSpeed/PCH.h"
#include "BowSpeed/SpeedHook.h"
#include "BowSpeed/Runtime.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "BowSpeed/Tuning.h"
#include "BowSpeed/InventoryWeight.h"
#include "BowSpeed/InventoryWeightHook.h"
#include "BowSpeed/UserConfig.h"
#include <fstream>

namespace {
std::string ReadFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}
void DataLoaded(SKSE::MessagingInterface::Message* message) {
    if (message->type==SKSE::MessagingInterface::kPreLoadGame || message->type==SKSE::MessagingInterface::kNewGame) BowSpeed::ClearMatchedSnapshots();
    if (message->type==SKSE::MessagingInterface::kPostLoadGame || message->type==SKSE::MessagingInterface::kNewGame)
        BowSpeed::InvalidatePlayerInventoryWeight();
    if (message->type != SKSE::MessagingInterface::kDataLoaded) return;
    const auto ini=std::filesystem::path("Data/SKSE/Plugins/BowSpeedVR.ini");
    auto config=BowSpeed::ParseConfig(ReadFile(ini));
    const auto userPath=std::filesystem::path("Data/SKSE/Plugins/BowSpeedVR.user.ini");
    if (std::filesystem::exists(userPath)) {
        const auto user=BowSpeed::ParseUserConfig(ReadFile(userPath));
        if (user.valid) {config=BowSpeed::ApplyUserConfig(config,user);spdlog::info("Desktop user overlay loaded; fingerprint={}",user.fingerprint);}
        else for (const auto& error:user.errors) spdlog::error("Desktop overlay rejected atomically: {}",error);
    }
    if (!std::filesystem::exists(ini)) spdlog::warn("Missing BowSpeedVR.ini; using built-in observation defaults");
    for (const auto& warning: config.warnings) spdlog::warn("{}",warning);
    if (!config.enabled) {spdlog::info("Disabled by configuration; no hook installed");return;}
    spdlog::info("Observation mode={} Diagnostic logging={} Player bow multiplier={} Player crossbow multiplier={}",config.observeOnly,config.diagnosticLogging,config.playerSpeedMult,config.playerCrossbowSpeedMult);
    std::optional<double> ricochetSpeed=1.0;
    const bool ricochetLoaded=REX::W32::GetModuleHandleW(L"RicochetFramework.dll")!=nullptr;
    if (config.compensateRicochet && ricochetLoaded)
        ricochetSpeed=BowSpeed::RicochetSpeedFromIni(ReadFile("Data/SKSE/Plugins/RicochetFramework.ini"));
    if (!ricochetSpeed) spdlog::warn("Invalid Ricochet speed setting; unpaired compensation will pass through");
    spdlog::info("Catalogue={} NPC tuning={} NPC multiplier={} Ricochet loaded={} compensation={} mounted speed={}",
        config.mgoProfiles ? "MGO" : "vanilla/DLC",config.npcSpeedEnabled,config.npcSpeedMult,
        ricochetLoaded,config.compensateRicochet,ricochetSpeed.value_or(0));
    auto* data=RE::TESDataHandler::GetSingleton();
    if (!data) {spdlog::error("No data handler; no hook installed");return;}
    BowSpeed::Factors factors;
    for (const auto& [key,factor]:config.bows) {
        auto* weapon=data->LookupForm<RE::TESObjectWEAP>(key.localID,key.plugin);
        if (!weapon || !weapon->IsBow()) {
            spdlog::warn("{}|0x{:06X} missing or not a bow; skipped",key.plugin,key.localID);
            continue;
        }
        factors.emplace(weapon,factor);
        spdlog::info("Mapped bow=0x{:08X} name={} factor={} recordSpeed={}",weapon->GetFormID(),weapon->GetName(),factor,weapon->GetSpeed());
    }
    BowSpeed::AmmoClasses ammoClasses;
    if (config.arrowPhysics) {
        for (const auto& [key,category]:config.ammoClasses) {
            auto* ammo=data->LookupForm<RE::TESAmmo>(key.localID,key.plugin);
            if (!ammo || ammo->IsBolt()) {
                spdlog::warn("{}|0x{:06X} missing or a bolt; ammo class skipped",key.plugin,key.localID);continue;
            }
            ammoClasses.emplace(ammo,category);
            const auto modifiers=BowSpeed::CalculateAmmoModifiers(category,config.massInfluence);
            spdlog::info("Mapped ammo=0x{:08X} name={} ammoSpeed={} ammoImpact={}",ammo->GetFormID(),ammo->GetName(),modifiers.speed,modifiers.impact);
        }
    }
    BowSpeed::MatchedProfiles profiles;
    const auto reference=BowSpeed::ResolveReferenceSettings(config);
    if (!reference) {spdlog::error("Invalid player reference settings; pairing disabled");config.pairingEnabled=false;}
    profiles.settings=std::make_shared<const BowSpeed::MatchedSettings>(reference.value_or(config.pairingSettings));
    spdlog::info("Player reference preset={} multiplier={} resolved full-draw reference={}",
        config.referencePreset==BowSpeed::ReferencePreset::Vanilla ? "Vanilla calibrated" : "Tuned",
        config.pairedPlayerSpeedMult,profiles.settings->referenceSpeed);
    if (config.pairingEnabled) {
        const auto catalog=BowSpeed::ResolvePhysicsCatalog(config);
        for (const auto& [key,profile]:catalog.bows) {
            const auto* weapon=data->LookupForm<RE::TESObjectWEAP>(key.localID,key.plugin);
            if (!weapon || !weapon->IsBow() || weapon->IsCrossbow() || !weapon->GetPlayable() ||
                !BowSpeed::UsableRangedSource(true,weapon->GetSpeed())) continue;
            profiles.bows.emplace(weapon->GetFormID(),profile);
        }
        for (const auto& [key,profile]:catalog.arrows) {
            auto* ammo=data->LookupForm<RE::TESAmmo>(key.localID,key.plugin);
            if (!ammo || ammo->IsBolt() || !ammo->GetPlayable()) continue;
            const auto* projectile=ammo->GetRuntimeData().data.projectile;
            if (!projectile || !projectile->data.types.all(RE::BGSProjectileData::Type::kArrow) ||
                !std::isfinite(projectile->data.force) || projectile->data.force<=0 || projectile->data.force>4) continue;
            if (profile.gravityException) spdlog::warn("Pairing gravity exception: {} remains native",BowSpeed::FormatFormKey(key));
            profiles.arrows.emplace(ammo->GetFormID(),profile);
        }
        spdlog::info("Pairing resolved reviewed bows={} arrows={} MomentumImpact={}",profiles.bows.size(),profiles.arrows.size(),config.pairingMomentumImpact);
    }
    BowSpeed::InventoryWeightForms inventoryWeights;
    for (const auto& [key,weight]:BowSpeed::BuildInventoryWeightProfiles(config)) {
        auto* ammo=data->LookupForm<RE::TESAmmo>(key.localID,key.plugin);
        if (!ammo || ammo->IsBolt() || !ammo->GetPlayable()) continue;
        inventoryWeights.emplace(ammo->GetFormID(),weight);
        spdlog::info("Inventory arrow=0x{:08X} name={} weightEach={}",ammo->GetFormID(),ammo->GetName(),weight);
    }
    BowSpeed::InstallInventoryWeightHook(config,std::move(inventoryWeights));
    BowSpeed::InstallSpeedHook(std::move(config),std::move(factors),std::move(ammoClasses),std::move(profiles),ricochetSpeed);
}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    // Check before initializing CommonLib or using any VR object layouts.
    if (!BowSpeed::SupportedSKSEVersion(skse->IsEditor(),skse->RuntimeVersion().pack())) return false;
    try {
        const auto directory=SKSE::log::log_directory();
        if (!directory) return false;
        auto sink=std::make_shared<spdlog::sinks::basic_file_sink_mt>((*directory / "BowSpeedVR.log").string(),true);
        auto logger=std::make_shared<spdlog::logger>("BowSpeedVR",std::move(sink));
        spdlog::set_default_logger(std::move(logger));
        spdlog::set_pattern("[%H:%M:%S.%e] [%l] %v");
        spdlog::flush_on(spdlog::level::info);
        spdlog::info("BowSpeedVR 1.1.0; SKSE runtime={} packed=0x{:08X}",skse->RuntimeVersion().string("."),skse->RuntimeVersion().pack());
        SKSE::Init(skse);
        const auto* messaging=SKSE::GetMessagingInterface();
        if (!messaging || !messaging->RegisterListener(DataLoaded)) return false;
        return true;
    } catch (const std::exception& error) {
        spdlog::error("Load failed: {}",error.what()); return false;
    }
}
