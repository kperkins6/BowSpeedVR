#pragma once
#include "PluginScan.h"
namespace BowSpeed::Editor {
struct InferredDefaults {
    double power = 1, mass = 1, capacity = 1.8;
    std::string method, note;
};
std::map<FormKey, InferredDefaults> InferDefaults(const ScanResult &, const PhysicsCatalog &);
} // namespace BowSpeed::Editor
