#include "BowSpeed/Config.h"
#include <algorithm>
#include <charconv>
#include <cmath>
#include <cctype>
#include <optional>
#include <sstream>
namespace BowSpeed {
namespace {
std::string_view Trim(std::string_view text) {
    const auto start=text.find_first_not_of(" \t\r\n");
    if (start==text.npos) return {};
    return text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);
}
std::string Lower(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(),result.end(),result.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return result;
}
std::optional<FormKey> ParseKey(std::string_view key) {
    const auto separator=key.find('|');
    if (separator==key.npos) return {};
    const auto plugin=Trim(key.substr(0,separator));
    auto id=Trim(key.substr(separator+1));
    const auto lower=Lower(plugin);
    if (plugin.empty() || plugin.find_first_of("\\/:*?<>|\"")!=plugin.npos ||
        !(lower.ends_with(".esm") || lower.ends_with(".esp") || lower.ends_with(".esl"))) return {};
    if (!(id.starts_with("0x") || id.starts_with("0X"))) return {};
    id.remove_prefix(2);
    std::uint32_t local=0;
    const auto [end,error]=std::from_chars(id.data(),id.data()+id.size(),local,16);
    if (id.empty() || error!=std::errc{} || end!=id.data()+id.size() || local==0 || local>0xFFFFFF) return {};
    return FormKey{lower=="skyrim.esm" ? "Skyrim.esm" : lower,local};
}
std::optional<double> PairingNumber(std::string_view value, double minimum, double maximum) {
    double parsed=0;
    const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),parsed);
    if (value.empty() || error!=std::errc{} || end!=value.data()+value.size() ||
        !std::isfinite(parsed) || parsed<minimum || parsed>maximum) return {};
    return parsed;
}
}
std::map<FormKey, AmmoClass> DefaultAmmoClasses() {
    using enum AmmoClass;
    return {
        {{"Skyrim.esm",0x1397D},Medium},{{"Skyrim.esm",0x1397F},Medium},
        {{"Skyrim.esm",0x34182},Medium},{{"Skyrim.esm",0x38341},Medium},
        {{"Skyrim.esm",0xEAFDF},Medium},{{"Skyrim.esm",0x10E2DE},Medium},
        {{"Skyrim.esm",0x139BB},Heavy},{{"Skyrim.esm",0x139BC},Heavy},
        {{"Skyrim.esm",0x139BD},Light},{{"Skyrim.esm",0x139BE},Light},
        {{"Skyrim.esm",0xCEE9E},Light},{{"Skyrim.esm",0x139BF},Massive},
        {{"Skyrim.esm",0x139C0},Massive},{{"Skyrim.esm",0x10B0A7},ExtraLight},
        {{"dawnguard.esm",0x98A0},Light},{{"dawnguard.esm",0x98A1},Light},
        {{"dawnguard.esm",0x176F4},Massive},
        {{"dragonborn.esm",0x2623B},Medium},{{"dragonborn.esm",0x26239},Medium}
    };
}
Config ParseConfig(std::string_view ini) {
    Config result;
    result.ammoClasses=DefaultAmmoClasses();
    std::istringstream input{std::string(ini)};
    std::string line,section;
    std::size_t number=0;
    while (std::getline(input,line)) {
        ++number;
        auto text=Trim(line);
        text=Trim(text.substr(0,text.find_first_of(";#")));
        if (text.empty()) continue;
        const auto warn=[&](std::string message){
            if (result.warnings.size()<64) result.warnings.push_back("Line "+std::to_string(number)+": "+message);
        };
        if (text.front()=='[' && text.back()==']') { section=Lower(Trim(text.substr(1,text.size()-2))); continue; }
        const auto equal=text.find('=');
        if (equal==text.npos) {warn("expected key=value");continue;}
        const auto key=Trim(text.substr(0,equal));
        const auto value=Trim(text.substr(equal+1));
        if (section=="pairing") {
            const auto normalized=Lower(key);
            if (normalized=="referencepreset") {
                const auto preset=Lower(value);
                if (preset=="tuned") result.referencePreset=ReferencePreset::Tuned;
                else if (preset=="vanilla") result.referencePreset=ReferencePreset::Vanilla;
                else warn("expected Tuned or Vanilla; retained reference preset");
                continue;
            }
            bool* booleanSetting=nullptr;
            if (normalized=="enabled") booleanSetting=&result.pairingEnabled;
            else if (normalized=="capacitylimits") booleanSetting=&result.pairingSettings.capacityLimits;
            else if (normalized=="momentumimpact") booleanSetting=&result.pairingMomentumImpact;
            if (booleanSetting) {
                const auto boolean=Lower(value);
                if (boolean=="true") *booleanSetting=true;
                else if (boolean=="false") *booleanSetting=false;
                else warn("expected true or false; retained pairing value");
                continue;
            }
            double* numericSetting=nullptr;
            double minimum=0, maximum=0;
            if (normalized=="referencespeed") { numericSetting=&result.pairingSettings.referenceSpeed; minimum=1000; maximum=10000; }
            else if (normalized=="playerspeedmult") { numericSetting=&result.pairedPlayerSpeedMult; minimum=.5; maximum=1.5; }
            else if (normalized=="virtualbowmass") { numericSetting=&result.pairingSettings.virtualBowMass; minimum=0; maximum=2; }
            else if (normalized=="maxspeedratio") { numericSetting=&result.pairingSettings.maxSpeedRatio; minimum=.5; maximum=1.5; }
            else { warn("unknown Pairing setting"); continue; }
            const auto numberValue=PairingNumber(value,minimum,maximum);
            if (numberValue) *numericSetting=*numberValue;
            else warn("invalid pairing number or outside safe range; retained previous value");
        } else if (section=="immersion") {
            if (Lower(key)!="arrowinventoryweight") {warn("unknown Immersion setting");continue;}
            const auto boolean=Lower(value);
            if (boolean=="true") result.arrowInventoryWeight=true;
            else if (boolean=="false") result.arrowInventoryWeight=false;
            else warn("expected true or false; retained inventory-weight value");
        } else if (section=="compatibility" || section=="speedtuning") {
            const auto normalized=Lower(key);
            if (section=="speedtuning" && normalized=="npcspeedmult") {
                const auto parsed=PairingNumber(value,.5,1.5);
                if (parsed) result.npcSpeedMult=*parsed;
                else warn("invalid NPC speed multiplier; retained previous value");
                continue;
            }
            bool* setting=nullptr;
            if (section=="compatibility" && normalized=="mgoprofiles") setting=&result.mgoProfiles;
            else if (section=="speedtuning" && normalized=="npcenabled") setting=&result.npcSpeedEnabled;
            else if (section=="speedtuning" && normalized=="compensatericochet") setting=&result.compensateRicochet;
            else {warn("unknown compatibility/speed tuning setting");continue;}
            const auto boolean=Lower(value);
            if (boolean=="true") *setting=true;
            else if (boolean=="false") *setting=false;
            else warn("expected true or false; retained previous value");
        } else if (section=="pairingbowpower" || section=="pairingammomass" || section=="pairingammocapacity") {
            const auto form=ParseKey(key);
            if (!form) { warn("invalid pairing plugin|local FormID; skipped"); continue; }
            auto* overrides=section=="pairingbowpower" ? &result.pairingBowPower :
                section=="pairingammomass" ? &result.pairingAmmoMass : &result.pairingAmmoCapacity;
            const auto numberValue=PairingNumber(value,.1,section=="pairingammocapacity" ? 20.0 : 8.0);
            if (numberValue) (*overrides)[*form]=static_cast<float>(*numberValue);
            else warn("invalid pairing override or outside safe range; retained previous value");
        } else if (section=="general") {
            bool* setting=nullptr;
            const auto normalized=Lower(key);
            if (normalized=="playerspeedmult" || normalized=="playercrossbowspeedmult") {
                float factor=1.0f;
                const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),factor);
                if (value.empty() || error!=std::errc{} || end!=value.data()+value.size() || !std::isfinite(factor) || factor<1.0f || factor>1.3f) {
                    factor=1.0f;warn("invalid player speed multiplier; using 1.0 (allowed 1.0 to 1.3)");
                }
                if (normalized=="playercrossbowspeedmult") result.playerCrossbowSpeedMult=factor;
                else result.playerSpeedMult=factor;
                continue;
            }
            if (normalized=="enabled") setting=&result.enabled;
            else if (normalized=="observeonly") setting=&result.observeOnly;
            else if (normalized=="diagnosticlogging") setting=&result.diagnosticLogging;
            else {warn("unknown General setting");continue;}
            const auto boolean=Lower(value);
            if (boolean=="true") *setting=true;
            else if (boolean=="false") *setting=false;
            else warn("expected true or false; retained previous value");
        } else if (section=="arrowphysics") {
            const auto normalized=Lower(key);
            if (normalized=="enabled") {
                const auto boolean=Lower(value);
                if (boolean=="true") result.arrowPhysics=true;
                else if (boolean=="false") result.arrowPhysics=false;
                else warn("expected true or false; retained arrow physics value");
            } else if (normalized=="massinfluence") {
                float influence=0;
                const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),influence);
                if (value.empty() || error!=std::errc{} || end!=value.data()+value.size() || !std::isfinite(influence) || influence<0.0f || influence>0.4f) {
                    influence=0;warn("invalid mass influence; using 0.0 (allowed 0.0 to 0.4)");
                }
                result.massInfluence=influence;
            } else warn("unknown ArrowPhysics setting");
        } else if (section=="ammoclasses") {
            const auto form=ParseKey(key);
            if (!form) {warn("invalid ammo plugin|local FormID; skipped");continue;}
            const auto name=Lower(value);
            const auto category=name=="extralight" ? std::optional{AmmoClass::ExtraLight} :
                name=="light" ? std::optional{AmmoClass::Light} : name=="medium" ? std::optional{AmmoClass::Medium} :
                name=="heavy" ? std::optional{AmmoClass::Heavy} : name=="massive" ? std::optional{AmmoClass::Massive} : std::nullopt;
            if (!category) {result.ammoClasses[*form]=AmmoClass::Medium;warn("invalid ammo class; using neutral Medium");}
            else result.ammoClasses[*form]=*category;
        } else if (section=="bows") {
            const auto form=ParseKey(key);
            if (!form) {warn("invalid plugin|local FormID; skipped");continue;}
            float factor=1.0f;
            const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),factor);
            if (value.empty() || error!=std::errc{} || end!=value.data()+value.size() || !std::isfinite(factor) || factor<1.0f || factor>1.3f) {
                factor=1.0f; warn("invalid speed factor; using 1.0 (allowed 1.0 to 1.3)");
            }
            result.bows[*form]=factor;
        }
    }
    return result;
}
}
