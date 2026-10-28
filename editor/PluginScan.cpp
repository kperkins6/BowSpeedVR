#include "PluginScan.h"
#include "BowSpeed/ProjectilePolicy.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <set>
#include <span>
#include <sstream>
#include <stdexcept>
extern "C" {
#include "vendor/puff.h"
}
namespace BowSpeed::Editor {
namespace {
using Bytes = std::vector<unsigned char>;
constexpr std::size_t MaxRecord = 16 * 1024 * 1024;
std::string Lower(std::string s) {
    for (auto &c : s)
        if (c >= 'A' && c <= 'Z')
            c += ('a' - 'A');
    return s;
}
std::string Trim(std::string s) {
    auto a = s.find_first_not_of(" \t\r\n");
    return a == s.npos ? "" : s.substr(a, s.find_last_not_of(" \t\r\n") - a + 1);
}
std::uint32_t U32(std::span<const unsigned char> b, std::size_t p) {
    if (p + 4 > b.size())
        throw std::runtime_error("Truncated field");
    return b[p] | (b[p + 1] << 8) | (b[p + 2] << 16) | (std::uint32_t(b[p + 3]) << 24);
}
std::uint16_t U16(std::span<const unsigned char> b, std::size_t p) {
    if (p + 2 > b.size())
        throw std::runtime_error("Truncated field");
    return std::uint16_t(b[p] | (b[p + 1] << 8));
}
double Float(std::span<const unsigned char> b, std::size_t p) {
    return std::bit_cast<float>(U32(b, p));
}
std::string Tag(std::span<const unsigned char> b, std::size_t p = 0) {
    if (p + 4 > b.size())
        throw std::runtime_error("Truncated tag");
    return {reinterpret_cast<const char *>(b.data() + p), 4};
}
std::string String(std::span<const unsigned char> b) {
    if (b.empty())
        return {};
    auto end = std::find(b.begin(), b.end(), 0);
    return {reinterpret_cast<const char *>(b.data()), static_cast<std::size_t>(end - b.begin())};
}
void Hash(std::uint64_t &h, std::string_view s) {
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
}
std::string Hex(std::uint64_t n) {
    std::ostringstream s;
    s << std::hex << std::setw(16) << std::setfill('0') << n;
    return s.str();
}
struct Raw {
    FormKey key;
    std::string sig, name, edid, winner;
    std::uint32_t flags{};
    std::optional<FormKey> parent, projectile;
    int weaponType = -1;
    double speed{}, gravity{}, force{}, damage{};
    unsigned ammoFlags{}, projFlags{}, projTypes{};
    bool scripted{}, hasDnam{};
};
struct File {
    std::string name;
    std::vector<std::string> masters;
    bool localized{}, light{};
};
using Subrecords = std::vector<std::pair<std::string, std::span<const unsigned char>>>;
Subrecords Subs(std::span<const unsigned char> b) {
    Subrecords out;
    std::size_t p = 0;
    std::optional<std::uint32_t> extended;
    while (p < b.size()) {
        if (b.size() - p < 6)
            throw std::runtime_error("Truncated subrecord header");
        auto tag = Tag(b, p);
        std::size_t size = U16(b, p + 4);
        p += 6;
        if (tag == "XXXX") {
            if (size != 4 || extended || b.size() - p < 4)
                throw std::runtime_error("Invalid extended subrecord");
            extended = U32(b, p);
            p += 4;
            continue;
        }
        if (extended) {
            size = *extended;
            extended.reset();
        }
        if (size > b.size() - p)
            throw std::runtime_error("Subrecord exceeds record");
        out.emplace_back(tag, b.subspan(p, size));
        p += size;
    }
    if (extended)
        throw std::runtime_error("Dangling extended subrecord");
    return out;
}
std::span<const unsigned char> One(const Subrecords &s, std::string_view tag) {
    for (auto &[t, b] : s)
        if (t == tag)
            return b;
    return {};
}
Bytes Inflate(std::span<const unsigned char> b) {
    if (b.size() < 10)
        throw std::runtime_error("Truncated compressed record");
    auto size = U32(b, 0);
    if (size > MaxRecord)
        throw std::runtime_error("Inflated record exceeds 16 MiB");
    b = b.subspan(4);
    if ((b[0] & 15) != 8 || (b[0] >> 4) > 7 || ((b[0] * 256 + b[1]) % 31) || (b[1] & 32))
        throw std::runtime_error("Invalid zlib header");
    Bytes output(size);
    auto input = b.subspan(2, b.size() - 6);
    unsigned long outSize = size, inSize = static_cast<unsigned long>(input.size());
    if (puff(output.data(), &outSize, input.data(), &inSize) != 0 || outSize != size ||
        inSize != input.size())
        throw std::runtime_error("Invalid compressed record");
    std::uint32_t a = 1, c = 0;
    for (auto byte : output) {
        a = (a + byte) % 65521;
        c = (c + a) % 65521;
    }
    auto n = b.size();
    auto expected = (std::uint32_t(b[n - 4]) << 24) | (b[n - 3] << 16) | (b[n - 2] << 8) | b[n - 1];
    if (((c << 16) | a) != expected)
        throw std::runtime_error("Compressed record checksum mismatch");
    return output;
}
FormKey Reference(std::uint32_t id, const File &file, const std::map<std::string, bool> &lights) {
    auto index = id >> 24;
    auto local = id & 0xFFFFFF;
    if (!local || index > file.masters.size())
        throw std::runtime_error("Unresolved file-local FormID in " + file.name);
    auto origin = index == file.masters.size() ? file.name : file.masters[index];
    auto key = CanonicalFormKey(origin, local);
    if (auto i = lights.find(Lower(origin)); i != lights.end() && i->second && local > 0xFFF)
        throw std::runtime_error("Invalid local ID for light plugin " + origin);
    return key;
}
void ParseFile(const std::filesystem::path &path, File &file, std::map<FormKey, Raw> &records,
               std::map<std::string, bool> &lights, const std::set<std::string> &loaded,
               std::uint64_t &hash) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("Cannot open enabled plugin " + file.name);
    const auto fileSize = std::filesystem::file_size(path);
    Hash(hash, Lower(file.name));
    Hash(hash, std::to_string(fileSize));
    std::function<void(std::uint64_t, std::uint64_t, unsigned)> walk;
    bool header = false;
    walk = [&](std::uint64_t begin, std::uint64_t limit, unsigned depth) {
        if (depth > 32)
            throw std::runtime_error("Plugin group nesting exceeds 32");
        auto at = begin;
        while (at < limit) {
            if (limit - at < 24)
                throw std::runtime_error("Truncated record/group header");
            input.seekg(static_cast<std::streamoff>(at));
            std::array<unsigned char, 24> h{};
            input.read(reinterpret_cast<char *>(h.data()), 24);
            if (!input)
                throw std::runtime_error("Cannot read record header");
            auto sig = Tag(h);
            auto size = U32(h, 4);
            if (sig == "GRUP") {
                if (size < 24 || size > limit - at)
                    throw std::runtime_error("Invalid group length");
                if (U32(h, 12) != 0 || Tag(h, 8) == "WEAP" || Tag(h, 8) == "AMMO" || Tag(h, 8) == "PROJ" ||
                    Tag(h, 8) == "KYWD")
                    walk(at + 24, at + size, depth + 1);
                at += size;
                continue;
            }
            if (size > limit - at - 24)
                throw std::runtime_error("Record exceeds containing group");
            const bool wanted =
                sig == "TES4" || sig == "WEAP" || sig == "AMMO" || sig == "PROJ" || sig == "KYWD";
            if (!header && sig != "TES4")
                throw std::runtime_error("Plugin does not begin with TES4 header");
            if (wanted) {
                if (size > MaxRecord)
                    throw std::runtime_error("Relevant record exceeds 16 MiB");
                Bytes payload(size);
                input.read(reinterpret_cast<char *>(payload.data()), size);
                if (!input)
                    throw std::runtime_error("Truncated payload");
                Hash(hash, {reinterpret_cast<const char *>(h.data()), h.size()});
                Hash(hash, {reinterpret_cast<const char *>(payload.data()), payload.size()});
                const auto flags = U32(h, 8);
                if (flags & 0x40000)
                    payload = Inflate(payload);
                auto subs = Subs(payload);
                if (sig == "TES4") {
                    if (header || at != 0)
                        throw std::runtime_error("Duplicate/misplaced TES4 header");
                    header = true;
                    file.localized = (flags & 0x80) != 0;
                    file.light = (flags & 0x200) != 0;
                    lights[Lower(file.name)] = file.light;
                    for (const auto &[tag, data] : subs)
                        if (tag == "MAST") {
                            auto name = String(data);
                            if (!loaded.contains(Lower(name)))
                                throw std::runtime_error("Missing or later master " + name + " required by " +
                                                         file.name);
                            file.masters.push_back(name);
                        }
                } else {
                    Raw r;
                    r.key = Reference(U32(h, 12), file, lights);
                    r.sig = sig;
                    r.flags = flags;
                    r.winner = file.name;
                    r.edid = String(One(subs, "EDID"));
                    auto full = One(subs, "FULL");
                    r.name = file.localized ? "" : String(full);
                    if (r.name.empty())
                        r.name = r.edid.empty() ? FormatFormKey(r.key) : r.edid;
                    r.scripted = !One(subs, "VMAD").empty();
                    auto data = One(subs, "DATA");
                    if (sig == "WEAP") {
                        auto d = One(subs, "DNAM");
                        if (d.size() >= 8) {
                            r.hasDnam = true;
                            r.weaponType = d[0];
                            r.speed = Float(d, 4);
                        }
                        auto t = One(subs, "CNAM");
                        if (t.size() >= 4 && U32(t, 0))
                            r.parent = Reference(U32(t, 0), file, lights);
                        if (data.size() >= 10)
                            r.damage = U16(data, 8);
                    } else if (sig == "AMMO") {
                        if (!(flags & 0x20) && data.size() < 16)
                            throw std::runtime_error("Missing ammunition DATA");
                        if (data.size() >= 16) {
                            if (U32(data, 0))
                                r.projectile = Reference(U32(data, 0), file, lights);
                            r.ammoFlags = U32(data, 4);
                            r.damage = Float(data, 8);
                        }
                    } else if (sig == "PROJ") {
                        if (!(flags & 0x20) && data.size() < 56)
                            throw std::runtime_error("Missing projectile DATA");
                        if (data.size() >= 56) {
                            r.projFlags = U16(data, 0);
                            r.projTypes = U16(data, 2);
                            r.gravity = Float(data, 4);
                            r.speed = Float(data, 8);
                            r.force = Float(data, 52);
                        }
                    }
                    if (!(flags & 0x1000))
                        records[r.key] = std::move(r);
                }
            }
            at += 24 + size;
        }
    };
    walk(0, fileSize, 0);
    if (!header)
        throw std::runtime_error("Empty plugin " + file.name);
}
std::string Json(std::string_view s) {
    std::ostringstream o;
    o << '"';
    for (unsigned char c : s) {
        if (c == '"' || c == '\\')
            o << '\\' << c;
        else if (c < 32)
            o << "\\u" << std::hex << std::setw(4) << std::setfill('0') << unsigned(c) << std::dec;
        else
            o << c;
    }
    o << '"';
    return o.str();
}
} // namespace
std::string ReadText(const std::filesystem::path &path, std::size_t maximum) {
    std::ifstream f(path, std::ios::binary);
    if (!f)
        throw std::runtime_error("Cannot read " + path.string());
    auto size = std::filesystem::file_size(path);
    if (size > maximum)
        throw std::runtime_error("Text file exceeds size limit");
    std::string s(static_cast<std::size_t>(size), '\0');
    f.read(s.data(), static_cast<std::streamsize>(size));
    if (!f)
        throw std::runtime_error("Incomplete text read");
    if (s.starts_with("\xEF\xBB\xBF"))
        s.erase(0, 3);
    return s;
}
ScanResult ScanPlugins(const std::filesystem::path &dataDir, const std::filesystem::path &pluginsFile,
                       const std::filesystem::path &orderFile, PluginResolver resolver) {
    ScanResult out;
    out.dataDir = dataDir;
    out.pluginsFile = pluginsFile;
    std::uint64_t hash = 14695981039346656037ULL;
    try {
        std::istringstream list(ReadText(pluginsFile));
        std::string line;
        std::set<std::string> enabled;
        bool anyName = false;
        while (std::getline(list, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#')
                continue;
            anyName = true;
            if (line[0] != '*')
                continue;
            auto name = Trim(line.substr(1));
            if (!ParseFormKey(name + "|0x1"))
                throw std::runtime_error("Invalid enabled plugin name");
            if (!enabled.insert(Lower(name)).second)
                throw std::runtime_error("Duplicate enabled plugin " + name);
            out.plugins.push_back(name);
        }
        if (anyName && out.plugins.empty())
            throw std::runtime_error("No starred active plugins; ambiguous legacy/disabled plugin list. "
                                     "Supply a current MO2 profile.");
        if (!resolver)
            resolver = [&](const std::string &name) {
                return dataDir / std::filesystem::path(std::u8string(
                                     reinterpret_cast<const char8_t *>(name.data()), name.size()));
            };
        const std::array core = {"Skyrim.esm",      "Update.esm",     "Dawnguard.esm",
                                 "HearthFires.esm", "Dragonborn.esm", "SkyrimVR.esm"};
        std::vector<std::string> ordered;
        for (auto name : core)
            if (std::filesystem::exists(resolver(name))) {
                ordered.push_back(name);
                enabled.insert(Lower(name));
            }
        for (const auto &name : out.plugins)
            if (std::none_of(ordered.begin(), ordered.end(),
                             [&](const auto &n) { return Lower(n) == Lower(name); }))
                ordered.push_back(name);
        out.plugins = ordered;
        // Starred plugins supply non-core order. Official masters have fixed
        // engine order. Companion data may omit those implicit masters, but
        // must agree on and cover every other active plugin.
        if (!orderFile.empty() && std::filesystem::exists(orderFile)) {
            std::istringstream order(ReadText(orderFile));
            std::set<std::string> seen;
            std::vector<std::string> companion;
            while (std::getline(order, line)) {
                line = Trim(line);
                if (line.empty() || line[0] == '#')
                    continue;
                if (!ParseFormKey(line + "|0x1"))
                    throw std::runtime_error("Invalid companion ordering entry");
                auto name = Lower(line);
                if (!seen.insert(name).second)
                    throw std::runtime_error("Duplicate companion ordering entry: " + line);
                if (enabled.contains(name))
                    companion.push_back(name);
            }
            std::vector<std::string> expected;
            for (const auto &name : out.plugins) {
                const auto lower = Lower(name);
                const bool official =
                    std::any_of(core.begin(), core.end(), [&](const auto *c) { return lower == Lower(c); });
                if (!official || seen.contains(lower))
                    expected.push_back(lower);
            }
            if (companion != expected)
                throw std::runtime_error(
                    "plugins.txt and loadorder.txt disagree, or companion ordering is incomplete");
        }
        if (out.plugins.empty())
            throw std::runtime_error("No base or enabled plugins found");
        std::map<FormKey, Raw> records;
        std::map<std::string, bool> lights;
        std::set<std::string> loaded;
        for (const auto &name : out.plugins) {
            File file{name};
            try {
                ParseFile(resolver(name), file, records, lights, loaded, hash);
            } catch (const std::exception &e) {
                throw std::runtime_error(name + ": " + e.what());
            }
            loaded.insert(Lower(name));
        }
        for (const auto &[key, raw] : records) {
            if (raw.flags & 0x20)
                continue;
            if (raw.sig != "WEAP" && raw.sig != "AMMO")
                continue;
            Raw r = raw;
            std::set<FormKey> chain;
            std::string reason;
            while (r.sig == "WEAP" && r.parent) {
                if (!chain.insert(r.key).second) {
                    reason = "Weapon template cycle";
                    break;
                }
                auto p = records.find(*r.parent);
                if (p == records.end() || p->second.sig != "WEAP" || (p->second.flags & 0x20)) {
                    reason = "Missing weapon template";
                    break;
                }
                r.weaponType = p->second.weaponType;
                r.speed = p->second.speed;
                r.damage = p->second.damage;
                r.hasDnam = p->second.hasDnam;
                r.parent = p->second.parent;
                r.key = p->second.key;
            }
            const auto type = raw.sig == "AMMO"   ? ItemKind::Arrow
                              : r.weaponType == 9 ? ItemKind::Crossbow
                                                  : ItemKind::Bow;
            if (raw.sig == "WEAP" && r.weaponType != 7 && r.weaponType != 9 && reason.empty())
                continue;
            Equipment item{key,    type,       raw.name,       raw.edid,    raw.winner,
                           reason, raw.parent, raw.projectile, r.speed,     0,
                           0,      raw.damage, false,          raw.scripted};
            if (reason.empty() && (raw.flags & 4))
                reason = "Non-playable equipment";
            if (reason.empty() && raw.scripted)
                reason = "Scripted equipment requires a reviewed integration";
            if (reason.empty() && type == ItemKind::Crossbow)
                reason = "Crossbow pairing is outside this release";
            if (raw.sig == "AMMO") {
                if (reason.empty() && (!(raw.ammoFlags & 4) || (raw.ammoFlags & 2)))
                    reason = "Bolt or non-playable ammunition";
                auto p = raw.projectile ? records.find(*raw.projectile) : records.end();
                if (p == records.end() || p->second.sig != "PROJ" || (p->second.flags & 0x20)) {
                    if (reason.empty())
                        reason = "Missing arrow projectile";
                } else {
                    const auto &proj = p->second;
                    item.nativeSpeed = proj.speed;
                    item.gravity = proj.gravity;
                    item.force = proj.force;
                    if (reason.empty() &&
                        (proj.scripted || !OrdinaryArrowPhysics(static_cast<std::uint16_t>(proj.projTypes),
                                                                static_cast<std::uint16_t>(proj.projFlags),
                                                                proj.gravity, proj.speed, proj.force)))
                        reason = "Special or invalid projectile physics";
                }
            } else if (reason.empty() && (!std::isfinite(r.speed) || r.speed <= 0))
                reason = "Invalid weapon speed";
            item.reason = reason;
            item.ordinary = reason.empty();
            out.equipment.emplace(key, std::move(item));
        }
        out.fingerprint = Hex(hash);
        out.complete = true;
    } catch (const std::exception &e) {
        out.errors.push_back(e.what());
        out.complete = false;
    }
    return out;
}
std::string ScanReport(const ScanResult &scan) {
    std::ostringstream s;
    s << std::boolalpha << std::setprecision(12) << "{\n  \"complete\": " << scan.complete
      << ",\n  \"fingerprint\": " << Json(scan.fingerprint) << ",\n  \"pluginCount\": " << scan.plugins.size()
      << ",\n  \"errors\": [";
    for (std::size_t i = 0; i < scan.errors.size(); ++i) {
        if (i)
            s << ',';
        s << Json(scan.errors[i]);
    }
    s << "],\n  \"equipment\": [\n";
    bool first = true;
    for (const auto &[key, item] : scan.equipment) {
        if (!first)
            s << ",\n";
        first = false;
        s << "    {\"key\":" << Json(FormatFormKey(key)) << ",\"name\":" << Json(item.name) << ",\"kind\":"
          << Json(item.kind == ItemKind::Arrow      ? "arrow"
                  : item.kind == ItemKind::Crossbow ? "crossbow"
                                                    : "bow")
          << ",\"winner\":" << Json(item.winningPlugin) << ",\"ordinary\":" << item.ordinary
          << ",\"reason\":" << Json(item.reason);
        if (std::isfinite(item.nativeSpeed))
            s << ",\"nativeSpeed\":" << item.nativeSpeed;
        if (std::isfinite(item.gravity))
            s << ",\"gravity\":" << item.gravity;
        if (std::isfinite(item.force))
            s << ",\"force\":" << item.force;
        s << '}';
    }
    s << "\n  ]\n}\n";
    return s.str();
}
} // namespace BowSpeed::Editor
