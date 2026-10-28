#include "Check.h"
#include "PluginScan.h"
#include <cstring>
#include <fstream>
#include <iostream>
using Bytes = std::vector<unsigned char>;
void U32(Bytes &b, std::uint32_t n) {
    for (int i = 0; i < 4; ++i)
        b.push_back(static_cast<unsigned char>(n >> (i * 8)));
}
void Put32(Bytes &b, std::size_t at, std::uint32_t n) {
    for (int i = 0; i < 4; ++i)
        b.at(at + i) = static_cast<unsigned char>(n >> (i * 8));
}
void Float(Bytes &b, std::size_t at, float f) {
    std::uint32_t n;
    std::memcpy(&n, &f, 4);
    Put32(b, at, n);
}
Bytes Sub(const char *tag, Bytes data) {
    Bytes b(tag, tag + 4);
    b.push_back(static_cast<unsigned char>(data.size()));
    b.push_back(static_cast<unsigned char>(data.size() >> 8));
    b.insert(b.end(), data.begin(), data.end());
    return b;
}
Bytes Text(const char *t) {
    return Bytes(t, t + std::strlen(t) + 1);
}
void Append(Bytes &b, const Bytes &more) {
    b.insert(b.end(), more.begin(), more.end());
}
Bytes Record(const char *tag, std::uint32_t id, Bytes data, std::uint32_t flags = 0) {
    Bytes b(tag, tag + 4);
    U32(b, static_cast<std::uint32_t>(data.size()));
    U32(b, flags);
    U32(b, id);
    U32(b, 0);
    U32(b, 44);
    Append(b, data);
    return b;
}
Bytes Header(std::vector<std::string> masters = {}, unsigned flags = 0) {
    Bytes d;
    for (const auto &m : masters)
        Append(d, Sub("MAST", Text(m.c_str())));
    return Record("TES4", 0, d, flags);
}
Bytes Bow(unsigned id, const char *name, float speed, unsigned flags = 0, std::optional<unsigned> parent = {},
          unsigned damage = 7, std::vector<unsigned> keywords = {}) {
    Bytes d = Sub("EDID", Text(name));
    Append(d, Sub("FULL", Text(name)));
    Bytes dn(100);
    dn[0] = 7;
    Float(dn, 4, speed);
    Append(d, Sub("DNAM", dn));
    Bytes stats(10);
    stats[8] = static_cast<unsigned char>(damage);
    stats[9] = static_cast<unsigned char>(damage >> 8);
    Append(d, Sub("DATA", stats));
    if (!keywords.empty()) {
        Bytes ids, count;
        U32(count, static_cast<unsigned>(keywords.size()));
        for (auto keyword : keywords)
            U32(ids, keyword);
        Append(d, Sub("KSIZ", count));
        Append(d, Sub("KWDA", ids));
    }
    if (parent) {
        Bytes ref;
        U32(ref, *parent);
        Append(d, Sub("CNAM", ref));
    }
    return Record("WEAP", id, d, flags);
}
void Save(const std::filesystem::path &p, const Bytes &b) {
    std::ofstream f(p, std::ios::binary);
    f.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
}
Bytes CompressedRecord(const char *tag, unsigned id, const Bytes &payload) {
    Bytes d;
    U32(d, static_cast<unsigned>(payload.size()));
    d.insert(d.end(), {0x78, 0x01, 0x01});
    auto len = static_cast<unsigned>(payload.size());
    d.push_back(static_cast<unsigned char>(len));
    d.push_back(static_cast<unsigned char>(len >> 8));
    d.push_back(static_cast<unsigned char>(~len));
    d.push_back(static_cast<unsigned char>((~len) >> 8));
    Append(d, payload);
    unsigned a = 1, b = 0;
    for (auto c : payload) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    auto checksum = (b << 16) | a;
    for (int i = 3; i >= 0; --i)
        d.push_back(static_cast<unsigned char>(checksum >> (8 * i)));
    return Record(tag, id, d, 0x40000);
}
int main() {
    try {
        using namespace BowSpeed;
        using namespace BowSpeed::Editor;
        auto root = std::filesystem::temp_directory_path() /
                    std::filesystem::path(
                        "bowspeed-scan-" +
                        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(root / "Data");
        auto data = root / "Data";
        auto base = Header();
        Append(base, Record("KYWD", 0x900, Sub("EDID", Text("WeapMaterialIron"))));
        Append(base, Record("KYWD", 0x901, Sub("EDID", Text("WeapMaterialGlass"))));
        Append(base, Bow(0x800, "Base bow", 1, 0, {}, 7, {0x900}));
        Save(data / "Skyrim.esm", base);
        auto patch = Header({"Skyrim.esm"});
        Append(patch, Bow(0x800, "Overridden bow", .75f, 0, {}, 12, {0x901}));
        Append(patch, Bow(0x01000801, "New bow", .875f));
        Save(data / "Patch.esp", patch);
        auto disabled = Header();
        Append(disabled, Bow(0x800, "Disabled bow", 1));
        Save(data / "Disabled.esp", disabled);
        {
            std::ofstream f(root / "plugins.txt");
            f << "*Skyrim.esm\n*Patch.esp\nDisabled.esp\n";
        }
        auto scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete, "Valid active plugin scan completes");
        Check(scan.equipment.size() == 2, "Disabled plugins do not add equipment");
        auto key = CanonicalFormKey("Skyrim.esm", 0x800);
        Check(scan.equipment.at(key).name == "Overridden bow", "Last enabled override wins");
        Check(scan.equipment.at(key).winningPlugin == "Patch.esp",
              "Origin identity remains separate from winning patch");
        Check(scan.equipment.at(key).materialKeywords == std::vector<std::string>{"WeapMaterialGlass"},
              "Winning keyword references resolve through the patch master table");
        Check(scan.equipment.at(key).damage == 12, "Winning base damage is exposed for inference");
        Check(scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).ordinary,
              "New physical bow discovered");
        Check(scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).nativeSpeed == .875,
              "Weapon speed is read as a record rating");
        auto first = scan.fingerprint;
        auto other = Header({"Skyrim.esm"});
        Append(other, Bow(0x800, "Other override", .5f));
        Save(data / "Other.esp", other);
        {
            std::ofstream f(root / "plugins.txt");
            f << "*Skyrim.esm\n*Patch.esp\n*Other.esp\nDisabled.esp\n";
        }
        {
            std::ofstream f(root / "loadorder.txt");
            f << "Skyrim.esm\nOther.esp\nPatch.esp\n";
        }
        Check(!ScanPlugins(data, root / "plugins.txt", root / "loadorder.txt").complete,
              "Conflicting ordering sources must block export");
        {
            std::ofstream f(root / "loadorder.txt");
            f << "Skyrim.esm\nPatch.esp\nOther.esp\nDisabled.esp\n";
        }
        auto ordered = ScanPlugins(data, root / "plugins.txt", root / "loadorder.txt");
        Check(ordered.complete && ordered.equipment.at(key).name == "Other override",
              "Consistent order selects the actual last patch");
        {
            std::ofstream f(root / "loadorder.txt");
            f << "Skyrim.esm\nOther.esp\n";
        }
        Check(!ScanPlugins(data, root / "plugins.txt", root / "loadorder.txt").complete,
              "Incomplete companion ordering must not synthesize an order");
        {
            std::ofstream f(root / "loadorder.txt");
            f << "Skyrim.esm\nPatch.esp\nPatch.esp\nOther.esp\n";
        }
        Check(!ScanPlugins(data, root / "plugins.txt", root / "loadorder.txt").complete,
              "Duplicate ordering entries are rejected");
        {
            std::ofstream f(root / "loadorder.txt");
            f << "Patch.esp\nOther.esp\n";
        }
        Check(ScanPlugins(data, root / "plugins.txt", root / "loadorder.txt").complete,
              "Implicit official masters do not require companion entries");
        Check(ScanPlugins(data, root / "plugins.txt", root / "no-order.txt").complete,
              "Starred list supplies order when no companion file exists");
        {
            std::ofstream f(root / "plugins.txt");
            f << "*Skyrim.esm\n*Patch.esp\nDisabled.esp\n";
        }
        Append(patch, Bow(0x800, "Deleted bow", 1, 0x20));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(!scan.equipment.contains(key), "Deleted winning records suppressed");
        Check(first != scan.fingerprint, "Changed relevant records invalidate scan fingerprint");
        Save(data / "Patch.esp", Header({"Missing.esm"}));
        Check(!ScanPlugins(data, root / "plugins.txt").complete, "Missing master blocks complete export");
        auto light = Header({"Skyrim.esm"}, 0x200);
        Append(light, Bow(0x01000822, "Light bow", 1));
        Save(data / "Light.esl", light);
        patch = Header({"Skyrim.esm", "Light.esl"});
        Append(patch, Bow(0x01000822, "Light override", .8f));
        Save(data / "Patch.esp", patch);
        {
            std::ofstream f(root / "plugins.txt");
            f << "*Skyrim.esm\n*Light.esl\n*Patch.esp\n";
        }
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete &&
                  scan.equipment.at(CanonicalFormKey("Light.esl", 0x822)).name == "Light override",
              "Light master references resolve independently of FE runtime slot");
        patch = Header({"Skyrim.esm"});
        Append(patch, Bow(0x01000801, "Templated bow", .125f, 0, 0x800, 99));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete && scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).nativeSpeed == 1,
              "Template statistics override serialized child statistics");
        Check(scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).damage == 7 &&
                  scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).materialKeywords ==
                      std::vector<std::string>{"WeapMaterialIron"},
              "Inference uses resolved template damage and inherited material evidence");
        auto malformed = Bow(0x01000802, "Malformed keywords", 1);
        Bytes badPayload(malformed.begin() + 24, malformed.end());
        Append(badPayload, Sub("KWDA", Bytes{1, 2, 3}));
        auto badPatch = patch;
        Append(badPatch, Record("WEAP", 0x01000802, badPayload));
        Save(data / "Patch.esp", badPatch);
        Check(!ScanPlugins(data, root / "plugins.txt").complete,
              "Malformed keyword arrays fail closed instead of supplying false material evidence");
        patch = Header({"Skyrim.esm"});
        Append(patch, Bow(0x01000801, "Cycle one", 1, 0, 0x01000802));
        Append(patch, Bow(0x01000802, "Cycle two", 1, 0, 0x01000801));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(!scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).ordinary,
              "Template cycles are excluded rather than guessed");
        patch = Header({"Skyrim.esm"});
        auto weapon = Bow(0x01000801, "Compressed bow", 1);
        Bytes payload(weapon.begin() + 24, weapon.end());
        Append(patch, CompressedRecord("WEAP", 0x01000801, payload));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete &&
                  scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).name == "Compressed bow",
              "Compressed record decoded with bounded output");
        patch.back() ^= 1;
        Save(data / "Patch.esp", patch);
        Check(!ScanPlugins(data, root / "plugins.txt").complete, "Corrupt zlib checksum rejected");
        patch = Header({"Skyrim.esm"});
        Bytes extended = Sub("XXXX", Bytes{0x10, 0x00, 0x01, 0x00});
        Append(extended, Bytes{'E', 'D', 'I', 'D', 0, 0});
        Append(extended, Bytes(65552, 'x'));
        Append(extended, Sub("DNAM", Bytes{7, 0, 0, 0, 0, 0, 0x80, 0x3f}));
        Append(patch, Record("WEAP", 0x01000801, extended));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete &&
                  scan.equipment.at(CanonicalFormKey("Patch.esp", 0x801)).editorID.size() == 65552,
              "XXXX sizes are honored beyond uint16 length");
        patch = Header({"Skyrim.esm"});
        Bytes proj(92);
        proj[2] = 64;
        Float(proj, 4, 1);
        Float(proj, 8, 3600);
        Float(proj, 52, 1);
        Append(patch, Record("PROJ", 0x01000802, Sub("DATA", proj)));
        Bytes ammo(16);
        Put32(ammo, 0, 0x01000802);
        Put32(ammo, 4, 4);
        Float(ammo, 8, 8);
        Bytes ad = Sub("EDID", Text("TestArrow"));
        Append(ad, Sub("DATA", ammo));
        Append(patch, Record("AMMO", 0x01000803, ad));
        Save(data / "Patch.esp", patch);
        scan = ScanPlugins(data, root / "plugins.txt");
        Check(scan.complete && scan.equipment.at(CanonicalFormKey("Patch.esp", 0x803)).ordinary &&
                  scan.equipment.at(CanonicalFormKey("Patch.esp", 0x803)).nativeSpeed == 3600,
              "AMMO resolves its winning PROJ speed and arrow classification");
        Check(!ScanPlugins(data, root / "absent.txt").complete,
              "Missing active list does not scan all installed files");
        Save(data / "Patch.esp", Bytes{'B', 'A', 'D'});
        Check(!ScanPlugins(data, root / "plugins.txt").complete, "Malformed plugin fails closed");
        std::filesystem::remove_all(root);
        std::cout << "Active scanner identity/override/deletion/light/error tests passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
