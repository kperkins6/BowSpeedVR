#pragma once
#include "BowSpeed/Config.h"
#include "BowSpeed/Eligibility.h"
#include "BowSpeed/MatchedRuntime.h"
namespace RE {class Projectile;}
namespace BowSpeed {
bool InstallSpeedHook(Config config, Factors factors,AmmoClasses ammoClasses,MatchedProfiles profiles,std::optional<double> ricochetSpeed=1.0);
bool SpeedPathUnchanged();
bool PairingDrawPathUnchanged();
std::optional<LaunchSnapshot> FindMatchedSnapshot(const RE::Projectile*,bool trusted);
void ClearMatchedSnapshots();
}
