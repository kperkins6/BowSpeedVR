#include "DesktopWindow.h"
#include "EditorModel.h"
#include "Export.h"
#include "SessionPaths.h"
#include <Windows.h>
#include <Shellapi.h>
#include <fstream>
#include <stdexcept>
namespace {
void Output(std::string_view s, bool error = false) {
    DWORD written{};
    WriteFile(GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE), s.data(),
              static_cast<DWORD>(s.size()), &written, nullptr);
}
std::filesystem::path ExecutableDir() {
    std::wstring s(32768, L'\0');
    auto n = GetModuleFileNameW(nullptr, s.data(), static_cast<DWORD>(s.size()));
    s.resize(n);
    return std::filesystem::path(s).parent_path();
}
} // namespace
int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    using namespace BowSpeed;
    using namespace BowSpeed::Editor;
    int count{};
    auto raw = CommandLineToArgvW(GetCommandLineW(), &count);
    std::vector<std::wstring> args;
    for (int i = 1; i < count; ++i)
        args.emplace_back(raw[i]);
    LocalFree(raw);
    EditorOptions options;
    std::filesystem::path report, import, preset, zip, directory;
    bool accept = false, headless = false;
    try {
        for (std::size_t i = 0; i < args.size(); ++i) {
            auto a = args[i];
            auto value = [&]() {
                if (++i == args.size())
                    throw std::runtime_error("Missing argument value");
                return std::filesystem::path(args[i]);
            };
            if (a == L"--game-dir")
                options.gameDir = value();
            else if (a == L"--profile-dir")
                options.profileDir = value();
            else if (a == L"--output")
                options.output = value();
            else if (a == L"--scan-report") {
                report = value();
                headless = true;
            } else if (a == L"--export-zip") {
                zip = value();
                headless = true;
            } else if (a == L"--export-dir") {
                directory = value();
                headless = true;
            } else if (a == L"--import")
                import = value();
            else if (a == L"--preset")
                preset = value();
            else if (a == L"--accept-scan")
                accept = true;
            else if (a == L"--validate-overlay") {
                auto file = value();
                auto u = ParseUserConfig(ReadText(file));
                if (!u.valid) {
                    for (auto &e : u.errors)
                        Output(e + "\n", true);
                    return 2;
                }
                Output("Valid BowSpeedVR schema 1 overlay\n");
                return 0;
            } else if (a == L"--write-mgo-preset") {
                auto file = value();
                AtomicWrite(file, MgoPreset());
                return 0;
            } else if (a == L"--version") {
                Output("BowSpeedVREditor 1.1.0\n");
                return 0;
            } else if (a == L"--runtime-version") {
                auto version = ReadRuntimeVersion(value());
                if (!version) {
                    Output("Unverified runtime version\n", true);
                    return 2;
                }
                Output(std::to_string(version->major) + "." + std::to_string(version->minor) + "." +
                       std::to_string(version->patch) + "\n");
                return version->compatible() ? 0 : 2;
            } else if (a == L"--help") {
                Output("BowSpeedVREditor --game-dir PATH [--profile-dir PATH] [--scan-report FILE|-] "
                       "[--import FILE] [--preset FILE] [--export-zip FILE] [--export-dir FOLDER] "
                       "[--accept-scan]\nLaunch without report/export options for the desktop interface.\n");
                return 0;
            } else
                throw std::runtime_error("Unknown argument");
        }
        if (!headless)
            return RunDesktop(options);
        auto paths = DetectSession(options.gameDir, options.profileDir);
        auto scan = ScanPlugins(paths.dataDir, paths.pluginsFile, paths.orderFile);
        if (!report.empty()) {
            auto text = ScanReport(scan);
            if (report == L"-")
                Output(text);
            else
                WriteScanReport(scan, report);
        }
        if (!scan.complete) {
            for (auto &error : scan.errors)
                Output(error + "\n", true);
            return 2;
        }
        auto base = ParseConfig("[General]\nObserveOnly=false\n[Pairing]\nEnabled=true\n");
        auto ini = paths.dataDir / "SKSE/Plugins/BowSpeedVR.ini";
        if (std::filesystem::exists(ini))
            base = ParseConfig(ReadText(ini));
        EditorModel model(std::move(scan), base);
        auto active = paths.dataDir / "SKSE/Plugins/BowSpeedVR.user.ini";
        if (std::filesystem::exists(active))
            model.Import(ParseUserConfig(ReadText(active)));
        if (!preset.empty())
            model.Import(ParseUserConfig(ReadText(preset)), true);
        if (!import.empty())
            model.Import(ParseUserConfig(ReadText(import)));
        if (accept)
            model.needsReview = false;
        if (!zip.empty())
            ExportSettings(model, zip, true);
        if (!directory.empty())
            ExportSettings(model, directory, false);
        return 0;
    } catch (const std::exception &e) {
        Output(std::string(e.what()) + "\n", true);
        if (!headless) {
            std::string msg = e.what();
            std::wstring wide(msg.begin(), msg.end());
            MessageBoxW(nullptr, wide.c_str(), L"BowSpeedVR", MB_OK | MB_ICONERROR);
        }
        return 2;
    }
}
