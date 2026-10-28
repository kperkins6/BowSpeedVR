#pragma once
#include <filesystem>
namespace BowSpeed::Editor {
struct SessionPaths {
    std::filesystem::path gameDir, dataDir, pluginsFile, orderFile;
    bool explicitProfile{};
};
SessionPaths DetectSession(std::filesystem::path gameDir, std::filesystem::path profileDir = {});
} // namespace BowSpeed::Editor
