#include "Export.h"
#include <chrono>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#include <Windows.h>
#endif
namespace BowSpeed::Editor {
namespace {
std::uint32_t CRC(std::string_view s) {
    std::uint32_t crc = ~0u;
    for (unsigned char c : s) {
        crc ^= c;
        for (int i = 0; i < 8; ++i)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
    }
    return ~crc;
}
void U16(std::string &b, unsigned n) {
    b += static_cast<char>(n);
    b += static_cast<char>(n >> 8);
}
void U32(std::string &b, std::uint32_t n) {
    U16(b, n);
    U16(b, n >> 16);
}
bool Inside(const std::filesystem::path &p, const std::filesystem::path &root) {
    auto a = std::filesystem::weakly_canonical(p), r = std::filesystem::weakly_canonical(root);
    auto i = a.begin();
    for (auto j = r.begin(); j != r.end(); ++j, ++i) {
        if (i == a.end())
            return false;
#ifdef _WIN32
        if (_wcsicmp(i->c_str(), j->c_str()) != 0)
            return false;
#else
        if (*i != *j)
            return false;
#endif
    }
    return true;
}
std::string Zip(const std::vector<std::pair<std::string, std::string>> &files) {
    std::string output, central;
    for (const auto &[name, contents] : files) {
        auto offset = static_cast<std::uint32_t>(output.size()), crc = CRC(contents),
             size = static_cast<std::uint32_t>(contents.size());
        U32(output, 0x04034B50);
        U16(output, 20);
        U16(output, 0x800);
        U16(output, 0);
        U16(output, 0);
        U16(output, 33);
        U32(output, crc);
        U32(output, size);
        U32(output, size);
        U16(output, static_cast<unsigned>(name.size()));
        U16(output, 0);
        output += name;
        output += contents;
        U32(central, 0x02014B50);
        U16(central, 20);
        U16(central, 20);
        U16(central, 0x800);
        U16(central, 0);
        U16(central, 0);
        U16(central, 33);
        U32(central, crc);
        U32(central, size);
        U32(central, size);
        U16(central, static_cast<unsigned>(name.size()));
        U16(central, 0);
        U16(central, 0);
        U16(central, 0);
        U16(central, 0);
        U32(central, 0);
        U32(central, offset);
        central += name;
    }
    auto start = static_cast<std::uint32_t>(output.size());
    output += central;
    U32(output, 0x06054B50);
    U16(output, 0);
    U16(output, 0);
    U16(output, static_cast<unsigned>(files.size()));
    U16(output, static_cast<unsigned>(files.size()));
    U32(output, static_cast<std::uint32_t>(central.size()));
    U32(output, start);
    U16(output, 0);
    return output;
}
} // namespace
void AtomicWrite(const std::filesystem::path &path, std::string_view bytes) {
    if (path.has_parent_path())
        std::filesystem::create_directories(path.parent_path());
    auto temp = path;
    temp += L".tmp." + std::to_wstring(std::chrono::steady_clock::now().time_since_epoch().count());
    try {
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
                throw std::runtime_error("Cannot create output file");
            out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
            out.close();
            if (!out)
                throw std::runtime_error("Cannot finish output file");
        }
        if (std::filesystem::exists(path)) {
            auto backup = path;
            backup += L".bak";
            std::filesystem::copy_file(path, backup, std::filesystem::copy_options::overwrite_existing);
        }
#ifdef _WIN32
        if (!MoveFileExW(temp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace output file " + path.string() + " (Windows error " +
                                     std::to_string(GetLastError()) + ")");
#else
        std::filesystem::rename(temp, path);
#endif
    } catch (...) {
        std::error_code ec;
        std::filesystem::remove(temp, ec);
        throw;
    }
}
void ExportSettings(const EditorModel &model, const std::filesystem::path &destination, bool zip) {
    const auto overlay = model.Overlay();
    if (destination.empty())
        throw std::runtime_error("Select an output destination");
    if (!model.scan.dataDir.empty() && Inside(destination, model.scan.dataDir.parent_path()))
        throw std::runtime_error("Choose a settings-mod destination outside the game directory");
    const std::string readme =
        "BowSpeedVR user settings\r\nRequires BowSpeedVR 1.1.0 or newer. Install as a separate mod in the "
        "scanned MO2 profile. Let BowSpeedVR.user.ini win file conflicts and restart Skyrim VR.\r\nThe "
        "settings mod affects every profile where enabled. It does not modify source ESPs or other INIs.\r\n";
    if (zip) {
        AtomicWrite(destination, Zip({{"SKSE/Plugins/BowSpeedVR.user.ini", overlay},
                                      {"BowSpeedVR/Settings-README.txt", readme}}));
        return;
    }
    if (std::filesystem::exists(destination / "SKSE/Plugins/BowSpeedVR.dll") ||
        std::filesystem::exists(destination / "SKSE/Plugins/BowSpeedVR.ini"))
        throw std::runtime_error(
            "Select a dedicated settings mod, separate from the runtime or supplied configuration patch");
    auto file = destination / "SKSE/Plugins/BowSpeedVR.user.ini";
    if (!Inside(file, destination))
        throw std::runtime_error("Output path leaves selected settings mod");
    AtomicWrite(file, overlay);
    AtomicWrite(destination / "BowSpeedVR/Settings-README.txt", readme);
}
} // namespace BowSpeed::Editor
