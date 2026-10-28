#pragma once
#include <filesystem>
namespace BowSpeed::Editor {
struct EditorOptions {
    std::filesystem::path gameDir, profileDir, output;
};
int RunDesktop(const EditorOptions &);
} // namespace BowSpeed::Editor
