#pragma once
#include "BowSpeed/Config.h"
#include <algorithm>
#include <charconv>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <sstream>

namespace BowSpeed {
inline std::string TuningLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(),out.end(),out.begin(),[](unsigned char c){return static_cast<char>(std::tolower(c));});
    return out;
}
inline std::string_view TuningTrim(std::string_view text) {
    const auto start=text.find_first_not_of(" \t\r\n");
    return start==text.npos ? std::string_view{} : text.substr(start,text.find_last_not_of(" \t\r\n")-start+1);
}
inline bool BindingInScope(bool expanded,std::string_view plugin) {
    const auto name=TuningLower(plugin);
    return expanded || name=="skyrim.esm" || name=="dawnguard.esm" || name=="dragonborn.esm";
}
inline std::optional<MatchedSettings> ResolveReferenceSettings(const Config& config) noexcept {
    auto settings=config.pairingSettings;
    if (!std::isfinite(config.pairedPlayerSpeedMult) || config.pairedPlayerSpeedMult<.5 || config.pairedPlayerSpeedMult>1.5) return {};
    if (config.referencePreset==ReferencePreset::Vanilla) settings.referenceSpeed=2520;
    settings.referenceSpeed*=config.pairedPlayerSpeedMult;
    if (!std::isfinite(settings.referenceSpeed) || settings.referenceSpeed<=0) return {};
    return settings;
}
struct BowTuningEligibility {
    bool trusted{}, observation{}, actor{}, bow{}, physicalArrow{}, ordinaryForce{}, player{};
    bool completeCatalog{};
};
inline float SelectUnpairedBowFactor(const BowTuningEligibility& shot,float original,float legacy,
    double playerMultiplier,double npcMultiplier,std::optional<double> externalSpeed,
    bool npcEnabled,bool compensate) noexcept {
    if (!shot.trusted || shot.observation) return original;
    if (shot.completeCatalog && shot.player) return original;
    if (!shot.actor || !shot.bow || !shot.physicalArrow || !shot.ordinaryForce) return shot.player ? legacy : original;
    if (!shot.player && !npcEnabled) return original;
    if (!std::isfinite(original) || original<=0) return original;
    const double multiplier=shot.player ? playerMultiplier : npcMultiplier;
    if (!std::isfinite(multiplier) || multiplier<.5 || multiplier>1.5) return original;
    double divisor=1;
    if (compensate) {
        if (!externalSpeed || !std::isfinite(*externalSpeed) || *externalSpeed<.3 || *externalSpeed>5) return original;
        divisor=*externalSpeed;
    }
    const double result=(shot.player ? legacy : original)*multiplier/divisor;
    if (!std::isfinite(result) || result<=0 || result>std::numeric_limits<float>::max()) return original;
    return static_cast<float>(result);
}
// Ricochet's public VR implementation applies this setting to shared base speed
// and clamps it to [.3,5]. Missing key means its framework default, 1.
inline std::optional<double> RicochetSpeedFromIni(std::string_view ini) {
    std::istringstream input{std::string(ini)};
    std::string line,section;
    std::optional<double> result=1;
    while (std::getline(input,line)) {
        auto text=TuningTrim(line);
        text=TuningTrim(text.substr(0,text.find_first_of(";#")));
        if (text.empty()) continue;
        if (text.front()=='[' && text.back()==']') {section=TuningLower(TuningTrim(text.substr(1,text.size()-2)));continue;}
        const auto equal=text.find('=');
        if (section!="global" || equal==text.npos || TuningLower(TuningTrim(text.substr(0,equal)))!="fprojectilespeedmult") continue;
        const auto value=TuningTrim(text.substr(equal+1));
        double parsed=0;
        const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),parsed);
        if (value.empty() || error!=std::errc{} || end!=value.data()+value.size() || !std::isfinite(parsed)) result.reset();
        else result=std::clamp(parsed,.3,5.0);
    }
    return result;
}
}
