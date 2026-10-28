#pragma once
#include "EditorModel.h"
namespace BowSpeed::Editor {
void AtomicWrite(const std::filesystem::path &, std::string_view);
void WriteScanReport(const ScanResult &, const std::filesystem::path &);
void ExportSettings(const EditorModel &, const std::filesystem::path &destination, bool zip = true);
} // namespace BowSpeed::Editor
