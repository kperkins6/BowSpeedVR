#pragma once
#include "BowSpeed/MatchedRuntime.h"
#include <optional>
namespace BowSpeed {
inline constexpr int UserSchemaVersion = 1;
inline constexpr std::string_view EditorRuntimeVersion = "1.1.0";
FormKey CanonicalFormKey(std::string_view plugin, std::uint32_t localID);
std::optional<FormKey> ParseFormKey(std::string_view);
std::string FormatFormKey(const FormKey &);
struct UserConfig {
    bool valid = false;
    Config values;
    std::map<std::string, std::string> globals;
    std::string fingerprint;
    std::vector<std::string> errors;
    std::map<FormKey, std::string> inferenceNotes;
};
struct PhysicsCatalog {
    std::map<FormKey, ResolvedBowPhysics> bows;
    std::map<FormKey, ResolvedArrowPhysics> arrows;
};
UserConfig ParseUserConfig(std::string_view);
Config ApplyUserConfig(const Config &, const UserConfig &);
PhysicsCatalog ResolvePhysicsCatalog(const Config &);
std::string SerializeUserConfig(const Config &, std::string_view fingerprint = {});
} // namespace BowSpeed
