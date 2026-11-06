#pragma once
// The game picker of the all-games download (one program for every game): finds the players'
// ROMs, reads each one's name and icon from the ROM itself (every DS game carries a 32x32 icon
// and its title in its banner), and draws the list. Kept free of any platform so that it can be
// drawn into a picture on a PC as well; the Switch shows it with switch_platform.cpp before the
// game's window opens.
#include <algorithm>
#include <climits>
#include <filesystem>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <cmath>
#include <vector>
#include <map>
#include <dirent.h>
#include "ssr_os.h"
#include "gamecode.h"
#include <sys/stat.h>

#include "ui.h"
#include "optional_features.h"
// (JPEG and PNG pictures for the list: the program that draws it defines STB_IMAGE_IMPLEMENTATION
// with STBI_ONLY_JPEG and STBI_ONLY_PNG before including this, in one file.)
#include "thirdparty/stb_image.h"

namespace Launcher
{

// Keep the established launcher font while the replacement design is unfinished.
inline void ModernText(Canvas& c, int x, int y, const std::string& text, int scale, uint32_t colour)
{
    c.Text(x, y, text, scale, colour);
}

// The consoles the list has games of, in the order its sections and tabs come in.
enum Platform { DS, GBA, N3DS, PLATFORMS };
inline const char* PlatformName(int p) { return p == DS ? "DS" : p == GBA ? "GBA" : "3DS"; }
// The tabs along the top: ALL (every section, each under its console's heading), then one per
// console.
// (HOME: the games the player added with X, their favourites; until they add any, the game played
// last. ROMHACKS: the hacks, whatever their console.)
enum TabId { TAB_HOME, TAB_ALL, TAB_DS, TAB_GBA, TAB_3DS, TAB_HACKS, TABS };
inline const char* TabName(int t) { static const char* n[] = { "HOME", "ALL", "DS", "GBA", "3DS", "ROMHACKS" }; return t >= 0 && t < TABS ? n[t] : "ALL"; }
inline const char* TabLabel(int t) { static const char* n[] = { "Home", "All", "DS", "GBA", "3DS", "Romhacks" }; return t >= 0 && t < TABS ? n[t] : "All"; }

struct Game
{
    int platform = DS;
    std::string rom;      // the ROM's full path
    std::string folder;   // where its save (and a funcs.txt of the player's own) live
    std::string code;     // the game code, e.g. IREO
    std::string name;     // as shown, e.g. POKEMON BLACK 2
    std::string saveName; // e.g. black2.sav
    bool known = false;   // one of the games this program plays
    bool hasSave = false;
    bool hasIcon = false;
    uint32_t icon[32 * 32] = {}; // ARGB, transparent where the icon is
    // A larger picture for the game, if the player has one (FindArt): ARGB, Art x Art pixels.
    std::vector<uint32_t> art;
    // Set when the same game is also in another folder: this one's folder, shown under it.
    std::string where;
    // A game on its way (AddComingSoon): its tile says so and it cannot be played.
    bool comingSoon = false;
    bool favourite = false;
    std::string customName; // the player's own name for it (Rename; fulldex-names.txt), empty: its own
    long long lastPlayed = 0;
};

inline std::string& ContinueRom() { static std::string r; return r; } // the game played last (last-game.txt)
// Where the arrows are on the glass launcher besides the games and the tabs: a section of the
// sidebar (side, -1: none) or the banner's Continue (banner 1) or Game Options (2).
// popup: the game's menu (A on a game) open on that row, -1 closed; where it was drawn (for taps).
// dialog: 1 Remove Game, 2 Delete ROM File (sure?), 3 Rename (text: what is typed); drow its row;
// dX..dRows where its rows were drawn (for taps).
struct GlassFocus { int side = -1; int banner = 0; int popup = -1; int popX = 0, popY = 0, popW = 0, popRow = 0;
                    int dialog = 0, drow = 0; std::string text; int dX = 0, dY = 0, dW = 0, dRow = 0, dRows = 0; };
inline GlassFocus& Focus() { static GlassFocus f; return f; }
inline int PopupCount() { return 6; }
inline std::string PopupLabel(const Game& g, int i)
{
    static const char* n[] = { "Play", "", "Rename", "Change Picture", "Manage Saves", "Remove Game" };
    return i == 1 ? (g.favourite ? "Remove From Home" : "Add To Home") : i >= 0 && i < 6 ? n[i] : "";
}
// The pause menu's keyboard page (Menu::keyGrid) for Rename: BACK, four rows of keys (capitals, small
// letters or signs: layer), then the two layer keys, SPACE, DELETE and done. ids: each item's key.
enum { KB_BACK = -1, KB_KEY0 = 1000, KB_LAYER = 2000, KB_SYMBOLS, KB_SPACE, KB_DEL, KB_DONE };
inline void BuildKeyboard(Menu& m, std::vector<int>& ids, const std::string& text, int layer, const char* prompt, const char* done)
{
    const int keep = m.open ? m.selected : -1;
    m.items.clear();
    ids.clear();
    m.open = true; m.style = 1; m.pageIcon = Glass::I_SPARKLE;
    m.keyGrid = true; m.keySecret = false; m.keyText = text; m.keyPrompt = prompt;
    m.items.push_back({ "BACK", "" });
    m.items.back().kind = Menu::K_BACK;
    ids.push_back(KB_BACK);
    static const char* const layers[3][4] = {
        { "1234567890", "QWERTYUIOP", "ASDFGHJKL-", "ZXCVBNM_.!" },
        { "1234567890", "qwertyuiop", "asdfghjkl-", "zxcvbnm_.!" },
        { "1234567890", "!#$%&*()+=", "[]{}<>/\\|~", ";:'\",?^`.@" } };
    auto key = [&](int id, const std::string& label, int row, int col, int span) {
        m.items.push_back({ label, "" });
        ids.push_back(id);
        MenuItem& it = m.items.back();
        it.kind = Menu::K_KEY; it.keyRow = row; it.keyCol = col; it.keySpan = span;
    };
    const char* const* rows = layers[std::max(0, std::min(2, layer))];
    for (int r = 0; r < 4; r++)
    {
        const int n = (int)strlen(rows[r]);
        for (int i = 0; i < n; i++) key(KB_KEY0 + r * 16 + i, std::string(1, rows[r][i]), r, 10 - n + i * 2, 2);
    }
    key(KB_LAYER, layer == 0 ? "abc" : "ABC", 4, 0, 3);
    key(KB_SYMBOLS, layer == 2 ? "abc" : "#+=", 4, 3, 3);
    key(KB_SPACE, "Space", 4, 6, 6);
    key(KB_DEL, "Delete", 4, 12, 4);
    key(KB_DONE, done, 4, 16, 4);
    m.selected = keep > 0 && keep < (int)m.items.size() ? keep : 1;
}
inline int DialogAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (!f.dialog || f.dW <= 0 || f.dRow <= 0 || x < f.dX || x >= f.dX + f.dW || y < f.dY) return -1;
    const int i = (y - f.dY) / f.dRow;
    return i < f.dRows ? i : -1;
}
// The player's own changes to the list, kept beside the saves: fulldex-names.txt (a ROM's path, a tab,
// the name it shows) and fulldex-hidden.txt (the ROMs taken off the list, the same way).
inline std::map<std::string, std::string> ReadPairs(const std::string& path)
{
    std::map<std::string, std::string> out;
    if (FILE* f = fopen(path.c_str(), "rb"))
    {
        char line[4096];
        while (fgets(line, sizeof line, f))
        {
            std::string s = line;
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            const size_t t = s.find('\t');
            if (t != std::string::npos && t > 0) out[s.substr(0, t)] = s.substr(t + 1);
        }
        fclose(f);
    }
    return out;
}
inline bool SetPair(const std::string& path, const std::string& key, const std::string& value)
{
    std::map<std::string, std::string> all = ReadPairs(path);
    if (value.empty()) all.erase(key);
    else all[key] = value;
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) return false;
    for (const auto& kv : all) fprintf(f, "%s\t%s\n", kv.first.c_str(), kv.second.c_str());
    return fclose(f) == 0;
}
inline void ApplyLocal(std::vector<Game>& games, const std::string& savesDir)
{
    const auto names = ReadPairs(savesDir + "/fulldex-names.txt");
    const auto hidden = ReadPairs(savesDir + "/fulldex-hidden.txt");
    for (Game& g : games)
    {
        const auto it = names.find(g.rom);
        g.customName = it == names.end() ? std::string() : it->second;
    }
    games.erase(std::remove_if(games.begin(), games.end(), [&](const Game& g) { return hidden.count(g.rom) > 0; }), games.end());
}
inline int PopupAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.popW <= 0 || f.popRow <= 0 || x < f.popX || x >= f.popX + f.popW || y < f.popY) return -1;
    const int i = (y - f.popY) / f.popRow;
    return i < PopupCount() ? i : -1;
}
// A romhack: a GBA game that is not one of the official ones by its code (Ultra Violet, ...). (DS
// hacks keep their game's code: they will be told apart by the ROMs page's checks.)
inline bool IsHack(const Game& g)
{
    if (!g.known || g.comingSoon || g.platform != GBA) return false;
    const std::string c = g.code.substr(0, 3);
    return c != "AXV" && c != "AXP" && c != "BPE" && c != "BPR" && c != "BPG";
}
inline bool InTab(const std::vector<Game>& games, int i, int tab)
{
    const Game& g = games[i];
    switch (tab)
    {
    case TAB_HOME:
        for (const Game& o : games)
            if (o.favourite) return g.favourite;
        return !ContinueRom().empty() && g.rom == ContinueRom();
    case TAB_DS: return g.platform == DS;
    case TAB_GBA: return g.platform == GBA;
    case TAB_3DS: return g.platform == N3DS;
    case TAB_HACKS: return IsHack(g);
    default: return true;
    }
}

// The size the larger pictures are kept at (the tiles of the list at 720 lines).
constexpr int Art = 224;

// Games on their way, in their console's section with COMING SOON over the picture: Pokemon
// Omega Ruby on the 3DS (its picture as any game's: icons/ECRA.jpg, or SSR_PC_ART's ECRA.jpg).
inline void AddComingSoon(std::vector<Game>& games)
{
    Game g;
    g.platform = N3DS;
    g.code = "ECRA";
    g.rom = "coming-soon:ECRA"; // (never a file: so never the last game played, nor started)
    g.name = "POKEMON OMEGA RUBY";
    g.comingSoon = true;
    games.push_back(std::move(g));
}

// The games this program plays, in the order the list shows them.
struct KnownGame
{
    const char* code;
    const char* name;
    const char* save;
};
inline const KnownGame* Known(const std::string& code)
{
    static const KnownGame games[] = {
        { "IRBO", "Pokemon Black", "black.sav" },
        { "IRBF", "Pokemon Black (FR)", "black-fr.sav" },
        { "IRAO", "Pokemon White", "white.sav" },
        { "IRAF", "Pokemon White (FR)", "white-fr.sav" },
        { "IREO", "Pokemon Black 2", "black2.sav" },
        { "IREF", "Pokemon Black 2 (FR)", "black2-fr.sav" },
        { "IRDO", "Pokemon White 2", "white2.sav" },
        { "IRDF", "Pokemon White 2 (FR)", "white2-fr.sav" },
        { "IPKE", "Pokemon HeartGold", "heartgold.sav" },
        { "IPKF", "Pokemon HeartGold (FR)", "heartgold-fr.sav" },
        { "IPGE", "Pokemon SoulSilver", "soulsilver.sav" },
        { "IPGF", "Pokemon SoulSilver (FR)", "soulsilver-fr.sav" },
        { "CPUE", "Pokemon Platinum", "platinum.sav" },
        { "ADAE", "Pokemon Diamond (Experimental)", "diamond.sav" },
        { "APAE", "Pokemon Pearl (Experimental)", "pearl.sav" },
        { "IRAS", "Pokemon White (ES)", "white-es.sav" },
        { "CPUS", "Pokemon Platinum (ES)", "platinum-es.sav" },
        { "IPGS", "Pokemon SoulSilver (ES)", "soulsilver-es.sav" },
        { "IPKS", "Pokemon HeartGold (ES)", "heartgold-es.sav" },
        { "ADAS", "Pokemon Diamond (ES, Experimental)", "diamond-es.sav" },
        { "APAF", "Pokemon Pearl (FR, Experimental)", "pearl-fr.sav" },
        { "APAS", "Pokemon Pearl (ES, Experimental)", "pearl-es.sav" },
        { "APAD", "Pokemon Pearl (DE, Experimental)", "pearl-de.sav" },
        { "ADAD", "Pokemon Diamond (DE, Experimental)", "diamond-de.sav" },
        { "ADAI", "Pokemon Diamond (IT, Experimental)", "diamond-it.sav" },
        { "IPKI", "Pokemon HeartGold (IT)", "heartgold-it.sav" },
        { "CPUF", "Pokemon Platinum (FR)", "platinum-fr.sav" },
    };
    for (const KnownGame& g : games)
        if (code == g.code) return &g;
    static const std::vector<KnownGame> regional = [] {
        std::vector<KnownGame> result;
        for (const auto& p : RegionalProfiles::profiles) result.push_back({p.code,p.name,p.save});
        return result;
    }();
    for (const auto& g : regional) if (code == g.code) return &g;
    return nullptr;
}
inline int Order(const std::string& code)
{
    // (DS, then GBA: Emerald, Ultra Violet (UV22: its own code here, as FireRed's is BPRE too),
    // FireRed, LeafGreen.)
    static const char* order[] = { "IRBO", "IRAO", "IREO", "IRDO", "IPKE", "IPGE", "CPUE", "ADAE", "APAE", "BPEE", "UV22", "BPRE", "BPGE" };
    const std::string base = BaseGameCode(code);
    const int n = (int)(sizeof order / sizeof order[0]);
    for (int i = 0; i < n; i++)
        if (base == order[i]) return i;
    return n;
}

// A name as the list has it (POKEMON EMERALD) as a title: Pokemon Emerald (FireRed and LeafGreen
// as they are spelt).
inline std::string TitleCase(const std::string& name)
{
    std::string out;
    bool start = true;
    for (char c : name)
    {
        out += start ? (char)toupper((unsigned char)c) : (char)tolower((unsigned char)c);
        start = c == ' ';
    }
    for (const char* word : { "Firered", "Leafgreen" })
    {
        const size_t at = out.find(word);
        if (at != std::string::npos) out[at + 4] = (char)toupper((unsigned char)out[at + 4]);
    }
    return out;
}

inline bool EndsWithNds(const std::string& name)
{
    if (name.size() < 4) return false;
    std::string ext = name.substr(name.size() - 4);
    for (char& c : ext) c = (char)tolower((unsigned char)c);
    return ext == ".nds";
}

// Reads a ROM's game code, title and icon. False if it is not a DS ROM.
inline bool ReadGame(const std::string& rom, const std::string& folder, Game& g)
{
    FILE* f = fopen(rom.c_str(), "rb");
    if (!f) return false;
    struct stat info;
    if (fstat(fileno(f), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0x200) { fclose(f); return false; }
    uint8_t header[0x70];
    bool ok = fread(header, 1, sizeof header, f) == sizeof header;
    g = Game();
    g.rom = rom;
    g.folder = folder;
    if (ok)
    {
        g.code.assign((const char*)header + 0x0C, 4);
        for (char c : g.code)
            if (c < 0x20 || c > 0x7E) ok = false;
    }
    // The banner: version, CRCs, then the icon's 4-bit pixels (4x4 tiles of 8x8), its 16
    // colours (BGR555, the first transparent) and the title in six languages (UTF-16).
    const uint32_t banner = ok ? header[0x68] | header[0x69] << 8 | header[0x6A] << 16 | (uint32_t)header[0x6B] << 24 : 0;
    uint8_t ban[0x440];
    if (ok && banner >= 0x200 && (uint64_t)banner + sizeof ban <= (uint64_t)info.st_size && fseek(f, (long)banner, SEEK_SET) == 0 && fread(ban, 1, sizeof ban, f) == sizeof ban)
    {
        uint32_t pal[16];
        for (int i = 0; i < 16; i++)
        {
            const uint16_t c = (uint16_t)(ban[0x220 + 2 * i] | ban[0x221 + 2 * i] << 8);
            const uint32_t r = (c & 31) * 255 / 31, gr = (c >> 5 & 31) * 255 / 31, b = (c >> 10 & 31) * 255 / 31;
            pal[i] = (i ? 0xFF000000u : 0u) | r << 16 | gr << 8 | b;
        }
        for (int ty = 0; ty < 4; ty++)
            for (int tx = 0; tx < 4; tx++)
                for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x++)
                    {
                        const uint8_t byte = ban[0x20 + (ty * 4 + tx) * 32 + y * 4 + x / 2];
                        g.icon[(ty * 8 + y) * 32 + tx * 8 + x] = pal[x & 1 ? byte >> 4 : byte & 15];
                    }
        g.hasIcon = true;
        // The English title's first line, for a game this program does not know.
        for (int i = 0; i < 64; i++)
        {
            const uint16_t ch = (uint16_t)(ban[0x340 + 2 * i] | ban[0x341 + 2 * i] << 8);
            if (ch == 0 || ch == '\n') break;
            g.name += ch == 0xE9 ? 'E' : ch < 0x80 ? (char)ch : ' ';
        }
    }
    fclose(f);
    if (!ok) return false;
    if (const KnownGame* k = Known(g.code))
    {
        g.known = true;
        g.name = k->name;
        g.saveName = k->save;
    }
    else
    {
        if (g.name.empty()) g.name = rom.substr(rom.find_last_of('/') + 1);
        g.saveName = "game.sav";
    }
    for (char& c : g.name) c = (char)toupper((unsigned char)c);
    struct stat st;
    g.hasSave = stat((folder + "/" + g.saveName).c_str(), &st) == 0;
    return true;
}

// A JPEG or PNG picture, scaled to Art x Art (each pixel the average of the ones it covers).
inline bool ImageAllowed(const unsigned char* data, size_t size)
{
    int w = 0, h = 0, n = 0;
    return data && size > 0 && size <= (16u << 20) && size <= INT_MAX &&
        stbi_info_from_memory(data, (int)size, &w, &h, &n) && w > 0 && h > 0 &&
        w <= 8192 && h <= 8192 && (uint64_t)w * h <= 16000000;
}

inline bool LoadArt(const unsigned char* data, size_t size, std::vector<uint32_t>& out)
{
    if (!ImageAllowed(data, size)) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return false;
    out.assign((size_t)Art * Art, 0);
    for (int y = 0; y < Art; y++)
        for (int x = 0; x < Art; x++)
        {
            const int x0 = x * w / Art, x1 = std::max(x0 + 1, (x + 1) * w / Art);
            const int y0 = y * h / Art, y1 = std::max(y0 + 1, (y + 1) * h / Art);
            uint32_t sum[4] = {}, count = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const unsigned char* p = px + ((size_t)yy * w + xx) * 4;
                    for (int c = 0; c < 4; c++) sum[c] += p[c];
                    count++;
                }
            out[(size_t)y * Art + x] = (sum[3] / count) << 24 | (sum[0] / count) << 16 | (sum[1] / count) << 8 | sum[2] / count;
        }
    stbi_image_free(px);
    return true;
}

inline bool ReadFile(const std::string& path, std::vector<unsigned char>& data, long offset = 0, long size = -1)
{
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    if (size < 0)
    {
        fseek(f, 0, SEEK_END);
        size = ftell(f) - offset;
    }
    bool ok = size > 0 && size < (16 << 20) && fseek(f, offset, SEEK_SET) == 0;
    if (ok)
    {
        data.resize((size_t)size);
        ok = fread(data.data(), 1, data.size(), f) == data.size();
    }
    fclose(f);
    return ok;
}

// The icon a Switch homebrew program carries (its Homebrew Menu icon, a JPEG), from a .nro file.
inline bool NroIcon(const std::string& nro, std::vector<unsigned char>& jpeg)
{
    std::vector<unsigned char> head;
    if (!ReadFile(nro, head, 0, 0x20) || memcmp(head.data() + 0x10, "NRO0", 4) != 0) return false;
    const long size = head[0x18] | head[0x19] << 8 | head[0x1A] << 16 | (long)head[0x1B] << 24;
    std::vector<unsigned char> aset;
    if (!ReadFile(nro, aset, size, 0x38) || memcmp(aset.data(), "ASET", 4) != 0) return false;
    uint64_t off = 0, len = 0;
    for (int i = 7; i >= 0; i--)
    {
        off = off << 8 | aset[0x08 + i];
        len = len << 8 | aset[0x10 + i];
    }
    return len > 0 && ReadFile(nro, jpeg, size + (long)off, (long)len);
}

// The game's larger picture, from the first of: <list folder>/icons/<game code>.jpg or .png (e.g.
// icons/IREO.jpg), a picture with the ROM's name (Black 2.nds: Black 2.jpg or .png), icon.jpg in
// the game's folder, or the Homebrew Menu icon of a one-game program (.nro) in its folder (not in
// the list's own folder: that one is the all-games program, whose icon is no game's). The
// downloads hold none: these are the player's own.
// Square ARGB pixels (side n) at side m: smooth both ways (bilinear when larger, the average of
// the pixels covered when smaller), with the colours weighted by their opacity.
inline std::vector<uint32_t> ResizeArt(const std::vector<uint32_t>& src, int n, int m)
{
    std::vector<uint32_t> out((size_t)m * m);
    auto at = [&](int x, int y) { return src[(size_t)std::min(n - 1, std::max(0, y)) * n + std::min(n - 1, std::max(0, x))]; };
    for (int y = 0; y < m; y++)
        for (int x = 0; x < m; x++)
        {
            double acc[4] = {}, wsum = 0;
            auto add = [&](uint32_t p, double w) {
                const double al = (p >> 24) / 255.0;
                acc[0] += w * al * ((p >> 16) & 255);
                acc[1] += w * al * ((p >> 8) & 255);
                acc[2] += w * al * (p & 255);
                acc[3] += w * (p >> 24);
                wsum += w;
            };
            if (m >= n)
            {
                const double fx = (x + 0.5) * n / m - 0.5, fy = (y + 0.5) * n / m - 0.5;
                const int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
                const double tx = fx - x0, ty = fy - y0;
                add(at(x0, y0), (1 - tx) * (1 - ty));
                add(at(x0 + 1, y0), tx * (1 - ty));
                add(at(x0, y0 + 1), (1 - tx) * ty);
                add(at(x0 + 1, y0 + 1), tx * ty);
            }
            else
            {
                const int sx0 = x * n / m, sx1 = std::max(sx0 + 1, (x + 1) * n / m);
                const int sy0 = y * n / m, sy1 = std::max(sy0 + 1, (y + 1) * n / m);
                for (int sy = sy0; sy < sy1; sy++)
                    for (int sx = sx0; sx < sx1; sx++) add(at(sx, sy), 1);
            }
            const double alpha = acc[3] / wsum;
            const double k = alpha > 0 ? 255.0 / (alpha * wsum) : 0;
            auto c = [&](double v) { return (uint32_t)std::min(255.0, std::max(0.0, v * k + 0.5)); };
            out[(size_t)y * m + x] = ((uint32_t)std::min(255.0, alpha + 0.5) << 24) | (c(acc[0]) << 16) | (c(acc[1]) << 8) | c(acc[2]);
        }
    return out;
}

// The game's picture at side m, for an icon outside the list (desktop shortcuts, HOME Menu
// entries): its box art or picture, else its DS icon (sharp, as a pixel picture). Empty when it
// has neither.
inline std::vector<uint32_t> GamePicture(const Game& g, int m)
{
    if (!g.art.empty()) return ResizeArt(g.art, Art, m);
    if (!g.hasIcon) return {};
    std::vector<uint32_t> out((size_t)m * m);
    for (int y = 0; y < m; y++)
        for (int x = 0; x < m; x++) out[(size_t)y * m + x] = g.icon[(y * 32 / m) * 32 + x * 32 / m];
    return out;
}

inline void FindArt(Game& g, const std::string& listFolder)
{
    std::vector<unsigned char> data;
    const std::string base = g.rom.substr(0, g.rom.size() - 4);
    for (const std::string& path : { listFolder + "/icons/" + g.code + ".jpg", listFolder + "/icons/" + g.code + ".png",
                                     base + ".jpg", base + ".png", g.folder + "/icon.jpg" })
        if (ReadFile(path, data) && LoadArt(data.data(), data.size(), g.art)) return;
    if (g.folder == listFolder) return;
    if (DIR* d = opendir(g.folder.c_str()))
    {
        std::vector<std::string> nros;
        while (dirent* e = readdir(d))
        {
            const std::string n = e->d_name;
            if (n.size() > 4 && n.compare(n.size() - 4, 4, ".nro") == 0) nros.push_back(g.folder + "/" + n);
        }
        closedir(d);
        std::sort(nros.begin(), nros.end());
        for (const std::string& nro : nros)
            if (NroIcon(nro, data) && LoadArt(data.data(), data.size(), g.art)) return;
    }
}

// The ROMs in a folder (and in its roms/ folder, with subfolder); their saves go in the folder
// itself.
inline void ScanFolder(const std::string& folder, std::vector<Game>& out, bool subfolder = true)
{
    for (const std::string& dir : { folder, subfolder ? folder + "/roms" : std::string() })
    {
        if (dir.empty()) continue;
        DIR* d = opendir(dir.c_str());
        if (!d) continue;
        std::vector<std::string> names;
        while (dirent* e = readdir(d))
            if (EndsWithNds(e->d_name)) names.push_back(e->d_name);
        closedir(d);
        std::sort(names.begin(), names.end());
        for (const std::string& n : names)
        {
            Game g;
            if (ReadGame(dir + "/" + n, folder, g)) out.push_back(g);
        }
    }
}

// By console (the list's sections), then known games first, in the list's order, then the rest
// by name.
inline void Sort(std::vector<Game>& games)
{
    std::stable_sort(games.begin(), games.end(), [](const Game& a, const Game& b) {
        if (a.platform != b.platform) return a.platform < b.platform;
        const int oa = Order(a.code), ob = Order(b.code);
        if (oa != ob) return oa < ob;
        return a.name < b.name;
    });
}

// The list's colours (ARGB): near-black with white text and red details, on the Switch and on a PC.
struct Theme
{
    uint32_t bg = 0xFF0B0B0C, band = 0xFF141416, bandLine = 0xFFE0242C, title = 0xFFFFFFFF;
    uint32_t tile = 0xFF1A1A1D, frame = 0xFFE0242C;
    uint32_t name = 0xFFB8B8BC, nameSel = 0xFFFFFFFF, sub = 0xFF707076, subSel = 0xFFE0242C, bad = 0xFFE0242C;
    uint32_t hint = 0xFF8A8A90;
    int bandLineH = 1;       // the band's line, in font pixels
    int frameW = 2;          // the chosen game's frame, in font pixels
    uint32_t selBar = 0;     // a bar under the chosen game's name (0: none)
    bool pokemon = false;
    struct Background* background = nullptr; // a picture behind the list (BACKGROUND: YOUR PICTURE)
};

// The player's own picture behind the list (background.jpg or .png beside the saves folder; on
// the Switch, with the saves): kept as read, and once fitted to the canvas, darkened so the
// names stay readable.
struct Background
{
    std::vector<uint32_t> px;
    int w = 0, h = 0;
    std::vector<uint32_t> fit;
    int fw = 0, fh = 0;
    // The picture covering a w x h canvas (cut at the sides or top and bottom), or nullptr.
    const uint32_t* Fitted(int cw, int ch)
    {
        if (px.empty() || cw <= 0 || ch <= 0) return nullptr;
        if (fw == cw && fh == ch) return fit.data();
        fit.assign((size_t)cw * ch, 0);
        const double sc = std::max((double)cw / w, (double)ch / h);
        const int ox = (int)((w * sc - cw) / 2), oy = (int)((h * sc - ch) / 2);
        for (int y = 0; y < ch; y++)
        {
            const int sy = std::min(h - 1, (int)((y + oy) / sc));
            for (int x = 0; x < cw; x++)
            {
                const uint32_t p = px[(size_t)sy * w + std::min(w - 1, (int)((x + ox) / sc))];
                // (A third of its brightness: the grey names stayed hard to read on a bright one
                // at two fifths.)
                const uint32_t r = (p >> 16 & 0xFF) / 3, g = (p >> 8 & 0xFF) / 3, b = (p & 0xFF) / 3;
                fit[(size_t)y * cw + x] = 0xFF000000u | r << 16 | g << 8 | b;
            }
        }
        fw = cw;
        fh = ch;
        return fit.data();
    }
};

// The accent colour of the list (COLOUR on the settings page): the band's line, the chosen
// game's frame and its save line.
inline Theme ThemeNamed(const std::string& colour)
{
    Theme t;
    t.bandLine = t.frame = t.subSel = AccentNamed(colour); // (ui.h; red when not set)
    return t;
}

// A setting's value in a settings file ("" when it has none).
inline std::string ConfigValue(const std::string& cfg, const char* key)
{
    std::string out;
    if (FILE* f = fopen(cfg.c_str(), "r"))
    {
        char buf[512];
        const size_t n = strlen(key);
        while (fgets(buf, sizeof buf, f))
            if (!strncmp(buf, key, n) && buf[n] == '=')
            {
                out = buf + n + 1;
                while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
            }
        fclose(f);
    }
    return out;
}

// The GBA games (Pokemon GBA Widescreen, built in) take the DS games' settings where both have
// one: written into its gba-settings.ini in `dataDir` as a GBA game starts, the others there (its
// WIDESCREEN shape, FAST FORWARD SPEED) left as they are. SMOOTH SCALING ON is its SMOOTH; OFF
// keeps its SHARP or WHOLE PIXELS, whichever was chosen there (SHARP to start: it fills the screen;
// WHOLE PIXELS, the crispest, leaves a border, which its WIDESCREEN 18:9 keeps to the top and
// bottom at 720p). FPS COUNTER, VOLUME and, but on the Switch, CONTROLLER BUTTONS as they are.
inline void SyncGbaSettings(const std::string& cfg, const std::string& dataDir)
{
    std::vector<std::pair<std::string, std::string>> set;
    const std::string ini = dataDir + "/gba-settings.ini";
    const std::string had = ConfigValue(ini, "scaling");
    set.push_back({ "scaling", ConfigValue(cfg, "smooth") == "1" ? "smooth" : had == "whole" ? "whole" : "sharp" });
    set.push_back({ "show_fps", ConfigValue(cfg, "show_fps") == "1" ? "1" : "0" });
    const std::string vol = ConfigValue(cfg, "volume");
    set.push_back({ "volume", vol.empty() ? "100" : vol });
#ifndef __SWITCH__
    set.push_back({ "buttons", ConfigValue(cfg, "buttons") == "positions" ? "positions" : "labels" });
#endif
    std::vector<std::string> lines;
    if (FILE* f = fopen(ini.c_str(), "r"))
    {
        char buf[512];
        while (fgets(buf, sizeof buf, f))
        {
            std::string l = buf;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            bool ours = false;
            for (const auto& kv : set) ours |= !l.compare(0, kv.first.size() + 1, kv.first + "=");
            if (!ours && !l.empty()) lines.push_back(l);
        }
        fclose(f);
    }
    for (const auto& kv : set) lines.push_back(kv.first + "=" + kv.second);
    if (FILE* f = fopen(ini.c_str(), "w"))
    {
        for (const std::string& l : lines) fprintf(f, "%s\n", l.c_str());
        fclose(f);
    }
}

// The list's look from the settings file (COLOUR, BACKGROUND): the picture read from `folder`
// (background.jpg or .png), at most 2048 pixels on a side (larger ones are read at a fraction).
inline void LoadLook(const std::string& cfg, const std::string& folder, Theme& t, Background& b)
{
    const std::string language = ConfigValue(cfg, "list_lang");
    UiLang::Set(language.empty() ? ConfigValue(cfg, "language") : language);
    t = ThemeNamed(ConfigValue(cfg, "list_colour"));
    b = Background();
    t.background = nullptr;
    if (ConfigValue(cfg, "list_background") != "picture") return;
    std::vector<unsigned char> data;
    for (const char* name : { "/background.jpg", "/background.png", "/background.jpeg" })
    {
        if (!ReadFile(folder + name, data)) continue;
        if (!ImageAllowed(data.data(), data.size())) continue;
        int w = 0, h = 0, n = 0;
        unsigned char* px = stbi_load_from_memory(data.data(), (int)data.size(), &w, &h, &n, 4);
        if (!px) continue;
        const int step = std::max(1, (std::max(w, h) + 2047) / 2048);
        b.w = std::max(1, w / step);
        b.h = std::max(1, h / step);
        b.px.resize((size_t)b.w * b.h);
        for (int y = 0; y < b.h; y++)
            for (int x = 0; x < b.w; x++)
            {
                const unsigned char* p = px + ((size_t)y * step * w + (size_t)x * step) * 4;
                b.px[(size_t)y * b.w + x] = 0xFF000000u | p[0] << 16 | p[1] << 8 | p[2];
            }
        stbi_image_free(px);
        t.background = &b;
        return;
    }
}

// Where the list's parts go on a canvas: the title band, the tabs under it, then the games'
// tiles (each the game's picture with its name below), four across at most, in a part of their
// own that scrolls: the content, with y from its top.
struct Grid
{
    int s, gap, labelH, tabsY, tabsH, headH, top, bottom, tile, cols, gx;
    int panelX = 0, panelW = 0;
    int rowH() const { return tile + labelH + gap; }
    // A card's size and the steps between cards (the classic list: a tile with its name below).
    int cw = 0, ch = 0, colStep = 0, rowStep = 0;
};
// The glass launcher (DrawGlass) on landscape screens with the clean font; the classic list otherwise.
inline bool& GlassOn() { static bool on = true; return on; }
inline bool Glassy(const Canvas& c) { return GlassOn() && c.w > c.h && Menu::UseClean(); }
inline int GlassTabAt(const Canvas& c, int x, int y);
inline Grid Layout(const Canvas& c)
{
    Grid g;
    g.s = std::max(1, std::min(c.w, c.h) / 240); // the font's scale: 3 at 720 lines
    if (Glassy(c))
    {
        const float u = c.h / 720.0f;
        g.gap = (int)(14 * u); g.labelH = 0; g.headH = 0;
        g.tabsY = (int)(28 * u); g.tabsH = (int)(44 * u);
        g.panelX = (int)(204 * u); g.panelW = c.w - g.panelX - (int)(28 * u);
        g.top = (int)(320 * u); g.bottom = c.h - (int)(82 * u);
        // (Box art is square: square pictures, two rows of cards in view, as many across as fit,
        // the room left over shared out between them.)
        const int minGap = (int)(14 * u);
        g.ch = (g.bottom - g.top - minGap) / 2;
        g.tile = g.ch - (int)(38 * u); // the picture
        g.cw = g.tile + (int)(16 * u);
        g.cols = std::max(2, std::min(8, (g.panelW + minGap) / std::max(1, g.cw + minGap)));
        g.gap = g.cols > 1 ? (g.panelW - g.cols * g.cw) / (g.cols - 1) : minGap;
        g.gx = g.panelX; g.colStep = g.cw + g.gap; g.rowStep = g.ch + minGap;
        return g;
    }
    g.gap = 8 * g.s;
    g.labelH = (UiLang::Tall() ? 32 : 20) * g.s;
    g.tabsY = (c.h > c.w ? 46 : 25) * g.s;
    g.tabsH = 13 * g.s;
    g.headH = 13 * g.s;
    g.top = g.tabsY + g.tabsH + 5 * g.s;
    g.bottom = c.h - 16 * g.s;
    // Tiles at most Art x Art at 720 lines, small enough for two rows and a heading on a screen.
    g.tile = std::max(32, std::min(Art * c.h / 720, (g.bottom - g.top - g.headH) / 2 - g.labelH - g.gap));
    if (c.h > c.w) g.tile = std::max(32, (c.w - 7 * g.gap) / 2);
    g.cols = std::max(1, std::min(4, (c.w - g.gap) / (g.tile + g.gap * 3)));
    const int gridW = g.cols * g.tile + (g.cols - 1) * g.gap * 3;
    g.gx = (c.w - gridW) / 2;
    g.cw = g.tile; g.ch = g.tile + g.labelH; g.colStep = g.tile + g.gap * 3; g.rowStep = g.rowH();
    return g;
}

// What one tab shows: each game's tile and each console's heading (on ALL) or the line saying it
// has none yet, in content coordinates.
struct View
{
    struct Spot { int game, x, y, section; };
    struct Head { int platform, y; bool empty; };
    Grid g;
    int tab = 0;
    std::vector<Spot> spots;
    std::vector<Head> heads;
    int height = 0;
    // The spot of game i, or -1.
    int SpotOf(int game) const
    {
        for (size_t k = 0; k < spots.size(); k++)
            if (spots[k].game == game) return (int)k;
        return -1;
    }
};
inline View MakeView(const Canvas& c, const std::vector<Game>& games, int tab)
{
    View v;
    v.g = Layout(c);
    v.tab = tab;
    const Grid& g = v.g;
    if (Glassy(c))
    {
        // (One grid of cards, the consoles one after another, without headings.)
        std::vector<int> mine;
        for (int p = 0; p < PLATFORMS; p++)
        {
            for (size_t i = 0; i < games.size(); i++)
                if (games[i].platform == p && InTab(games, (int)i, tab)) mine.push_back((int)i);
        }
        if (mine.empty()) { v.heads.push_back({ 0, 0, true }); return v; }
        for (size_t k = 0; k < mine.size(); k++)
            v.spots.push_back({ mine[k], g.gx + (int)(k % g.cols) * g.colStep, (int)(k / g.cols) * g.rowStep, games[mine[k]].platform });
        v.height = (int)((mine.size() + g.cols - 1) / g.cols) * g.rowStep;
        return v;
    }
    const int emptyH = 16 * g.s;
    int y = 0;
    for (int p = 0; p < PLATFORMS; p++)
    {
        if (tab >= TAB_DS && tab <= TAB_3DS && p != tab - TAB_DS) continue;
        std::vector<int> mine;
        for (size_t i = 0; i < games.size(); i++)
            if (games[i].platform == p && InTab(games, (int)i, tab)) mine.push_back((int)i);
        // (HOME and ROMHACKS: only the consoles they have games of, without headings.)
        if ((tab == TAB_HOME || tab == TAB_HACKS) && mine.empty()) continue;
        // (ALL: every console under its heading, also one without games yet.)
        if (tab == TAB_ALL)
        {
            v.heads.push_back({ p, y, false });
            y += g.headH;
        }
        if (mine.empty())
        {
            v.heads.push_back({ p, y, true });
            y += emptyH + g.gap;
            continue;
        }
        for (size_t k = 0; k < mine.size(); k++)
            v.spots.push_back({ mine[k], g.gx + (int)(k % g.cols) * g.colStep, y + (int)(k / g.cols) * g.rowStep, p });
        y += (int)((mine.size() + g.cols - 1) / g.cols) * g.rowStep + g.gap;
    }
    if (v.spots.empty() && v.heads.empty()) v.heads.push_back({ 0, 0, true });
    v.height = y;
    return v;
}

// Keeps the chosen game in view (with its console's heading when it is in the first row under
// it): scroll is how far down the content the screen is. follow false: only kept within the
// content (a finger scrolling the list).
inline void FollowSelection(const View& v, int selected, int& scroll, bool follow = true)
{
    const Grid& g = v.g;
    const int viewH = g.bottom - g.top;
    const int k = follow ? v.SpotOf(selected) : -1;
    if (k >= 0)
    {
        int top = v.spots[k].y;
        for (const View::Head& h : v.heads)
            if (!h.empty && h.platform == v.spots[k].section && top - h.y <= g.headH) top = h.y;
        const int bottom = v.spots[k].y + g.ch;
        if (top < scroll) scroll = top;
        if (bottom > scroll + viewH) scroll = bottom - viewH;
    }
    scroll = std::max(0, std::min(scroll, std::max(0, v.height - viewH)));
}

// The game whose tile (with its name below it) is at x, y on the canvas, or -1.
inline int TileAt(const View& v, int scroll, int x, int y)
{
    const Grid& g = v.g;
    if (y < g.top || y >= g.bottom) return -1;
    for (const View::Spot& sp : v.spots)
    {
        const int ty = g.top + sp.y - scroll;
        if (x >= sp.x && x < sp.x + g.cw && y >= ty && y < ty + g.ch) return sp.game;
    }
    return -1;
}

// Where the tabs are drawn (TabsDraw) and which one is at x, y, or -1.
inline int TabX(const Canvas& c, const Grid& g, int t)
{
    const int step = 28 * 6 * g.s / 3; // (each tab's room, at the font's scale)
    return c.w / 2 + (t * 2 - (TABS - 1)) * step / 2;
}
inline int TabAt(const Canvas& c, int x, int y)
{
    if (Glassy(c)) return GlassTabAt(c, x, y);
    const Grid g = Layout(c);
    if (y < g.tabsY - 3 * g.s || y >= g.tabsY + g.tabsH + 3 * g.s) return -1;
    const int step = 28 * 6 * g.s / 3;
    for (int t = 0; t < TABS; t++)
        if (std::abs(x - TabX(c, g, t)) < step / 2) return t;
    return -1;
}

// The next game from `selected` in direction dir (1 left, 2 right, 3 up, 4 down): left and right
// go through the tab's games in order, up and down to the nearest tile in the row above or below
// (across the headings). -1 when there is nothing that way (up from the first row: the caller's
// title band; down from the last: stay).
inline int Step(const View& v, int selected, int dir)
{
    const int k = v.SpotOf(selected);
    if (k < 0) return v.spots.empty() ? -1 : v.spots[0].game;
    if (dir == 1) return k > 0 ? v.spots[k - 1].game : selected;
    if (dir == 2) return k + 1 < (int)v.spots.size() ? v.spots[k + 1].game : selected;
    // The row above or below: the nearest y beyond this one, then the nearest x in it.
    const int y = v.spots[k].y, x = v.spots[k].x;
    int rowY = -1;
    for (const View::Spot& sp : v.spots)
        if ((dir == 3 ? sp.y < y : sp.y > y) && (rowY < 0 || (dir == 3 ? sp.y > rowY : sp.y < rowY))) rowY = sp.y;
    if (rowY < 0) return dir == 3 ? -1 : selected;
    int best = -1, bestD = 0;
    for (const View::Spot& sp : v.spots)
        if (sp.y == rowY && (best < 0 || std::abs(sp.x - x) < bestD))
        {
            best = sp.game;
            bestD = std::abs(sp.x - x);
        }
    return best;
}

// A tab's own first game (when the one chosen is not in it), or -1 when it has none.
inline int FirstIn(const View& v) { return v.spots.empty() ? -1 : v.spots[0].game; }

// ---- The glass launcher (the new design: "Option A", the pause menu's frosted glass) ----------
// A sidebar of sections on the left, the tabs as pills along the top, a Continue Playing banner
// and the tab's games as cards below it, all drawn with the pause menu's own glass (glass.h,
// Menu::FrostBox) and clean font over the chosen game's picture, blurred. Used on landscape
// screens with the clean font (Glassy); otherwise the classic list (DrawClassic).
enum Side { SIDE_HOME, SIDE_LIBRARY, SIDE_HUB, SIDE_ROMS, SIDE_UPDATES, SIDE_SETTINGS, SIDE_ABOUT, SIDES };
inline const char* SideName(int i)
{
    static const char* n[] = { "Home", "Library", "Hub", "ROMs", "Updates", "Settings", "About" };
    return n[i];
}
inline int SideIcon(int i)
{
    static const int ic[] = { Glass::I_POKEBALL, Glass::I_GRID, Glass::I_PEOPLE, Glass::I_CHIP, Glass::I_LOAD, Glass::I_GEAR, Glass::I_INFO };
    return ic[i];
}
inline float GU(const Canvas& c) { return c.h / 720.0f; } // one design pixel (the designs are 1280 x 720)
inline int GPx(float px) { return std::max(6, (int)(px + 0.5f)); }
inline int GW(const std::string& s, float px, bool bold) { return CleanFont::Width(s, GPx(px), bold); }
// Text with its capitals centred on cy.
inline void GT(Canvas& c, int x, float cy, const std::string& s, float px, uint32_t col, bool bold)
{
    CleanFont::Draw(c, x, (int)(cy + CleanFont::CapHeight(GPx(px), bold) * 0.5f), s, GPx(px), col, bold);
}
// A game's name as the cards show it: HeartGold, Black 2, Ultra Violet.
inline std::string ShortName(const Game& g)
{
    if (!g.customName.empty()) return g.customName;
    std::string n = g.name;
    if (const KnownGame* k = Known(g.code)) n = k->name;
    else n = TitleCase(n);
    for (const char* p : { "Pokemon ", "POKEMON ", "Pokémon " })
        if (n.rfind(p, 0) == 0) { n = n.substr(strlen(p)); break; }
    return n;
}
// Where the sidebar's sections and the tab pills are (Draw and the taps use the same).
struct GlassSpots
{
    int sideX, sideY, sideW, sideH, rowH, rowGap, firstRow;
    int tabsX, tabsY, tabsH;
    std::vector<int> tabX, tabW;
    int bannerX, bannerY, bannerW, bannerH;
    int contX, contY, contW, contH; // the Continue button
    int headY;
};
inline GlassSpots Spots(const Canvas& c)
{
    const float u = GU(c);
    GlassSpots s;
    // (The sidebar flush with the screen's left, top and bottom edges; its rows start level with the tabs.)
    s.sideX = 0; s.sideY = 0; s.sideW = (int)(180 * u); s.sideH = c.h;
    // (text sizes: bigger since 6 November, the sidebar and tabs most)
    s.rowH = (int)(54 * u); s.rowGap = (int)(6 * u); s.firstRow = (int)(22 * u); // (level with the tab pills)
    s.tabsX = (int)(204 * u); s.tabsY = (int)(18 * u); s.tabsH = (int)(54 * u);
    int x = s.tabsX + (int)(4 * u);
    for (int t = 0; t < TABS; t++)
    {
        const int w = GW(TabLabel(t), 18 * u, true) + (int)(40 * u);
        s.tabX.push_back(x); s.tabW.push_back(w);
        x += w + (int)(2 * u);
    }
    s.bannerX = s.tabsX; s.bannerY = (int)(106 * u); s.bannerW = c.w - s.bannerX - (int)(28 * u); s.bannerH = (int)(160 * u);
    s.contX = s.bannerX + (int)(168 * u); s.contY = s.bannerY + (int)(100 * u); s.contW = (int)(150 * u); s.contH = (int)(44 * u);
    s.headY = (int)(282 * u);
    return s;
}
inline bool Glassy(const Canvas& c);
inline int SideAt(const Canvas& c, int x, int y)
{
    if (!Glassy(c)) return -1;
    const GlassSpots s = Spots(c);
    if (x < s.sideX || x >= s.sideX + s.sideW) return -1;
    for (int i = 0; i < SIDES; i++)
    {
        const int ry = s.firstRow + i * (s.rowH + s.rowGap);
        if (y >= ry && y < ry + s.rowH) return i;
    }
    return -1;
}
inline bool ContinueAt(const Canvas& c, int x, int y)
{
    if (!Glassy(c)) return false;
    const GlassSpots s = Spots(c);
    return x >= s.contX && x < s.contX + s.contW && y >= s.contY && y < s.contY + s.contH;
}
// The game the banner offers: the one played last, if it is still here, else the chosen one.
inline int ContinueGame(const std::vector<Game>& games, int selected)
{
    for (size_t i = 0; i < games.size(); i++)
        if (!ContinueRom().empty() && games[i].rom == ContinueRom()) return (int)i;
    return selected;
}
// A game's picture (or its DS icon on glass) filling a rounded box, cropped to fill it.
inline void DrawCover(Canvas& c, const Game& g, int x, int y, int w, int h, float r)
{
    if (w <= 0 || h <= 0) return;
    Glass::Mark(c, x, y, x + w, y + h);
    const int ri = (int)r;
    auto inside = [&](int px, int py) {
        const int dx = std::max(0, std::max(ri - px, px - (w - 1 - ri))), dy = std::max(0, std::max(ri - py, py - (h - 1 - ri)));
        return dx * dx + dy * dy <= ri * ri;
    };
    if (!g.art.empty())
    {
        const float sc = std::max(w, h) / (float)Art;
        for (int py = 0; py < h; py++)
        {
            const int Y = y + py;
            if (Y < 0 || Y >= c.h) continue;
            const int sy = std::max(0, std::min(Art - 1, (int)((py - h * 0.5f) / sc + Art * 0.5f)));
            for (int px = 0; px < w; px++)
            {
                const int X = x + px;
                if (X < 0 || X >= c.w || !inside(px, py)) continue;
                const int sx = std::max(0, std::min(Art - 1, (int)((px - w * 0.5f) / sc + Art * 0.5f)));
                const uint32_t p = g.art[(size_t)sy * Art + sx];
                uint32_t& d = c.px[(size_t)Y * c.w + X];
                d = Glass::Over(d, p | 0xFF000000u, (int)(p >> 24));
            }
        }
        return;
    }
    Glass::Round(c, x, y, w, h, r, 0x26FFFFFF);
    if (g.hasIcon)
    {
        const int is = std::max(1, std::min(w, h) / 40), ox = x + (w - 32 * is) / 2, oy = y + (h - 32 * is) / 2;
        for (int py = 0; py < 32; py++)
            for (int px = 0; px < 32; px++)
                if (g.icon[py * 32 + px] >> 24) c.Fill(ox + px * is, oy + py * is, is, is, g.icon[py * 32 + px]);
    }
}
// The background: the chosen game's picture, blurred as the pause menu blurs the paused game
// (made again only when the game or the window changes); a calm blue without one.
inline void GlassBackdrop(Canvas& c, const Game* g)
{
    static std::string key;
    static int kw = 0, kh = 0;
    const std::string k = g && !g->art.empty() ? g->rom : std::string();
    if (k == key && kw == c.w && kh == c.h && c.backdrop.size() == (size_t)c.w * c.h) return;
    key = k; kw = c.w; kh = c.h;
    // (Made small, as MakeBackdrop blurs at a fraction of the size anyway, then drawn up by it.)
    std::vector<uint32_t> src((size_t)c.w * c.h);
    if (!k.empty())
    {
        const float sc = std::max(c.w, c.h) / (float)Art;
        for (int y = 0; y < c.h; y++)
        {
            const int sy = std::max(0, std::min(Art - 1, (int)((y - c.h * 0.5f) / sc + Art * 0.5f)));
            for (int x = 0; x < c.w; x++)
            {
                const int sx = std::max(0, std::min(Art - 1, (int)((x - c.w * 0.5f) / sc + Art * 0.5f)));
                const uint32_t p = g->art[(size_t)sy * Art + sx];
                src[(size_t)y * c.w + x] = Glass::Over(0xFF1C2A3Au, p | 0xFF000000u, (int)(p >> 24));
            }
        }
    }
    else
        for (int y = 0; y < c.h; y++)
            for (int x = 0; x < c.w; x++)
            {
                const float t = (float)x / c.w * 0.6f + (float)y / c.h * 0.4f;
                const int r = (int)(0x1E + t * 0x18), gg = (int)(0x36 + t * 0x30), b = (int)(0x5A + t * 0x10);
                src[(size_t)y * c.w + x] = 0xFF000000u | (r << 16) | (gg << 8) | b;
            }
    Glass::MakeBackdrop(c.backdrop, src.data(), c.w, c.h);
}

inline void DrawGlass(Canvas& c, const std::vector<Game>& games, const View& v, int tab, int selected, int& scroll,
                      const std::string& note, const std::string& hintText, bool follow, bool onTabs, bool settingsChosen)
{
    const float u = GU(c);
    const Grid& grid = v.g;
    const GlassSpots sp = Spots(c);
    const int cont = ContinueGame(games, selected);
    const Game* shown = selected >= 0 && selected < (int)games.size() ? &games[selected]
                      : cont >= 0 && cont < (int)games.size() ? &games[cont] : nullptr;
    GlassBackdrop(c, shown);
    const bool solid = Glass::Backdrop(c);
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint, dark = 0xFF18222Eu;

    // The sidebar: the sections, Home chosen (Settings while it is chosen with the arrows).
    // (Frosted, square, with a soft shadow and a faint line where it meets the rest of the screen.)
    Glass::Shadow(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, 18 * u, 0x60);
    if (solid) Glass::Frost(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0);
    else Glass::Round(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, Menu::GlassShade);
    Glass::Round(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, 0x30101826);
    Glass::Round(c, sp.sideX + sp.sideW - std::max(1, (int)u), sp.sideY, std::max(1, (int)u), sp.sideH, 0, 0x3DFFFFFF);
    for (int i = 0; i < SIDES; i++)
    {
        const int ry = sp.firstRow + i * (sp.rowH + sp.rowGap), rx = sp.sideX + (int)(16 * u), rw = sp.sideW - (int)(32 * u);
        const int fs = Focus().side;
        const bool on = fs >= 0 ? i == fs : settingsChosen ? i == SIDE_SETTINGS : i == SIDE_HOME;
        const bool lit = fs >= 0 ? i == fs : settingsChosen && i == SIDE_SETTINGS; // (chosen with the arrows)
        if (on)
        {
            if (lit) Glass::Glow(c, rx, ry, rw, sp.rowH, 12 * u, u);
            Glass::Round(c, rx, ry, rw, sp.rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
        }
        Glass::DrawIcon(c, SideIcon(i), rx + 27 * u, ry + sp.rowH * 0.5f, 12 * u, text);
        GT(c, rx + (int)(52 * u), ry + sp.rowH * 0.5f, SideName(i), 19 * u, text, true);
    }

    // The tabs, as pills: the shown one white.
    {
        // (A bar along the top, flush with the screen's top and right edges, meeting the sidebar.)
        const int barX = sp.sideX + sp.sideW, barW = c.w - barX, barH = sp.tabsY * 2 + sp.tabsH, line = std::max(1, (int)u);
        if (solid) Glass::Frost(c, barX, 0, barW, barH, 0);
        else Glass::Round(c, barX, 0, barW, barH, 0, Menu::GlassShade);
        Glass::Round(c, barX, 0, barW, barH, 0, 0x30101826);
        Glass::Round(c, barX, barH - line, barW, line, 0, 0x3DFFFFFF);
        for (int t = 0; t < TABS; t++)
        {
            const int px = sp.tabX[t], py = sp.tabsY + (int)(4 * u), pw = sp.tabW[t], ph = sp.tabsH - (int)(8 * u);
            const std::string name = TabLabel(t);
            if (t == tab)
            {
                if (onTabs) Glass::Glow(c, px, py, pw, ph, ph * 0.5f, u);
                Glass::Round(c, px, py, pw, ph, ph * 0.5f, 0xFFFFFFFFu);
            }
            GT(c, px + (pw - GW(name, 18 * u, true)) / 2, py + ph * 0.5f, name, 18 * u, t == tab ? dark : text, true);
        }
    }

    // The banner: the game played last, ready to carry on.
    if (cont >= 0 && cont < (int)games.size())
    {
        const Game& g = games[cont];
        Menu::FrostBox(c, solid, sp.bannerX, sp.bannerY, sp.bannerW, sp.bannerH, 18 * u);
        const int ax = sp.bannerX + (int)(16 * u), ay = sp.bannerY + (int)(16 * u), ah = sp.bannerH - (int)(32 * u), aw = ah;
        DrawCover(c, g, ax, ay, aw, ah, 12 * u);
        const int tx = ax + aw + (int)(24 * u);
        const bool played = !ContinueRom().empty() && g.rom == ContinueRom();
        const std::string meta = std::string(played ? "Continue Playing" : "Ready To Play") + "   \xC2\xB7   " + PlatformName(g.platform)
                               + "   \xC2\xB7   " + (g.hasSave ? "Save Found" : "New Game");
        GT(c, tx, sp.bannerY + 30 * u, meta, 16 * u, hint, true);
        GT(c, tx, sp.bannerY + 66 * u, ShortName(g), 36 * u, text, true);
        // Continue (white) and Game Options.
        if (Focus().banner == 1) Glass::Glow(c, sp.contX, sp.contY, sp.contW, sp.contH, 12 * u, u);
        Glass::Round(c, sp.contX, sp.contY, sp.contW, sp.contH, 12 * u, 0xFFFFFFFFu);
        Glass::DrawIcon(c, Glass::I_PLAY, sp.contX + 30 * u, sp.contY + sp.contH * 0.5f, 8 * u, dark);
        GT(c, sp.contX + (int)(48 * u), sp.contY + sp.contH * 0.5f, played ? "Continue" : "Play", 18 * u, dark, true);
        const int ox = sp.contX + sp.contW + (int)(10 * u), ow = GW("Game Options", 18 * u, true) + (int)(36 * u);
        if (Focus().banner == 2)
        {
            Glass::Glow(c, ox, sp.contY, ow, sp.contH, 12 * u, u);
            Glass::Round(c, ox, sp.contY, ow, sp.contH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
        }
        else Glass::Round(c, ox, sp.contY, ow, sp.contH, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
        GT(c, ox + (int)(18 * u), sp.contY + sp.contH * 0.5f, "Game Options", 18 * u, text, true);
    }

    // "Your Games" and how many.
    GT(c, sp.tabsX, sp.headY + 14 * u, "Your Games", 24 * u, text, true);
    {
        const std::string n = std::to_string(v.spots.size()) + (v.spots.size() == 1 ? " Game" : " Games");
        GT(c, sp.bannerX + sp.bannerW - GW(n, 16 * u, true), sp.headY + 14 * u, n, 16 * u, hint, true);
    }

    // The cards.
    FollowSelection(v, selected, scroll, follow);
    for (const View::Head& h : v.heads)
        if (h.empty)
        {
            const std::string none = tab == TAB_HOME ? "Add Games To Home With X In The Other Tabs"
                                   : tab == TAB_HACKS ? "No Romhacks Yet" : "No " + std::string(TabLabel(tab)) + " Games Yet";
            GT(c, sp.tabsX + (grid.cols * grid.colStep - GW(none, 16 * u, true)) / 2, (grid.top + grid.bottom) * 0.5f, none, 16 * u, hint, true);
        }
    for (const View::Spot& s : v.spots)
    {
        const int x = s.x, y = grid.top + s.y - scroll;
        if (y < grid.top - 2 || y + grid.ch > grid.bottom + 2) continue;
        const Game& g = games[s.game];
        const bool sel = s.game == selected;
        if (sel) Glass::Glow(c, x, y, grid.cw, grid.ch, 16 * u, u);
        Menu::FrostBox(c, solid, x, y, grid.cw, grid.ch, 16 * u, sel, sel ? 1.5f * u : 1.0f);
        const int ix = x + (int)(8 * u), iy = y + (int)(8 * u), iw = grid.tile, ih = grid.tile;
        DrawCover(c, g, ix, iy, iw, ih, 10 * u);
        if (g.comingSoon)
        {
            Glass::Round(c, ix, iy, iw, ih, 10 * u, 0x99000000u);
            GT(c, ix + (iw - GW("Coming Soon", 14 * u, true)) / 2, iy + ih * 0.5f, "Coming Soon", 14 * u, text, true);
        }
        const float cy = iy + ih + 16 * u;
        std::string name = ShortName(g);
        float npx = 17 * u;
        const int room = iw - GW(PlatformName(g.platform), 13 * u, true) - (int)(14 * u);
        while (npx > 10 * u && GW(name, npx, true) > room) npx -= u;
        GT(c, ix + (int)(4 * u), cy, name, npx, text, true);
        const std::string sub = !g.known && !g.comingSoon ? "Not Playable" : PlatformName(g.platform);
        GT(c, ix + iw - (int)(4 * u) - GW(sub, 13 * u, true), cy, sub, 13 * u, !g.known && !g.comingSoon ? 0xFFF2A08Au : hint, true);
        if (g.favourite) Glass::DrawIcon(c, Glass::I_SPARKLE, ix + iw - 14 * u, iy + 14 * u, 7 * u, 0xFFFFFFFFu);
    }
    // More below.
    if (scroll + (grid.bottom - grid.top) < v.height - grid.gap)
        Glass::Round(c, sp.tabsX + grid.cols * grid.colStep / 2 - (int)(16 * u), grid.bottom + (int)(4 * u), (int)(32 * u), (int)(4 * u), 2 * u, 0x80FFFFFFu);

    // The game's menu (A on a game), beside its card: a glass panel of rows as the sidebar's.
    Focus().popW = 0;
    if (Focus().popup >= 0 && selected >= 0 && selected < (int)games.size())
    {
        const int k = v.SpotOf(selected);
        if (k >= 0)
        {
            static const int icons[] = { Glass::I_PLAY, Glass::I_POKEBALL, Glass::I_SPARKLE, Glass::I_WINDOW, Glass::I_SAVE, Glass::I_BACK };
            const Game& g = games[selected];
            const int cx = v.spots[k].x, cyy = grid.top + v.spots[k].y - scroll;
            const int n = PopupCount(), rowH = (int)(44 * u), rg = (int)(4 * u), pad = (int)(8 * u), pw = (int)(250 * u);
            const int ph = n * rowH + (n - 1) * rg + 2 * pad;
            int px = cx + grid.cw + (int)(12 * u);
            if (px + pw > c.w - (int)(28 * u)) px = cx - (int)(12 * u) - pw;
            const int py = std::max((int)(28 * u), std::min(cyy, c.h - (int)(82 * u) - ph));
            Focus().popX = px + pad; Focus().popY = py + pad; Focus().popW = pw - 2 * pad; Focus().popRow = rowH + rg;
            // (The rest of the screen dimmed, and the blurred background put back under the menu so
            // its glass frosts that, not the cards behind it.)
            Glass::Round(c, 0, 0, c.w, c.h, 0, 0x59000000u);
            if (solid)
            {
                const int r = (int)(18 * u);
                for (int yy = std::max(0, py); yy < std::min(c.h, py + ph); yy++)
                    for (int xx = std::max(0, px); xx < std::min(c.w, px + pw); xx++)
                    {
                        const int ex = std::max(0, std::max(px + r - xx, xx - (px + pw - 1 - r))), ey = std::max(0, std::max(py + r - yy, yy - (py + ph - 1 - r)));
                        if (ex * ex + ey * ey <= r * r) c.px[(size_t)yy * c.w + xx] = c.backdrop[(size_t)yy * c.w + xx];
                    }
            }
            Menu::FrostBox(c, solid, px, py, pw, ph, 18 * u);
            for (int i = 0; i < n; i++)
            {
                const int ry = py + pad + i * (rowH + rg), rx = px + pad, rw = pw - 2 * pad;
                const uint32_t ink = i == 5 ? 0xFFF2A08Au : text;
                if (i == Focus().popup)
                {
                    Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
                    Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
                }
                Glass::DrawIcon(c, icons[i], rx + 22 * u, ry + rowH * 0.5f, 9 * u, ink);
                GT(c, rx + (int)(44 * u), ry + rowH * 0.5f, PopupLabel(g, i), 17 * u, ink, true);
            }
        }
    }

    // A dialog from the game's menu (Remove Game, Delete ROM File, Rename) in the middle, the rest dimmed.
    Focus().dW = 0;
    if (Focus().dialog && Focus().dialog != 3 && selected >= 0 && selected < (int)games.size()) // (Rename: the keyboard page)
    {
        GlassFocus& F = Focus();
        const Game& g = games[selected];
        Game own = g;
        own.customName.clear();
        std::string title, msg;
        std::vector<std::string> rows;
        int red = -1;
        if (F.dialog == 1) { title = "Remove " + ShortName(g) + "?"; msg = "Remove From List hides it here and keeps the ROM in your folder. Your saves are never deleted."; rows = { "Remove From List", "Delete ROM File", "Cancel" }; red = 1; }
        else if (F.dialog == 2)
        {
            const size_t sl = g.rom.find_last_of("/\\");
            title = "Delete The ROM File?";
            msg = "This deletes " + (sl == std::string::npos ? g.rom : g.rom.substr(sl + 1)) + " from your folder. Your saves are kept.";
            rows = { "Delete ROM File", "Cancel" }; red = 0;
        }
        else { title = "Rename " + ShortName(own); msg = "Type a new name. Leave it empty to go back to the game's own name."; rows = { "Save", "Cancel" }; }
        const int pw = (int)(560 * u), pad = (int)(24 * u), rowH = (int)(48 * u), rg = (int)(8 * u), fieldH = (int)(52 * u);
        std::vector<std::string> lines;
        {
            std::string line, word;
            auto flush = [&]() { if (!word.empty()) { const std::string t = line.empty() ? word : line + " " + word; if (!line.empty() && GW(t, 15 * u, false) > pw - 2 * pad) { lines.push_back(line); line = word; } else line = t; word.clear(); } };
            for (char ch : msg) { if (ch == ' ') flush(); else word += ch; }
            flush();
            if (!line.empty()) lines.push_back(line);
        }
        const int ph = pad + (int)(36 * u) + (int)lines.size() * (int)(22 * u) + (int)(16 * u) + (F.dialog == 3 ? fieldH + rg : 0)
                     + (int)rows.size() * (rowH + rg) - rg + pad;
        const int px = (c.w - pw) / 2, py = (c.h - ph) / 2;
        Glass::Round(c, 0, 0, c.w, c.h, 0, 0x73000000u);
        if (solid)
        {
            const int r = (int)(20 * u);
            for (int yy = std::max(0, py); yy < std::min(c.h, py + ph); yy++)
                for (int xx = std::max(0, px); xx < std::min(c.w, px + pw); xx++)
                {
                    const int ex = std::max(0, std::max(px + r - xx, xx - (px + pw - 1 - r))), ey = std::max(0, std::max(py + r - yy, yy - (py + ph - 1 - r)));
                    if (ex * ex + ey * ey <= r * r) c.px[(size_t)yy * c.w + xx] = c.backdrop[(size_t)yy * c.w + xx];
                }
        }
        Menu::FrostBox(c, solid, px, py, pw, ph, 20 * u);
        int y = py + pad;
        GT(c, px + pad, y + 14 * u, title, 22 * u, text, true);
        y += (int)(36 * u);
        for (const std::string& l : lines) { GT(c, px + pad, y + 10 * u, l, 15 * u, hint, false); y += (int)(22 * u); }
        y += (int)(16 * u);
        if (F.dialog == 3)
        {
            Glass::Round(c, px + pad, y, pw - 2 * pad, fieldH, 12 * u, 0x24FFFFFFu, 0xF2FFFFFFu, 1.5f * u);
            GT(c, px + pad + (int)(16 * u), y + fieldH * 0.5f, F.text + "|", 18 * u, text, true);
            y += fieldH + rg;
        }
        F.dX = px + pad; F.dY = y; F.dW = pw - 2 * pad; F.dRow = rowH + rg; F.dRows = (int)rows.size();
        for (int i = 0; i < (int)rows.size(); i++)
        {
            const int ry = y + i * (rowH + rg), rx = px + pad, rw = pw - 2 * pad;
            const uint32_t ink = i == red ? 0xFFF2A08Au : text;
            if (i == F.drow)
            {
                Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            GT(c, rx + (rw - GW(rows[i], 16 * u, true)) / 2, ry + rowH * 0.5f, rows[i], 16 * u, ink, true);
        }
    }

    // The hints in the bottom right corner, lined up with the cards' right edge: each button a
    // white badge before what it does (a message, when there is one, in their place).
    {
        const int bx = sp.tabsX, bw = c.w - sp.tabsX - (int)(28 * u), bh = (int)(44 * u), by = c.h - (int)(24 * u) - bh;
        const float cy = by + bh * 0.5f;
        if (!note.empty())
        {
            const std::string n = CleanFont::Title(UiLang::Tr(note));
            const int w = (int)(26 * u) + GW(n, 16 * u, true), x0 = bx + bw - w;
            Glass::DrawIcon(c, Glass::I_INFO, x0 + 9 * u, cy, 9 * u, text);
            GT(c, x0 + (int)(26 * u), cy, n, 16 * u, text, true);
        }
        else
        {
            const std::string all = !hintText.empty() ? hintText : "A: PLAY    Y: SETTINGS    L / R: TABS    +: QUIT";
            struct Item { std::string key, act; };
            std::vector<Item> items;
            for (size_t i = 0; i < all.size();)
            {
                size_t j = all.find("    ", i);
                if (j == std::string::npos) j = all.size();
                std::string part = all.substr(i, j - i);
                while (!part.empty() && part.front() == ' ') part.erase(0, 1);
                while (!part.empty() && part.back() == ' ') part.pop_back();
                // (SETTINGS is not shown: it is in the sidebar, one press of left away. Its button still works.)
                if (!part.empty() && part.find(": SETTINGS") == std::string::npos)
                {
                    const size_t k = part.find(": ");
                    if (k == std::string::npos) items.push_back({ "", CleanFont::Title(UiLang::Tr(part)) });
                    else items.push_back({ CleanFont::Title(part.substr(0, k)), CleanFont::Title(UiLang::Tr(part.substr(k + 2))) });
                }
                i = j + 4;
            }
            auto keyW = [&](const Item& it, float f) { return std::max((int)(f * 1.7f), GW(it.key, f * 0.9f, true) + (int)(f * 1.1f)); };
            auto widthOf = [&](float f) {
                int w = 0;
                for (const Item& it : items) w += (it.key.empty() ? 0 : keyW(it, f) + (int)(f * 0.6f)) + GW(it.act, f, true) + (int)(f * 2.4f);
                return w - (int)(f * 2.4f);
            };
            float fp = 16 * u;
            while (fp > 9 * u && widthOf(fp) > bw - 40 * u) fp -= u;
            int x = bx + bw - widthOf(fp);
            for (const Item& it : items)
            {
                if (!it.key.empty())
                {
                    const int kh = (int)(fp * 1.7f), kw = keyW(it, fp);
                    Glass::Round(c, x, (int)(cy - kh * 0.5f), kw, kh, kh * 0.5f, 0xF2FFFFFFu);
                    GT(c, x + (kw - GW(it.key, fp * 0.9f, true)) / 2, cy, it.key, fp * 0.9f, dark, true);
                    x += kw + (int)(fp * 0.6f);
                }
                GT(c, x, cy, it.act, fp, text, true);
                x += GW(it.act, fp, true) + (int)(fp * 2.4f);
            }
        }
    }
    if (solid) Glass::Opaque(c);
}

inline int GlassTabAt(const Canvas& c, int x, int y)
{
    const GlassSpots s = Spots(c);
    if (y < s.tabsY || y >= s.tabsY + s.tabsH) return -1;
    for (int t = 0; t < TABS; t++)
        if (x >= s.tabX[t] && x < s.tabX[t] + s.tabW[t]) return t;
    return -1;
}

// The classic list, drawn over the whole canvas (1280x720 on the Switch): the title band, the tabs, then
// the tab's games as tiles, each the game's picture (or its DS icon, enlarged) with its name and
// whether it has a save, under each console's heading on ALL. selected: -1 for none. scroll is
// kept so the chosen game is in view (follow false: as it is, a finger having scrolled the list).
// hint: the line along the bottom (the Switch's buttons when empty). onTabs: the tabs are chosen
// (up from the first row of games; left and right then change the tab), the shown one framed.
inline void DrawClassic(Canvas& c, const std::vector<Game>& games, int tab, int selected, int& scroll, const std::string& note,
                 const std::string& hintText = std::string(), const Theme& th = Theme(), bool follow = true, bool onTabs = false)
{
    const View v = MakeView(c, games, tab);
    const Grid& grid = v.g;
    const int s = grid.s;
    c.Fill(0, 0, c.w, c.h, th.bg);
    if (th.background)
        if (const uint32_t* bgPx = th.background->Fitted(c.w, c.h)) std::copy(bgPx, bgPx + (size_t)c.w * c.h, c.px.begin());
    // A band along the top with the title.
    c.Fill(0, 0, c.w, (c.h > c.w ? 43 : 22) * s, th.band);
    c.Fill(0, (c.h > c.w ? 43 : 22) * s, c.w, th.bandLineH * s, th.bandLine);
    const std::string title = "POK\x05MON FULLDEX"; // (Pokémon Fulldex: \x05 is the font's É, ui.h)
    c.Text((c.w - Canvas::TextWidth(title, s + 1)) / 2, 6 * s, title, s + 1, th.title);
    // The tabs: the chosen one bright and underlined.
    for (int t = 0; t < TABS; t++)
    {
        const std::string name = TabName(t);
        const int x = TabX(c, grid, t) - Canvas::TextWidth(name, s) / 2, y = grid.tabsY + 3 * s;
        if (onTabs && t == tab) c.Fill(x - 3 * s, y - 3 * s, Canvas::TextWidth(name, s) + 5 * s, 13 * s, th.frame);
        c.Text(x, y, name, s, t == tab ? th.title : th.hint);
        if (t == tab) c.Fill(x - 2 * s, grid.tabsY + grid.tabsH - s, Canvas::TextWidth(name, s) + 3 * s, s, th.frame);
    }

    FollowSelection(v, selected, scroll, follow);
    const int top = grid.top, bottom = grid.bottom, tile = grid.tile, labelH = grid.labelH;
    // The headings (on ALL): the console's name and a line to the right edge of the tiles.
    for (const View::Head& h : v.heads)
    {
        const int y = top + h.y - scroll;
        if (h.empty)
        {
            const std::string none = UiLang::Tr("NO GAMES FOUND") + " (" + PlatformName(h.platform) + ")";
            // (On its own tab: in the middle of the page.)
            if (tab != TAB_ALL) c.Text((c.w - Canvas::TextWidth(none, s)) / 2, (top + bottom) / 2 - 4 * s, none, s, th.hint);
            else if (y >= top && y + 10 * s <= bottom) c.Text(grid.gx, y + 2 * s, none, s, th.hint);
            continue;
        }
        if (y < top || y + 10 * s > bottom) continue;
        const std::string name = PlatformName(h.platform);
        c.Text(grid.gx, y + s, name, s + 1, th.frame);
        const int lx = grid.gx + Canvas::TextWidth(name, s + 1) + 4 * s;
        const int rx = grid.gx + grid.cols * tile + (grid.cols - 1) * grid.gap * 3;
        if (rx > lx) c.Fill(lx, y + 6 * s, rx - lx, std::max(1, s / 2), th.tile);
    }
    // The tiles that are wholly in view.
    for (const View::Spot& sp : v.spots)
    {
        const int x = sp.x, y = top + sp.y - scroll;
        if (y < top || y + tile + labelH > bottom) continue;
        const Game& g = games[sp.game];
        const bool sel = sp.game == selected;
        // A frame round the chosen one.
        if (sel) c.Fill(x - th.frameW * s, y - th.frameW * s, tile + 2 * th.frameW * s, tile + 2 * th.frameW * s, th.frame);
        c.Fill(x, y, tile, tile, th.tile);
        if (!g.art.empty())
        {
            for (int py = 0; py < tile; py++)
            {
                const uint32_t* row = &g.art[(size_t)(py * Art / tile) * Art];
                for (int px = 0; px < tile; px++)
                {
                    const uint32_t p = row[px * Art / tile];
                    if (p >> 24 >= 0x80) c.px[(size_t)(y + py) * c.w + x + px] = p | 0xFF000000u;
                }
            }
            c.Fill(x, y, 1, 1, th.tile); // (so the canvas knows the tile was drawn)
        }
        else if (g.hasIcon)
        {
            // The DS icon, enlarged by whole pixels.
            const int is = std::max(1, tile / 40), o = (tile - 32 * is) / 2;
            for (int py = 0; py < 32; py++)
                for (int px = 0; px < 32; px++)
                    if (g.icon[py * 32 + px] >> 24) c.Fill(x + o + px * is, y + o + py * is, is, is, g.icon[py * 32 + px]);
        }
        // COMING SOON across the picture of a game on its way, the picture darkened.
        if (g.comingSoon)
        {
            for (int py = 0; py < tile; py++)
                for (int px = 0; px < tile; px++)
                {
                    uint32_t& p = c.px[(size_t)(y + py) * c.w + x + px];
                    p = 0xFF000000u | ((p >> 1) & 0x7F7F7F);
                }
            const std::string soon = "COMING SOON";
            const int bs = Canvas::TextWidth(soon, s) + 8 * s <= tile ? s : std::max(1, s - 1);
            c.Fill(x, y + tile / 2 - 8 * bs, tile, 16 * bs, th.band);
            c.Text(x + (tile - Canvas::TextWidth(soon, bs)) / 2, y + tile / 2 - 4 * bs, soon, bs, th.title);
        }
        // The name (shortened: POKEMON is the same for all) and the save.
        std::string name = g.name;
        if (name.rfind("POKEMON ", 0) == 0) name = name.substr(8);
        const int ns = Canvas::TextWidth(name, s) <= tile ? s : std::max(1, s - 1);
        c.Text(x + (tile - Canvas::TextWidth(name, ns)) / 2, y + tile + 3 * s, name, ns, sel ? th.nameSel : th.name);
        if (sel && th.selBar) c.Fill(x + tile / 4, y + tile + (UiLang::Tall() ? 31 : 19) * s, tile / 2, s, th.selBar);
        std::string sub = g.comingSoon ? "COMING SOON" : !g.known ? "NOT PLAYABLE" : g.hasSave ? "SAVE FOUND" : "NEW GAME";
        if (!g.where.empty() && g.known) sub = (g.hasSave ? "SAVE " : "NEW ") + g.where; // e.g. SAVE IN BLACK2
        if(g.favourite)c.Text(x+tile-24*s,y+4*s,"FAV",s,th.nameSel);
        const int ss = std::max(1, s * 2 / 3);
        c.Text(x + (tile - Canvas::TextWidth(sub, ss)) / 2, y + tile + (UiLang::Tall() ? 19 : 12) * s, sub, ss, !g.known && !g.comingSoon ? th.bad : sel ? th.subSel : th.sub);
    }
    // More games below the ones shown. (None drawn for above: under the tabs it looked like one
    // of their underlines, and the cut tiles at the top say it already.)
    if (scroll + (bottom - top) < v.height - grid.gap) c.Fill(c.w / 2 - 8 * s, bottom - 2 * s, 16 * s, s, th.hint);
    const std::string hint = !note.empty() ? note : !hintText.empty() ? hintText : "A: PLAY    Y: SETTINGS    L / R: TABS    +: QUIT";
    int hintScale=s;
        while(hintScale>1 && Canvas::TextWidth(hint,hintScale)>c.w-8)--hintScale;
    c.Text((c.w - Canvas::TextWidth(hint, hintScale)) / 2, c.h - 12 * s, hint, hintScale, th.hint);
}

// The list: the glass launcher where it can be drawn (Glassy), else the classic one. settingsChosen:
// the sidebar's Settings chosen with the arrows (the classic list draws its own SETTINGS button).
inline void Draw(Canvas& c, const std::vector<Game>& games, int tab, int selected, int& scroll, const std::string& note,
                 const std::string& hintText = std::string(), const Theme& th = Theme(), bool follow = true, bool onTabs = false,
                 bool settingsChosen = false)
{
    if (Glassy(c))
    {
        const View v = MakeView(c, games, tab);
        DrawGlass(c, games, v, tab, selected, scroll, note, hintText, follow, onTabs, settingsChosen);
        return;
    }
    DrawClassic(c, games, tab, selected, scroll, note, hintText, th, follow, onTabs);
}

// The settings page (Y on the Switch's list, H or a controller's Y on a PC's): the pause menu's
// options with what each does, set here before playing. They are written to the same settings
// file the games read (main.cpp, Settings: keys, values and defaults must stay in step with it),
// a line at a time, leaving the others as they are. Entries without a key are what can only be
// done in the pause menu. The list's font has capitals, digits and : - + . / < > ! ( ) only.
struct Setting
{
    const char* key;  // nullptr: told about, not set here; "!...": something to do, not stored
    const char* name;
    std::vector<std::pair<std::string, std::string>> values; // stored, shown
    std::string def;  // the stored value when the file has none ("": the game decides, AUTO)
    const char* text;
};
inline std::vector<Setting> SettingsList()
{
    const std::vector<std::pair<std::string, std::string>> onOff = { { "1", "ON" }, { "0", "OFF" } };
    std::vector<Setting> l;
#ifdef __ANDROID__
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. INSTALL DOWNLOADS IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT AND ANDROID ASKS "
                  "BEFORE INSTALLING. THE OLD VERSION IS KEPT. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#elif defined(__APPLE__)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. WHEN THERE IS ONE: CHOOSE AGAIN TO OPEN ITS PAGE AND DOWNLOAD IT THERE." });
#elif defined(__SWITCH__) && defined(SSR_ALL_GAMES)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. CHOOSE AGAIN TO INSTALL IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT "
                  "AND THE OLD .NRO IS KEPT IN THE UPDATE FOLDER. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#elif !defined(__SWITCH__)
    l.push_back({ "!updates", "CHECK FOR UPDATES", { { "", "CHECK" } }, "",
                  "CHECKS GITHUB FOR A NEW VERSION. CHOOSE AGAIN TO INSTALL IT: IT IS CHECKED AGAINST THE RELEASE SHA256SUMS.TXT "
                  "AND THE OLD VERSION IS KEPT IN THE UPDATE FOLDER. YOUR ROMS / SAVES AND SETTINGS STAY AS THEY ARE." });
#endif

#ifdef __SWITCH__
    // (The first start's 60 FPS and WORLD DETAIL depend on the game: SoulSilver and HeartGold
    // start lighter. Until either is set, AUTO.)
    l.push_back({ "fps60", "60 FPS", onOff, "", "ON: THE 3D WORLD GETS IN-BETWEEN FRAMES AND MOVES AT 60 FRAMES A SECOND (THE GAMES DRAW IT AT 30). OFF: 30 AS ON A DS." });
#else
    // (A PC, and the Android app: no BORDERLESS FULLSCREEN, WORLD ON GPU or STICK TOUCH CURSOR
    // there, which is always full screen, draws the world with the processor and has a touch
    // screen.)
    l.push_back({ "view", "SCREEN LAYOUT", { { "merged", "MERGED" }, { "hybrid", "HYBRID" }, { "side", "SIDE" }, { "vertical", "VERTICAL" }, { "top", "TOP" }, { "bottom", "BOTTOM" } }, "merged",
                  "MERGED: THE ONE WIDESCREEN PICTURE. THE OTHERS SHOW THE TWO DS SCREENS LAID OUT AS ON OTHER EMULATORS." });
    l.push_back({ "large_screen", "LARGE SCREEN", { { "auto", "AUTO" }, { "top", "TOP" }, { "bottom", "BOTTOM" } }, "auto",
                  "FOR THE LAYOUTS WITH A LARGE AND A SMALL SCREEN: WHICH ONE IS LARGE. AUTO FOLLOWS WHAT THE GAME IS DOING." });
    l.push_back({ "fps60", "60 FPS", onOff, "1", "ON: THE 3D WORLD GETS IN-BETWEEN FRAMES AND MOVES AT 60 FRAMES A SECOND (THE GAMES DRAW IT AT 30). OFF: 30 AS ON A DS." });
#ifndef SSR_PHONE
    l.push_back({ "fullscreen", "BORDERLESS FULLSCREEN", onOff, "1", "ON: FILLS YOUR SCREEN AS A BORDERLESS WINDOW AT ITS OWN RESOLUTION. SWITCHING TO OTHER WINDOWS IS INSTANT. F11 DOES THE SAME." });
#endif
#endif
    l.push_back({ "smooth", "SMOOTH SCALING", onOff, "0", "ON: THE PICTURE IS SOFTENED AS IT IS SCALED UP TO YOUR SCREEN. OFF: SHARP PIXELS." });
    l.push_back({ "wide", "WIDESCREEN WORLD", onOff, "1", "ON: THE 3D WORLD IS DRAWN OUT TO THE SIDES OF THE 16:9 SCREEN. OFF: ONLY AS WIDE AS A DS SCREEN." });
    l.push_back({ "wide_map", "WIDE MAP LOADING", onOff, "1", "BLACK / WHITE / BLACK 2 / WHITE 2: LOADS MORE OF THE MAP AROUND YOU SO THE WIDE VIEW HAS LAND AT ITS EDGES." });
    l.push_back({ "battle_buttons", "BATTLE BUTTONS", { { "corner", "CORNER" }, { "panel", "PANEL" }, { "strip", "STRIP" } }, "strip",
                  "WHERE THE BATTLE MENU GOES. CORNER: SMALL BUTTONS IN A CORNER. PANEL: THE FIGHT / BAG / POKEMON / RUN PANEL FROM THE GAME BESIDE THE BATTLE. "
                  "STRIP: LARGE BUTTONS IN THE MESSAGE BOX IN BLACK AND WHITE (CORNER IN THE OTHERS). SOULSILVER / HEARTGOLD / PLATINUM HAVE THEIR OWN." });
#ifdef __SWITCH__
    l.push_back({ "wide_lines", "WORLD DETAIL", { { "192", "192" }, { "288", "288" }, { "384", "384" }, { "540", "540" }, { "720", "720" } }, "",
                  "HOW MANY LINES THE WIDE 3D WORLD IS DRAWN WITH. 720 IS FULL SHARPNESS ON THE SWITCH. LOWER IS LIGHTER ON THE CONSOLE." });
    l.push_back({ "busy_detail", "LOWER DETAIL WHEN BUSY", onOff, "0", "ON: DROPS THE WORLD DETAIL FOR A MOMENT WHEN DRAWING CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
#else
#ifdef SSR_PHONE
    l.push_back({ "wide_lines", "3D RESOLUTION", { { "192", "192P (DS)" }, { "288", "288P" }, { "384", "384P" }, { "540", "540P" }, { "720", "720P" }, { "1080", "1080P" } }, "720",
                  "HOW SHARP THE 3D WORLD IS. 720P SUITS MOST PHONES. LOWER IF YOURS CANNOT KEEP UP OR TO SAVE BATTERY. SPRITES AND TEXT KEEP THEIR DS PIXELS." });
    l.push_back({ "busy_detail", "LOWER RES WHEN BUSY", onOff, "0", "ON: DROPS THE 3D RESOLUTION FOR A MOMENT WHEN THE PHONE CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
#else
    l.push_back({ "wide_lines", "3D RESOLUTION", { { "192", "192P (DS)" }, { "288", "288P" }, { "384", "384P" }, { "540", "540P" }, { "720", "720P" }, { "1080", "1080P" }, { "1440", "1440P (2K)" }, { "2160", "2160P (4K)" } },
#ifdef __APPLE__
                  "720", // (drawn by the processor on a Mac)
#else
                  "1080",
#endif

                  "HOW SHARP THE 3D WORLD IS. PICK YOUR SCREEN RESOLUTION OR LOWER IF YOUR PC CANNOT KEEP UP. SPRITES AND TEXT KEEP THEIR DS PIXELS." });
    l.push_back({ "busy_detail", "LOWER RES WHEN BUSY", onOff, "0", "ON: DROPS THE 3D RESOLUTION FOR A MOMENT WHEN YOUR PC CANNOT KEEP UP (A MOVE FULL OF EFFECTS) INSTEAD OF SLOWING THE GAME DOWN." });
    l.push_back({ "ff_low_res", "FAST FORWARD LOW RES", onOff, "0", "ON: THE 3D WORLD DROPS TO 360P WHILE YOU FAST FORWARD FOR MORE FPS (A FASTER FAST FORWARD IF YOUR PC CANNOT KEEP UP). OFF: YOUR 3D RESOLUTION STAYS." });
#ifndef __APPLE__ // (a Mac's OpenGL stops at 4.1: the processor draws the world)
    l.push_back({ "wide_gpu", "WORLD ON GPU", onOff, "1", "ON: THE WIDE 3D WORLD IS DRAWN BY YOUR GRAPHICS CARD (OPENGL 4.3). TURN IT OFF IF THE WORLD LOOKS WRONG OR THE GAME WILL NOT START: THE PROCESSOR THEN DRAWS IT (THAT WANTS A FAST PC)." });
#endif
#endif
#endif
    l.push_back({ "show_fps", "FPS COUNTER", onOff, "0", "SHOWS THE FRAMES A SECOND IN A CORNER." });
    l.push_back({ "volume", "VOLUME", { { "0", "OFF" }, { "10", "10%" }, { "20", "20%" }, { "30", "30%" }, { "40", "40%" }, { "50", "50%" }, { "60", "60%" }, { "70", "70%" }, { "80", "80%" }, { "90", "90%" }, { "100", "100%" } }, "100",
                  "THE GAME'S SOUND AND MUSIC. OFF FOR NONE. ALSO IN THE PAUSE MENU WHILE YOU PLAY." });
#ifdef __SWITCH__
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "menu",
                  "PAUSE MENU: + OPENS THE PAUSE MENU WHILE PLAYING. DS START: + IS THE DS START BUTTON (THE GAMES HARDLY USE IT) AND + AND - TOGETHER OPEN THE MENU." });
#elif defined(SSR_PHONE)
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "menu",
                  "PAUSE MENU: START ON A CONTROLLER OPENS THE PAUSE MENU WHILE PLAYING. DS START: IT IS THE DS START BUTTON (THE GAMES HARDLY USE IT). THE START ON THE SCREEN IS ALWAYS THE DS START." });
#else
    l.push_back({ "start_button", "START BUTTON", { { "menu", "PAUSE MENU" }, { "ds", "DS START" } }, "ds",
                  "DS START: START ON A CONTROLLER IS THE DS START; THE XBOX / PS BUTTON, OR START AND BACK TOGETHER, OPEN THE PAUSE MENU. PAUSE MENU: START (MENU ON XBOX / OPTIONS ON PS) OPENS IT. ENTER IS ALWAYS THE DS START." });
#endif
#ifndef __SWITCH__
    l.push_back({ "buttons", "BUTTONS", { { "labels", "BY LABEL" }, { "positions", "BY POSITION" } }, "labels",
                  "BY LABEL: THE BUTTON MARKED A IS THE DS A ON ANY CONTROLLER. BY POSITION: THE BUTTONS WHERE A DS HAS THEM (XBOX A IS DS B)." });
#ifndef SSR_PHONE
    l.push_back({ "pad_touch", "STICK TOUCH CURSOR", onOff, "0", "ON: THE RIGHT STICK MOVES A CURSOR ON THE TOUCH SCREEN AND THE LEFT TRIGGER PRESSES IT." });
    // (frontend/shortcuts.h, made by the game list: pc_launcher.cpp. Not on a Mac: its Dock and
    // Launchpad start the app itself.)
#if defined(_WIN32)
    l.push_back({ "!shortcuts", "DESKTOP SHORTCUTS", { { "", "CREATE" } }, "",
                  "PUTS A SHORTCUT FOR EACH GAME IN THIS LIST ON YOUR DESKTOP WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY (QUIT IN THE PAUSE MENU STILL COMES BACK TO THIS LIST). CREATE AGAIN AFTER ADDING GAMES OR MOVING THIS FOLDER." });
#elif !defined(__APPLE__)
    l.push_back({ "!shortcuts", "DESKTOP SHORTCUTS", { { "", "CREATE" } }, "",
                  "PUTS A SHORTCUT FOR EACH GAME IN THIS LIST ON YOUR DESKTOP AND IN YOUR APPS MENU WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY. ON THE STEAM DECK THEY ALSO SHOW UP IN ADD A NON-STEAM GAME. CREATE AGAIN AFTER ADDING GAMES OR MOVING THIS FOLDER." });
#endif
#endif
#endif
#if defined(__SWITCH__) && defined(SSR_ALL_GAMES)
    // (frontend/switch_forwarder.cpp, made by the game list: switch_platform.cpp.)
    l.push_back({ "!forwarders", "HOME MENU ICONS", { { "", "INSTALL" } }, "",
                  "PUTS AN ICON FOR EACH GAME IN THIS LIST ON THE HOME MENU WITH ITS PICTURE. IT STARTS THAT GAME STRAIGHT AWAY. MADE ON THIS CONSOLE: NO PC OR KEYS FILE NEEDED. LIKE ANY FORWARDER THE ICONS NEED SIGPATCHES TO START. INSTALL AGAIN AFTER ADDING GAMES. REMOVE ONE IN THE HOME MENU (OPTIONS / DELETE SOFTWARE)." });
#endif
    // The interface's language (UiLang, ui_lang.h), shown first among DISPLAY's options.
    l.insert(l.begin(), { "list_lang", "LANGUAGE", UiLang::Choices(), "auto",
                          "THE LANGUAGE OF THE GAME LIST, THE SETTINGS AND THE PAUSE MENU (NOT OF THE GAMES)." });
#ifndef __SWITCH__
    l.push_back({"list_sort","SORT",{{"original","ORIGINAL"},{"favourites","FAVOURITES FIRST"},{"recent","RECENTLY PLAYED"}},"original",
        "KEEPS THE CONSOLE SECTIONS. ORIGINAL RETAINS THE EXISTING GAME ORDER."});
    l.push_back({"!favourite","TOGGLE FAVOURITE",{},"","ADDS OR REMOVES THE SELECTED GAME. ALSO F ON A KEYBOARD OR X ON A CONTROLLER."});
#endif
    size_t extraCount;const auto* extras=OptionalFeatures::Options(extraCount);
    for(size_t i=0;i<extraCount;i++)l.push_back({extras[i].key,extras[i].name,onOff,"0",extras[i].help});
    // The game list's own look (GAME LIST: ThemeNamed, Background).
    l.push_back({ "list_colour", "COLOUR", { { "red", "RED" }, { "blue", "BLUE" }, { "green", "GREEN" }, { "purple", "PURPLE" },
                  { "gold", "GOLD" }, { "pink", "PINK" }, { "teal", "TEAL" } }, "red",
                  "THE GAME LIST'S COLOUR: ITS LINES, THE FRAME ROUND THE CHOSEN GAME AND THE TABS." });
#ifdef __SWITCH__
    l.push_back({ "list_background", "BACKGROUND", { { "plain", "PLAIN" }, { "picture", "YOUR PICTURE" } }, "plain",
                  "YOUR PICTURE: PUT A PICTURE NAMED BACKGROUND.JPG OR BACKGROUND.PNG IN /SWITCH/POKEMON-DS-WIDE/ (WITH YOUR SAVES). BEST AT 1280 X 720 OR 1920 X 1080 (16:9); ANY SIZE WORKS, CUT TO FILL THE SCREEN. IT IS SHOWN DARKENED BEHIND THE GAMES." });
#else
    l.push_back({ "list_background", "BACKGROUND", { { "plain", "PLAIN" }, { "picture", "YOUR PICTURE" } }, "plain",
                  "YOUR PICTURE: PUT A PICTURE NAMED BACKGROUND.JPG OR BACKGROUND.PNG BESIDE YOUR SAVES FOLDER. BEST AT 1920 X 1080 (16:9); ANY SIZE WORKS, CUT TO FILL THE SCREEN. IT IS SHOWN DARKENED BEHIND THE GAMES." });
#endif
#if !defined(__SWITCH__)
    // RetroAchievements from the game list too (pc_launcher.cpp: the login, as the pause menu's).
    l.push_back({ "ra", "RETROACHIEVEMENTS", onOff, "0",
                  "ON: YOUR RETROACHIEVEMENTS ACCOUNT FOLLOWS THE GAMES YOU PLAY AND UNLOCKS POP UP AS YOU PLAY (CASUAL MODE: SAVE STATES AND FAST FORWARD STILL WORK; PAUSED WHILE A CHEAT IS ON)." });
    l.push_back({ "!ra_login", "ACCOUNT", { { "", "LOG IN" } }, "",
                  "LOG IN WITH YOUR RETROACHIEVEMENTS USER NAME AND PASSWORD (A FREE ACCOUNT FROM RETROACHIEVEMENTS.ORG). THE PASSWORD IS SENT ONCE AND NOT KEPT. CHOSEN AGAIN WHEN LOGGED IN: LOG OUT." });
#endif
    // Only in the pause menu.
#ifdef __SWITCH__
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "PRESS + WHILE PLAYING (+ AND - TOGETHER WITH START BUTTON ON DS START). EVERYTHING ABOVE IS THERE TOO AND CAN BE CHANGED AS YOU PLAY." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "HOLD ZR (UP TO 4X). FAST FORWARD IN THE PAUSE MENU KEEPS IT ON WITHOUT HOLDING. THE BUTTON IS CHANGED IN KEYS AND BUTTONS." });
#elif defined(SSR_PHONE)
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "THE MENU BUTTON AT THE TOP OF THE SCREEN WHILE PLAYING OR START OR GUIDE ON A CONTROLLER. EVERYTHING ABOVE IS THERE TOO." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "HOLD THE >> BUTTON BESIDE R AT THE TOP, OR THE RIGHT TRIGGER OF A CONTROLLER (UP TO 4X). FAST FORWARD IN THE PAUSE MENU KEEPS IT ON WITHOUT HOLDING. THE BUTTON IS CHANGED IN KEYS AND BUTTONS." });
#else
    l.push_back({ nullptr, "THE PAUSE MENU", {}, "", "ESC ON THE KEYBOARD. ON A CONTROLLER: START (MENU ON XBOX / OPTIONS ON PS) OR ITS GUIDE BUTTON. EVERYTHING ABOVE IS THERE TOO." });
    l.push_back({ nullptr, "FAST FORWARD", {}, "", "HOLD THE RIGHT TRIGGER OR LEFT SHIFT (UP TO 4X). FAST FORWARD IN THE PAUSE MENU KEEPS IT ON WITHOUT HOLDING. THE KEY AND BUTTON ARE CHANGED IN KEYS AND BUTTONS." });
#endif
#ifdef __SWITCH__
    l.push_back({ "!keys", "KEYS AND BUTTONS", { { "", "CHANGE" } }, "",
                  "GIVE ANY DS BUTTON / THE PAUSE MENU / FAST FORWARD / SWAP SCREENS / SAVE STATE / LOAD STATE ANOTHER BUTTON (ANY CONTROLLER: PRO / JOY-CON OR ONE ADDED WITH SYS-CON OR MISSIONCONTROL). GIVING ONE A BUTTON ANOTHER ONE HAD SWAPS THE TWO. SAVE STATE AND LOAD STATE START NOT SET. ALSO IN THE PAUSE MENU (CONTROLS). THIS LIST FOLLOWS THE ONES YOU CHANGE." });
#else
    l.push_back({ "!keys", "KEYS AND BUTTONS", { { "", "CHANGE" } }, "",
                  "GIVE ANY DS BUTTON / THE PAUSE MENU / FAST FORWARD / SWAP SCREENS / SAVE STATE / LOAD STATE ANOTHER KEY OR CONTROLLER BUTTON. GIVING ONE A KEY OR BUTTON ANOTHER ONE HAD SWAPS THE TWO. SAVE STATE STARTS AS F5 AND LOAD STATE AS F8 (NOT SET ON A CONTROLLER). ALSO IN THE PAUSE MENU (CONTROLS). THIS LIST FOLLOWS THE ONES YOU CHANGE (A PLAYS / B GOES BACK / Y OPENS THESE SETTINGS)." });
#endif
#ifdef __SWITCH__
    l.push_back({ nullptr, "SAVE STATE / LOAD STATE", {}, "", "ONE QUICK SLOT PER GAME, FROM THE PAUSE MENU. CHOOSE A BUTTON FOR EACH IN KEYS AND BUTTONS (THEY START NOT SET). YOUR SAVE IN THE GAME IS SEPARATE AND STILL WORKS AS NORMAL." });
#else
    l.push_back({ nullptr, "SAVE STATE / LOAD STATE", {}, "", "ONE QUICK SLOT PER GAME. F5 SAVES AND F8 LOADS UNTIL YOU CHANGE THEM IN KEYS AND BUTTONS. A CONTROLLER BUTTON IS NOT SET UNTIL YOU CHOOSE ONE. YOUR SAVE IN THE GAME IS SEPARATE AND STILL WORKS AS NORMAL." });
#endif
    l.push_back({ nullptr, "RESET GAME / QUIT", {}, "", "IN THE PAUSE MENU: START THE GAME AGAIN FROM ITS TITLE SCREEN / BACK TO THIS GAME LIST." });
    return l;
}

// Words to lines of at most `chars` characters.
inline std::vector<std::string> Wrap(const std::string& text, int chars)
{
    return UiLang::Wrap(text, chars * 6);
}

struct SettingsGroup
{
    const char* key;  // its row in the list of groups (a "!" key: chosen, not stored)
    const char* name;
    const char* text;
};
inline const std::vector<SettingsGroup>& SettingsGroups()
{
    static const std::vector<SettingsGroup> g = {
        // (What each holds is listed under it: its options' names, ShowGroups.)
        { "!group0", "DISPLAY", "" },
        { "!group1", "SOUND", "" },
        { "!group2", "GRAPHICS", "" },
        { "!group3", "CONTROLS", "" },
        { "!group4", "SHORTCUTS", "" },
        { "!group5", "GAME LIST", "" },
        { "!group6", "RETROACHIEVEMENTS", "" },
        { "!group7", "HELP", "" },
        { "!group8", "OPTIONAL FEATURES", "" },
    };
    return g;
}
// Which group an option is in (SettingsGroups).
inline int GroupOf(const Setting& s)
{
    if (s.key && !strcmp(s.key, "!updates")) return 7;
    if (s.key && !strcmp(s.key,"!favourite")) return 5;
    if (!s.key) return !strcmp(s.name, "KEYS AND BUTTONS") ? 3 : 7;
    if (!strncmp(s.key,"extra_",6)) return 8;
    if (!strcmp(s.key, "list_lang")) return 0;
    if (!strncmp(s.key, "list_", 5)) return 5;
    if (!strcmp(s.key, "ra") || !strcmp(s.key, "!ra_login")) return 6;
    static const char* display[] = { "view", "large_screen", "fullscreen", "smooth", "battle_buttons", "show_fps" };
    static const char* controls[] = { "start_button", "buttons", "pad_touch" };
    for (const char* k : display)
        if (!strcmp(s.key, k)) return 0;
    if (!strcmp(s.key, "volume")) return 1;
    for (const char* k : controls)
        if (!strcmp(s.key, k)) return 3;
    if (!strcmp(s.key, "!keys")) return 3;
    if (s.key[0] == '!') return 4; // (DESKTOP SHORTCUTS, HOME MENU ICONS)
    return 2;
}

struct SettingsPage
{
    std::string path;
    std::vector<Setting> all = SettingsList();
    std::vector<Setting> items; // what is shown: the groups, or one group's options
    int group = -1;             // the group shown, -1: the list of groups
    std::string groupText[9];   // (the groups' lines: their options' names)
    std::vector<std::string> lines; // the file, as it is
    int selected = 0, scroll = 0;
    // Draw keeps the chosen option in view while follow is set (the keys and the D-pad); a
    // finger scrolling the page clears it. Draw also notes, for taps (ItemAt), where each
    // option's name line is and how tall a line is.
    bool follow = true;
    int lineH = 20;
    std::vector<std::pair<int, int>> rows; // (option, the top of its name line)

    // The option whose part of the page (its name and the lines below it) is at y, or -1.
    int ItemAt(int y) const
    {
        int found = -1;
        for (const auto& r : rows)
            if (r.second - lineH / 4 <= y) found = r.first;
        if (found >= 0 && !rows.empty() && y > rows.back().second + 6 * lineH) return -1;
        return found;
    }

    void Open(const std::string& cfgPath, bool keepChoice = false)
    {
        path = cfgPath;
        lines.clear();
        if (FILE* f = fopen(path.c_str(), "r"))
        {
            char buf[1024];
            while (fgets(buf, sizeof buf, f))
            {
                std::string l = buf;
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                lines.push_back(l);
            }
            fclose(f);
        }
        if (keepChoice) return;
        ShowGroups(0);
    }
    // The list of groups, with group `sel` chosen.
    void ShowGroups(int sel)
    {
        group = -1;
        items.clear();
        int row = 0;
        for (int g = 0; g < (int)SettingsGroups().size(); g++)
        {
            bool any = false;
            for (const Setting& s : all) any |= GroupOf(s) == g;
            if (!any) continue; // (a platform without any of it: no SHORTCUTS on a Mac)
            if (g == sel) row = (int)items.size();
            const SettingsGroup& sg = SettingsGroups()[g];
            std::string names;
            for (const Setting& s : all)
                if (GroupOf(s) == g) names += (names.empty() ? "" : " / ") + std::string(s.name);
            groupText[g] = names + ".";
            items.push_back({ sg.key, sg.name, { { "", ">" } }, "", groupText[g].c_str() });
        }
        selected = row;
        scroll = 0;
        follow = true;
    }
    // Group g's options.
    void Enter(int g)
    {
        group = g;
        items.clear();
        for (const Setting& s : all)
            if (GroupOf(s) == g) items.push_back(s);
        selected = 0;
        scroll = 0;
        follow = true;
    }
    // B: from a group back to the list of groups; false when already there (the page closes).
    bool Back()
    {
        if (group < 0) return false;
        ShowGroups(group);
        return true;
    }
    std::string Stored(const std::string& key) const
    {
        for (const std::string& l : lines)
            if (l.size() > key.size() && !l.compare(0, key.size(), key) && l[key.size()] == '=') return l.substr(key.size() + 1);
        return "";
    }
    // The index of the shown value (-1: AUTO).
    int ValueIndex(const Setting& s) const
    {
        if (s.key[0] == '!') return 0; // (something to do: its one value is what it says)
        std::string v = Stored(s.key);
        if (v.empty()) v = s.def;
        if (v.empty()) return -1;
        for (size_t i = 0; i < s.values.size(); i++)
            if (s.values[i].first == v) return (int)i;
        // Values the file may hold in other spellings: numbers between the choices (the
        // nearest below), ON/OFF as anything but "0" or "1".
        if (!strcmp(s.key, "wide_lines"))
        {
            int best = 0;
            for (size_t i = 0; i < s.values.size(); i++)
                if (atoi(s.values[i].first.c_str()) <= atoi(v.c_str())) best = (int)i;
            return best;
        }
        for (size_t i = 0; i < s.values.size(); i++)
            if (s.values[i].first == s.def) return (int)i;
        return 0;
    }
    std::string Shown(const Setting& s) const
    {
        const int i = ValueIndex(s);
        return i < 0 ? "AUTO" : s.values[i].second;
    }
    void Move(int d)
    {
        follow = true;
        if (selected + d >= 0 && selected + d < (int)items.size()) selected += d;
    }
    // The chosen option's next (d 1) or previous (d -1) value, written to the file. Returns its
    // key, or nullptr.
    const char* Change(int d)
    {
        if (selected < 0 || selected >= (int)items.size() || !items[selected].key) return nullptr;
        const Setting& s = items[selected];
        if (!strncmp(s.key, "!group", 6))
        {
            if (d > 0) Enter(atoi(s.key + 6));
            return nullptr;
        }
        if (s.key[0] == '!') return s.key; // (done by the caller)
        const int n = (int)s.values.size();
        const int i = ValueIndex(s);
        const int next = i < 0 ? (d > 0 ? 0 : n - 1) : (i + d + n) % n;
        const std::string line = std::string(s.key) + "=" + s.values[next].first;
        bool found = false;
        for (std::string& l : lines)
            if (l.size() > strlen(s.key) && !l.compare(0, strlen(s.key), s.key) && l[strlen(s.key)] == '=')
            {
                l = line;
                found = true;
            }
        if (!found) lines.push_back(line);
        if (!strcmp(s.key, "wide_lines"))
        {
            // Match the in-game menu: choosing a manual resolution disables automatic
            // lowering, so starting the game honours that choice.
            bool haveBusy = false;
            for (std::string& l : lines)
                if (l.rfind("busy_detail=", 0) == 0) { l = "busy_detail=0"; haveBusy = true; }
            if (!haveBusy) lines.push_back("busy_detail=0");
        }
        const std::string tmp = path + ".tmp";
        if (FILE* f = fopen(tmp.c_str(), "w"))
        {
            bool ok = true;
            for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
            ok = fclose(f) == 0 && ok;
            std::error_code ec;
            if (ok) std::filesystem::rename(tmp, path, ec);
            if (!ok || ec) fprintf(stderr, "[launcher] cannot save settings: %s\n", path.c_str());
        }
        return s.key;
    }

    // Over the whole canvas: the options with their values (the chosen one marked), each with
    // what it does below it, kept in view as the choice moves.
    void Draw(Canvas& c, const std::string& hint, const Theme& th = Theme())
    {
        const int s = std::max(1, std::min(c.w, c.h) / 240);
#ifdef __ANDROID__
        // Phone settings use readable touch targets in either orientation.
        const int ts = std::max(2, s);
#else
        const int ts = std::max(1, (s * 2 + 1) / 3); // the text's scale: 2 at 720 lines, 3 at 1080
#endif
        c.Fill(0, 0, c.w, c.h, th.bg);
        c.Fill(0, 0, c.w, (c.h > c.w ? 43 : 22) * s, th.band);
        c.Fill(0, (c.h > c.w ? 43 : 22) * s, c.w, th.bandLineH * s, th.bandLine);
        const std::string title = group < 0 ? "SETTINGS" : std::string("SETTINGS: ") + UiLang::Tr(SettingsGroups()[group].name);
        int titleScale=s+1;
        while(titleScale>1 && Canvas::TextWidth(title,titleScale)>c.w-8)--titleScale;
        c.Text((c.w-Canvas::TextWidth(title,titleScale))/2,6*s,title,titleScale,th.title);
        const int width = std::min(c.w - 24 * s, 100 * 6 * ts);
        const int x = (c.w - width) / 2;
        struct Line { std::string text, value; int item; bool name; };
        std::vector<Line> out;
        int selFirst = 0, selLast = 0;
        bool infoHeading = false;
        for (int i = 0; i < (int)items.size(); i++)
        {
            const Setting& e = items[i];
            if (!e.key && !infoHeading)
            {
                out.push_back({ "", "", -1, false });
                infoHeading = true;
            }
            if (i == selected) selFirst = (int)out.size();
            const std::string value = e.key ? Shown(e) : "";
            const bool arrows = !value.empty() && i == selected && e.key && strncmp(e.key, "!group", 6);
            const std::string drawnValue = arrows ? "< " + UiLang::Tr(value) + " >" : UiLang::Tr(value);
            const bool stacked = Canvas::TextWidth(e.name, ts) + Canvas::TextWidth(drawnValue, ts) + 8 * ts > width;
            out.push_back({ e.name, stacked ? "" : value, i, true });
            if (stacked && !value.empty())
                for (const std::string& l : Wrap(UiLang::Tr(value), width / (6 * ts) - 2)) out.push_back({ l, "", i, false });
            for (const std::string& l : Wrap(UiLang::Tr(e.text), width / (6 * ts) - 2)) out.push_back({ l, "", i, false });
            if (i == selected) selLast = (int)out.size() - 1;
            out.push_back({ "", "", -1, false });
        }
        const int top = (c.h > c.w ? 48 : 30) * s, bottom = c.h - 18 * s;
        lineH = (UiLang::Tall() ? 16 : 10) * ts;
        const int fit = std::max(1, (bottom - top) / lineH);
        if (follow)
        {
            if (selFirst < scroll) scroll = selFirst;
            if (selLast >= scroll + fit) scroll = selLast - fit + 1;
        }
        scroll = std::max(0, std::min(scroll, (int)out.size() - fit));
        rows.clear();
        int y = top;
        for (int i = scroll; i < (int)out.size() && y + lineH <= bottom; i++, y += lineH)
        {
            const Line& l = out[i];
            if (l.name) rows.push_back({ l.item, y });
            const bool sel = l.item == selected && l.item >= 0;
            if (l.name)
            {
                if (sel) c.Fill(x - 4 * ts, y - 2 * ts, width + 8 * ts, lineH, th.tile);
                const bool info = !items[l.item].key;
                c.Text(x, y, l.text, ts, info ? th.hint : sel ? th.nameSel : th.subSel);
                if (!l.value.empty())
                {
                    const bool group = !strncmp(items[l.item].key, "!group", 6);
                    const std::string v = sel && !group ? "< " + UiLang::Tr(l.value) + " >" : l.value;
                    c.Text(x + width - Canvas::TextWidth(v, ts), y, v, ts, sel ? th.nameSel : th.name);
                }
            }
            else if (!l.text.empty())
                c.TextRaw(x + 2 * 6 * ts, y, l.text, ts, sel ? th.name : th.sub);
        }
        if (scroll > 0) c.Fill(c.w / 2 - 8 * s, top - 4 * s, 16 * s, s, th.hint);
        if (scroll + fit < (int)out.size()) c.Fill(c.w / 2 - 8 * s, bottom + s, 16 * s, s, th.hint);
        int hintScale=s;
        while(hintScale>1 && Canvas::TextWidth(hint,hintScale)>c.w-8)--hintScale;
    c.Text((c.w - Canvas::TextWidth(hint, hintScale)) / 2, c.h - 12 * s, hint, hintScale, th.pokemon ? 0xFFDBE9FAu : th.hint);
    }
};

}
