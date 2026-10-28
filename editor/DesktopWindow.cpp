#include "DesktopWindow.h"
#include "BowSpeed/PhysicsCatalog.h"
#include "EditorModel.h"
#include "Export.h"
#include "SessionPaths.h"
#include <Windows.h>
#include <CommCtrl.h>
#include <ShlObj.h>
#include <algorithm>
#include <cmath>
#include <commdlg.h>
#include <cwctype>
#include <future>
#include <iomanip>
#include <memory>
#include <sstream>
#include <stdexcept>
namespace BowSpeed::Editor {
namespace {
enum ID {
    Title = 10,
    GameLabel,
    Game,
    GameBrowse,
    ProfileLabel,
    Profile,
    ProfileBrowse,
    Scan,
    Import,
    MGO,
    Status,
    FilterLabel,
    Filter,
    Kind,
    List,
    ItemTitle,
    Identity,
    ArchetypeLabel,
    Archetype,
    Assign,
    PowerLabel,
    Power,
    MassLabel,
    Mass,
    CapacityLabel,
    Capacity,
    Enable,
    ApplyItem,
    GlobalTitle,
    PlayerEnabled,
    ReferenceLabel,
    Reference,
    SpeedLabel,
    Speed,
    NPCLabel,
    NPC,
    ImpactLabel,
    Impact,
    CapLimits,
    Momentum,
    Weight,
    NPCEnabled,
    Ricochet,
    Vanilla,
    ApplyGlobals,
    PreviewTitle,
    PreviewBow,
    PreviewArrow,
    Draw,
    PreviewOutput,
    Runtime,
    Review,
    ExportZip,
    ExportDir
};
std::wstring Wide(std::string_view s) {
    if (s.empty())
        return {};
    auto n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
std::string UTF8(std::wstring_view s) {
    if (s.empty())
        return {};
    auto n =
        WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0, nullptr, nullptr);
    std::string t(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), t.data(), n, nullptr, nullptr);
    return t;
}
std::wstring Num(double n) {
    std::wostringstream s;
    s << std::setprecision(9) << n;
    return s.str();
}
std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(), [](wchar_t c) { return std::towlower(c); });
    return s;
}
struct Loaded {
    std::unique_ptr<EditorModel> model;
    SessionPaths paths;
    std::wstring error, runtime;
};
struct Window {
    HWND hwnd{};
    HFONT font{}, heading{};
    std::map<int, HWND> controls;
    EditorOptions options;
    std::unique_ptr<EditorModel> model;
    SessionPaths paths;
    std::vector<FormKey> rows, bows, arrows;
    std::future<Loaded> pending;
    bool busy = false, dirty = false;
    std::optional<FormKey> selected;
    HWND C(int id) const {
        return controls.at(id);
    }
    void Text(int id, std::wstring_view s) {
        SetWindowTextW(C(id), std::wstring(s).c_str());
    }
    std::wstring Text(int id) const {
        auto n = GetWindowTextLengthW(C(id));
        std::wstring s(n + 1, L'\0');
        GetWindowTextW(C(id), s.data(), n + 1);
        s.resize(n);
        return s;
    }
    double Number(int id) const {
        auto s = Text(id);
        std::size_t end{};
        double n = std::stod(s, &end);
        if (end != s.size() || !std::isfinite(n))
            throw std::runtime_error("Enter a finite number without trailing text");
        return n;
    }
    bool Checked(int id) const {
        return SendMessageW(C(id), BM_GETCHECK, 0, 0) == BST_CHECKED;
    }
    void Check(int id, bool b) {
        SendMessageW(C(id), BM_SETCHECK, b ? BST_CHECKED : BST_UNCHECKED, 0);
    }
    void Add(int id, const wchar_t *type, const wchar_t *text, DWORD style = 0) {
        controls[id] = CreateWindowExW((type == std::wstring_view(L"EDIT") ? WS_EX_CLIENTEDGE : 0), type,
                                       text, WS_CHILD | WS_VISIBLE | style, 0, 0, 10, 10, hwnd,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleW(nullptr), nullptr);
        SendMessageW(C(id), WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    }
    void Build() {
        font = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        heading =
            CreateFontW(-23, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                        CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Segoe UI");
        Add(Title, L"STATIC", L"BowSpeedVR  /  Physics editor");
        SendMessageW(C(Title), WM_SETFONT, reinterpret_cast<WPARAM>(heading), TRUE);
        Add(GameLabel, L"STATIC", L"Skyrim VR");
        Add(Game, L"EDIT", options.gameDir.c_str(), ES_AUTOHSCROLL | WS_TABSTOP);
        Add(GameBrowse, L"BUTTON", L"Browse...", WS_TABSTOP);
        Add(ProfileLabel, L"STATIC", L"MO2 profile");
        Add(Profile, L"EDIT", options.profileDir.c_str(), ES_AUTOHSCROLL | WS_TABSTOP);
        Add(ProfileBrowse, L"BUTTON", L"Browse...", WS_TABSTOP);
        Add(Scan, L"BUTTON", L"Scan active profile", WS_TABSTOP);
        Add(Import, L"BUTTON", L"Import settings...", WS_TABSTOP);
        Add(MGO, L"BUTTON", L"MGO preset (optional)", WS_TABSTOP);
        Add(Status, L"STATIC",
            L"Launch through MO2. Leave profile blank to use its projected active plugin list.");
        Add(FilterLabel, L"STATIC", L"Find");
        Add(Filter, L"EDIT", L"", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(Kind, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP);
        for (auto s : {L"All equipment", L"Bows", L"Arrows", L"Needs assignment", L"Excluded"})
            SendMessageW(C(Kind), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(s));
        SendMessageW(C(Kind), CB_SETCURSEL, 0, 0);
        Add(List, WC_LISTVIEWW, L"", LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | WS_TABSTOP);
        ListView_SetExtendedListViewStyle(C(List),
                                          LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER | LVS_EX_LABELTIP);
        int col = 0;
        for (auto [name, width] :
             std::initializer_list<std::pair<const wchar_t *, int>>{{L"Equipment", 225},
                                                                    {L"Type", 65},
                                                                    {L"Pairing", 95},
                                                                    {L"Profile", 130},
                                                                    {L"Winning plugin", 165},
                                                                    {L"Identity", 210}}) {
            LVCOLUMNW c{};
            c.mask = LVCF_TEXT | LVCF_WIDTH;
            c.pszText = const_cast<PWSTR>(name);
            c.cx = width;
            ListView_InsertColumn(C(List), col++, &c);
        }
        Add(ItemTitle, L"STATIC", L"Select equipment to tune");
        Add(Identity, L"STATIC", L"", SS_LEFT);
        Add(ArchetypeLabel, L"STATIC", L"Use an archetype");
        Add(Archetype, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        Add(Assign, L"BUTTON", L"Assign", WS_TABSTOP);
        Add(PowerLabel, L"STATIC", L"Bow energy (0.1 - 8)");
        Add(Power, L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(MassLabel, L"STATIC", L"Arrow mass (0.1 - 8)");
        Add(Mass, L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(CapacityLabel, L"STATIC", L"Shaft capacity (0.1 - 20)");
        Add(Capacity, L"EDIT", L"1.8", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(Enable, L"BUTTON", L"Enable pairing for this item", BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(ApplyItem, L"BUTTON", L"Apply item values", WS_TABSTOP);
        Add(GlobalTitle, L"STATIC", L"Global tuning");
        Add(PlayerEnabled, L"BUTTON", L"Enable player pairing", BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(ReferenceLabel, L"STATIC", L"Reference speed");
        Add(Reference, L"EDIT", L"5000", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(SpeedLabel, L"STATIC", L"Player multiplier");
        Add(Speed, L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(NPCLabel, L"STATIC", L"NPC multiplier");
        Add(NPC, L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP);
        Add(ImpactLabel, L"STATIC", L"Physical impact");
        Add(Impact, L"EDIT", L"1", ES_AUTOHSCROLL | WS_TABSTOP);
        for (auto [id, t] :
             std::initializer_list<std::pair<int, const wchar_t *>>{{CapLimits, L"Shaft capacity limits"},
                                                                    {Momentum, L"Momentum-based impact"},
                                                                    {Weight, L"Arrow inventory weight"},
                                                                    {NPCEnabled, L"Enable NPC speed tuning"},
                                                                    {Ricochet, L"Compensate Ricochet speed"},
                                                                    {Vanilla, L"Vanilla reference (2520)"}})
            Add(id, L"BUTTON", t, BS_AUTOCHECKBOX | WS_TABSTOP);
        Add(ApplyGlobals, L"BUTTON", L"Apply global values", WS_TABSTOP);
        Add(PreviewTitle, L"STATIC", L"Combination preview");
        Add(PreviewBow, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        Add(PreviewArrow, L"COMBOBOX", L"", CBS_DROPDOWNLIST | WS_TABSTOP | WS_VSCROLL);
        Add(Draw, TRACKBAR_CLASSW, L"", TBS_AUTOTICKS | WS_TABSTOP);
        SendMessageW(C(Draw), TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
        SendMessageW(C(Draw), TBM_SETPOS, TRUE, 100);
        Add(PreviewOutput, L"STATIC", L"Choose an enabled bow and arrow.");
        Add(Runtime, L"STATIC", L"Settings exports require BowSpeedVR runtime 1.1.0 or newer.");
        Add(Review, L"BUTTON", L"Accept current scan", WS_TABSTOP);
        Add(ExportZip, L"BUTTON", L"Export settings ZIP...", WS_TABSTOP);
        Add(ExportDir, L"BUTTON", L"Export folder...", WS_TABSTOP);
        if (options.gameDir.empty())
            Text(Game, std::filesystem::current_path().wstring());
        EnableWindow(C(Review), FALSE);
        EnableWindow(C(ExportZip), FALSE);
        EnableWindow(C(ExportDir), FALSE);
        SetTimer(hwnd, 1, 100, nullptr);
        Layout();
    }
    void Layout() {
        RECT r{};
        GetClientRect(hwnd, &r);
        int w = r.right, h = r.bottom;
        int right = std::max(650, w - 405), leftWidth = right - 40, tableHeight = std::max(190, h - 520),
            bottom = 210 + tableHeight + 16;
        auto p = [&](int id, int x, int y, int cw, int ch) { MoveWindow(C(id), x, y, cw, ch, TRUE); };
        p(Title, 20, 15, w - 40, 34);
        p(GameLabel, 20, 62, 90, 22);
        p(Game, 118, 58, right - 224, 27);
        p(GameBrowse, right - 100, 57, 100, 29);
        p(ProfileLabel, 20, 100, 95, 22);
        p(Profile, 118, 96, right - 224, 27);
        p(ProfileBrowse, right - 100, 95, 100, 29);
        p(Scan, right + 20, 57, 170, 30);
        p(Import, right + 200, 57, 180, 30);
        p(MGO, right + 20, 96, 360, 30);
        p(Status, 20, 139, w - 40, 40);
        p(FilterLabel, 20, 183, 45, 22);
        p(Filter, 65, 179, right - 280, 27);
        p(Kind, right - 205, 179, 205, 200);
        p(List, 20, 213, leftWidth, tableHeight);
        p(ItemTitle, right + 20, 181, 360, 26);
        p(Identity, right + 20, 215, 360, 72);
        p(ArchetypeLabel, right + 20, 294, 160, 22);
        p(Archetype, right + 20, 319, 265, 360);
        p(Assign, right + 294, 318, 86, 29);
        p(PowerLabel, right + 20, 358, 235, 24);
        p(Power, right + 270, 354, 110, 27);
        p(MassLabel, right + 20, 393, 235, 24);
        p(Mass, right + 270, 389, 110, 27);
        p(CapacityLabel, right + 20, 428, 235, 24);
        p(Capacity, right + 270, 424, 110, 27);
        p(Enable, right + 20, 463, 360, 26);
        p(ApplyItem, right + 20, 499, 360, 30);
        p(GlobalTitle, 20, bottom, leftWidth, 24);
        p(PlayerEnabled, 225, bottom, 250, 24);
        int colWidth = leftWidth / 4;
        for (auto [index, label, edit] :
             std::initializer_list<std::tuple<int, int, int>>{{0, ReferenceLabel, Reference},
                                                              {1, SpeedLabel, Speed},
                                                              {2, NPCLabel, NPC},
                                                              {3, ImpactLabel, Impact}}) {
            p(label, 20 + index * colWidth, bottom + 32, colWidth - 12, 22);
            p(edit, 20 + index * colWidth, bottom + 57, colWidth - 18, 27);
        }
        p(CapLimits, 20, bottom + 95, 230, 24);
        p(Momentum, 260, bottom + 95, 240, 24);
        p(Weight, 500, bottom + 95, leftWidth - 480, 24);
        p(NPCEnabled, 20, bottom + 128, 230, 24);
        p(Ricochet, 260, bottom + 128, 240, 24);
        p(Vanilla, 500, bottom + 128, leftWidth - 480, 24);
        p(ApplyGlobals, 20, bottom + 167, leftWidth, 30);
        p(PreviewTitle, right + 20, bottom, 360, 23);
        p(PreviewBow, right + 20, bottom + 29, 360, 300);
        p(PreviewArrow, right + 20, bottom + 65, 360, 300);
        p(Draw, right + 20, bottom + 101, 360, 28);
        p(PreviewOutput, right + 20, bottom + 135, 360, 95);
        p(Runtime, 20, h - 65, right - 205, 52);
        p(Review, right - 180, h - 64, 180, 32);
        p(ExportZip, right + 20, h - 64, 175, 32);
        p(ExportDir, right + 205, h - 64, 175, 32);
    }
    std::filesystem::path ChooseFolder(const wchar_t *title) {
        IFileDialog *d = nullptr;
        if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&d))))
            return {};
        DWORD flags{};
        d->GetOptions(&flags);
        d->SetOptions(flags | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM);
        d->SetTitle(title);
        std::filesystem::path out;
        if (SUCCEEDED(d->Show(hwnd))) {
            IShellItem *item = nullptr;
            if (SUCCEEDED(d->GetResult(&item))) {
                PWSTR p = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &p))) {
                    out = p;
                    CoTaskMemFree(p);
                }
                item->Release();
            }
        }
        d->Release();
        return out;
    }
    std::filesystem::path ChooseFile(bool save) {
        std::wstring path(32768, L'\0');
        if (save) {
            auto initial = options.output.empty() ? std::filesystem::path(L"BowSpeedVR - User Settings.zip")
                                                  : options.output;
            auto s = initial.wstring();
            std::copy(s.begin(), s.end(), path.begin());
        }
        OPENFILENAMEW o{};
        o.lStructSize = sizeof(o);
        o.hwndOwner = hwnd;
        o.lpstrFile = path.data();
        o.nMaxFile = static_cast<DWORD>(path.size());
        o.lpstrFilter = save ? L"Settings mod ZIP\0*.zip\0\0"
                             : L"BowSpeedVR settings or preset\0*.ini\0All files\0*.*\0\0";
        o.lpstrDefExt = save ? L"zip" : L"ini";
        o.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
        if (save ? GetSaveFileNameW(&o) : GetOpenFileNameW(&o))
            return std::filesystem::path(path.c_str());
        return {};
    }
    void StartScan() {
        if (busy)
            return;
        if (dirty && MessageBoxW(hwnd, L"Rescanning discards draft edits. Continue?", L"Rescan",
                                 MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        busy = true;
        Text(Status, L"Scanning enabled plugin records and their overrides...");
        EnableWindow(C(Scan), FALSE);
        EnableWindow(C(ExportZip), FALSE);
        EnableWindow(C(ExportDir), FALSE);
        auto game = std::filesystem::path(Text(Game)), profile = std::filesystem::path(Text(Profile));
        pending = std::async(std::launch::async, [game, profile]() {
            Loaded l;
            try {
                l.paths = DetectSession(game, profile);
                auto s = ScanPlugins(l.paths.dataDir, l.paths.pluginsFile, l.paths.orderFile);
                auto c = ParseConfig("[General]\nObserveOnly=false\n[Pairing]\nEnabled=true\n");
                auto ini = l.paths.dataDir / L"SKSE/Plugins/BowSpeedVR.ini";
                if (std::filesystem::exists(ini))
                    c = ParseConfig(ReadText(ini));
                l.model = std::make_unique<EditorModel>(std::move(s), c);
                auto user = l.paths.dataDir / L"SKSE/Plugins/BowSpeedVR.user.ini";
                if (std::filesystem::exists(user)) {
                    auto u = ParseUserConfig(ReadText(user));
                    if (u.valid)
                        l.model->Import(u);
                    else
                        l.model->notices.push_back("Installed user overlay rejected: " + u.errors.front());
                }
                auto v = ReadRuntimeVersion(l.paths.dataDir / L"SKSE/Plugins/BowSpeedVR.dll");
                l.runtime =
                    v && v->compatible()
                        ? L"Installed runtime supports editor settings. Restart Skyrim VR after applying "
                          L"exports."
                        : L"Runtime upgrade needed or version unverified. Exports require BowSpeedVR 1.1.0+.";
            } catch (const std::exception &e) {
                l.error = Wide(e.what());
            }
            return l;
        });
    }
    void Poll() {
        if (!busy || !pending.valid() ||
            pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        auto l = pending.get();
        busy = false;
        EnableWindow(C(Scan), TRUE);
        if (!l.error.empty()) {
            Text(Status, l.error);
            return;
        }
        paths = l.paths;
        model = std::move(l.model);
        dirty = false;
        Text(Runtime, l.runtime);
        GlobalsToControls();
        Refresh();
        if (!model->scan.complete)
            Text(Status, Wide(model->scan.errors.front()));
        else {
            auto status = std::to_wstring(model->scan.plugins.size()) + L" enabled plugins; " +
                          std::to_wstring(model->items.size()) + L" equipment records. Active list: " +
                          paths.pluginsFile.wstring();
            if (!model->notices.empty())
                status += L"\n" + Wide(model->notices.front());
            Text(Status, status);
        }
        EnableWindow(C(ExportZip), model->scan.complete);
        EnableWindow(C(ExportDir), model->scan.complete);
    }
    void GlobalsToControls() {
        if (!model)
            return;
        auto &c = model->settings;
        Text(Reference, Num(c.pairingSettings.referenceSpeed));
        Text(Speed, Num(c.pairedPlayerSpeedMult));
        Text(NPC, Num(c.npcSpeedMult));
        Text(Impact, Num(c.pairingSettings.impactStrength));
        Check(CapLimits, c.pairingSettings.capacityLimits);
        Check(PlayerEnabled, c.enabled && !c.observeOnly && c.pairingEnabled);
        Check(Momentum, c.pairingMomentumImpact);
        Check(Weight, c.arrowInventoryWeight);
        Check(NPCEnabled, c.npcSpeedEnabled);
        Check(Ricochet, c.compensateRicochet);
        Check(Vanilla, c.referencePreset == ReferencePreset::Vanilla);
    }
    void ApplyGlobalValues() {
        if (!model)
            return;
        auto c = model->settings;
        c.enabled = true;
        c.observeOnly = false;
        c.pairingEnabled = Checked(PlayerEnabled);
        c.pairingSettings.referenceSpeed = Number(Reference);
        c.pairedPlayerSpeedMult = Number(Speed);
        c.npcSpeedMult = Number(NPC);
        c.pairingSettings.impactStrength = Number(Impact);
        c.pairingSettings.capacityLimits = Checked(CapLimits);
        c.pairingMomentumImpact = Checked(Momentum);
        c.arrowInventoryWeight = Checked(Weight);
        c.npcSpeedEnabled = Checked(NPCEnabled);
        c.compensateRicochet = Checked(Ricochet);
        c.referencePreset = Checked(Vanilla) ? ReferencePreset::Vanilla : ReferencePreset::Tuned;
        auto u = ParseUserConfig(SerializeUserConfig(c));
        if (!u.valid)
            throw std::runtime_error(u.errors.front());
        model->settings = c;
        dirty = true;
        UpdatePreview();
        Text(Status, L"Global draft values updated. Export settings to apply them on the next game launch.");
    }
    void Refresh() {
        if (!model)
            return;
        selected.reset();
        rows.clear();
        ListView_DeleteAllItems(C(List));
        auto query = Lower(Text(Filter));
        int kind = static_cast<int>(SendMessageW(C(Kind), CB_GETCURSEL, 0, 0));
        for (auto &[key, i] : model->items) {
            auto &e = i.equipment;
            auto hay = Lower(Wide(e.name + " " + FormatFormKey(key) + " " + e.winningPlugin));
            if (!query.empty() && hay.find(query) == hay.npos)
                continue;
            if (kind == 1 && e.kind != ItemKind::Bow)
                continue;
            if (kind == 2 && e.kind != ItemKind::Arrow)
                continue;
            if (kind == 3 && i.profile != "Unassigned")
                continue;
            if (kind == 4 && e.ordinary && !i.gravityException)
                continue;
            int n = static_cast<int>(rows.size());
            rows.push_back(key);
            auto name = Wide(e.name);
            LVITEMW row{};
            row.mask = LVIF_TEXT;
            row.iItem = n;
            row.pszText = name.data();
            ListView_InsertItem(C(List), &row);
            int col = 1;
            for (auto text : {std::wstring(e.kind == ItemKind::Arrow      ? L"Arrow"
                                           : e.kind == ItemKind::Crossbow ? L"Crossbow"
                                                                          : L"Bow"),
                              std::wstring(i.enabled ? L"Enabled" : L"Unchanged"), Wide(i.profile),
                              Wide(e.winningPlugin), Wide(FormatFormKey(key))}) {
                ListView_SetItemText(C(List), n, col++, text.data());
            }
        }
        bows.clear();
        arrows.clear();
        SendMessageW(C(PreviewBow), CB_RESETCONTENT, 0, 0);
        SendMessageW(C(PreviewArrow), CB_RESETCONTENT, 0, 0);
        for (auto &[key, i] : model->items)
            if (i.enabled) {
                auto label = Wide(i.equipment.name + " [" + FormatFormKey(key) + "]");
                if (i.equipment.kind == ItemKind::Bow) {
                    bows.push_back(key);
                    SendMessageW(C(PreviewBow), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                } else if (i.equipment.kind == ItemKind::Arrow) {
                    arrows.push_back(key);
                    SendMessageW(C(PreviewArrow), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
                }
            }
        SendMessageW(C(PreviewBow), CB_SETCURSEL, 0, 0);
        SendMessageW(C(PreviewArrow), CB_SETCURSEL, 0, 0);
        UpdatePreview();
        EnableWindow(C(Review), model->needsReview);
        Text(ItemTitle, L"Select equipment to tune");
        Text(Identity, L"");
    }
    void SelectRow() {
        if (!model)
            return;
        int n = ListView_GetNextItem(C(List), -1, LVNI_SELECTED);
        if (n < 0 || static_cast<std::size_t>(n) >= rows.size())
            return;
        selected = rows[n];
        auto &i = model->items.at(*selected);
        Text(ItemTitle, Wide(i.equipment.name));
        auto detail = FormatFormKey(*selected) + "\n" +
                      (i.gravityException           ? "Gravity exception; native flight"
                       : i.equipment.reason.empty() ? "Profile: " + i.profile
                                                    : i.equipment.reason);
        Text(Identity, Wide(detail));
        Text(Power, Num(i.power));
        Text(Mass, Num(i.mass));
        Text(Capacity, Num(i.capacity));
        Check(Enable, i.enabled);
        SendMessageW(C(Archetype), CB_RESETCONTENT, 0, 0);
        if (i.equipment.kind == ItemKind::Bow) {
            for (auto &p : BowPhysicsProfiles()) {
                auto w = Wide(p.name);
                SendMessageW(C(Archetype), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
            }
        } else {
            for (auto &p : ArrowPhysicsProfiles()) {
                auto w = Wide(p.name);
                SendMessageW(C(Archetype), CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(w.c_str()));
            }
        }
        SendMessageW(C(Archetype), CB_SETCURSEL, 0, 0);
        EnableWindow(C(Power), i.equipment.kind == ItemKind::Bow);
        EnableWindow(C(Mass), i.equipment.kind == ItemKind::Arrow);
        EnableWindow(C(Capacity), i.equipment.kind == ItemKind::Arrow);
    }
    void UpdatePreview() {
        if (!model)
            return;
        auto b = SendMessageW(C(PreviewBow), CB_GETCURSEL, 0, 0),
             a = SendMessageW(C(PreviewArrow), CB_GETCURSEL, 0, 0);
        if (b < 0 || a < 0 || static_cast<std::size_t>(b) >= bows.size() ||
            static_cast<std::size_t>(a) >= arrows.size()) {
            Text(PreviewOutput, L"Choose an enabled bow and arrow.");
            return;
        }
        double draw = SendMessageW(C(Draw), TBM_GETPOS, 0, 0) / 100.;
        auto shot = model->Preview(bows[b], arrows[a], draw);
        if (!shot) {
            Text(PreviewOutput, L"Invalid combination or global values.");
            return;
        }
        std::wostringstream s;
        s << std::fixed << std::setprecision(1) << draw * 100 << L"% draw   |   " << shot->launchSpeed
          << L" game units/s\n"
          << std::setprecision(3) << L"Relative momentum: " << shot->momentumRatio
          << L"\nPhysical-impact factor: " << shot->forceRatio << L"x"
          << (shot->capacityLimited ? L"   [shaft cap]" : L"")
          << (shot->speedLimited ? L"   [speed cap]" : L"") << L"\nImpact preview assumes launch speed.";
        Text(PreviewOutput, s.str());
    }
    void ImportFile(const std::filesystem::path &file, bool preset) {
        if (!model)
            throw std::runtime_error("Scan the active profile first");
        auto u = ParseUserConfig(ReadText(file));
        if (!u.valid)
            throw std::runtime_error(u.errors.front());
        if (preset &&
            MessageBoxW(hwnd,
                        L"Apply the MGO equipment preset and its global defaults to this draft? Missing "
                        L"equipment is skipped. Your game files are unchanged until you install an export.",
                        L"Optional MGO preset", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
            return;
        model->Import(u, preset);
        GlobalsToControls();
        Refresh();
        dirty = true;
        Text(Status, model->needsReview
                         ? L"This configuration came from a different scan. Review the equipment, then "
                           L"accept the current scan."
                         : L"Settings imported into the draft. Missing preset entries are skipped.");
    }
    void Export(bool zip) {
        if (!model || busy)
            throw std::runtime_error("Complete the active-profile scan first");
        ApplyGlobalValues();
        auto fresh = ScanPlugins(paths.dataDir, paths.pluginsFile, paths.orderFile);
        if (!fresh.complete || fresh.fingerprint != model->scan.fingerprint)
            throw std::runtime_error(
                "Active files changed. Relaunch the editor through MO2 and review a fresh scan.");
        auto dest = zip ? ChooseFile(true) : ChooseFolder(L"Select a dedicated BowSpeedVR settings mod");
        if (dest.empty())
            return;
        ExportSettings(*model, dest, zip);
        dirty = false;
        Text(Status, L"Exported settings to " + dest.wstring() +
                         L". Enable the settings mod in MO2 and restart Skyrim VR.");
    }
    void Command(int id, int notification) {
        if (id == GameBrowse) {
            auto p = ChooseFolder(L"Select Skyrim VR game directory");
            if (!p.empty())
                Text(Game, p.wstring());
        } else if (id == ProfileBrowse) {
            auto p = ChooseFolder(L"Select the active MO2 profile directory");
            if (!p.empty())
                Text(Profile, p.wstring());
        } else if (id == Scan)
            StartScan();
        else if (id == Filter && notification == EN_CHANGE)
            Refresh();
        else if (id == Kind && notification == CBN_SELCHANGE)
            Refresh();
        else if (id == Import) {
            auto p = ChooseFile(false);
            if (!p.empty())
                ImportFile(p, false);
        } else if (id == MGO) {
            std::wstring path(32768, L'\0');
            auto n = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
            path.resize(n);
            ImportFile(std::filesystem::path(path).parent_path() / L"presets/MGO.ini", true);
        } else if (id == Assign && selected) {
            model->AssignProfile(*selected, UTF8(Text(Archetype)));
            dirty = true;
            Refresh();
        } else if (id == ApplyItem && selected) {
            auto key = *selected;
            if (!Checked(Enable))
                model->SetEnabled(key, false);
            else if (model->items.at(key).equipment.kind == ItemKind::Bow)
                model->SetBow(key, Number(Power));
            else
                model->SetArrow(key, Number(Mass), Number(Capacity));
            dirty = true;
            Refresh();
        } else if (id == ApplyGlobals)
            ApplyGlobalValues();
        else if ((id == PreviewBow || id == PreviewArrow) && notification == CBN_SELCHANGE)
            UpdatePreview();
        else if (id == Review && model) {
            model->needsReview = false;
            EnableWindow(C(Review), FALSE);
            Text(Status, L"Current scan accepted. Export to apply your reviewed settings.");
        } else if (id == ExportZip)
            Export(true);
        else if (id == ExportDir)
            Export(false);
    }
};
LRESULT CALLBACK Procedure(HWND hwnd, UINT message, WPARAM w, LPARAM l) {
    auto *state = reinterpret_cast<Window *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        state = static_cast<Window *>(reinterpret_cast<CREATESTRUCTW *>(l)->lpCreateParams);
        state->hwnd = hwnd;
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(state));
    }
    if (!state)
        return DefWindowProcW(hwnd, message, w, l);
    try {
        switch (message) {
        case WM_CREATE:
            state->Build();
            return 0;
        case WM_SIZE:
            if (!state->controls.empty())
                state->Layout();
            return 0;
        case WM_COMMAND:
            state->Command(LOWORD(w), HIWORD(w));
            return 0;
        case WM_TIMER:
            state->Poll();
            return 0;
        case WM_HSCROLL:
            state->UpdatePreview();
            return 0;
        case WM_NOTIFY:
            if (reinterpret_cast<NMHDR *>(l)->idFrom == List &&
                reinterpret_cast<NMHDR *>(l)->code == LVN_ITEMCHANGED)
                state->SelectRow();
            return 0;
        case WM_GETMINMAXINFO:
            reinterpret_cast<MINMAXINFO *>(l)->ptMinTrackSize = {1100, 900};
            return 0;
        case WM_CLOSE:
            if (state->dirty && MessageBoxW(hwnd, L"Close without exporting your draft changes?",
                                            L"Unsaved draft", MB_OKCANCEL | MB_ICONQUESTION) != IDOK)
                return 0;
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, 1);
            PostQuitMessage(0);
            return 0;
        }
    } catch (const std::exception &e) {
        MessageBoxW(hwnd, Wide(e.what()).c_str(), L"BowSpeedVR", MB_OK | MB_ICONERROR);
    }
    return DefWindowProcW(hwnd, message, w, l);
}
} // namespace
int RunDesktop(const EditorOptions &options) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES};
    InitCommonControlsEx(&controls);
    Window state;
    state.options = options;
    WNDCLASSW cls{};
    cls.lpfnWndProc = Procedure;
    cls.hInstance = GetModuleHandleW(nullptr);
    cls.lpszClassName = L"BowSpeedVREditorWindow";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    cls.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&cls);
    auto hwnd =
        CreateWindowExW(0, cls.lpszClassName, L"BowSpeedVR Desktop Editor", WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, 1320, 940, nullptr, nullptr, cls.hInstance, &state);
    if (!hwnd)
        throw std::runtime_error("Cannot create desktop window");
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);
    if (std::filesystem::exists(std::filesystem::path(state.Text(Game)) / L"SkyrimVR.exe"))
        state.StartScan();
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (!IsDialogMessageW(hwnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    if (state.pending.valid())
        state.pending.wait();
    DeleteObject(state.font);
    DeleteObject(state.heading);
    CoUninitialize();
    return static_cast<int>(msg.wParam);
}
} // namespace BowSpeed::Editor
