#pragma once
#include "PluginScan.h"
namespace BowSpeed::Editor {
struct EditableItem {
    Equipment equipment;
    bool enabled = false, gravityException = false;
    bool explicitlyExcluded = false;
    double power = 1, mass = 1, capacity = 1.8;
    std::string profile = "Unassigned";
};
class EditorModel {
  public:
    ScanResult scan;
    Config settings;
    std::map<FormKey, EditableItem> items;
    std::vector<std::string> notices;
    bool needsReview = false;
    explicit EditorModel(
        ScanResult, Config base = ParseConfig("[General]\nObserveOnly=false\n[Pairing]\nEnabled=true\n"));
    void AssignProfile(const FormKey &, std::string_view);
    void SetBow(const FormKey &, double power);
    void SetArrow(const FormKey &, double mass, double capacity);
    void SetEnabled(const FormKey &, bool);
    void Import(const UserConfig &, bool preset = false);
    Config BuildConfig() const;
    std::string Overlay() const;
    std::optional<MatchedShot> Preview(const FormKey &bow, const FormKey &arrow, double draw) const;
};
struct RuntimeVersion {
    unsigned major{}, minor{}, patch{};
    bool compatible() const {
        return major == 1 && minor >= 1;
    }
};
std::optional<RuntimeVersion> ReadRuntimeVersion(const std::filesystem::path &);
std::string MgoPreset();
} // namespace BowSpeed::Editor
