#include "SessionPaths.h"
#include <stdexcept>
#ifdef _WIN32
#include <ShlObj.h>
#include <Windows.h>
#endif
namespace BowSpeed::Editor {
SessionPaths DetectSession(std::filesystem::path game, std::filesystem::path profile) {
    if (game.empty())
        game = std::filesystem::current_path();
    game = std::filesystem::absolute(game).lexically_normal();
    if (!std::filesystem::exists(game / "SkyrimVR.exe"))
        throw std::runtime_error("Select the Skyrim VR directory containing SkyrimVR.exe");
    SessionPaths result{game, game / "Data", {}, {}, !profile.empty()};
    if (!profile.empty()) {
        result.pluginsFile = std::filesystem::absolute(profile) / "plugins.txt";
        result.orderFile = std::filesystem::absolute(profile) / "loadorder.txt";
    } else {
#ifdef _WIN32
        PWSTR path = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path)))
            throw std::runtime_error("Cannot locate Skyrim VR application data");
        std::filesystem::path local(path);
        CoTaskMemFree(path);
        result.pluginsFile = local / "Skyrim VR" / "plugins.txt";
        result.orderFile = local / "Skyrim VR" / "loadorder.txt";
#else
        throw std::runtime_error("Automatic MO2 session discovery requires Windows");
#endif
    }
    if (!std::filesystem::exists(result.pluginsFile))
        throw std::runtime_error(
            "No projected active plugin list. Launch through MO2 or select the active profile directory.");
    return result;
}
} // namespace BowSpeed::Editor
