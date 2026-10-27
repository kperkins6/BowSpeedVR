#pragma once
#include "BowSpeed/Config.h"
#include <cstdint>
#include <map>
namespace BowSpeed {
using InventoryWeightForms=std::map<std::uint32_t,float>;
bool InstallInventoryWeightHook(const Config& config,InventoryWeightForms weights);
void InvalidatePlayerInventoryWeight();
}
