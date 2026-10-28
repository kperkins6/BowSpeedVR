#pragma once
#include "BowSpeed/UserConfig.h"
#include <filesystem>
#include <functional>
namespace BowSpeed::Editor {
enum class ItemKind { Bow, Arrow, Crossbow };
struct Equipment {
    FormKey key;
    ItemKind kind{};
    std::string name,editorID,winningPlugin,reason;
    std::optional<FormKey> weaponTemplate,projectile;
    double nativeSpeed{},gravity{},force{},damage{};
    bool ordinary{},scripted{};
};
struct ScanResult {
    bool complete=false;
    std::map<FormKey,Equipment> equipment;
    std::vector<std::string> plugins,errors,warnings;
    std::string fingerprint;
    std::filesystem::path dataDir,pluginsFile;
};
using PluginResolver=std::function<std::filesystem::path(const std::string&)>;
ScanResult ScanPlugins(const std::filesystem::path& dataDir,const std::filesystem::path& pluginsFile,
    const std::filesystem::path& orderFile={},PluginResolver resolver={});
std::string ScanReport(const ScanResult&);
std::string ReadText(const std::filesystem::path&,std::size_t maximum=4*1024*1024);
}
