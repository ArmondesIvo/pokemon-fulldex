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
#include <atomic>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <dirent.h>
#include "ssr_os.h"
#include "gamecode.h"
#include <sys/stat.h>

#include "ui.h"
#include "optional_features.h"
#include "cloud_saves.h"
#include "hub_link.h"
#include "support_qr.h"
#include "play_time.h"
#ifdef SSR_POKEMON_HUB
#include "pokemon_hub.h"
#include "companion_rom.h"
#endif
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
// Tabs of the player's own, after the built-in ones (tab TABS + k): a name and its games' ROM paths;
// kept in fulldex-tabs.txt beside the saves, a line a tab (its name, then its ROMs), tab-separated.
struct CustomTab { std::string name; std::vector<std::string> roms; };
inline std::vector<CustomTab>& CustomTabs() { static std::vector<CustomTab> t; return t; }
inline std::string& TabsFile() { static std::string f; return f; }
inline int TabCount() { return TABS + (int)CustomTabs().size(); }
inline std::string TabsSig() { std::string s; for (const CustomTab& t : CustomTabs()) s += t.name + "|" + std::to_string(t.roms.size()) + ";"; return s; }
inline void LoadTabs(const std::string& dir)
{
    TabsFile() = dir + "/fulldex-tabs.txt";
    std::vector<CustomTab>& tabs = CustomTabs();
    tabs.clear();
    if (FILE* f = fopen(TabsFile().c_str(), "rb"))
    {
        char line[8192];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            CustomTab t;
            size_t at = 0;
            while (at <= l.size())
            {
                size_t end = l.find('\t', at);
                if (end == std::string::npos) end = l.size();
                const std::string part = l.substr(at, end - at);
                if (t.name.empty() && at == 0) t.name = part;
                else if (!part.empty()) t.roms.push_back(part);
                at = end + 1;
            }
            if (!t.name.empty()) tabs.push_back(t);
        }
        fclose(f);
    }
}
inline bool SaveTabs()
{
    if (TabsFile().empty()) return false;
    const std::string tmp = TabsFile() + ".tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = true;
    for (const CustomTab& t : CustomTabs())
    {
        std::string l = t.name;
        for (const std::string& r : t.roms) l += "\t" + r;
        if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
    }
    ok = fclose(f) == 0 && ok;
    if (!ok) return false;
    if (rename(tmp.c_str(), TabsFile().c_str()) != 0) { remove(TabsFile().c_str()); if (rename(tmp.c_str(), TabsFile().c_str()) != 0) return false; }
    return true;
}
// What the list is searched for (Search on the tab bar; "": not searched) and its order (list_sort):
// kept apart from the focus, which a click can start again.
inline std::string& SearchText() { static std::string s; return s; }
inline std::string& SortKey() { static std::string s; return s; }
inline const char* TabName(int t) { static const char* n[] = { "HOME", "ALL", "DS", "GBA", "3DS", "ROMHACKS" }; return t >= 0 && t < TABS ? n[t] : t < TabCount() ? "CUSTOM" : "ALL"; }
inline std::string TabLabel(int t)
{
    static const char* n[] = { "Home", "All", "DS", "GBA", "3DS", "Romhacks" };
    if (t >= 0 && t < TABS) return n[t];
    const int k = t - TABS;
    return k >= 0 && k < (int)CustomTabs().size() ? CustomTabs()[k].name : std::string("All");
}

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
    // Its own picture as found (art, hasIcon), kept so Change Picture can put it back (ApplyArt).
    std::vector<uint32_t> ownArt;
    bool ownIcon = false, ownKept = false;
    long long lastPlayed = 0;
    // (The ROMs page: taken off the list (fulldex-hidden.txt); the file's size.)
    bool hidden = false;
    long long fileSize = 0;
};

inline std::string& ContinueRom() { static std::string r; return r; } // the game played last (last-game.txt)
// The game whose picture is blurred behind the glass launcher; empty: the chosen one. (The Switch sets
// it a moment after the chosen game changes: making it again takes most of a redraw there.)
inline std::string& BackdropRom() { static std::string r; return r; }
// Where the arrows are on the glass launcher besides the games and the tabs: a section of the
// sidebar (side, -1: none) or the banner's Continue (banner 1) or Game Options (2).
// popup: the game's menu (A on a game) open on that row, -1 closed; where it was drawn (for taps).
// dialog: 1 Remove Game, 2 Delete ROM File (sure?), 3 Rename (text: what is typed); drow its row;
// dX..dRows where its rows were drawn (for taps).
struct GlassFocus { int side = -1; int banner = 0; int popup = -1; int popX = 0, popY = 0, popW = 0, popRow = 0;
                    int dialog = 0, drow = 0; std::string text; int dX = 0, dY = 0, dW = 0, dRow = 0, dRows = 0;
                    // (Dialogs 5 and up, Manage Saves: their title, words and rows as the launcher made them;
                    // dPaths each row's backup, or #folder / #close.)
                    std::string dTitle, dMsg; std::vector<std::string> dItems, dPaths;
                    // (The sidebar's pages: PageRows and below.)
                    int page = -1, prow = 0; int pX = 0, pY = 0, pW = 0, pRow = 0;
                    // (The Updates page: the updater's state as the platform gives it, -1 none here; Updater::State's order.)
                    int upState = -1; bool upCan = false; std::string upHave, upNew, upText, upNotes;
                    // (Its What's New: prow 2 when chosen; the first line shown, the last that can be, where it was drawn.)
                    int nScroll = 0, nMax = 0, nX = 0, nY = 0, nW = 0, nH = 0, nLine = 0;
                    // (The ROMs page: pact the chosen ROM's action, -1 the list; rScroll its first row shown, rVis how many;
                    // aX..aRow where the actions were drawn.)
                    int pact = -1, rScroll = 0, rVis = 0, aX = 0, aY = 0, aW = 0, aRow = 0;
                    // (Sort at the tab bar's right end: chosen (with the tabs), the list's order, where it was drawn.)
                    bool onSort = false; std::string sortKey; int sortX = 0, sortY = 0, sortW = 0, sortH = 0;
                    // (The tab bar's tools: which is chosen while onSort (1 +, 2 Search, 3 Sort), where they were drawn; the
                    // keyboard's use (0 a game's name, 1 a new tab (kbRom put in it), 2 the search, 3 tab kbTab's name).)
                    int tool = 3; int toolX[4] = {}, toolW[4] = {}, toolY = 0, toolH = 0;
                    int kbFor = 0, kbTab = 0; std::string kbRom;
                    // (The settings drawn as a page: its title; where its list and preview go, for the menu.)
                    std::string pageTitle; int sX = 0, sY = 0, sW = 0, sH = 0, sListW = 0, sPvX = 0; };
inline GlassFocus& Focus() { static GlassFocus f; return f; }
inline int PopupCount() { return 7; }
inline std::string PopupLabel(const Game& g, int i)
{
    static const char* n[] = { "Play", "", "Rename", "Change Picture", "Manage Saves", "Add To Tab", "Remove Game" };
    return i == 1 ? (g.favourite ? "Remove From Home" : "Add To Home") : i >= 0 && i < 7 ? n[i] : "";
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
// The search's words in a game's name (its own or the player's), any case.
inline bool NameHas(const Game& g, const std::string& q)
{
    auto low = [](std::string s) { for (char& ch : s) ch = (char)tolower((unsigned char)ch); return s; };
    const std::string k = low(q);
    return low(g.customName).find(k) != std::string::npos || low(g.name).find(k) != std::string::npos;
}
inline bool InTab(const std::vector<Game>& games, int i, int tab)
{
    const Game& g = games[i];
    if (!SearchText().empty() && !NameHas(g, SearchText())) return false;
    if (tab >= TABS)
    {
        const int k = tab - TABS;
        if (k >= (int)CustomTabs().size()) return false;
        for (const std::string& r : CustomTabs()[k].roms) if (r == g.rom) return true;
        return false;
    }
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

// A picture of the player's own (Change Picture): its middle square, made Art x Art. PNG or JPG.
inline bool LoadSquareArt(const unsigned char* data, size_t size, std::vector<uint32_t>& out)
{
    if (!ImageAllowed(data, size)) return false;
    int w = 0, h = 0, n = 0;
    unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
    if (!px) return false;
    const int side = std::min(w, h), ox = (w - side) / 2, oy = (h - side) / 2;
    out.assign((size_t)Art * Art, 0);
    for (int y = 0; y < Art; y++)
        for (int x = 0; x < Art; x++)
        {
            const int x0 = ox + x * side / Art, x1 = std::max(x0 + 1, ox + (x + 1) * side / Art);
            const int y0 = oy + y * side / Art, y1 = std::max(y0 + 1, oy + (y + 1) * side / Art);
            uint32_t sum[4] = {}, count = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++)
                {
                    const unsigned char* p = px + ((size_t)yy * w + xx) * 4;
                    for (int k = 0; k < 4; k++) sum[k] += p[k];
                    count++;
                }
            out[(size_t)y * Art + x] = (sum[3] / count) << 24 | (sum[0] / count) << 16 | (sum[1] / count) << 8 | sum[2] / count;
        }
    stbi_image_free(px);
    return true;
}
// The pictures chosen with Change Picture (fulldex-art.txt beside the saves: a ROM's path, a tab, the
// picture's file in saves/artwork, or placeholder for the plain card), over each game's own picture.
inline void ApplyArt(std::vector<Game>& games, const std::string& savesDir)
{
    const auto chosen = ReadPairs(savesDir + "/fulldex-art.txt");
    for (Game& g : games)
    {
        if (!g.ownKept) { g.ownArt = g.art; g.ownIcon = g.hasIcon; g.ownKept = true; }
        g.art = g.ownArt;
        g.hasIcon = g.ownIcon;
        const auto it = chosen.find(g.rom);
        if (it == chosen.end()) continue;
        if (it->second == "placeholder") { g.art.clear(); g.hasIcon = false; continue; }
        std::vector<unsigned char> data;
        std::vector<uint32_t> art;
        if (ReadFile(savesDir + "/artwork/" + it->second, data) && LoadSquareArt(data.data(), data.size(), art)) g.art = art;
    }
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
        g.top = (int)(304 * u); g.bottom = c.h - (int)(82 * u);
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
enum Side { SIDE_HOME, SIDE_HUB, SIDE_ROMS, SIDE_UPDATES, SIDE_SETTINGS, SIDE_ABOUT, SIDES }; // (Library is Home: its tabs, Sort)
inline const char* SideName(int i)
{
    static const char* n[] = { "Home", "Hub", "ROMs", "Updates", "Settings", "About" };
    return n[i];
}
inline int SideIcon(int i)
{
    static const int ic[] = { Glass::I_POKEBALL, Glass::I_PEOPLE, Glass::I_CHIP, Glass::I_LOAD, Glass::I_GEAR, Glass::I_INFO };
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
    float tabFont = 18;
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
    s.rowH = (int)(54 * u); s.rowGap = (int)(6 * u); s.firstRow = (int)(12 * u); // (level with the tab pills)
    s.tabsX = (int)(204 * u); s.tabsY = (int)(14 * u); s.tabsH = (int)(46 * u);
    // (The tabs smaller as they grow in number, to leave room for + after them and Search and Sort.)
    const int room = c.w - (int)(28 * u) - (int)(470 * u) - s.tabsX;
    s.tabFont = 18 * u;
    float pad = 40 * u;
    auto total = [&]() { int w = 0; for (int t = 0; t < TabCount(); t++) w += GW(TabLabel(t), s.tabFont, true) + (int)pad + (int)(2 * u); return w; };
    while (total() > room && s.tabFont > 12 * u) { s.tabFont -= u; pad = std::max(20 * u, pad - 2 * u); }
    int x = s.tabsX + (int)(4 * u);
    for (int t = 0; t < TabCount(); t++)
    {
        const int w = GW(TabLabel(t), s.tabFont, true) + (int)pad;
        s.tabX.push_back(x); s.tabW.push_back(w);
        x += w + (int)(2 * u);
    }
    s.bannerX = s.tabsX; s.bannerY = (int)(90 * u); s.bannerW = c.w - s.bannerX - (int)(28 * u); s.bannerH = (int)(160 * u);
    s.contX = s.bannerX + (int)(168 * u); s.contY = s.bannerY + (int)(100 * u); s.contW = (int)(150 * u); s.contH = (int)(44 * u);
    s.headY = (int)(266 * u);
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
// The game the banner offers: the one played last, if it is still here, else the first that can be played
// (not the chosen one: the banner would change, and be drawn again, with every press).
inline int ContinueGame(const std::vector<Game>& games, int selected)
{
    for (size_t i = 0; i < games.size(); i++)
        if (!ContinueRom().empty() && games[i].rom == ContinueRom()) return (int)i;
    for (size_t i = 0; i < games.size(); i++)
        if (games[i].known && !games[i].comingSoon) return (int)i;
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
    if (!g.hasIcon && g.comingSoon) return; // (its card says COMING SOON over it)
    if (!g.hasIcon)
    {
        // (The plain card: the game's name in the middle.)
        const std::string n = ShortName(g);
        float px = h / 7.0f;
        while (px > 8 && GW(n, px, true) > w - h / 8) px -= 1;
        GT(c, x + (w - GW(n, px, true)) / 2, y + h * 0.5f, n, px, 0xE6FFFFFFu, true);
        return;
    }
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
    const std::string k = g && !g->art.empty() ? g->rom + "#" + std::to_string(g->art[(size_t)Art * Art / 2 + Art / 2]) : std::string();
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

// The ROMs page's list: every ROM found, those taken off the list too (hidden), with the player's names
// and each file's size; in name order. Made by the platform from its whole list (BuildRomList).
inline std::string& HubDir() { static std::string d; return d; } // (the Hub page's settings folder: BuildRomList sets it)
inline std::vector<Game>& RomList() { static std::vector<Game> r; return r; }
inline void BuildRomList(const std::vector<Game>& all, const std::string& savesDir)
{
    HubDir() = savesDir;
    const auto names = ReadPairs(savesDir + "/fulldex-names.txt");
    const auto hidden = ReadPairs(savesDir + "/fulldex-hidden.txt");
    std::vector<Game>& r = RomList();
    r.clear();
    for (const Game& g : all)
    {
        if (g.comingSoon) continue;
        r.push_back(g);
        Game& x = r.back();
        const auto it = names.find(x.rom);
        if (it != names.end()) x.customName = it->second;
        x.hidden = hidden.count(x.rom) > 0;
        x.fileSize = 0;
        if (FILE* f = fopen(x.rom.c_str(), "rb"))
        {
            if (fseek(f, 0, SEEK_END) == 0) x.fileSize = (long long)ftell(f);
            fclose(f);
        }
    }
    std::stable_sort(r.begin(), r.end(), [](const Game& a, const Game& b) { return ShortName(a) < ShortName(b); });
}
// What can be done with the chosen ROM: show it in the list again (or hide it); on a PC, open its folder.
inline int RomActions()
{
#if defined(__SWITCH__) || defined(__ANDROID__)
    return 1;
#else
    return 2;
#endif
}
// One line of the settings file set (key=value), the rest kept: written to a .tmp and put in place
// (on the Switch's SD card the old file goes first: it does not rename over one).
inline bool SetConfigLine(const std::string& cfg, const std::string& key, const std::string& value)
{
    std::vector<std::string> lines;
    if (FILE* f = fopen(cfg.c_str(), "r"))
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
    bool found = false;
    for (std::string& l : lines)
        if (l.size() > key.size() && !l.compare(0, key.size(), key) && l[key.size()] == '=') { l = key + "=" + value; found = true; }
    if (!found) lines.push_back(key + "=" + value);
    const std::string tmp = cfg + ".tmp";
    FILE* f = fopen(tmp.c_str(), "w");
    if (!f) return false;
    bool ok = true;
    for (const std::string& l : lines) if (fprintf(f, "%s\n", l.c_str()) < 0) ok = false;
    ok = fclose(f) == 0 && ok;
    if (!ok) return false;
    if (rename(tmp.c_str(), cfg.c_str()) != 0) { remove(cfg.c_str()); if (rename(tmp.c_str(), cfg.c_str()) != 0) return false; }
    return true;
}
// Sort on the tab bar: the next order, and how it is named.
inline const char* NextSort(const std::string& now) { return now == "favourites" ? "recent" : now == "recent" ? "original" : "favourites"; }
inline const char* SortLabel(const std::string& k) { return k == "favourites" ? "Favourites First" : k == "recent" ? "Recently Played" : "Original"; }
// The tab bar's tools under a point (1 +, 2 Search, 3 Sort; 0 none).
inline int ToolAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.page >= 0 || y < f.toolY || y >= f.toolY + f.toolH) return 0;
    for (int k = 1; k < 4; k++)
        if (f.toolW[k] > 0 && x >= f.toolX[k] && x < f.toolX[k] + f.toolW[k]) return k;
    return 0;
}
// The keyboard's title, line and done key for what it is for (GlassFocus::kbFor).
inline const char* KbTitle() { const int k = Focus().kbFor; return k == 4 ? "TRADE CODE" : k == 1 ? "NEW TAB" : k == 2 ? "SEARCH" : k == 3 ? "RENAME TAB" : "RENAME"; }
inline const char* KbPrompt() { const int k = Focus().kbFor; return k == 4 ? "The Other Player's Code" : k == 1 || k == 3 ? "Tab Name" : k == 2 ? "Part Of A Game's Name" : "The Game's Own Name"; }
inline const char* KbDone() { const int k = Focus().kbFor; return k == 4 ? "Join" : k == 1 ? "Make Tab" : k == 2 ? "Search" : "Save"; }
// Add To Tab (the game's menu): the player's tabs, each marked if the game is in it, New Tab and Close.
inline void OpenAddToTab(const std::string& rom)
{
    GlassFocus& F = Focus();
    const std::vector<CustomTab>& tabs = CustomTabs();
    F.dTitle = "Add To Tab";
    F.dMsg = tabs.empty() ? "You have no tabs of your own yet. Make one and this game goes in it."
                          : "Choose a tab to put this game in, or to take it out.";
    F.dItems.clear();
    F.dPaths.clear();
    for (size_t k = 0; k < tabs.size() && k < 6; k++)
    {
        bool in = false;
        for (const std::string& r : tabs[k].roms) in = in || r == rom;
        F.dItems.push_back(tabs[k].name + (in ? "   \xC2\xB7   In This Tab" : ""));
        F.dPaths.push_back("#tab:" + std::to_string(k));
    }
    F.dItems.push_back("New Tab");
    F.dPaths.push_back("#new");
    F.dItems.push_back("Close");
    F.dPaths.push_back("#close");
    F.dialog = 8;
    F.drow = 0;
    F.kbRom = rom;
}
// A tab of the player's own (A on it): Rename, Move Left, Move Right, Delete Tab, Close.
inline void OpenTabMenu(int k)
{
    GlassFocus& F = Focus();
    if (k < 0 || k >= (int)CustomTabs().size()) return;
    F.dTitle = CustomTabs()[k].name;
    F.dMsg = "A tab of your own. Deleting it keeps its games.";
    F.dItems = { "Rename Tab", "Move Left", "Move Right", "Delete Tab", "Close" };
    F.dPaths = { "#rename", "#left", "#right", "#delete", "#close" };
    F.dialog = 9;
    F.drow = 0;
    F.kbTab = k;
}
// A row of those two chosen (pick: its path). tab: the tab shown, moved with its tab. Returns 1 when
// the dialog stays (made again), 2 when the keyboard opens (dialog 3, kbFor set), else 0 (closed).
inline int TabDialogPick(const std::string& pick, int& tab, std::string& note)
{
    GlassFocus& F = Focus();
    std::vector<CustomTab>& tabs = CustomTabs();
    auto upper = [](std::string s) { for (char& ch : s) ch = (char)toupper((unsigned char)ch); return s; };
    if (pick.rfind("#tab:", 0) == 0)
    {
        const int k = atoi(pick.c_str() + 5);
        if (k < 0 || k >= (int)tabs.size()) { F.dialog = 0; return 0; }
        std::vector<std::string>& roms = tabs[k].roms;
        const auto it = std::find(roms.begin(), roms.end(), F.kbRom);
        const bool was = it != roms.end();
        if (was) roms.erase(it);
        else roms.push_back(F.kbRom);
        note = !SaveTabs() ? "COULD NOT SAVE THE TABS" : was ? "TAKEN OUT OF " + upper(tabs[k].name) : "ADDED TO " + upper(tabs[k].name);
        const int keep = F.drow;
        OpenAddToTab(F.kbRom);
        F.drow = keep;
        return 1;
    }
    if (pick == "#new") { F.kbFor = 1; F.text.clear(); F.dialog = 3; F.drow = 0; return 2; }
    const int k = F.kbTab;
    if (k < 0 || k >= (int)tabs.size()) { F.dialog = 0; return 0; }
    if (pick == "#rename") { F.kbFor = 3; F.text = tabs[k].name; F.dialog = 3; F.drow = 0; return 2; }
    if (pick == "#left" || pick == "#right")
    {
        const int to = pick == "#left" ? k - 1 : k + 1;
        if (to < 0 || to >= (int)tabs.size()) return 1;
        std::swap(tabs[k], tabs[to]);
        if (tab == TABS + k) tab = TABS + to;
        else if (tab == TABS + to) tab = TABS + k;
        if (!SaveTabs()) note = "COULD NOT SAVE THE TABS";
        const int keep = F.drow;
        OpenTabMenu(to);
        F.drow = keep;
        return 1;
    }
    if (pick == "#delete")
    {
        tabs.erase(tabs.begin() + k);
        if (tab == TABS + k) tab = TAB_ALL;
        else if (tab > TABS + k) tab--;
        note = SaveTabs() ? "TAB DELETED. ITS GAMES ARE KEPT" : "COULD NOT SAVE THE TABS";
        F.dialog = 0;
        return 0;
    }
    F.dialog = 0;
    return 0;
}
// The keyboard's done key for anything but a game's name: a new tab (the game it came from put in it),
// the search ("" clears it), a tab's name. tab: the tab shown after.
inline void TradeEnterCode(const std::string& code); // (the Hub's Trade page, below)
inline void KeyboardDone(int& tab, std::string& note)
{
    GlassFocus& F = Focus();
    std::string t = F.text;
    while (!t.empty() && t.back() == ' ') t.pop_back();
    while (!t.empty() && t.front() == ' ') t.erase(0, 1);
    std::vector<CustomTab>& tabs = CustomTabs();
    if (F.kbFor == 1 && !t.empty())
    {
        CustomTab nt;
        nt.name = t;
        if (!F.kbRom.empty()) nt.roms.push_back(F.kbRom);
        tabs.push_back(nt);
        note = SaveTabs() ? "TAB MADE" : "COULD NOT SAVE THE TABS";
        tab = TabCount() - 1;
    }
    else if (F.kbFor == 2)
    {
        SearchText() = t;
        note = t.empty() ? "SEARCH CLEARED" : "";
    }
    else if (F.kbFor == 4) TradeEnterCode(t);
    else if (F.kbFor == 3 && !t.empty() && F.kbTab >= 0 && F.kbTab < (int)tabs.size())
    {
        tabs[F.kbTab].name = t;
        note = SaveTabs() ? "TAB RENAMED" : "COULD NOT SAVE THE TABS";
    }
    F.kbFor = 0;
    F.kbRom.clear();
    F.dialog = 0;
}
inline bool SortAt(int x, int y)
{
    const GlassFocus& f = Focus();
    return f.page < 0 && f.sortW > 0 && x >= f.sortX && x < f.sortX + f.sortW && y >= f.sortY && y < f.sortY + f.sortH;
}
inline const char* PlatformLabel();
// THE HUB PAGE: cloud saves (cloud_saves.h). Row 0 the account; then each game's save, with how it
// last synced. The network is used on a thread of its own, so the page keeps drawing; HubSig()
// changes whenever what the page shows does (the Switch redraws on that).
struct HubState
{
    std::mutex m;
    std::string dir;                        // where cloud.ini and cloud-state.txt are
    bool loaded = false;
    CloudSaves::Settings set;
    int busy = 0;                           // 0 nothing, 1 signing in, 2 syncing
    std::string code, url, msg;             // signing in: the code and where to type it; the last word
    std::map<std::string, CloudSaves::Result> results; // by the save's path
    std::map<std::string, std::string> synced;          // the saves in the cloud (cloud-state.txt): name, as last synced
    std::atomic<int> version{ 0 };
    std::atomic<bool> stop{ false }, cancel{ false };
    std::thread worker;
    ~HubState() { stop = true; if (worker.joinable()) worker.join(); }
};
inline HubState& Hub() { static HubState h; return h; }
struct HubGame { std::string name, path; const Game* g; };
// (A game's other folder as the old list wrote it, "IN BLACK2": just the folder.)
inline std::string WhereName(const std::string& w) { return w.rfind("IN ", 0) == 0 ? w.substr(3) : w; }
inline std::string HubSavePath(const Game& g)
{
    if (g.platform == GBA) return g.folder + "/" + g.saveName;
#ifdef __SWITCH__
    return g.folder + "/" + g.saveName;
#else
    return HubDir() + "/" + g.saveName;
#endif
}
// (The saves this device last synced, from cloud-state.txt: name, a tab, what it was. Lock held.)
inline void HubReadState(HubState& h)
{
    h.synced.clear();
    if (FILE* f = fopen((h.dir + "/cloud-state.txt").c_str(), "rb"))
    {
        char line[1024];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            const size_t t = l.find('\t');
            if (t != std::string::npos && t > 0) h.synced[l.substr(0, t)] = l.substr(t + 1);
        }
        fclose(f);
    }
}
// The games with a save of their own (the save's path: beside the ROM on the Switch, else the saves folder).
inline std::vector<HubGame> HubGames()
{
    std::vector<HubGame> r;
    for (const Game& g : RomList())
    {
        if (!g.known || g.saveName.empty()) continue;
        const std::string p = HubSavePath(g);
        bool dup = false;
        for (const HubGame& h : r) if (h.path == p) dup = true;
        if (!dup) r.push_back({ ShortName(g), p, &g });
    }
    return r;
}
inline void HubLoad()
{
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (h.loaded && h.dir == HubDir()) return;
    h.dir = HubDir();
    h.loaded = true;
    h.set = CloudSaves::LoadSettings(h.dir + "/cloud.ini");
    HubReadState(h);
    h.version++;
}
inline std::string HubDevice()
{
    const std::string p = PlatformLabel();
    return p == "Windows" ? "Windows PC" : p == "Apple" ? "Mac" : p == "Nintendo Switch" ? p : p + " Device";
}
inline bool HubOnLocked(const HubState& h) { return h.set.provider != "off" && !(h.set.provider == "google" && h.set.googleRefreshToken.empty()); }
inline const char* HubProvider(const std::string& p) { return p == "google" ? "Google Drive" : p == "webdav" ? "WebDAV" : p == "folder" ? "A Synced Folder" : "Off"; }
inline std::string HubSig() { return std::to_string(Hub().version.load()) + "." + std::to_string(HubLink::Version()); }
inline std::string SettingSentence(const std::string& s); // (below: capitals made a sentence)
// MOVE POKEMON (the Hub's second row): the Pokémon in a box of one of the player's games, and moving
// one to another of them (pokemon_hub.h: the rules, the conversion, both saves backed up first).
// Its rows: 0 the game they come from, 1 its box, 2 the game they go to, then that box's Pokémon.
const int HUB_FIRST = 1; // (Cloud Saves' first save row: after the account)
// (The Hub's page: 0 its first, Cloud Saves and Move Pokémon to open; 1 Cloud Saves. Move Pokémon: Mover().open.)
inline int& HubMode() { static int m = 0; return m; }
struct MoverGame { std::string name, path; bool hack; };
struct MoverState
{
    bool open = false;
    int from = 0, to = 1, box = 0;
    std::string msg;
#ifdef SSR_POKEMON_HUB
    std::unique_ptr<PokemonHub::Save> a, b;
    std::string aPath, bPath, aErr, bErr;
#endif
    // (Where a Pokémon goes in the other game: -1 the first free place, -2 the party, else a box.
    // Swapping: the Pokémon chosen in this game, while one is chosen in the other.)
    int put = -1;
    bool swapping = false;
    int swapBox = 0, swapSlot = 0;
    std::string swapName;
#ifdef SSR_POKEMON_HUB
#endif
};
inline MoverState& Mover() { static MoverState m; return m; }
inline std::vector<MoverGame> MoverGames()
{
    std::vector<MoverGame> r;
    for (const HubGame& g : HubGames()) if (g.g && g.g->hasSave) r.push_back({ g.name + (g.g->where.empty() ? std::string() : "  \xC2\xB7  " + WhereName(g.g->where)), g.path, IsHack(*g.g) });
    return r;
}
#ifdef SSR_POKEMON_HUB
// (The two saves open as chosen: opened again after a move, or when the choice changes.)
inline void MoverOpenSaves(bool again = false)
{
    MoverState& m = Mover();
    const std::vector<MoverGame> gs = MoverGames();
    if (gs.empty()) { m.a.reset(); m.b.reset(); m.aPath.clear(); m.bPath.clear(); return; }
    m.from = std::max(0, std::min(m.from, (int)gs.size() - 1));
    m.to = std::max(0, std::min(m.to, (int)gs.size() - 1));
    if (again || !m.a || m.aPath != gs[m.from].path) { m.aPath = gs[m.from].path; m.aErr.clear(); m.a = PokemonHub::Save::Open(m.aPath, m.aErr, gs[m.from].hack); }
    if (again || !m.b || m.bPath != gs[m.to].path) { m.bPath = gs[m.to].path; m.bErr.clear(); m.b = PokemonHub::Save::Open(m.bPath, m.bErr, gs[m.to].hack); }
    if (m.a && m.box != PokemonHub::PARTY) m.box = std::max(0, std::min(m.box, m.a->Boxes() - 1));
    if (m.b && m.put >= 0) m.put = std::min(m.put, m.b->Boxes() - 1);
}
#endif
#ifdef SSR_POKEMON_HUB
// The Pokémon's box icons, taken from one of the player's own DS ROMs as the companion takes them
// (companion_rom.h): a Black / White (2) ROM has the first five generations' 649, a Gen 4 one 493.
// Only the parts needed are read from the file, never all of it. Later Pokémon: none (a Poké Ball).
inline bool RomPart(FILE* f, uint32_t at, uint32_t n, std::vector<uint8_t>& out)
{
    out.resize(n);
    return n == 0 || (fseek(f, (long)at, SEEK_SET) == 0 && fread(out.data(), 1, n, f) == n);
}
inline std::vector<uint8_t> RomFileFrom(FILE* f, std::string path)
{
    std::vector<uint8_t> h, fnt, fat;
    auto U32 = [](const std::vector<uint8_t>& b, size_t o) { return (uint32_t)b[o] | (uint32_t)b[o + 1] << 8 | (uint32_t)b[o + 2] << 16 | (uint32_t)b[o + 3] << 24; };
    auto U16 = [](const std::vector<uint8_t>& b, size_t o) { return (uint16_t)(b[o] | b[o + 1] << 8); };
    if (!RomPart(f, 0, 0x200, h)) return {};
    if (U32(h, 0x44) > (1u << 20) || U32(h, 0x4C) > (1u << 20)) return {};
    if (!RomPart(f, U32(h, 0x40), U32(h, 0x44), fnt) || !RomPart(f, U32(h, 0x48), U32(h, 0x4C), fat)) return {};
    uint16_t dir = 0xF000;
    for (int depth = 0; depth < 32; depth++)
    {
        const size_t entry = (size_t)(dir & 0xFFF) * 8, slash = path.find('/');
        if (entry + 8 > fnt.size()) return {};
        size_t pos = U32(fnt, entry);
        uint32_t id = U16(fnt, entry + 4);
        bool down = false;
        while (pos < fnt.size())
        {
            const int tag = fnt[pos++];
            if (!tag) break;
            const size_t n = tag & 127;
            if (pos + n > fnt.size()) return {};
            const std::string name((const char*)fnt.data() + pos, n);
            pos += n;
            if (tag & 128)
            {
                if (pos + 2 > fnt.size()) return {};
                const uint16_t sub = U16(fnt, pos);
                pos += 2;
                if (slash != std::string::npos && name == path.substr(0, slash)) { dir = sub; path = path.substr(slash + 1); down = true; break; }
            }
            else
            {
                if (slash == std::string::npos && name == path)
                {
                    if ((size_t)id * 8 + 8 > fat.size()) return {};
                    const uint32_t b = U32(fat, id * 8), e = U32(fat, id * 8 + 4);
                    std::vector<uint8_t> out;
                    if (e < b || e - b > (32u << 20) || !RomPart(f, b, e - b, out)) return {};
                    return out;
                }
                id++;
            }
        }
        if (!down) return {};
    }
    return {};
}
struct MonIconSet { bool tried = false; Companion::Metadata meta; };
inline MonIconSet& MonIcons() { static MonIconSet m; return m; }
inline bool MonIcon(int species, bool egg, std::vector<uint32_t>& out)
{
    MonIconSet& I = MonIcons();
    if (!I.tried)
    {
        I.tried = true;
        const Game* pick = nullptr;
        bool gen5 = false;
        for (const Game& g : RomList())
        {
            if (!g.known || g.platform != DS || g.code.size() < 4) continue;
            const bool five = g.code[0] == 'I' && g.code[1] == 'R';
            const bool four = g.code.rfind("IPK", 0) == 0 || g.code.rfind("IPG", 0) == 0 || g.code.rfind("CPU", 0) == 0;
            if (five && !gen5) { pick = &g; gen5 = true; }
            else if (four && !pick) pick = &g;
        }
        if (pick)
            if (FILE* f = fopen(pick->rom.c_str(), "rb"))
            {
                const bool hg = pick->code.rfind("IPK", 0) == 0 || pick->code.rfind("IPG", 0) == 0;
                Companion::Metadata& m = I.meta;
                m.gen5 = gen5;
                m.monIcons = Companion::Narc(RomFileFrom(f, gen5 ? "a/0/0/7" : hg ? "a/0/2/0" : "poketool/icongra/pl_poke_icon.narc"));
                if (!m.monIcons.empty()) m.iconPalettes = CompanionIcons::Palettes(m.monIcons[0]);
                if (gen5 && !m.monIcons.empty())
                {
                    // (The ARM9, for the table of each Pokémon's palette: the start of the ROM up to its end.)
                    std::vector<uint8_t> h;
                    if (RomPart(f, 0, 0x200, h))
                    {
                        const uint32_t off = h[0x20] | h[0x21] << 8 | h[0x22] << 16 | (uint32_t)h[0x23] << 24;
                        const uint32_t size = h[0x2C] | h[0x2D] << 8 | h[0x2E] << 16 | (uint32_t)h[0x2F] << 24;
                        std::vector<uint8_t> head;
                        if (size <= 0x400000 && off <= 0x100000 && RomPart(f, 0, off + size, head)) m.FindGen5IconPalettes(Companion::RomArm9(head));
                    }
                    for (size_t i = 0; i < m.monIcons.size() && i < 16; i++)
                        if (m.monIcons[i].size() > 4 && !memcmp(m.monIcons[i].data(), "RGCN", 4)) { m.gen5IconStart = (int)i; break; }
                }
                fclose(f);
                fprintf(stderr, "[hub] Pokemon icons from %s: %zu\n", pick->rom.c_str(), m.monIcons.size());
            }
    }
    return I.meta.MonIcon(species, egg, out);
}
inline void DrawMonIcon(Canvas& c, const std::vector<uint32_t>& ic, int x, int y, int size)
{
    if (ic.size() < 32 * 32 || size <= 0) return;
    for (int j = 0; j < size; j++)
    {
        const int yy = y + j;
        if (yy < 0 || yy >= c.h) continue;
        for (int i = 0; i < size; i++)
        {
            const int xx = x + i;
            if (xx < 0 || xx >= c.w) continue;
            const uint32_t p = ic[(size_t)(j * 32 / size) * 32 + (i * 32 / size)];
            if (p >> 24) c.px[(size_t)yy * c.w + xx] = p | 0xFF000000u;
        }
    }
}
// "SWAMPERT" as "Swampert" (and "MR. MIME" as "Mr. Mime", "HO-OH" as "Ho-Oh").
inline std::string MonName(const std::string& up)
{
    std::string s;
    bool start = true;
    for (char ch : up)
    {
        const unsigned char b = (unsigned char)ch;
        s += b < 128 ? (char)(start ? toupper(b) : tolower(b)) : ch;
        start = b == ' ' || b == '-' || b == '.';
    }
    return s;
}
#endif
// (Names from a save as the font can show them: Japanese ones, and empty ones, give way.)
inline bool Drawable(const std::string& s)
{
    bool any = false;
    for (size_t i = 0; i < s.size();)
    {
        const unsigned char b = (unsigned char)s[i];
        uint32_t cp = b;
        int n = 1;
        if (b >= 0xF0) { n = 4; cp = b & 7; } else if (b >= 0xE0) { n = 3; cp = b & 15; } else if (b >= 0xC0) { n = 2; cp = b & 31; }
        for (int k = 1; k < n && i + k < s.size(); k++) cp = cp << 6 | ((unsigned char)s[i + k] & 63);
        if (cp >= 0x2000 || cp < 0x20) return false;
        if (cp != ' ') any = true;
        i += n;
    }
    return any;
}
inline std::string Shown(const std::string& s, const std::string& otherwise) { return Drawable(s) ? s : otherwise; }
// (Mover rows: 0 the game they come from, 1 its box (or party), 2 the game they go to, 3 where they
// go in it (or, swapping, which of its boxes to choose from), then the Pokémon.)
const int MOVER_FIRST = 4;
#ifdef SSR_POKEMON_HUB
inline std::vector<PokemonHub::Mon> MonsIn(const PokemonHub::Save* s, int box)
{
    if (!s) return {};
    return box == PokemonHub::PARTY ? PokemonHub::Party(*s) : s->Box(std::max(0, box));
}
inline std::vector<PokemonHub::Mon> MoverBox()
{
    MoverOpenSaves();
    return MonsIn(Mover().a.get(), Mover().box);
}
inline int MoverTargetBox()
{
    const int p = Mover().put;
    return p == PokemonHub::TO_PARTY ? PokemonHub::PARTY : std::max(0, p);
}
// The Pokémon listed: those in the chosen box, or (swapping) those in the other game's.
inline std::vector<PokemonHub::Mon> MoverList()
{
    MoverOpenSaves();
    MoverState& m = Mover();
    return m.swapping ? MonsIn(m.b.get(), MoverTargetBox()) : MonsIn(m.a.get(), m.box);
}
inline std::string BoxLabel(const PokemonHub::Save* s, int box)
{
    if (box == PokemonHub::PARTY) return "Party";
    return s ? Shown(s->BoxName(box), "Box " + std::to_string(box + 1)) : std::string("-");
}
inline std::string PutLabel()
{
    MoverState& m = Mover();
    if (m.put == PokemonHub::ANY_BOX) return "First Free Place";
    if (m.put == PokemonHub::TO_PARTY) return "Party";
    return BoxLabel(m.b.get(), m.put);
}
// (A box's next, its previous, or the party, in a game of n boxes.)
inline int StepBox(int box, int n, int how)
{
    if (n <= 0) return 0;
    if (how == 2) return PokemonHub::PARTY;
    if (box < 0) return how == 0 ? 0 : n - 1;
    return how == 0 ? (box + 1) % n : (box + n - 1) % n;
}
#endif
inline int MoverRows()
{
#ifdef SSR_POKEMON_HUB
    return MOVER_FIRST + (int)MoverList().size();
#else
    return 3;
#endif
}
inline std::vector<std::string> MoverActions(int row)
{
    std::vector<std::string> r;
    const std::vector<MoverGame> gs = MoverGames();
    if (row == 0 || row == 2)
    {
        for (size_t i = 0; i < gs.size() && i < 7; i++) r.push_back(gs[i].name);
        return r;
    }
#ifdef SSR_POKEMON_HUB
    MoverState& m = Mover();
    if (row == 1) return { "Next Box", "Previous Box", "Party" };
    if (row == 3) return m.swapping ? std::vector<std::string>{ "Next Box", "Previous Box", "Party" }
                                    : std::vector<std::string>{ "First Free Place", "Party", "Next Box", "Previous Box" };
    const std::vector<PokemonHub::Mon> mons = MoverList();
    const int i = row - MOVER_FIRST;
    if (i < 0 || i >= (int)mons.size() || !m.a || !m.b || m.from == m.to) return m.swapping ? std::vector<std::string>{ "Cancel Swap" } : r;
    const std::string toName = gs[std::min(m.to, (int)gs.size() - 1)].name;
    if (m.swapping)
    {
        const PokemonHub::Check ck = PokemonHub::CanSwap(*m.a, m.swapBox, m.swapSlot, *m.b, mons[i].box, mons[i].slot);
        if (ck.verdict == PokemonHub::Verdict::Ok)
        {
            r.push_back("Swap");
            if (ck.evolvesTo) r.push_back("Swap And Evolve");
        }
        r.push_back("Cancel Swap");
        return r;
    }
    const PokemonHub::Check ck = PokemonHub::CanMoveTo(*m.a, mons[i].box, mons[i].slot, *m.b, m.put, -1);
    if (ck.verdict == PokemonHub::Verdict::Ok)
    {
        r.push_back("Move To " + toName);
        if (ck.evolvesTo) r.push_back("Move And Evolve");
    }
    if (m.a->Generation() == m.b->Generation()) r.push_back("Swap With One In " + toName);
#endif
    return r;
}
inline std::string MoverDo(int row, int act)
{
    MoverState& m = Mover();
    const std::vector<std::string> acts = MoverActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    Hub().version++;
    if (row == 0) { m.from = act; m.box = 0; m.swapping = false; m.msg.clear(); return ""; }
    if (row == 2) { m.to = act; m.put = -1; m.swapping = false; m.msg.clear(); return ""; }
#ifdef SSR_POKEMON_HUB
    MoverOpenSaves();
    const std::string a = acts[act];
    if (row == 1)
    {
        if (m.a) m.box = StepBox(m.box, m.a->Boxes(), a == "Next Box" ? 0 : a == "Previous Box" ? 1 : 2);
        return "";
    }
    if (row == 3)
    {
        if (a == "First Free Place") m.put = PokemonHub::ANY_BOX;
        else if (a == "Party") m.put = PokemonHub::TO_PARTY;
        else if (m.b) m.put = StepBox(m.put == PokemonHub::TO_PARTY || m.put == PokemonHub::ANY_BOX ? -1 : m.put, m.b->Boxes(), a == "Next Box" ? 0 : 1);
        return "";
    }
    const std::vector<PokemonHub::Mon> mons = MoverList();
    const int i = row - MOVER_FIRST;
    if (a == "Cancel Swap") { m.swapping = false; m.put = -1; m.msg.clear(); Focus().pact = -1; return ""; }
    if (i < 0 || i >= (int)mons.size() || !m.a || !m.b) return "";
    const std::vector<MoverGame> gs = MoverGames();
    const std::string toName = gs[std::min(m.to, (int)gs.size() - 1)].name;
    auto nameOf = [](const PokemonHub::Mon& mon) {
        const std::string n = Shown(mon.nickname, MonName(mon.speciesName));
        return MonName(n) == MonName(mon.speciesName) ? MonName(mon.speciesName) : n + " (" + MonName(mon.speciesName) + ")";
    };
    std::string err;
    std::vector<std::string> backups;
    if (a.rfind("Swap With", 0) == 0)
    {
        m.swapping = true;
        m.swapBox = mons[i].box; m.swapSlot = mons[i].slot; m.swapName = nameOf(mons[i]);
        if (m.put == PokemonHub::ANY_BOX) m.put = 0;
        m.msg = "Now choose the Pok\xC3\xA9mon in " + toName + " to swap for " + m.swapName + ".";
        Focus().prow = MOVER_FIRST; Focus().pact = -1; Focus().rScroll = 0;
        return "";
    }
    if (m.swapping)
    {
        const std::string other = nameOf(mons[i]);
        if (!PokemonHub::Swap(*m.a, m.swapBox, m.swapSlot, *m.b, mons[i].box, mons[i].slot, a == "Swap And Evolve", err)
            || !PokemonHub::Commit(*m.a, *m.b, err, &backups))
            m.msg = "Could not swap them. " + err;
        else
            m.msg = m.swapName + " and " + other + " swapped. Both saves were backed up first.";
        m.swapping = false;
        m.put = -1;
        Focus().pact = -1;
        MoverOpenSaves(true);
        Hub().version++;
        return "";
    }
    const std::string name = nameOf(mons[i]);
    if (!PokemonHub::Move(*m.a, mons[i].box, mons[i].slot, *m.b, a == "Move And Evolve", err, m.put, -1)
        || !PokemonHub::Commit(*m.a, *m.b, err, &backups))
        m.msg = "Could not move it. " + err;
    else
        m.msg = name + " moved to " + toName + " (" + PutLabel() + "). Both saves were backed up first.";
    MoverOpenSaves(true);
    Hub().version++;
#endif
    return "";
}
// TRADE OVER WI-FI (the Hub's third page, hub_link.h): one of the player's games and its box, the
// link to the other device, then the box's Pokémon to offer or send. Rows: 0 the game, 1 the box,
// 2 the link (and what the other side offers), then the Pokémon.
const int TRADE_FIRST = 3;
struct TradeState
{
    int game = 0, box = 0;
    std::string msg;
    int offerBox = 0, offerSlot = 0, offerSpecies = 0; // (where this side's offer came from)
    std::string checked;                               // (the other side's offer last checked)
#ifdef SSR_POKEMON_HUB
    std::unique_ptr<PokemonHub::Save> save;
    std::string path, err;
#endif
};
inline TradeState& Trade() { static TradeState t; return t; }
#ifdef SSR_POKEMON_HUB
inline void TradeOpenSave(bool again = false)
{
    TradeState& t = Trade();
    const std::vector<MoverGame> gs = MoverGames();
    if (gs.empty()) { t.save.reset(); t.path.clear(); return; }
    t.game = std::max(0, std::min(t.game, (int)gs.size() - 1));
    if (again || !t.save || t.path != gs[t.game].path) { t.path = gs[t.game].path; t.err.clear(); t.save = PokemonHub::Save::Open(t.path, t.err, gs[t.game].hack); }
    if (t.save && t.box != PokemonHub::PARTY) t.box = std::max(0, std::min(t.box, t.save->Boxes() - 1));
}
inline std::vector<PokemonHub::Mon> TradeList() { TradeOpenSave(); return MonsIn(Trade().save.get(), Trade().box); }
inline PokemonHub::Packed TheirPacked(const HubLink::Offer& o)
{
    PokemonHub::Packed p;
    p.generation = o.generation;
    p.data = o.data;
    p.game = o.game;
    return p;
}
// (Each time things change: the other side's offer checked against this game, and, once both have
// said yes, this side's save written.)
inline void TradeTick()
{
    TradeState& t = Trade();
    const HubLink::Status s = HubLink::Snapshot();
    if (s.state != HubLink::State::Linked) return;
    TradeOpenSave();
    if (s.theirs.any)
    {
        const std::string key = std::to_string(s.theirs.data.size()) + "/" + std::to_string(s.theirs.speciesId) + "/" + s.theirs.name + "/" + t.path;
        if (key != t.checked && t.save)
        {
            t.checked = key;
            const PokemonHub::Check ck = PokemonHub::CanReceive(TheirPacked(s.theirs), *t.save, s.mine.any ? t.offerSpecies : 0);
            HubLink::TellCheck(ck.verdict == PokemonHub::Verdict::Ok, ck.reason);
        }
    }
    else t.checked.clear();
    if (!s.finish || !t.save) return;
    std::string err;
    std::vector<std::string> backups;
    bool ok = true;
    if (s.theirs.any) ok = PokemonHub::Receive(*t.save, TheirPacked(s.theirs), true, s.mine.any ? t.offerSpecies : 0, err);
    if (ok && s.mine.any) ok = PokemonHub::Remove(*t.save, t.offerBox, t.offerSlot, err);
    if (ok) ok = PokemonHub::Commit(*t.save, err, &backups);
    const std::string got = s.theirs.any ? s.theirs.name : std::string();
    t.msg = !ok ? "It could not be written: " + err
          : s.mine.any && s.theirs.any ? "Traded! " + got + " arrived. The save was backed up first."
          : s.theirs.any ? got + " arrived. The save was backed up first." : "Sent. The save was backed up first.";
    HubLink::FinishDone(ok, ok ? (s.theirs.any ? "Done: " + got + " arrived." : "Done.") : err);
    TradeOpenSave(true);
    Hub().version++;
}
#endif
// (The relay's address: fulldex-relay.txt beside the settings, for a test, else the one built in.)
inline std::string TradeRelay()
{
    if (FILE* f = fopen((HubDir() + "/fulldex-relay.txt").c_str(), "rb"))
    {
        char b[256] = {};
        if (!fgets(b, sizeof b, f)) b[0] = 0;
        fclose(f);
        std::string r = b;
        while (!r.empty() && isspace((unsigned char)r.back())) r.pop_back();
        if (!r.empty()) return r;
    }
#ifdef FULLDEX_RELAY_URL
    return FULLDEX_RELAY_URL;
#else
    return "";
#endif
}
inline std::string TradeGameName()
{
    const std::vector<MoverGame> gs = MoverGames();
    return gs.empty() ? std::string() : gs[std::max(0, std::min(Trade().game, (int)gs.size() - 1))].name;
}
inline std::string TradeTrainer();
inline void TradeEnterCode(const std::string& code)
{
    if (code.empty()) return;
    HubLink::Online(TradeRelay(), code, TradeTrainer(), TradeGameName());
    Hub().version++;
}
inline int TradeRows()
{
#ifdef SSR_POKEMON_HUB
    return TRADE_FIRST + (int)TradeList().size();
#else
    return TRADE_FIRST;
#endif
}
inline std::string TradeTrainer()
{
#ifdef SSR_POKEMON_HUB
    TradeOpenSave();
    if (Trade().save) return Shown(Trade().save->Trainer(), "A Trainer");
#endif
    return "A Trainer";
}
inline std::vector<std::string> TradeActions(int row)
{
    std::vector<std::string> r;
    const std::vector<MoverGame> gs = MoverGames();
    const HubLink::Status s = HubLink::Snapshot();
    if (row == 0)
    {
        if (s.state == HubLink::State::Linked) return r; // (not while linked: the game is in the trade)
        for (size_t i = 0; i < gs.size() && i < 7; i++) r.push_back(gs[i].name);
        return r;
    }
    if (row == 1) return { "Next Box", "Previous Box", "Party" };
    if (row == 2)
    {
        switch (s.state)
        {
        case HubLink::State::Off: return { "Open A Room", "Join A Room", "Make An Online Code", "Enter An Online Code" };
        case HubLink::State::Online: return { "Stop" };
        case HubLink::State::Hosting: return { "Close The Room" };
        case HubLink::State::Browsing:
            for (size_t i = 0; i < s.rooms.size() && i < 5; i++) r.push_back("Join " + s.rooms[i].name + " (" + s.rooms[i].game + ")");
            r.push_back("Stop Looking");
            return r;
        case HubLink::State::Linked:
            if (s.theirs.any && !s.iAccepted)
            {
                if (s.theirs.gift && !s.mine.any) r.push_back("Accept The Gift");
                else if (!s.theirs.gift && s.mine.any && s.theyCanTake) r.push_back("Accept The Trade");
            }
            if (s.iAccepted) r.push_back("Not Yet");
            if (s.mine.any) r.push_back("Take Back My Offer");
            r.push_back("Leave");
            return r;
        }
        return r;
    }
#ifdef SSR_POKEMON_HUB
    if (s.state != HubLink::State::Linked) return r;
    const std::vector<PokemonHub::Mon> mons = TradeList();
    const int i = row - TRADE_FIRST;
    if (i < 0 || i >= (int)mons.size() || !Trade().save) return r;
    if (PokemonHub::CanRemove(*Trade().save, mons[i].box, mons[i].slot).verdict != PokemonHub::Verdict::Ok) return r;
    r.push_back("Offer For A Trade");
    r.push_back("Send As A Gift");
#endif
    return r;
}
inline std::string TradeDo(int row, int act)
{
    TradeState& t = Trade();
    const std::vector<std::string> acts = TradeActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    const std::string a = acts[act];
    const std::vector<MoverGame> gs = MoverGames();
    const HubLink::Status s = HubLink::Snapshot();
    Hub().version++;
    if (row == 0) { t.game = act; t.box = 0; return ""; }
#ifdef SSR_POKEMON_HUB
    TradeOpenSave();
    if (row == 1) { if (t.save) t.box = StepBox(t.box, t.save->Boxes(), a == "Next Box" ? 0 : a == "Previous Box" ? 1 : 2); return ""; }
    const std::string game = gs.empty() ? std::string() : gs[std::min(t.game, (int)gs.size() - 1)].name;
    if (row == 2)
    {
        if (a == "Open A Room") HubLink::Host(TradeTrainer(), game);
        else if (a == "Join A Room") HubLink::Browse(TradeTrainer(), game);
        else if (a == "Close The Room" || a == "Stop Looking" || a == "Leave" || a == "Stop") HubLink::Leave();
        else if ((a == "Make An Online Code" || a == "Enter An Online Code") && TradeRelay().empty()) t.msg = "Online trades are not set up in this build of Fulldex.";
        else if (a == "Make An Online Code") HubLink::Online(TradeRelay(), HubLink::NewCode(), TradeTrainer(), game);
        else if (a == "Enter An Online Code") { GlassFocus& K = Focus(); K.kbFor = 4; K.text.clear(); K.dialog = 3; K.drow = 0; t.msg.clear(); }
        else if (a.rfind("Join ", 0) == 0 && act < (int)s.rooms.size()) HubLink::Join(s.rooms[act]);
        else if (a == "Accept The Gift" || a == "Accept The Trade") HubLink::Accept(true);
        else if (a == "Not Yet") HubLink::Accept(false);
        else if (a == "Take Back My Offer") HubLink::SetOffer(HubLink::Offer());
        Focus().pact = -1;
        return "";
    }
    const std::vector<PokemonHub::Mon> mons = TradeList();
    const int i = row - TRADE_FIRST;
    if (i < 0 || i >= (int)mons.size() || !t.save) return "";
    PokemonHub::Packed p;
    std::string err;
    if (!PokemonHub::Pack(*t.save, mons[i].box, mons[i].slot, p, err)) { t.msg = "It cannot be sent. " + err; return ""; }
    HubLink::Offer o;
    o.any = true;
    o.gift = a == "Send As A Gift";
    o.generation = p.generation;
    o.data = p.data;
    o.name = Shown(mons[i].nickname, MonName(mons[i].speciesName));
    o.species = MonName(mons[i].speciesName);
    o.game = game;
    o.speciesId = mons[i].species;
    o.level = mons[i].level;
    o.shiny = mons[i].shiny;
    t.offerBox = mons[i].box; t.offerSlot = mons[i].slot; t.offerSpecies = mons[i].species;
    HubLink::SetOffer(o);
    t.msg.clear();
    Focus().prow = 2; Focus().pact = -1;
#endif
    return "";
}
inline void MoverShow(bool open)
{
    MoverState& m = Mover();
    m.open = open;
    m.msg.clear();
    m.swapping = false;
    if (open && m.from == m.to) m.to = m.from == 0 ? 1 : 0;
    Hub().version++;
}
// A game's save and the cloud, for Home: 0 not there (or cloud saves off), 1 kept in the cloud, 2 changed on both.
inline int HubCloudOf(const Game& g)
{
    if (g.saveName.empty() || !g.known) return 0;
    HubLoad();
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (!HubOnLocked(h)) return 0;
    const auto it = h.results.find(HubSavePath(g));
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Conflict) return 2;
    return h.synced.count(g.saveName) ? 1 : 0;
}
// A small cloud, centred on cx, cy; s half its width.
inline void DrawCloud(Canvas& c, float cx, float cy, float s, uint32_t col)
{
    Glass::Round(c, (int)(cx - s), (int)(cy - s * 0.05f), (int)(2 * s), (int)(s * 0.75f), s * 0.37f, col);
    Glass::Round(c, (int)(cx - s * 0.7f), (int)(cy - s * 0.5f), (int)(s * 0.85f), (int)(s * 0.85f), s * 0.42f, col);
    Glass::Round(c, (int)(cx - s * 0.15f), (int)(cy - s * 0.8f), (int)(s * 1.0f), (int)(s * 1.0f), s * 0.5f, col);
}
// A on the Hub's first page: Cloud Saves or Move Pokémon opened. (False: not that page.)
inline bool HubEnter(int row)
{
    if (Mover().open || HubMode() != 0) return false;
    GlassFocus& F = Focus();
    if (row == 0) { HubMode() = 1; F.prow = 0; }
    else if (row == 2) { HubMode() = 2; F.prow = 2; }
    else { MoverShow(true); F.prow = MOVER_FIRST; }
    F.pact = -1; F.rScroll = 0;
    Hub().version++;
    return true;
}
inline bool HubTop() { return !Mover().open && HubMode() == 0; }
inline int HubRows() { return Mover().open ? MoverRows() : HubMode() == 0 ? 3 : HubMode() == 2 ? TradeRows() : HUB_FIRST + (int)HubGames().size(); }
inline std::vector<std::string> HubActions(int row)
{
    if (Mover().open) return MoverActions(row);
    if (HubMode() == 2) return TradeActions(row);
    if (HubMode() == 0) return {};
    HubLoad();
    const std::vector<HubGame> games = HubGames();
    HubState& h = Hub();
    std::lock_guard<std::mutex> l(h.m);
    if (row <= 0)
    {
        if (h.busy == 1) return { "Cancel" };
        if (h.busy) return {};
        if (!HubOnLocked(h)) return { "Sign In With Google" };
        return { "Sync All Now", "Sign Out" };
    }
    if (!HubOnLocked(h) || h.busy || row - HUB_FIRST >= (int)games.size()) return {};
    const auto it = h.results.find(games[row - HUB_FIRST].path);
    if (it != h.results.end() && it->second.outcome == CloudSaves::Outcome::Conflict) return { "Keep This Device's", "Keep The Cloud's" };
    return { "Sync Now" };
}
// (On the worker: each save in turn; resolve -1 a sync, 0 keep this device's, 1 the cloud's.)
inline void HubSyncJob(std::vector<std::string> paths, int resolve)
{
    HubState& h = Hub();
    CloudSaves::Settings s;
    std::string dir;
    { std::lock_guard<std::mutex> l(h.m); s = h.set; dir = h.dir; }
    std::unique_ptr<CloudSaves::Sync> sync = CloudSaves::FromSettings(s, dir + "/cloud-state.txt", HubDevice());
    int up = 0, down = 0, both = 0, failed = 0;
    for (const std::string& p : paths)
    {
        if (h.stop) break;
        CloudSaves::Result r;
        if (!sync) { r.outcome = CloudSaves::Outcome::Off; r.message = "CLOUD SAVES ARE OFF"; }
        else if (resolve >= 0) r = sync->Resolve(p, resolve == 1);
        else
        {
            r = sync->BeforePlay(p);
            if (r.outcome == CloudSaves::Outcome::InSync) r = sync->AfterPlay(p);
        }
        up += r.outcome == CloudSaves::Outcome::Pushed;
        down += r.outcome == CloudSaves::Outcome::Pulled;
        both += r.outcome == CloudSaves::Outcome::Conflict;
        failed += r.outcome == CloudSaves::Outcome::Failed;
        { std::lock_guard<std::mutex> l(h.m); h.results[p] = r; }
        h.version++;
    }
    std::lock_guard<std::mutex> l(h.m);
    h.busy = 0;
    HubReadState(h);
    if (!sync) h.msg = "Cloud saves are not set up.";
    else if (failed) h.msg = "Could not reach " + std::string(HubProvider(s.provider)) + ". The saves here are kept, and sync next time.";
    else
    {
        h.msg = "Synced.";
        if (up) h.msg += " " + std::to_string(up) + " sent up.";
        if (down) h.msg += " " + std::to_string(down) + " brought down.";
        if (both) h.msg += " " + std::to_string(both) + (both == 1 ? " changed on both: choose which to keep." : " changed on both: choose which to keep.");
        if (!up && !down && !both) h.msg += " Everything was already up to date.";
    }
    h.version++;
}
inline void HubSignInJob(std::vector<std::string> paths)
{
    HubState& h = Hub();
    CloudSaves::GoogleClient client;
    { std::lock_guard<std::mutex> l(h.m); client = h.set.google.id.empty() ? CloudSaves::BuiltInGoogleClient() : h.set.google; }
    auto done = [&](const std::string& m) {
        std::lock_guard<std::mutex> l(h.m);
        h.busy = 0; h.code.clear(); h.url.clear(); h.msg = m;
        h.version++;
    };
    if (client.id.empty()) { done("This build of Fulldex has no Google sign-in built in."); return; }
    CloudSaves::SignIn in;
    std::string err, refresh;
    if (!CloudSaves::GoogleSignInStart(client, in, err)) { done("Could not reach Google. Check the internet connection and try again."); return; }
    {
        std::lock_guard<std::mutex> l(h.m);
        h.code = in.userCode; h.url = in.verificationUrl; h.msg.clear();
        h.version++;
    }
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(in.expiresIn);
    while (!h.stop && !h.cancel && std::chrono::steady_clock::now() < end)
    {
        for (int i = 0; i < in.interval * 10 && !h.stop && !h.cancel; i++) std::this_thread::sleep_for(std::chrono::milliseconds(100));
        if (h.stop || h.cancel) break;
        const CloudSaves::SignInState st = CloudSaves::GoogleSignInPoll(client, in, refresh, err);
        if (st == CloudSaves::SignInState::Done)
        {
            {
                std::lock_guard<std::mutex> l(h.m);
                h.set.provider = "google";
                h.set.googleRefreshToken = refresh;
                if (h.set.deviceName.empty()) h.set.deviceName = HubDevice();
                CloudSaves::SaveSettings(h.dir + "/cloud.ini", h.set);
                h.busy = 2; h.code.clear(); h.url.clear(); h.msg = "Signed in. Syncing your saves...";
                h.version++;
            }
            HubSyncJob(paths, -1);
            return;
        }
        if (st == CloudSaves::SignInState::SlowDown) in.interval += 5;
        else if (st == CloudSaves::SignInState::Denied) { done("Sign-in was turned down on the phone."); return; }
        else if (st != CloudSaves::SignInState::Waiting) { done(st == CloudSaves::SignInState::Expired ? "The code ran out. Choose Sign In again for a new one." : "Sign-in did not work. Try again."); return; }
    }
    done(h.cancel ? "Sign-in cancelled." : "The code ran out. Choose Sign In again for a new one.");
}
inline void HubStart(std::function<void()> job)
{
    HubState& h = Hub();
    if (h.worker.joinable()) h.worker.join(); // (the last one has finished: busy was 0)
    h.worker = std::thread(std::move(job));
}
// A on one of the Hub's actions (row, its action). What to say, if anything ("" for nothing).
inline std::string HubDo(int row, int act)
{
    if (Mover().open) return MoverDo(row, act);
    if (HubMode() == 2) return TradeDo(row, act);
    const std::vector<std::string> acts = HubActions(row);
    if (act < 0 || act >= (int)acts.size()) return "";
    const std::string a = acts[act];
    HubState& h = Hub();
    std::vector<std::string> paths;
    for (const HubGame& g : HubGames()) paths.push_back(g.path);
    if (a == "Cancel") { h.cancel = true; return ""; }
    std::lock_guard<std::mutex> l(h.m);
    if (h.busy) return "";
    if (a == "Sign In With Google")
    {
        h.busy = 1; h.cancel = false; h.msg = "Asking Google for a code..."; h.version++;
        HubStart([paths] { HubSignInJob(paths); });
        return "";
    }
    if (a == "Sign Out")
    {
        h.set.provider = "off";
        h.set.googleRefreshToken.clear();
        const bool ok = CloudSaves::SaveSettings(h.dir + "/cloud.ini", h.set);
        h.results.clear();
        h.msg = ok ? "Signed out. Your saves stay here, and in your Drive." : "Could not save the cloud settings.";
        h.version++;
        return "";
    }
    if (row >= HUB_FIRST && row - HUB_FIRST < (int)paths.size()) paths = { paths[row - HUB_FIRST] };
    const int resolve = a == "Keep The Cloud's" ? 1 : a == "Keep This Device's" ? 0 : -1;
    h.busy = 2; h.msg = "Syncing..."; h.version++;
    HubStart([paths, resolve] { HubSyncJob(paths, resolve); });
    return "";
}
// (A save's time as shown: "6 Nov, 17:04".)
inline std::string HubWhen(int64_t t)
{
    if (t <= 0) return "-";
    const time_t tt = (time_t)t;
    const struct tm* tm = std::localtime(&tt);
    if (!tm) return "-";
    char b[32];
    strftime(b, sizeof b, "%d %b, %H:%M", tm);
    return b[0] == '0' ? std::string(b + 1) : std::string(b);
}
inline std::string HubStatus(const CloudSaves::Result* r, bool on, uint32_t& col)
{
    col = 0xFFAEB7C4u;
    if (!r) return on ? "Not Synced Yet" : "Only Here";
    switch (r->outcome)
    {
    case CloudSaves::Outcome::InSync: col = 0xFF8FE3A2u; return "Up To Date";
    case CloudSaves::Outcome::Pushed: col = 0xFF8FE3A2u; return "Sent Up";
    case CloudSaves::Outcome::Pulled: col = 0xFF8CC8FFu; return "Brought Down";
    case CloudSaves::Outcome::NothingYet: return "No Save Yet";
    case CloudSaves::Outcome::Conflict: col = 0xFFF2C46Bu; return "Changed On Both";
    case CloudSaves::Outcome::Failed: col = 0xFFF2A08Au; return "Could Not Reach";
    default: return "Only Here";
    }
}
// The sidebar's pages (About; the others to come): page, the section shown (-1: the games); prow the
// row chosen in its list; pX..pRow where its rows were drawn (for taps).
// A QR code (support_qr.h) on white, with the quiet edge round it that phones need.
inline void DrawQr(Canvas& c, const char* const* rows, int n, int x, int y, int size)
{
    const int q = n + 8;
    Glass::Round(c, x, y, size, size, size * 0.05f, 0xFFFFFFFFu);
    const float m = size / (float)q;
    for (int r = 0; r < n; r++)
        for (int k = 0; k < n; k++)
            if (rows[r][k] == '#')
            {
                const int x0 = x + (int)((k + 4) * m), y0 = y + (int)((r + 4) * m), x1 = x + (int)((k + 5) * m), y1 = y + (int)((r + 5) * m);
                for (int yy = std::max(0, y0); yy < std::min(c.h, y1); yy++)
                    for (int xx = std::max(0, x0); xx < std::min(c.w, x1); xx++) c.px[(size_t)yy * c.w + xx] = 0xFF000000u;
            }
}
// The one message after 20 hours of play (PlayTime::AskNow): a beer, if they like.
inline void OpenBeer()
{
    GlassFocus& F = Focus();
    F.dialog = 10;
    F.drow = 0;
    F.dTitle = "Enjoying Fulldex?";
    F.dMsg = "You've played for over 20 hours. Fulldex is free, made by one dude in his spare time: if it's given you some good hours, consider buying me a beer. Scan the code with your phone. Cheers, James";
#if defined(__SWITCH__)
    F.dItems = { "Done" };
    F.dPaths = { "#close" };
#else
    F.dItems = { "Buy Me A Beer", "Maybe Later" };
    F.dPaths = { "#beer", "#close" };
#endif
}
inline int PageRows(int page) { return page == SIDE_ABOUT ? 5 : page == SIDE_UPDATES ? 2 : page == SIDE_ROMS ? std::max(1, (int)RomList().size()) : page == SIDE_HUB ? HubRows() : 0; }
inline std::string AboutUrl(int row)
{
    return row == 0 ? "https://buymeacoffee.com/mamesj" : row == 2 ? "https://github.com/mamesj/pokemon-fulldex/issues" : row == 4 ? "https://github.com/mamesj/pokemon-fulldex" : "";
}
// The page a row of a page opens (none: "").
inline std::string PageUrl(int page, int row)
{
    if (page == SIDE_UPDATES) return row >= 1 ? "https://github.com/mamesj/pokemon-fulldex/releases" : "";
    return page == SIDE_ABOUT ? AboutUrl(row) : "";
}
inline const char* PlatformLabel()
{
#if defined(__SWITCH__)
    return "Nintendo Switch";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__)
    return "Apple";
#elif defined(_WIN32)
    return "Windows";
#else
    return "Linux";
#endif
}
// The arrows while a page is open: up and down in the sidebar or the page's list, right into the
// list, left back to the sidebar. (False: no page open.)
inline bool PageMove(int dir)
{
    GlassFocus& F = Focus();
    if (F.page < 0) return false;
    if (F.side >= 0)
    {
        if (dir == 3) F.side = std::max(0, F.side - 1);
        else if (dir == 4) F.side = std::min((int)SIDES - 1, F.side + 1);
        else if (dir == 2) F.side = -1;
        return true;
    }
    if (F.page == SIDE_ROMS || F.page == SIDE_HUB)
    {
        // (The list, and right of it the chosen row's actions: a ROM's, or the Hub's.)
        const int n = F.page == SIDE_HUB ? HubRows() : (int)RomList().size();
        const int na = F.page == SIDE_HUB ? (int)HubActions(F.prow).size() : RomActions();
        if (F.pact >= 0)
        {
            if (dir == 3) F.pact = std::max(0, F.pact - 1);
            else if (dir == 4) F.pact = std::min(na - 1, F.pact + 1);
            else if (dir == 1) F.pact = -1;
            return true;
        }
        if (dir == 3) F.prow = std::max(0, F.prow - 1);
        else if (dir == 4) F.prow = std::min(std::max(0, n - 1), F.prow + 1);
        else if (dir == 1) F.side = F.page;
        else if (dir == 2 && n > 0 && na > 0) F.pact = 0;
        return true;
    }
    if (F.page == SIDE_UPDATES)
    {
        // (What's New, left of the column: up and down scroll it.)
        if (F.prow == 2)
        {
            if (dir == 3) F.nScroll = std::max(0, F.nScroll - 2);
            else if (dir == 4) F.nScroll = std::min(F.nMax, F.nScroll + 2);
            else if (dir == 1) F.side = F.page;
            else if (dir == 2) F.prow = 0;
            return true;
        }
        if (dir == 1) { F.prow = 2; return true; }
    }
    if (dir == 3) F.prow = std::max(0, F.prow - 1);
    else if (dir == 4) F.prow = std::min(PageRows(F.page) - 1, F.prow + 1);
    else if (dir == 1) F.side = F.page;
    return true;
}
// What's New on the Updates page: under a point (for taps and dragging), and the wheel.
inline bool NotesAt(int x, int y)
{
    const GlassFocus& f = Focus();
    return f.page == SIDE_UPDATES && f.nH > 0 && x >= f.nX && x < f.nX + f.nW && y >= f.nY && y < f.nY + f.nH;
}
inline bool NotesScroll(int by)
{
    GlassFocus& f = Focus();
    if (f.page != SIDE_UPDATES) return false;
    f.nScroll = std::max(0, std::min(f.nMax, f.nScroll + by));
    return true;
}
// B while a page is open: the sidebar first, then back to the games. (False: no page open.)
inline bool PageBack()
{
    GlassFocus& F = Focus();
    if (F.page < 0) return false;
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && Mover().open && Mover().swapping) { Mover().swapping = false; Mover().put = -1; Mover().msg.clear(); Hub().version++; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && Mover().open) { MoverShow(false); HubMode() = 0; F.prow = 1; F.rScroll = 0; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && HubMode() == 2) { HubLink::Leave(); HubMode() = 0; F.prow = 2; F.rScroll = 0; Hub().version++; return true; }
    if (F.page == SIDE_HUB && F.side < 0 && F.pact < 0 && HubMode() != 0) { HubMode() = 0; F.prow = 0; F.rScroll = 0; Hub().version++; return true; }
    if ((F.page == SIDE_ROMS || F.page == SIDE_HUB) && F.side < 0 && F.pact >= 0) { F.pact = -1; return true; }
    if (F.side < 0) F.side = F.page;
    else { F.page = -1; F.side = SIDE_HOME; }
    return true;
}
// A on a section of the sidebar: Home back to the games, About its page. (False: not made yet.)
inline bool OpenSide(int side)
{
    GlassFocus& F = Focus();
    if (side == SIDE_HOME) { F.page = -1; F.side = -1; return true; }
    if (side == SIDE_HUB) { HubMode() = 0; Mover().open = false; Hub().version++; }
    if (PageRows(side) > 0) { F.page = side; F.prow = 0; F.side = -1; F.nScroll = 0; F.pact = -1; F.rScroll = 0; return true; }
    return false;
}
inline int PageRowAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if (f.page < 0 || f.pW <= 0 || f.pRow <= 0 || x < f.pX || x >= f.pX + f.pW || y < f.pY) return -1;
    const int i = (y - f.pY) / f.pRow;
    return i < PageRows(f.page) ? i : -1;
}
inline int RomRowAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if ((f.page != SIDE_ROMS && f.page != SIDE_HUB) || f.pW <= 0 || f.pRow <= 0 || x < f.pX || x >= f.pX + f.pW || y < f.pY) return -1;
    const int k = (y - f.pY) / f.pRow;
    const int i = f.rScroll + k;
    return k < f.rVis && i < (f.page == SIDE_HUB ? HubRows() : (int)RomList().size()) ? i : -1;
}
inline int RomActionAt(int x, int y)
{
    const GlassFocus& f = Focus();
    if ((f.page != SIDE_ROMS && f.page != SIDE_HUB) || f.aW <= 0 || f.aRow <= 0 || x < f.aX || x >= f.aX + f.aW || y < f.aY) return -1;
    const int i = (y - f.aY) / f.aRow;
    return i < (f.page == SIDE_HUB ? (int)HubActions(f.prow).size() : RomActions()) ? i : -1;
}
inline std::vector<std::string> WrapLines(const std::string& msg, float px, int w, bool bold = false)
{
    std::vector<std::string> lines;
    std::string line, word;
    auto flush = [&]() {
        if (word.empty()) return;
        const std::string t = line.empty() ? word : line + " " + word;
        if (!line.empty() && GW(t, px, bold) > w) { lines.push_back(line); line = word; }
        else line = t;
        word.clear();
    };
    for (char ch : msg) { if (ch == ' ') flush(); else word += ch; }
    flush();
    if (!line.empty()) lines.push_back(line);
    return lines;
}
// A release's notes (Markdown, as written on GitHub) as plain points: list marks, headings, emphasis,
// code marks, pictures and links' addresses taken out; empty lines and headings left out.
inline std::vector<std::string> ReleasePoints(const std::string& md)
{
    std::vector<std::string> out;
    // (From a heading with "new" in it on, "What's new in this update", when there is one: the
    // download notes above it are not news.)
    size_t i = 0;
    for (size_t at = 0; at < md.size();)
    {
        size_t end = md.find('\n', at);
        if (end == std::string::npos) end = md.size();
        const size_t k = md.find_first_not_of(" \t", at);
        if (k != std::string::npos && k < end && md[k] == '#')
        {
            std::string low = md.substr(k, end - k);
            for (char& ch : low) ch = (char)tolower((unsigned char)ch);
            if (low.find("new") != std::string::npos) { i = end + 1; break; }
        }
        at = end + 1;
    }
    while (i <= md.size())
    {
        size_t j = md.find('\n', i);
        if (j == std::string::npos) j = md.size();
        std::string l = md.substr(i, j - i);
        i = j + 1;
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ' || l.back() == '\t')) l.pop_back();
        size_t k = 0;
        while (k < l.size() && (l[k] == ' ' || l[k] == '\t')) k++;
        l = l.substr(k);
        if (l.empty() || l[0] == '#' || l[0] == '|' || l[0] == '<' || l.compare(0, 3, "---") == 0 || l.compare(0, 3, "***") == 0 || l.compare(0, 2, "![") == 0) continue;
        if (l.size() > 1 && (l[0] == '-' || l[0] == '*' || l[0] == '+') && l[1] == ' ') l = l.substr(2);
        else
        {
            size_t d = 0;
            while (d < l.size() && isdigit((unsigned char)l[d])) d++;
            if (d > 0 && d + 1 < l.size() && (l[d] == '.' || l[d] == ')') && l[d + 1] == ' ') l = l.substr(d + 2);
        }
        if (l.compare(0, 4, "[ ] ") == 0 || l.compare(0, 4, "[x] ") == 0) l = l.substr(4);
        std::string t;
        for (size_t a = 0; a < l.size(); a++)
        {
            const char ch = l[a];
            if (ch == '*' || ch == '`' || (ch == '_' && a + 1 < l.size() && l[a + 1] == '_')) { if (ch == '_') a++; continue; }
            if (ch == '!' && a + 1 < l.size() && l[a + 1] == '[') continue;
            if (ch == '[')
            {
                const size_t close = l.find(']', a);
                if (close != std::string::npos && close + 1 < l.size() && l[close + 1] == '(')
                {
                    const size_t end = l.find(')', close);
                    if (end != std::string::npos) { t += l.substr(a + 1, close - a - 1); a = end; continue; }
                }
            }
            t += ch;
        }
        // (Typographic marks as the font's own: minus and dashes, curly quotes, the ellipsis.)
        static const char* const swaps[][2] = { { "\xE2\x88\x92", "-" }, { "\xE2\x80\x93", "-" }, { "\xE2\x80\x94", "-" },
                                                { "\xE2\x80\x98", "'" }, { "\xE2\x80\x99", "'" }, { "\xE2\x80\x9C", "\"" },
                                                { "\xE2\x80\x9D", "\"" }, { "\xE2\x80\xA6", "..." }, { "\xC2\xA0", " " } };
        for (const auto& sw : swaps)
            for (size_t at = t.find(sw[0]); at != std::string::npos; at = t.find(sw[0], at + strlen(sw[1])))
                t.replace(at, strlen(sw[0]), sw[1]);
        while (!t.empty() && t.back() == ' ') t.pop_back();
        if (!t.empty()) out.push_back(t);
    }
    return out;
}
// A page, right of the sidebar: its title in the bar, its sections in a glass panel on the left
// (what Fulldex is at the bottom), the chosen one in a wider panel on the right.
inline void DrawPage(Canvas& c, const GlassSpots& sp, bool solid, float u)
{
    GlassFocus& F = Focus();
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint;
    const int barH = sp.tabsY * 2 + sp.tabsH;
    const int L = sp.bannerX, R = sp.bannerX + sp.bannerW, top = barH + (int)(18 * u), bottom = c.h - (int)(64 * u);
    GT(c, sp.tabsX + (int)(6 * u), barH * 0.5f, F.page == SIDE_UPDATES ? "Updates" : F.page == SIDE_ROMS ? "ROMs" : F.page == SIDE_HUB ? (Mover().open ? "Move Pok\xC3\xA9mon" : HubMode() == 1 ? "Cloud Saves" : HubMode() == 2 ? "Trade" : "Hub") : F.page == SIDE_SETTINGS ? (F.pageTitle.empty() ? std::string("Settings") : F.pageTitle) : "About", 26 * u, text, true);
    const std::string where = std::string("Fulldex   \xC2\xB7   ") + PlatformLabel();
    GT(c, R - GW(where, 16 * u, true), barH * 0.5f, where, 16 * u, hint, true);
    if (F.page == SIDE_ROMS)
    {
        const std::vector<Game>& roms = RomList();
        const int n = (int)roms.size();
        const int rw = (int)(340 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        auto status = [&](const Game& g, uint32_t& col) -> std::string {
            if (g.hidden) { col = 0xFFAEB7C4u; return "Hidden"; }
            if (!g.known) { col = 0xFFF2A08Au; return "Not Playable"; }
            if (IsHack(g)) { col = 0xFFC9B6FFu; return "Romhack"; }
            col = 0xFF8FE3A2u; return "Ready";
        };
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        auto fileOf = [](const std::string& p) { const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1); };
        auto dirOf = [](std::string p) {
            const size_t k = p.find_last_of("/\\");
            p = k == std::string::npos ? std::string() : p.substr(0, k);
            if (p.rfind("sdmc:", 0) == 0) p = p.substr(5);
            return p;
        };
        // (A file name also in another folder: shown with its folder, the last two parts of it.)
        auto shownFile = [&](const Game& g) {
            const std::string f = fileOf(g.rom);
            int same = 0;
            for (const Game& o : roms) if (fileOf(o.rom) == f) same++;
            if (same < 2) return f;
            std::string d = dirOf(g.rom);
            size_t k = d.find_last_of("/\\");
            if (k != std::string::npos && k > 0) { const size_t k2 = d.find_last_of("/\\", k - 1); if (k2 != std::string::npos) d = d.substr(k2 + 1); }
            return d + "   \xC2\xB7   " + f;
        };
        // The list.
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        const int vis = std::max(1, (bottom - top - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.prow < F.rScroll) F.rScroll = F.prow;
        if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1;
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        if (n == 0)
        {
            const std::string none = "No ROMs found yet. Put your ROMs in the roms folder beside Fulldex.";
            GT(c, L + (lw2 - GW(none, 16 * u, true)) / 2, (top + bottom) * 0.5f, none, 16 * u, hint, true);
        }
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const Game& g = roms[i];
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            uint32_t col = 0;
            const std::string st = status(g, col);
            Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + (int)(32 * u), mid, fit(ShortName(g), 16 * u, true, (int)(200 * u)), 16 * u, g.hidden ? hint : text, true);
            const int fx = rx + (int)(248 * u);
            GT(c, fx, mid, fit(shownFile(g), 13 * u, false, chipX - fx - (int)(12 * u)), 13 * u, hint, false);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = bottom - top - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen ROM.
        const int dx = R - rw;
        Menu::FrostBox(c, solid, dx, top, rw, bottom - top, 18 * u);
        F.aW = 0;
        if (n > 0)
        {
            const Game& g = roms[F.prow];
            const int ix = dx + (int)(18 * u), iw = rw - (int)(36 * u), ps = (int)(110 * u);
            int y = top + (int)(18 * u);
            DrawCover(c, g, ix, y, ps, ps, 12 * u);
            const int tx = ix + ps + (int)(16 * u), tw = iw - ps - (int)(16 * u);
            GT(c, tx, y + 28 * u, fit(ShortName(g), 20 * u, true, tw), 20 * u, text, true);
            GT(c, tx, y + 54 * u, PlatformName(g.platform), 14 * u, hint, true);
            uint32_t col = 0;
            const std::string st = status(g, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u);
            Glass::Round(c, tx, y + (int)(70 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, tx + (int)(10 * u), y + 81 * u, st, 12 * u, col, true);
            y += ps + (int)(22 * u);
            char size[32];
            if (g.fileSize > 0) snprintf(size, sizeof size, "%.1f MB", g.fileSize / 1048576.0);
            else snprintf(size, sizeof size, "-");
            const std::string cells[4][2] = { { "Size", size }, { "Save", g.hasSave ? "Found" : "None Yet" },
                                              { "Game Code", g.code.empty() ? "-" : g.code }, { "Save File", g.saveName.empty() ? "-" : g.saveName } };
            for (int i = 0; i < 4; i++)
            {
                const int cx = ix + (i % 2) * (iw / 2), cy = y + (i / 2) * (int)(48 * u);
                GT(c, cx, cy + 8 * u, cells[i][0], 12 * u, hint, true);
                GT(c, cx, cy + 28 * u, fit(cells[i][1], 15 * u, true, iw / 2 - (int)(8 * u)), 15 * u, text, true);
            }
            y += 2 * (int)(48 * u) + (int)(10 * u);
            GT(c, ix, y + 8 * u, "File", 12 * u, hint, true);
            y += (int)(26 * u);
            const std::vector<std::string> fl = WrapLines(fileOf(g.rom), 13 * u, iw);
            for (size_t k = 0; k < fl.size() && k < 3; k++) { GT(c, ix, y, fl[k], 13 * u, text, false); y += (int)(18 * u); }
            y += (int)(12 * u);
            GT(c, ix, y + 8 * u, "Folder", 12 * u, hint, true);
            y += (int)(26 * u);
            const std::vector<std::string> dl = WrapLines(dirOf(g.rom), 13 * u, iw);
            for (size_t k = 0; k < dl.size() && k < 3; k++) { GT(c, ix, y, dl[k], 13 * u, text, false); y += (int)(18 * u); }
            // What can be done with it.
            const int na = RomActions(), ah = (int)(44 * u), ag = (int)(6 * u);
            const int ay = bottom - (int)(18 * u) - na * ah - (na - 1) * ag;
            F.aX = ix; F.aY = ay; F.aW = iw; F.aRow = ah + ag;
            const std::string labels[2] = { g.hidden ? "Show In List Again" : "Hide From List", "Open Folder" };
            const int icons[2] = { Glass::I_GRID, Glass::I_LOAD };
            for (int a = 0; a < na; a++)
            {
                const int ry = ay + a * (ah + ag);
                if (a == F.pact)
                {
                    if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                    Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
                }
                else Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
                Glass::DrawIcon(c, icons[a], ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
                GT(c, ix + (int)(42 * u), ry + ah * 0.5f, labels[a], 15 * u, text, true);
            }
        }
        return;
    }
    if (F.page == SIDE_SETTINGS)
    {
        // (Two frosted boxes; the settings menu draws its rows and preview in them: DrawSettingsPage.)
        const int rw = (int)(400 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap;
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        F.sX = L; F.sY = top; F.sW = R - L; F.sH = bottom - top; F.sListW = lw2; F.sPvX = R - rw;
        return;
    }
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 2)
    {
        TradeState& t = Trade();
#ifdef SSR_POKEMON_HUB
        TradeTick();
#endif
        const HubLink::Status s = HubLink::Snapshot();
        const std::vector<MoverGame> gs = MoverGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        auto fit = [&](std::string str, float px, bool bold, int w) {
            if (GW(str, px, bold) <= w) return str;
            while (!str.empty() && GW(str + "...", px, bold) > w)
            {
                while (!str.empty() && ((unsigned char)str.back() & 0xC0) == 0x80) str.pop_back();
                if (!str.empty()) str.pop_back();
            }
            return str + "...";
        };
        const uint32_t okCol = 0xFF8FE3A2u, noCol = 0xFFF2C46Bu, linkCol = 0xFF8CC8FFu;
        const std::string game = gs.empty() ? std::string("-") : gs[std::max(0, std::min(t.game, (int)gs.size() - 1))].name;
        std::string linkV, linkChip;
        uint32_t linkCol2 = hint;
        switch (s.state)
        {
        case HubLink::State::Off: linkV = "Not Linked"; linkChip = "Same Wi-Fi"; break;
        case HubLink::State::Hosting: linkV = "Room " + std::to_string(s.code); linkChip = "Waiting"; linkCol2 = noCol; break;
        case HubLink::State::Browsing: linkV = "Looking For Rooms"; linkChip = std::to_string(s.rooms.size()) + " Found"; linkCol2 = linkCol; break;
        case HubLink::State::Linked: linkV = "Linked With " + s.peerName; linkChip = s.onlineCode.empty() ? s.peerGame : "Online"; linkCol2 = okCol; break;
        case HubLink::State::Online: linkV = "Code " + s.onlineCode; linkChip = "Waiting Online"; linkCol2 = noCol; break;
        }
#ifdef SSR_POKEMON_HUB
        const std::vector<PokemonHub::Mon> mons = TradeList();
#else
        const std::vector<int> mons;
#endif
        const int n = TradeRows();
        const int rw = (int)(380 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        const int vis = std::max(1, (bottom - top - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (F.prow < F.rScroll) F.rScroll = F.prow;
        if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1;
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            std::string k, v, chip, spn;
            uint32_t chipCol = hint;
            std::vector<uint32_t> ic;
            if (i == 0) { k = "Game"; v = game; }
            else if (i == 1) { k = "Box"; v = "-"; }
            else if (i == 2) { k = "Link"; v = linkV; chip = linkChip; chipCol = linkCol2; }
#ifdef SSR_POKEMON_HUB
            if (i == 0 && t.save) chip = Shown(t.save->Trainer(), "-");
            if (i == 1 && t.save) { v = BoxLabel(t.save.get(), t.box); chip = std::to_string(mons.size()) + " Pok\xC3\xA9mon"; }
            if (i >= TRADE_FIRST)
            {
                const PokemonHub::Mon& mon = mons[i - TRADE_FIRST];
                k = mon.egg ? std::string("Egg") : "Lv " + std::to_string(mon.level);
                v = Shown(mon.nickname, MonName(mon.speciesName));
                if (!mon.egg) spn = MonName(mon.speciesName);
                if (s.mine.any && t.offerBox == mon.box && t.offerSlot == mon.slot) { chip = s.mine.gift ? "Sending" : "Offered"; chipCol = linkCol; }
                if (MonIcon(mon.species, mon.egg, ic)) DrawMonIcon(c, ic, rx + (int)(2 * u), mid - (int)(21 * u), (int)(40 * u));
                if (mon.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, rx + 38 * u, mid - 12 * u, 6 * u, 0xFFF2C46Bu);
            }
#endif
            if (i < TRADE_FIRST) Glass::DrawIcon(c, i == 1 ? Glass::I_GRID : i == 2 ? Glass::I_GLOBE : Glass::I_POKEBALL, rx + 18 * u, mid, 9 * u, text);
            if (!chip.empty())
            {
                const int chipW = GW(chip, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
                Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
                GT(c, chipX + (int)(10 * u), mid, chip, 12 * u, chipCol, true);
            }
            GT(c, rx + (int)(48 * u), mid, k, 13 * u, hint, true);
            GT(c, rx + (int)(118 * u), mid, fit(v, 16 * u, true, (int)(230 * u)), 16 * u, text, true);
            if (!spn.empty() && MonName(v) != spn)
                GT(c, rx + (int)(118 * u) + std::min(GW(v, 16 * u, true), (int)(230 * u)) + (int)(12 * u), mid, spn, 14 * u, hint, true);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = bottom - top - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The panel: the link, both offers, or the chosen row.
        const int dx = R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, top, rw, bottom - top, 18 * u);
        int y = top + (int)(20 * u);
        auto para = [&](const std::string& str, uint32_t col, float px) {
            for (const std::string& line : WrapLines(str, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto offerCard = [&](const char* who, const HubLink::Offer& o, const std::string& under, uint32_t underCol) {
            const int ch = (int)(78 * u);
            Glass::Round(c, ix, y, iw, ch, 14 * u, 0x1AFFFFFFu, 0x3DFFFFFF, 1.0f * u);
            GT(c, ix + (int)(14 * u), y + 16 * u, who, 12 * u, hint, true);
            std::vector<uint32_t> oi;
#ifdef SSR_POKEMON_HUB
            if (o.any && MonIcon(o.speciesId, false, oi)) DrawMonIcon(c, oi, ix + iw - (int)(70 * u), y + (int)(6 * u), (int)(64 * u));
#endif
            if (o.any)
            {
                GT(c, ix + (int)(14 * u), y + 40 * u, fit(Shown(o.name, o.species) + (o.shiny ? "   \xC2\xB7   Shiny" : ""), 17 * u, true, iw - (int)(90 * u)), 17 * u, text, true);
                GT(c, ix + (int)(14 * u), y + 62 * u, fit(o.species + "   \xC2\xB7   Lv " + std::to_string(o.level) + "   \xC2\xB7   " + o.game, 12 * u, true, iw - (int)(90 * u)), 12 * u, hint, true);
            }
            else GT(c, ix + (int)(14 * u), y + 46 * u, "Nothing yet", 15 * u, hint, true);
            y += ch + (int)(6 * u);
            if (!under.empty()) para(under, underCol, 12 * u);
            else y += (int)(6 * u);
        };
        if (F.prow == 2 || (s.state == HubLink::State::Linked && F.prow >= TRADE_FIRST && s.mine.any))
        {
            GT(c, ix, y + 14 * u, s.state == HubLink::State::Linked ? "Linked With " + s.peerName : "Trade", 20 * u, text, true);
            y += (int)(40 * u);
            if (s.state == HubLink::State::Off)
            {
                para("Trade with a friend, or send Pok\xC3\xA9mon to another of your own devices, on the same Wi-Fi.", text, 14 * u);
                para("Same Wi-Fi: one device chooses Open A Room, the other Join A Room. Online: one makes a code, the other enters it. Then each offers a Pok\xC3\xA9mon, or sends one as a gift.", hint, 13 * u);
            }
            else if (s.state == HubLink::State::Online)
            {
                para("Your code:", hint, 13 * u);
                const int bh = (int)(70 * u);
                Glass::Round(c, ix, y, iw, bh, 14 * u, 0x24FFFFFFu, 0x3DFFFFFF, 1.0f * u);
                GT(c, ix + (iw - GW(s.onlineCode, 34 * u, true)) / 2, y + bh * 0.5f, s.onlineCode, 34 * u, text, true);
                y += bh + (int)(16 * u);
                para("The other player chooses Hub, Trade, Enter An Online Code, and types it. Share it on Discord or in a message. It works for a few minutes.", hint, 13 * u);
            }
            else if (s.state == HubLink::State::Hosting) para("Room " + std::to_string(s.code) + " is open. On the other device: Hub, Trade Over Wi-Fi, Join A Room.", hint, 13 * u);
            if (!s.myAddress.empty() && s.state != HubLink::State::Linked) para("This device: " + s.myAddress, hint, 12 * u);
            else if (s.state == HubLink::State::Browsing) para(s.rooms.empty() ? "Looking for rooms on this network. Open one on the other device." : "Rooms on this network are listed below: choose one to join.", hint, 13 * u);
            else
            {
                std::string mineUnder, theirUnder;
                uint32_t mineCol = hint, theirCol = hint;
                if (s.mine.any) { mineUnder = s.theirReason.empty() && !s.theyCanTake ? "Waiting for " + s.peerName + "'s game to check it..." : s.theyCanTake ? s.peerName + "'s game can take it." : "Their game can't take it: " + SettingSentence(s.theirReason); mineCol = s.theyCanTake ? okCol : noCol; }
#ifdef SSR_POKEMON_HUB
                if (s.theirs.any && t.save)
                {
                    const PokemonHub::Check ck = PokemonHub::CanReceive(TheirPacked(s.theirs), *t.save, s.mine.any ? t.offerSpecies : 0);
                    theirUnder = ck.verdict == PokemonHub::Verdict::Ok ? (ck.evolvesTo ? SettingSentence(ck.evolution) + "." : game + " can take it.") : "Your game can't take it: " + SettingSentence(ck.reason);
                    theirCol = ck.verdict == PokemonHub::Verdict::Ok ? okCol : noCol;
                }
#endif
                offerCard(s.mine.any && s.mine.gift ? "You Send" : "You Offer", s.mine, mineUnder, mineCol);
                offerCard(s.theirs.any && s.theirs.gift ? (s.peerName + " Sends").c_str() : (s.peerName + " Offers").c_str(), s.theirs, theirUnder, theirCol);
                if (s.iAccepted || s.theyAccepted)
                    para(std::string(s.iAccepted ? "You said yes. " : "") + (s.theyAccepted ? s.peerName + " said yes." : "Waiting for " + s.peerName + "..."), linkCol, 13 * u);
            }
        }
        else if (F.prow == 0) { GT(c, ix, y + 14 * u, game, 20 * u, text, true); y += (int)(40 * u); para(s.state == HubLink::State::Linked ? "Leave the link to change the game." : "The game to trade from and into.", hint, 13 * u); }
        else if (F.prow == 1) { GT(c, ix, y + 14 * u, "Box", 20 * u, text, true); y += (int)(40 * u); para("Next Box and Previous Box go through its boxes; Party shows the Pok\xC3\xA9mon with you.", hint, 13 * u); }
#ifdef SSR_POKEMON_HUB
        else if (F.prow - TRADE_FIRST < (int)mons.size())
        {
            const PokemonHub::Mon& mon = mons[F.prow - TRADE_FIRST];
            std::vector<uint32_t> big;
            const int bs = (int)(96 * u);
            if (MonIcon(mon.species, mon.egg, big)) DrawMonIcon(c, big, ix + iw - bs, y - (int)(18 * u), bs);
            GT(c, ix, y + 14 * u, fit(Shown(mon.nickname, MonName(mon.speciesName)), 22 * u, true, iw - bs), 22 * u, text, true);
            GT(c, ix, y + 42 * u, (mon.egg ? std::string("Egg") : MonName(mon.speciesName)) + (mon.shiny ? "   \xC2\xB7   Shiny" : ""), 15 * u, mon.shiny ? 0xFFF2C46Bu : hint, true);
            y += (int)(72 * u);
            if (s.state != HubLink::State::Linked) para("Link with the other device first: the Link row above.", hint, 13 * u);
            else
            {
                const PokemonHub::Check rm = t.save ? PokemonHub::CanRemove(*t.save, mon.box, mon.slot) : PokemonHub::Check();
                para(rm.verdict == PokemonHub::Verdict::Ok ? "Offer it for a trade (one each way, both say yes), or send it as a gift." : SettingSentence(rm.reason), rm.verdict == PokemonHub::Verdict::Ok ? hint : noCol, 13 * u);
            }
        }
#endif
        const int na = (int)acts.size(), ah = (int)(44 * u), ag = (int)(6 * u);
        const int ay = bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag;
        const std::string word = !t.msg.empty() ? t.msg : s.note;
        if (!word.empty())
        {
            const std::vector<std::string> ml = WrapLines(word, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, linkCol, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            const int icon = F.prow == 1 ? Glass::I_GRID : F.prow == 2 ? (acts[a] == "Leave" || acts[a] == "Not Yet" || acts[a] == "Stop Looking" || acts[a] == "Close The Room" || acts[a] == "Take Back My Offer" ? Glass::I_BACK : Glass::I_GLOBE) : F.prow >= TRADE_FIRST ? Glass::I_PEOPLE : Glass::I_POKEBALL;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, fit(acts[a], 15 * u, true, iw - (int)(60 * u)), 15 * u, text, true);
        }
        return;
    }
    if (F.page == SIDE_HUB && !Mover().open && HubMode() == 0)
    {
        // The Hub's first page: what it holds, each opened with A.
        HubLoad();
        const std::vector<HubGame> games = HubGames();
        HubState& h = Hub();
        std::unique_lock<std::mutex> lk(h.m);
        const bool on = HubOnLocked(h);
        const int busy = h.busy;
        const std::string msg = h.msg, prov = h.set.provider;
        const std::string dev = h.set.deviceName.empty() ? HubDevice() : h.set.deviceName;
        int synced = 0;
        for (const HubGame& g : games)
        {
            const auto it = h.results.find(g.path);
            if (h.synced.count(g.g ? g.g->saveName : std::string()) || (it != h.results.end() && (it->second.outcome == CloudSaves::Outcome::InSync || it->second.outcome == CloudSaves::Outcome::Pushed || it->second.outcome == CloudSaves::Outcome::Pulled))) synced++;
        }
        lk.unlock();
        const int moverGames = (int)MoverGames().size();
        const int rw = (int)(400 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, pad = (int)(14 * u);
        const int rowH = (int)(72 * u), rg = (int)(10 * u), n = 3;
        const HubLink::Status link = HubLink::Snapshot();
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        F.prow = std::max(0, std::min(F.prow, n - 1));
        F.pact = -1;
        F.rScroll = 0;
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = n;
        F.aW = 0;
        for (int i = 0; i < n; i++)
        {
            const int rx = L + pad, ry = top + pad + i * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 14 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 14 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, rx, ry, rww, rowH, 14 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            std::string name, sub, st;
            uint32_t col = 0xFFAEB7C4u;
            if (i == 0)
            {
                name = "Cloud Saves";
                sub = on ? std::string(HubProvider(prov)) + "   \xC2\xB7   " + std::to_string(synced) + " of " + std::to_string(games.size()) + " up to date"
                         : "Keep your saves the same on every device";
                if (busy == 1) { st = "Signing In"; col = 0xFFF2C46Bu; }
                else if (busy == 2) { st = "Syncing"; col = 0xFF8CC8FFu; }
                else if (on) { st = "On"; col = 0xFF8FE3A2u; }
                else st = "Off";
            }
            else if (i == 2)
            {
                name = "Trade";
                sub = "With a friend online or on the same Wi-Fi, or to another of your devices";
                st = link.state == HubLink::State::Linked ? "Linked" : link.state == HubLink::State::Online ? "Online" : link.state == HubLink::State::Hosting ? "Room Open" : link.state == HubLink::State::Browsing ? "Looking" : "Same Wi-Fi";
                if (link.state != HubLink::State::Off) col = 0xFF8CC8FFu;
            }
            else
            {
                name = "Move Pok\xC3\xA9mon";
                sub = "Between your own games, as a trade would";
                st = std::to_string(moverGames) + (moverGames == 1 ? " Save" : " Saves");
            }
            Glass::DrawIcon(c, i == 0 ? Glass::I_GLOBE : i == 2 ? Glass::I_PEOPLE : Glass::I_POKEBALL, rx + 30 * u, mid, 12 * u, text);
            GT(c, rx + (int)(60 * u), mid - 11 * u, name, 18 * u, text, true);
            GT(c, rx + (int)(60 * u), mid + 13 * u, sub, 13 * u, hint, false);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(46 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + rww - (int)(26 * u), mid, ">", 16 * u, hint, true);
        }
        // The chosen one: what it does, how it stands.
        const int dx = R - rw, ix = dx + (int)(22 * u), iw = rw - (int)(44 * u);
        Menu::FrostBox(c, solid, dx, top, rw, bottom - top, 18 * u);
        int y = top + (int)(22 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, v, 15 * u, text, true);
            (void)cw;
        };
        GT(c, ix, y + 14 * u, F.prow == 0 ? "Cloud Saves" : F.prow == 2 ? "Trade" : "Move Pok\xC3\xA9mon", 22 * u, text, true);
        y += (int)(44 * u);
        if (F.prow == 0)
        {
            if (on)
            {
                cell(ix, y, iw / 2, "Saved To", HubProvider(prov));
                cell(ix + iw / 2, y, iw / 2, "This Device", dev);
                y += (int)(52 * u);
                cell(ix, y, iw / 2, "Up To Date", std::to_string(synced) + " of " + std::to_string(games.size()));
                y += (int)(60 * u);
                para("Saves sync when a game starts and when it closes, and the DS games after each save too. Open it to sync now, see each game, or choose which save to keep.", hint, 13 * u);
            }
            else
            {
                para("Keep your saves the same on every device: play on your PC, then carry on on the Switch.", text, 14 * u);
                para("Open it to sign in with Google. Your saves go to your own Drive, in a folder called Fulldex Cloud Saves.", hint, 13 * u);
            }
            if (!msg.empty()) para(msg, busy ? 0xFF8CC8FFu : hint, 13 * u);
        }
        else if (F.prow == 2)
        {
            para("Trade with a friend, online or on the same Wi-Fi, or send Pok\xC3\xA9mon to another of your own devices: a PC and a Switch, say.", text, 14 * u);
            para("Trades follow the real games' rules: one each way, both say yes, and those that evolve by trading evolve, Karrablast and Shelmet too. Each save is backed up before it is written.", hint, 13 * u);
        }
        else
        {
            para("Move a Pok\xC3\xA9mon from one of your games to another, as a trade between them would.", text, 14 * u);
            para("They can go to a newer game, never an older one. Fulldex says first what one would lose on the way, and backs up both saves before it writes them.", hint, 13 * u);
            cell(ix, y, iw, "Games With A Save", std::to_string(moverGames));
            y += (int)(56 * u);
        }
        return;
    }
    if (F.page == SIDE_HUB && Mover().open)
    {
        MoverState& m = Mover();
        const std::vector<MoverGame> gs = MoverGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        const uint32_t okCol = 0xFF8FE3A2u, noCol = 0xFFF2C46Bu, greyCol = 0xFFAEB7C4u, swapCol = 0xFF8CC8FFu;
        const std::string fromName = gs.empty() ? std::string("-") : gs[std::max(0, std::min(m.from, (int)gs.size() - 1))].name;
        const std::string toName = gs.empty() ? std::string("-") : gs[std::max(0, std::min(m.to, (int)gs.size() - 1))].name;
#ifdef SSR_POKEMON_HUB
        const std::vector<PokemonHub::Mon> mons = MoverList();
        auto check = [&](const PokemonHub::Mon& mon) {
            PokemonHub::Check ck;
            if (!m.a || !m.b) { ck.verdict = PokemonHub::Verdict::Empty; ck.reason = "That game's save could not be read."; }
            else if (m.from == m.to) { ck.verdict = PokemonHub::Verdict::Empty; ck.reason = "Choose two different games."; }
            else if (m.swapping) ck = PokemonHub::CanSwap(*m.a, m.swapBox, m.swapSlot, *m.b, mon.box, mon.slot);
            else ck = PokemonHub::CanMoveTo(*m.a, mon.box, mon.slot, *m.b, m.put, -1);
            return ck;
        };
#else
        const std::vector<int> mons;
#endif
        const int n = MoverRows();
        const int rw = (int)(360 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        const int vis = std::max(1, (bottom - top - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (F.prow < F.rScroll) F.rScroll = F.prow;
        if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1;
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else if (m.swapping && i == 3) Glass::Round(c, rx, ry, rww, rowH, 12 * u, 0x1A8CC8FFu);
            std::string k, v, chip, spn;
            std::vector<uint32_t> ic;
            uint32_t col = greyCol, chipCol = hint;
            int icon = Glass::I_POKEBALL;
            if (i == 0) { k = "From"; v = fromName; }
            else if (i == 1) { k = "Box"; v = "-"; icon = Glass::I_GRID; }
            else if (i == 2) { k = "To"; v = toName; }
            else if (i == 3) { k = m.swapping ? "Swap In" : "Put In"; v = "-"; icon = m.swapping ? Glass::I_GRID : Glass::I_LOAD; }
#ifdef SSR_POKEMON_HUB
            if (i == 0 && m.a) chip = Shown(m.a->Trainer(), "-");
            if (i == 2 && m.b) chip = Shown(m.b->Trainer(), "-");
            if (i == 1 && m.a)
            {
                v = BoxLabel(m.a.get(), m.box);
                const size_t cnt = MonsIn(m.a.get(), m.box).size();
                chip = std::to_string(cnt) + " Pok\xC3\xA9mon";
            }
            if (i == 3)
            {
                v = m.swapping ? BoxLabel(m.b.get(), MoverTargetBox()) : PutLabel();
                if (m.swapping) { chip = "Swapping " + m.swapName; chipCol = swapCol; }
                else if (m.b && m.put >= 0) chip = std::to_string(30 - (int)MonsIn(m.b.get(), m.put).size()) + " Free";
                else if (m.b && m.put == PokemonHub::TO_PARTY) chip = std::to_string(6 - (int)PokemonHub::Party(*m.b).size()) + " Free";
            }
            if (i >= MOVER_FIRST)
            {
                const PokemonHub::Mon& mon = mons[i - MOVER_FIRST];
                const PokemonHub::Check ck = check(mon);
                col = ck.verdict == PokemonHub::Verdict::Ok ? okCol : noCol;
                k = mon.egg ? std::string("Egg") : "Lv " + std::to_string(mon.level);
                v = Shown(mon.nickname, MonName(mon.speciesName));
                if (!mon.egg && !mon.speciesName.empty()) spn = MonName(mon.speciesName);
                chip = ck.verdict == PokemonHub::Verdict::Ok ? (m.swapping ? (ck.evolvesTo ? "Can Swap, Evolves" : "Can Swap") : (ck.evolvesTo ? "Can Move, Evolves" : "Can Move"))
                                                             : (m.swapping ? "Can't Swap" : "Can't Move");
                chipCol = col;
                if (MonIcon(mon.species, mon.egg, ic)) DrawMonIcon(c, ic, rx + (int)(2 * u), mid - (int)(21 * u), (int)(40 * u));
                else Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
                if (mon.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, rx + 38 * u, mid - 12 * u, 6 * u, 0xFFF2C46Bu);
            }
#endif
            if (i < MOVER_FIRST) Glass::DrawIcon(c, icon, rx + 18 * u, mid, 9 * u, text);
            if (!chip.empty())
            {
                const int chipW = GW(chip, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
                Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
                GT(c, chipX + (int)(10 * u), mid, chip, 12 * u, chipCol, true);
            }
            GT(c, rx + (int)(48 * u), mid, k, 13 * u, hint, true);
            GT(c, rx + (int)(118 * u), mid, fit(v, 16 * u, true, (int)(210 * u)), 16 * u, text, true);
            if (!spn.empty() && MonName(v) != spn)
            {
                const int sx = rx + (int)(118 * u) + std::min(GW(v, 16 * u, true), (int)(210 * u)) + (int)(12 * u);
                GT(c, sx, mid, spn, 14 * u, hint, true);
            }
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = bottom - top - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen row.
        const int dx = R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, top, rw, bottom - top, 18 * u);
        int y = top + (int)(20 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, fit(v, 15 * u, true, cw - (int)(8 * u)), 15 * u, text, true);
        };
#ifdef SSR_POKEMON_HUB
        if (F.prow == 0 || F.prow == 2)
        {
            const PokemonHub::Save* s = F.prow == 0 ? m.a.get() : m.b.get();
            GT(c, ix, y + 14 * u, F.prow == 0 ? "Take From" : "Send To", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(F.prow == 0 ? fromName : toName, 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (!s) para("This save could not be read. " + (F.prow == 0 ? m.aErr : m.bErr), noCol, 13 * u);
            else
            {
                cell(ix, y, iw / 2, "Trainer", Shown(s->Trainer(), "-"));
                cell(ix + iw / 2, y, iw / 2, "Generation", std::to_string(s->Generation()));
                y += (int)(56 * u);
                if (s->ReadOnly()) para("Pok\xC3\xA9mon can be looked at in this game but not moved in or out yet.", noCol, 13 * u);
                para(F.prow == 0 ? "Choose the game to take Pok\xC3\xA9mon from." : "Choose the game to send them to. They can go to a newer game, never an older one, as in the real games.", hint, 13 * u);
            }
        }
        else if (F.prow == 1)
        {
            GT(c, ix, y + 14 * u, "Box", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(BoxLabel(m.a.get(), m.box), 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (m.a) cell(ix, y, iw / 2, "Box", m.box == PokemonHub::PARTY ? std::string("Party") : std::to_string(m.box + 1) + " of " + std::to_string(m.a->Boxes()));
            cell(ix + iw / 2, y, iw / 2, "Pok\xC3\xA9mon", std::to_string(MonsIn(m.a.get(), m.box).size()));
            y += (int)(56 * u);
            para("Next Box and Previous Box go through its boxes; Party shows the Pok\xC3\xA9mon with you.", hint, 13 * u);
        }
        else if (F.prow == 3)
        {
            GT(c, ix, y + 14 * u, m.swapping ? "Swap In" : "Put In", 13 * u, hint, true);
            GT(c, ix, y + 40 * u, fit(m.swapping ? BoxLabel(m.b.get(), MoverTargetBox()) : PutLabel(), 22 * u, true, iw), 22 * u, text, true);
            y += (int)(64 * u);
            if (m.swapping) para("Choose the box in " + toName + " to pick the Pok\xC3\xA9mon from, to swap for " + m.swapName + ". B: no swap.", hint, 13 * u);
            else para("Where it goes in " + toName + ": the first free place in its boxes, the party, or a box of your choosing.", hint, 13 * u);
        }
        else if (F.prow - MOVER_FIRST < (int)mons.size())
        {
            const PokemonHub::Mon& mon = mons[F.prow - MOVER_FIRST];
            const PokemonHub::Check ck = check(mon);
            std::vector<uint32_t> big;
            const int bs = (int)(96 * u);
            if (MonIcon(mon.species, mon.egg, big)) DrawMonIcon(c, big, ix + iw - bs, y - (int)(18 * u), bs);
            GT(c, ix, y + 14 * u, fit(Shown(mon.nickname, MonName(mon.speciesName)), 22 * u, true, iw - bs), 22 * u, text, true);
            GT(c, ix, y + 42 * u, (mon.egg ? std::string("Egg") : MonName(mon.speciesName)) + (mon.shiny ? "   \xC2\xB7   Shiny" : ""), 15 * u, mon.shiny ? 0xFFF2C46Bu : hint, true);
            y += (int)(66 * u);
            cell(ix, y, iw / 2, "Level", mon.egg ? std::string("Egg") : std::to_string(mon.level));
            cell(ix + iw / 2, y, iw / 2, "Trainer", Shown(mon.trainer, "-"));
            y += (int)(58 * u);
            GT(c, ix, y + 8 * u, m.swapping ? "Swap For " + m.swapName : "To " + toName + "   \xC2\xB7   " + PutLabel(), 12 * u, m.swapping ? swapCol : hint, true);
            y += (int)(24 * u);
            if (ck.verdict == PokemonHub::Verdict::Ok)
            {
                para(ck.losses.empty() ? (m.swapping ? "They can swap, and keep everything." : "It can move, and keeps everything.") : (m.swapping ? "They can swap, but on the way:" : "It can move, but on the way:"), okCol, 14 * u);
                for (const std::string& l : ck.losses) para("\xC2\xB7  " + SettingSentence(l), text, 13 * u);
                if (ck.evolvesTo) para(SettingSentence(ck.evolution) + (m.swapping ? ", if you choose Swap And Evolve." : ", if you choose Move And Evolve."), text, 13 * u);
            }
            else para(ck.reason.empty() ? std::string("It can't go to that game.") : SettingSentence(ck.reason), noCol, 14 * u);
        }
#else
        para("Moving Pok\xC3\xA9mon is not in this build of Fulldex.", hint, 14 * u);
#endif
        const int na = (int)acts.size(), ah = (int)(44 * u), ag = (int)(6 * u);
        const int ay = bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag;
        if (!m.msg.empty())
        {
            const std::vector<std::string> ml = WrapLines(m.msg, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, swapCol, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        const int cur = F.prow == 0 ? m.from : F.prow == 2 ? m.to : -1;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            const int icon = F.prow == 1 || F.prow == 3 ? Glass::I_GRID : F.prow >= MOVER_FIRST ? (acts[a].rfind("Swap", 0) == 0 ? Glass::I_PEOPLE : acts[a] == "Cancel Swap" ? Glass::I_BACK : Glass::I_LOAD) : Glass::I_POKEBALL;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, fit(acts[a], 15 * u, true, iw - (int)(60 * u)), 15 * u, text, true);
            if (a == cur) GT(c, ix + iw - (int)(14 * u) - GW("Chosen", 12 * u, true), ry + ah * 0.5f, "Chosen", 12 * u, okCol, true);
        }
        return;
    }
    if (F.page == SIDE_HUB)
    {
        HubLoad();
        const std::vector<HubGame> games = HubGames();
        const std::vector<std::string> acts = HubActions(F.prow);
        HubState& h = Hub();
        std::unique_lock<std::mutex> lk(h.m);
        const bool on = HubOnLocked(h);
        const int busy = h.busy;
        const std::string code = h.code, url = h.url, msg = h.msg, prov = h.set.provider;
        const std::string dev = h.set.deviceName.empty() ? HubDevice() : h.set.deviceName;
        const std::map<std::string, CloudSaves::Result> results = h.results;
        lk.unlock();
        auto resultOf = [&](const std::string& p) -> const CloudSaves::Result* { const auto it = results.find(p); return it == results.end() ? nullptr : &it->second; };
        auto fit = [&](std::string s, float px, bool bold, int w) {
            if (GW(s, px, bold) <= w) return s;
            while (!s.empty() && GW(s + "...", px, bold) > w)
            {
                while (!s.empty() && ((unsigned char)s.back() & 0xC0) == 0x80) s.pop_back();
                if (!s.empty()) s.pop_back();
            }
            return s + "...";
        };
        auto fileOf = [](const std::string& p) { const size_t k = p.find_last_of("/\\"); return k == std::string::npos ? p : p.substr(k + 1); };
        const int n = HUB_FIRST + (int)games.size();
        const int rw = (int)(360 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, pad = (int)(12 * u);
        const int rowH = (int)(46 * u), rg = (int)(6 * u);
        // The list: the account, then each game's save.
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        const int vis = std::max(1, (bottom - top - 2 * pad + rg) / (rowH + rg));
        F.prow = std::max(0, std::min(F.prow, n - 1));
        if (F.pact >= (int)acts.size()) F.pact = acts.empty() ? -1 : (int)acts.size() - 1;
        if (F.prow < F.rScroll) F.rScroll = F.prow;
        if (F.prow >= F.rScroll + vis) F.rScroll = F.prow - vis + 1;
        F.rScroll = std::max(0, std::min(F.rScroll, std::max(0, n - vis)));
        F.pX = L + pad; F.pY = top + pad; F.pW = lw2 - 2 * pad; F.pRow = rowH + rg; F.rVis = vis;
        for (int i = F.rScroll; i < n && i < F.rScroll + vis; i++)
        {
            const int rx = L + pad, ry = top + pad + (i - F.rScroll) * (rowH + rg), rww = lw2 - 2 * pad, mid = ry + rowH / 2;
            if (i == F.prow)
            {
                if (F.side < 0 && F.pact < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            uint32_t col = 0xFFAEB7C4u;
            std::string st, name, sub;
            if (i == 0)
            {
                name = "Account";
                sub = on ? std::string(HubProvider(prov)) + "   \xC2\xB7   " + dev : "Keep your saves the same on every device";
                if (busy == 1) { st = "Signing In"; col = 0xFFF2C46Bu; }
                else if (busy == 2) { st = "Syncing"; col = 0xFF8CC8FFu; }
                else if (on) { st = "On"; col = 0xFF8FE3A2u; }
                else st = "Off";
                Glass::DrawIcon(c, Glass::I_GLOBE, rx + 18 * u, mid, 9 * u, text);
            }
            else
            {
                const HubGame& g = games[i - HUB_FIRST];
                name = g.name;
                sub = (g.g && !g.g->where.empty() ? WhereName(g.g->where) + "   \xC2\xB7   " : std::string()) + fileOf(g.path) + (g.g && !g.g->hasSave ? "   \xC2\xB7   none here yet" : "");
                st = HubStatus(resultOf(g.path), on, col);
                Glass::Round(c, rx + (int)(14 * u), mid - (int)(4 * u), (int)(8 * u), (int)(8 * u), 4 * u, col);
            }
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u), chipX = rx + rww - (int)(10 * u) - chipW;
            Glass::Round(c, chipX, mid - (int)(11 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, chipX + (int)(10 * u), mid, st, 12 * u, col, true);
            GT(c, rx + (int)(36 * u), mid, fit(name, 16 * u, true, (int)(200 * u)), 16 * u, text, true);
            const int fx = rx + (int)(248 * u);
            GT(c, fx, mid, fit(sub, 13 * u, false, chipX - fx - (int)(12 * u)), 13 * u, hint, false);
        }
        if (n > vis)
        {
            const int tx = L + lw2 - (int)(7 * u), ty = top + (int)(16 * u), th = bottom - top - (int)(32 * u);
            Glass::Round(c, tx, ty, (int)(3 * u), th, 1.5f * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), th * vis / n), thumbY = ty + (th - thumbH) * F.rScroll / std::max(1, n - vis);
            Glass::Round(c, tx, thumbY, (int)(3 * u), thumbH, 1.5f * u, 0xCCFFFFFFu);
        }
        // The chosen row.
        const int dx = R - rw, ix = dx + (int)(20 * u), iw = rw - (int)(40 * u);
        Menu::FrostBox(c, solid, dx, top, rw, bottom - top, 18 * u);
        int y = top + (int)(20 * u);
        auto para = [&](const std::string& s, uint32_t col, float px) {
            for (const std::string& line : WrapLines(s, px, iw)) { GT(c, ix, y + px * 0.6f, line, px, col, false); y += (int)(px * 1.45f); }
            y += (int)(8 * u);
        };
        auto cell = [&](int cx, int cy, int cw, const std::string& k, const std::string& v) {
            GT(c, cx, cy + 8 * u, k, 12 * u, hint, true);
            GT(c, cx, cy + 28 * u, fit(v, 15 * u, true, cw - (int)(8 * u)), 15 * u, text, true);
        };
        if (F.prow == 0)
        {
            GT(c, ix, y + 14 * u, "Account", 22 * u, text, true);
            y += (int)(40 * u);
            if (busy == 1 && !code.empty())
            {
                para("On your phone or computer, go to", hint, 14 * u);
                GT(c, ix, y + 8 * u, fit(url.empty() ? std::string("google.com/device") : url, 16 * u, true, iw), 16 * u, text, true);
                y += (int)(30 * u);
                para("and enter this code:", hint, 14 * u);
                const int bh = (int)(70 * u);
                Glass::Round(c, ix, y, iw, bh, 14 * u, 0x24FFFFFFu, 0x3DFFFFFF, 1.0f * u);
                GT(c, ix + (iw - GW(code, 34 * u, true)) / 2, y + bh * 0.5f, code, 34 * u, text, true);
                y += bh + (int)(16 * u);
                para("Fulldex is waiting for you to say yes. It only sees the saves it puts in your Drive.", hint, 13 * u);
            }
            else if (!on)
            {
                para("Keep your saves the same on every device: play on your PC, then carry on on the Switch.", text, 14 * u);
                para("Saves go to your own Google Drive, in a folder called Fulldex Cloud Saves. Fulldex sees only the files it made there.", hint, 13 * u);
                para("If a save changed on two devices, Fulldex asks which one to keep, and puts the other in the backups.", hint, 13 * u);
            }
            else
            {
                cell(ix, y, iw / 2, "Saved To", HubProvider(prov));
                cell(ix + iw / 2, y, iw / 2, "This Device", dev);
                y += (int)(48 * u);
                int synced = 0;
                for (const HubGame& g : games) { const CloudSaves::Result* r = resultOf(g.path); if (r && (r->outcome == CloudSaves::Outcome::InSync || r->outcome == CloudSaves::Outcome::Pushed || r->outcome == CloudSaves::Outcome::Pulled)) synced++; }
                cell(ix, y, iw / 2, "Folder", prov == "google" ? "Fulldex Cloud Saves" : prov == "webdav" ? "WebDAV" : "Synced Folder");
                cell(ix + iw / 2, y, iw / 2, "Up To Date", std::to_string(synced) + " of " + std::to_string(games.size()));
                y += (int)(56 * u);
                para("Choose a game in the list to sync it on its own, or to choose which save to keep.", hint, 13 * u);
            }
        }
        else
        {
            const HubGame& hg = games[F.prow - HUB_FIRST];
            const CloudSaves::Result* r = resultOf(hg.path);
            const int ps = (int)(84 * u);
            if (hg.g) DrawCover(c, *hg.g, ix, y, ps, ps, 12 * u);
            const int tx = ix + ps + (int)(14 * u), tw = iw - ps - (int)(14 * u);
            GT(c, tx, y + 22 * u, fit(hg.name, 19 * u, true, tw), 19 * u, text, true);
            uint32_t col = 0;
            const std::string st = HubStatus(r, on, col);
            const int chipW = GW(st, 12 * u, true) + (int)(20 * u);
            Glass::Round(c, tx, y + (int)(42 * u), chipW, (int)(22 * u), 11 * u, 0x24FFFFFFu);
            GT(c, tx + (int)(10 * u), y + 53 * u, st, 12 * u, col, true);
            y += ps + (int)(18 * u);
            std::string cloudDev = r && !r->cloud.device.empty() ? r->cloud.device : std::string("another device");
            if (r && r->outcome == CloudSaves::Outcome::Conflict)
            {
                cell(ix, y, iw / 2, "This Device", HubWhen(r->local.savedAt));
                cell(ix + iw / 2, y, iw / 2, "Cloud (" + cloudDev + ")", HubWhen(r->cloud.savedAt));
                y += (int)(56 * u);
                para("This save changed here and on " + cloudDev + " since they last matched. Choose which to keep: the other goes into the backups.", text, 14 * u);
            }
            else if (!on) para("Cloud saves are off. Sign in on the Cloud Saves row first.", hint, 14 * u);
            else if (!r) para("Not synced yet. Choose Sync Now to send it up, or bring down a newer one.", hint, 14 * u);
            else switch (r->outcome)
            {
            case CloudSaves::Outcome::InSync: para("This save is the same here and in the cloud.", text, 14 * u); break;
            case CloudSaves::Outcome::Pushed: para("This save was sent up to the cloud.", text, 14 * u); break;
            case CloudSaves::Outcome::Pulled: para("A newer save from " + cloudDev + " was brought down. The one that was here is in the backups.", text, 14 * u); break;
            case CloudSaves::Outcome::NothingYet: para("There is no save for this game yet, here or in the cloud.", hint, 14 * u); break;
            default: para("Could not reach the cloud. The save here is kept, and syncs next time.", hint, 14 * u); break;
            }
            GT(c, ix, y + 8 * u, "Save File", 12 * u, hint, true);
            GT(c, ix, y + 28 * u, fit(fileOf(hg.path), 14 * u, false, iw), 14 * u, text, false);
            y += (int)(48 * u);
        }
        // What can be done (and the last word, above it).
        const int na = (int)acts.size(), ah = (int)(44 * u), ag = (int)(6 * u);
        const int ay = bottom - (int)(18 * u) - na * ah - std::max(0, na - 1) * ag;
        if (!msg.empty())
        {
            const std::vector<std::string> ml = WrapLines(msg, 13 * u, iw);
            int my = ay - (int)(14 * u) - (int)ml.size() * (int)(19 * u);
            if (my > y) for (const std::string& line : ml) { GT(c, ix, my + 8 * u, line, 13 * u, busy ? 0xFF8CC8FFu : hint, true); my += (int)(19 * u); }
        }
        F.aX = ix; F.aY = ay; F.aW = na > 0 ? iw : 0; F.aRow = ah + ag;
        for (int a = 0; a < na; a++)
        {
            const int ry = ay + a * (ah + ag);
            if (a == F.pact)
            {
                if (F.side < 0) Glass::Glow(c, ix, ry, iw, ah, 12 * u, u);
                Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, ix, ry, iw, ah, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            const int icon = acts[a] == "Sign In With Google" ? Glass::I_GLOBE : acts[a] == "Sign Out" || acts[a] == "Cancel" ? Glass::I_BACK : Glass::I_LOAD;
            Glass::DrawIcon(c, icon, ix + 22 * u, ry + ah * 0.5f, 9 * u, text);
            GT(c, ix + (int)(42 * u), ry + ah * 0.5f, acts[a], 15 * u, text, true);
        }
        return;
    }
    if (F.page == SIDE_UPDATES)
    {
        const int rw = (int)(360 * u), gap = (int)(18 * u), lw2 = R - L - rw - gap, ip = (int)(22 * u), pad = (int)(12 * u);
        // The state, and what an update keeps.
        Menu::FrostBox(c, solid, L, top, lw2, bottom - top, 18 * u);
        const std::string have = F.upHave.empty() ? std::string("Unknown") : F.upHave;
        std::string title, sub;
        switch (F.upState)
        {
        case 0: title = "Not Checked Yet"; sub = "This Version: " + have; break;
        case 1: title = "Checking..."; sub = "This Version: " + have; break;
        case 2: title = "Fulldex Is Up To Date"; sub = "Version " + have; break;
        case 3: title = "A New Version Is Ready"; sub = "You Have " + have + "   \xC2\xB7   New: " + F.upNew; break;
        case 4: title = "Downloading..."; sub = "New: " + F.upNew; break;
        case 5: title = "Installed"; sub = "The new version starts when you close the list"; break;
        case 6: title = "Could Not Check"; sub = CleanFont::Title(F.upText); break;
        default: title = "Check For Updates"; sub = "Fulldex looks for a new version on GitHub"; break;
        }
        const int cx = L + ip, cy = top + ip, cw = lw2 - 2 * ip, ch = (int)(96 * u);
        Glass::Round(c, cx, cy, cw, ch, 14 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
        Glass::Round(c, cx + (int)(20 * u), cy + (int)(20 * u), (int)(56 * u), (int)(56 * u), 16 * u, 0x2EFFFFFFu, 0x4DFFFFFFu, 1.0f * u);
        Glass::DrawIcon(c, Glass::I_LOAD, cx + 48 * u, cy + 48 * u, 14 * u, text);
        GT(c, cx + (int)(96 * u), cy + 36 * u, title, 22 * u, text, true);
        float fs = 14 * u;
        while (fs > 9 * u && GW(sub, fs, true) > cw - (int)(112 * u)) fs -= u;
        GT(c, cx + (int)(96 * u), cy + 66 * u, sub, fs, hint, true);
        int y = cy + ch + (int)(28 * u);
        GT(c, cx, y, "What's New", 18 * u, text, true);
        y += (int)(24 * u);
        // (The newest release's notes from GitHub; without any, what an update keeps.)
        std::vector<std::string> points = ReleasePoints(F.upNotes);
        const bool fromRelease = !points.empty();
        if (!fromRelease)
            points = { "The changes in each version are listed on its release page on GitHub.",
                       "Your ROMs, saves and settings stay when Fulldex updates.",
                       "The version before is kept, in case you need to go back." };
        if (F.upState == 3 && !F.upCan) points.push_back("On this device, Open Download takes you to the release page to get it.");
        // (All of them, a line at a time: up and down while it is chosen, a finger or the mouse
        // dragging it, the wheel. Its rows: nScroll the first shown, nMax the last that can be.)
        struct NoteLine { std::string s; bool first, last, note; };
        std::vector<NoteLine> lines;
        for (const std::string& pnt : points)
        {
            const std::vector<std::string> ls = WrapLines(pnt, 15 * u, cw - (int)(64 * u));
            for (size_t k = 0; k < ls.size(); k++) lines.push_back({ ls[k], k == 0, k + 1 == ls.size(), false });
        }
        if (fromRelease) lines.push_back({ "The rest, and the downloads, are on the release page.", true, true, true });
        const int lineH = (int)(22 * u), gapH = (int)(12 * u), topPad = (int)(26 * u), cardBottom = bottom - ip;
        auto stepOf = [&](size_t k) { return lineH + (lines[k].last ? gapH : 0); };
        int total = 0;
        for (size_t k = 0; k < lines.size(); k++) total += stepOf(k);
        const int room = cardBottom - y - topPad;
        int maxStart = 0;
        if (total > room)
        {
            int hh = 0;
            size_t k = lines.size();
            while (k > 0 && hh + stepOf(k - 1) <= room) { hh += stepOf(k - 1); k--; }
            maxStart = (int)k;
        }
        F.nMax = maxStart;
        F.nScroll = std::max(0, std::min(F.nScroll, maxStart));
        const bool chosen = F.prow == 2 && F.side < 0;
        const int cardH = total > room ? cardBottom - y : std::min(total + topPad, cardBottom - y);
        if (chosen)
        {
            Glass::Glow(c, cx, y, cw, cardH, 14 * u, u);
            Glass::Round(c, cx, y, cw, cardH, 14 * u, 0x24FFFFFFu, Menu::GlassChosenEdge, 1.5f * u);
        }
        else Glass::Round(c, cx, y, cw, cardH, 14 * u, 0x14FFFFFFu);
        F.nX = cx; F.nY = y; F.nW = cw; F.nH = cardH; F.nLine = lineH;
        int py = y + topPad;
        for (size_t k = (size_t)F.nScroll; k < lines.size(); k++)
        {
            if (py + lineH / 2 > y + cardH - (int)(6 * u)) break;
            if (lines[k].first && !lines[k].note) Glass::Round(c, cx + (int)(18 * u), py - (int)(3 * u), (int)(6 * u), (int)(6 * u), 3 * u, 0xE6FFFFFFu);
            GT(c, cx + (int)(36 * u), py, lines[k].s, 15 * u, lines[k].note ? hint : text, lines[k].note);
            py += stepOf(k);
        }
        if (maxStart > 0)
        {
            // (The scroll bar.)
            const int tx = cx + cw - (int)(14 * u), ty = y + (int)(14 * u), th = cardH - (int)(28 * u);
            Glass::Round(c, tx, ty, (int)(4 * u), th, 2 * u, 0x26FFFFFFu);
            const int thumbH = std::max((int)(24 * u), (int)((long long)th * room / std::max(1, total)));
            const int thumbY = ty + (int)((long long)(th - thumbH) * F.nScroll / maxStart);
            Glass::Round(c, tx, thumbY, (int)(4 * u), thumbH, 2 * u, 0xCCFFFFFFu);
        }
        // What to do.
        const int rx0 = R - rw, rowH = (int)(50 * u), rg = (int)(6 * u);
        Menu::FrostBox(c, solid, rx0, top, rw, bottom - top, 18 * u);
        const std::string action = F.upState == 1 || F.upState == 4 ? "Working..." : F.upState == 3 ? (F.upCan ? "Install Now" : "Open Download") : "Check Now";
        const std::string rows[2] = { action, "Release Page" };
        const int icons[2] = { Glass::I_LOAD, Glass::I_GLOBE };
        F.pX = rx0 + pad; F.pY = top + pad; F.pW = rw - 2 * pad; F.pRow = rowH + rg;
        for (int i = 0; i < 2; i++)
        {
            const int rx = rx0 + pad, ry = top + pad + i * (rowH + rg), rww = rw - 2 * pad;
            if (i == F.prow)
            {
                if (F.side < 0) Glass::Glow(c, rx, ry, rww, rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rww, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            Glass::DrawIcon(c, icons[i], rx + 24 * u, ry + rowH * 0.5f, 10 * u, text);
            GT(c, rx + (int)(48 * u), ry + rowH * 0.5f, rows[i], 17 * u, text, true);
        }
        int hy = top + pad + 2 * (rowH + rg) + (int)(16 * u);
        for (const std::string& l : WrapLines(fromRelease ? "Fulldex checks once each time it starts. Your ROMs, saves and settings stay when it updates, and the version before is kept."
                                                          : "Fulldex checks once each time it starts.", 14 * u, rw - 2 * pad - (int)(16 * u)))
        { GT(c, rx0 + pad + (int)(8 * u), hy, l, 14 * u, hint, false); hy += (int)(20 * u); }
        GT(c, rx0 + pad + (int)(8 * u), hy + 6 * u, "github.com/mamesj/pokemon-fulldex/releases", 13 * u, hint, true);
        return;
    }
    // The sections.
    const int lw = (int)(300 * u), pad = (int)(12 * u), rowH = (int)(50 * u), rg = (int)(6 * u);
    Menu::FrostBox(c, solid, L, top, lw, bottom - top, 18 * u);
    static const char* names[] = { "Support", "Credits", "Report A Bug", "Licences", "Website" };
    static const int icons[] = { Glass::I_SPARKLE, Glass::I_PEOPLE, Glass::I_INFO, Glass::I_LAYERS, Glass::I_GLOBE };
    F.pX = L + pad; F.pY = top + pad; F.pW = lw - 2 * pad; F.pRow = rowH + rg;
    for (int i = 0; i < 5; i++)
    {
        const int rx = L + pad, ry = top + pad + i * (rowH + rg), rw = lw - 2 * pad;
        if (i == F.prow)
        {
            if (F.side < 0) Glass::Glow(c, rx, ry, rw, rowH, 12 * u, u);
            Glass::Round(c, rx, ry, rw, rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
        }
        Glass::DrawIcon(c, icons[i], rx + 24 * u, ry + rowH * 0.5f, 10 * u, text);
        GT(c, rx + (int)(48 * u), ry + rowH * 0.5f, names[i], 17 * u, text, true);
    }
    {
        const std::vector<std::string> lines = WrapLines("Fulldex doesn't include any games. Play with ROMs from games you own.", 13 * u, lw - 2 * pad - (int)(28 * u));
        const int ch = (int)(66 * u) + (int)lines.size() * (int)(18 * u);
        const int cx = L + pad, cy = bottom - pad - ch, cw = lw - 2 * pad;
        Glass::Round(c, cx, cy, cw, ch, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
        GT(c, cx + (int)(14 * u), cy + 24 * u, "Fulldex", 20 * u, text, true);
        GT(c, cx + (int)(14 * u), cy + 48 * u, PlatformLabel(), 14 * u, hint, true);
        int ly = cy + (int)(70 * u);
        for (const std::string& l : lines) { GT(c, cx + (int)(14 * u), ly, l, 13 * u, hint, false); ly += (int)(18 * u); }
    }
    // The chosen section.
    const int px = L + lw + (int)(18 * u), pw = R - px, ip = (int)(22 * u);
    Menu::FrostBox(c, solid, px, top, pw, bottom - top, 18 * u);
    int y = top + ip;
    auto heading = [&](const char* h) { GT(c, px + ip, y + 14 * u, h, 22 * u, text, true); y += (int)(46 * u); };
    auto para = [&](const std::string& s) {
        for (const std::string& l : WrapLines(s, 16 * u, pw - 2 * ip)) { GT(c, px + ip, y + 11 * u, l, 16 * u, text, false); y += (int)(24 * u); }
        y += (int)(10 * u);
    };
    auto card = [&](int x, int cy, int w, int h, const std::string& a, const std::string& b) {
        Glass::Round(c, x, cy, w, h, 12 * u, 0x14FFFFFFu);
        float fa = 16 * u, fb = 13 * u;
        while (fa > 10 * u && GW(a, fa, true) > w - (int)(28 * u)) fa -= u;
        while (fb > 9 * u && GW(b, fb, true) > w - (int)(28 * u)) fb -= u;
        GT(c, x + (int)(14 * u), cy + h * 0.36f, a, fa, text, true);
        GT(c, x + (int)(14 * u), cy + h * 0.70f, b, fb, hint, true);
    };
#ifdef __SWITCH__
    const char* open = "Open it on your phone or PC";
#else
    const char* open = "Choose it to open it in your browser";
#endif
    if (F.prow == 0)
    {
        heading("Support Fulldex");
        para("Hi, I'm James, AKA Mamesj, the mind behind Fulldex. I'm just a dude who wanted to play some Pok\xC3\xA9mon and got a bit carried away.");
        para("Fulldex is free and always will be. If it's given you some good hours, a coffee or a beer is always appreciated, never expected.");
        const int gap = (int)(16 * u), cw = (pw - 2 * ip - gap) / 2, ch = (int)(64 * u);
        const int qs = std::max((int)(80 * u), std::min(cw - (int)(60 * u), bottom - ip - (y + ch + (int)(14 * u)) - (int)(30 * u)));
        const char* names2[2] = { "Buy Me A Coffee", "PayPal" };
        const char* where[2] = { "buymeacoffee.com/mamesj", "paypal.me/mamesj" };
        for (int i = 0; i < 2; i++)
        {
            const int cx = px + ip + i * (cw + gap);
            card(cx, y, cw, ch, names2[i], where[i]);
            if (i == 0) DrawQr(c, kBeerQr, kBeerQrSize, cx + (cw - qs) / 2, y + ch + (int)(14 * u), qs);
            else DrawQr(c, kPaypalQr, kPaypalQrSize, cx + (cw - qs) / 2, y + ch + (int)(14 * u), qs);
        }
        y += ch + (int)(14 * u) + qs + (int)(14 * u);
#ifdef __SWITCH__
        GT(c, px + ip, y + 8 * u, "Scan a code with your phone's camera.", 13 * u, hint, true);
#else
        GT(c, px + ip, y + 8 * u, "Choose Support to open Buy Me A Coffee, or scan a code with your phone.", 13 * u, hint, true);
#endif
    }
    else if (F.prow == 1)
    {
        heading("Credits And Acknowledgements");
        static const char* credits[][2] = {
            { "ssrecomp by ClickingMaster", "The single-screen and widescreen view Fulldex grew from" },
            { "melonDS", "The DS emulation" },
            { "mGBA", "The GBA games, through Pok\xC3\xA9mon GBA Widescreen" },
            { "Pok\xC3\xA9" "Daisy by Lidor Itzhari", "Companion readers and text decoders" },
            { "pokebw2, aletheia, pret/pokeplatinum", "Function names from their decompilations" },
            { "DeadSkullzJr's cheat database", "The built-in DS cheats, via libretro-database" },
            { "sphaira", "How the HOME Menu icons are made" },
            { "WeeabooGod", "Cheats menu ideas (PR #16)" },
            { "Nils Menn", "Fucking legend" },
            { "Everyone who reported bugs and helped test", "Thank you" } };
        const int gap = (int)(12 * u), cw = (pw - 2 * ip - gap) / 2, ch = (int)(62 * u);
        for (int i = 0; i < 10; i++)
            card(px + ip + (i % 2) * (cw + gap), y + (i / 2) * (ch + (int)(10 * u)), cw, ch, credits[i][0], credits[i][1]);
    }
    else if (F.prow == 2)
    {
        heading("Report A Bug");
        para("Found something wrong? Open an issue on the GitHub page: say which game, which device, and what happened just before.");
        para("A screenshot helps, and so does game.log from the folder Fulldex runs from.");
        card(px + ip, y, pw - 2 * ip, (int)(64 * u), "github.com/mamesj/pokemon-fulldex/issues", open);
    }
    else if (F.prow == 3)
    {
        heading("Licences");
        static const char* lic[][2] = {
            { "Fulldex", "GPL-3.0-or-later" }, { "melonDS", "GPL-3.0" }, { "Pok\xC3\xA9" "Daisy readers", "GPL-3.0" },
            { "HOME Menu icon loader (nx-hbloader)", "GPL-3.0" }, { "mGBA", "MPL-2.0" }, { "Nunito and Noto fonts", "SIL Open Font License" } };
        const int rh = (int)(44 * u);
        for (int i = 0; i < 6; i++)
        {
            Glass::Round(c, px + ip, y, pw - 2 * ip, rh, 12 * u, 0x14FFFFFFu);
            GT(c, px + ip + (int)(14 * u), y + rh * 0.5f, lic[i][0], 16 * u, text, true);
            GT(c, px + pw - ip - (int)(14 * u) - GW(lic[i][1], 15 * u, true), y + rh * 0.5f, lic[i][1], 15 * u, hint, true);
            y += rh + (int)(6 * u);
        }
        y += (int)(10 * u);
        para("Not affiliated with or endorsed by Nintendo, Game Freak, The Pok\xC3\xA9mon Company or the melonDS project. Pok\xC3\xA9mon is a trademark of its owners. The full licence texts are on the GitHub page.");
    }
    else
    {
        heading("Website");
        para("Downloads, the latest changes and the full instructions are on the GitHub page.");
        card(px + ip, y, pw - 2 * ip, (int)(64 * u), "github.com/mamesj/pokemon-fulldex", open);
        y += (int)(84 * u);
        para("Tips are welcome but never expected: see Support, at the top of this page.");
    }
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
    const Game* bg = shown;
    if (!BackdropRom().empty())
        for (const Game& g : games)
            if (g.rom == BackdropRom()) { bg = &g; break; }
    GlassBackdrop(c, bg);
    const uint32_t text = Menu::GlassText, hint = Menu::GlassHint, dark = 0xFF18222Eu;
    FollowSelection(v, selected, scroll, follow);
    // (Everything but the chosen card, the game's menu, the dialogs and the hints is kept as drawn and
    // copied back while nothing on it changes; a press then redraws only the chosen card. The Switch
    // draws in software about 20 times slower than a PC.)
    auto sig = [](const Game& g) { return g.art.empty() ? std::string("-") : std::to_string(g.art[(size_t)Art * Art / 2 + Art / 2]); };
    std::string key = std::to_string(c.w) + "x" + std::to_string(c.h) + "/" + std::to_string(tab) + "/" + std::to_string(scroll) + "/"
                    + (bg ? bg->rom + "#" + sig(*bg) : std::string()) + "/"
                    + std::to_string(onTabs) + std::to_string(settingsChosen) + "/" + std::to_string(cont) + "/" + ContinueRom()
                    + "/t" + Focus().pageTitle + "/h" + HubSig() + (Focus().page == SIDE_HUB ? std::to_string(Mover().open) + std::to_string(HubMode()) + "," + std::to_string(Focus().pact) + "," + std::to_string(Focus().rScroll) : std::string()) + "/p" + std::to_string(Focus().page) + "," + std::to_string(Focus().page >= 0 ? Focus().prow : 0) + std::to_string(Focus().page >= 0 && Focus().side < 0) + "/s" + std::to_string(Focus().onSort) + SortKey() + "/q" + SearchText() + "/t" + TabsSig()
                    + (Focus().page == SIDE_UPDATES ? "/u" + std::to_string(Focus().upState) + std::to_string(Focus().upCan) + Focus().upHave + "/" + Focus().upNew + "/" + Focus().upText + "/" + Focus().upNotes + "/" + std::to_string(Focus().nScroll) : std::string())
                    + (Focus().page == SIDE_ROMS ? "/r" + std::to_string(Focus().pact) + "," + std::to_string(RomList().size()) + ","
                       + std::to_string(std::count_if(RomList().begin(), RomList().end(), [](const Game& g) { return g.hidden; })) : std::string());
    for (const Game& g : games) key += "|" + g.rom + g.customName + (g.favourite ? "*" : "") + sig(g);
    static std::vector<uint32_t> base;
    static std::string baseKey;
    bool solid = c.backdrop.size() == c.px.size();
    auto visible = [&](const View::Spot& s) { const int y = grid.top + s.y - scroll; return y >= grid.top - 2 && y + grid.ch <= grid.bottom + 2; };
    auto drawCard = [&](const View::Spot& s, bool sel) {
        const int x = s.x, y = grid.top + s.y - scroll;
        const Game& g = games[s.game];
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
        // (Its save kept in the cloud: a cloud in the corner; amber when it changed on both.)
        if (const int cl = HubCloudOf(g))
        {
            Glass::Round(c, ix + (int)(6 * u), iy + (int)(6 * u), (int)(32 * u), (int)(22 * u), 11 * u, 0xA0000000u);
            DrawCloud(c, ix + 22 * u, iy + 18 * u, 8 * u, cl == 2 ? 0xFFF2C46Bu : 0xFFFFFFFFu);
        }
    };
    // The sidebar's rows (the chosen one framed) and the banner's buttons: drawn over the kept picture
    // each time, as they change with the arrows.
    auto drawSideRows = [&]() {
        for (int i = 0; i < SIDES; i++)
        {
            const int ry = sp.firstRow + i * (sp.rowH + sp.rowGap), rx = sp.sideX + (int)(16 * u), rw = sp.sideW - (int)(32 * u);
            const int fs = Focus().side;
            const bool on = fs >= 0 ? i == fs : settingsChosen ? i == SIDE_SETTINGS : i == (Focus().page >= 0 ? Focus().page : (int)SIDE_HOME);
            const bool lit = fs >= 0 ? i == fs : settingsChosen && i == SIDE_SETTINGS; // (chosen with the arrows)
            if (on)
            {
                if (lit) Glass::Glow(c, rx, ry, rw, sp.rowH, 12 * u, u);
                Glass::Round(c, rx, ry, rw, sp.rowH, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            Glass::DrawIcon(c, SideIcon(i), rx + 27 * u, ry + sp.rowH * 0.5f, 12 * u, text);
            GT(c, rx + (int)(52 * u), ry + sp.rowH * 0.5f, SideName(i), 19 * u, text, true);
        }
    };
    auto drawBannerButtons = [&]() {
        if (cont < 0 || cont >= (int)games.size() || Focus().page >= 0) return;
        const bool played = !ContinueRom().empty() && games[cont].rom == ContinueRom();
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
    };
    auto moreBelow = [&]() {
        if (scroll + (grid.bottom - grid.top) < v.height - grid.gap)
            Glass::Round(c, sp.tabsX + grid.cols * grid.colStep / 2 - (int)(16 * u), grid.bottom + (int)(4 * u), (int)(32 * u), (int)(4 * u), 2 * u, 0x80FFFFFFu);
    };
    if (key == baseKey && base.size() == c.px.size())
    {
        std::copy(base.begin(), base.end(), c.px.begin());
        Glass::Mark(c, 0, 0, c.w, c.h);
    }
    else
    {
    solid = Glass::Backdrop(c);

    // The sidebar: the sections, Home chosen (Settings while it is chosen with the arrows).
    // (Frosted, square, with a soft shadow and a faint line where it meets the rest of the screen.)
    Glass::Shadow(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, 18 * u, 0x60);
    if (solid) Glass::Frost(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0);
    else Glass::Round(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, Menu::GlassShade);
    Glass::Round(c, sp.sideX, sp.sideY, sp.sideW, sp.sideH, 0, 0x30101826);
    Glass::Round(c, sp.sideX + sp.sideW - std::max(1, (int)u), sp.sideY, std::max(1, (int)u), sp.sideH, 0, 0x3DFFFFFF);
    // (Its rows: drawSideRows, over the kept picture.)

    // The tabs, as pills: the shown one white.
    {
        // (A bar along the top, flush with the screen's top and right edges, meeting the sidebar.)
        const int barX = sp.sideX + sp.sideW, barW = c.w - barX, barH = sp.tabsY * 2 + sp.tabsH, line = std::max(1, (int)u);
        if (solid) Glass::Frost(c, barX, 0, barW, barH, 0);
        else Glass::Round(c, barX, 0, barW, barH, 0, Menu::GlassShade);
        Glass::Round(c, barX, 0, barW, barH, 0, 0x30101826);
        Glass::Round(c, barX, barH - line, barW, line, 0, 0x3DFFFFFF);
        if (Focus().page < 0) for (int t = 0; t < TabCount(); t++)
        {
            const int px = sp.tabX[t], py = sp.tabsY + (int)(4 * u), pw = sp.tabW[t], ph = sp.tabsH - (int)(8 * u);
            const std::string name = TabLabel(t);
            if (t == tab)
            {
                if (onTabs && !Focus().onSort) Glass::Glow(c, px, py, pw, ph, ph * 0.5f, u);
                Glass::Round(c, px, py, pw, ph, ph * 0.5f, 0xFFFFFFFFu);
            }
            GT(c, px + (pw - GW(name, sp.tabFont, true)) / 2, py + ph * 0.5f, name, sp.tabFont, t == tab ? dark : text, true);
        }
    }

    // A page of the sidebar (About) in place of the games.
    if (Focus().page >= 0) DrawPage(c, sp, solid, u);

    // The banner: the game played last, ready to carry on.
    if (Focus().page < 0 && cont >= 0 && cont < (int)games.size())
    {
        const Game& g = games[cont];
        Menu::FrostBox(c, solid, sp.bannerX, sp.bannerY, sp.bannerW, sp.bannerH, 18 * u);
        const int ax = sp.bannerX + (int)(16 * u), ay = sp.bannerY + (int)(16 * u), ah = sp.bannerH - (int)(32 * u), aw = ah;
        DrawCover(c, g, ax, ay, aw, ah, 12 * u);
        const int tx = ax + aw + (int)(24 * u);
        const bool played = !ContinueRom().empty() && g.rom == ContinueRom();
        const std::string meta = std::string(played ? "Continue Playing" : "Ready To Play") + "   \xC2\xB7   " + PlatformName(g.platform)
                               + "   \xC2\xB7   " + (HubCloudOf(g) == 2 ? "Cloud Save: Changed On Both" : HubCloudOf(g) ? "Cloud Save" : g.hasSave ? "Save Found" : "New Game");
        GT(c, tx, sp.bannerY + 30 * u, meta, 16 * u, hint, true);
        GT(c, tx, sp.bannerY + 66 * u, ShortName(g), 36 * u, text, true);
        // (Continue and Game Options: drawBannerButtons, over the kept picture.)
    }

    if (Focus().page < 0)
    {
    // "Your Games" and how many.
    GT(c, sp.tabsX, sp.headY + 14 * u, SearchText().empty() ? std::string("Your Games") : "Results For \"" + SearchText() + "\"", 24 * u, text, true);
    {
        const std::string n = std::to_string(v.spots.size()) + (v.spots.size() == 1 ? " Game" : " Games");
        GT(c, sp.bannerX + sp.bannerW - GW(n, 16 * u, true), sp.headY + 14 * u, n, 16 * u, hint, true);
    }

    // The cards (none chosen: the chosen one is drawn over them below).
    for (const View::Head& h : v.heads)
        if (h.empty)
        {
            const std::string none = !SearchText().empty() ? "No Games Match \"" + SearchText() + "\""
                                   : tab >= TABS ? std::string("Add Games To This Tab From A Game's Menu (A On A Game)")
                                   : tab == TAB_HOME ? "Add Games To Home From A Game's Menu (A On A Game)"
                                   : tab == TAB_HACKS ? "No Romhacks Yet" : "No " + std::string(TabLabel(tab)) + " Games Yet";
            GT(c, sp.tabsX + (grid.cols * grid.colStep - GW(none, 16 * u, true)) / 2, (grid.top + grid.bottom) * 0.5f, none, 16 * u, hint, true);
        }
    for (const View::Spot& s : v.spots)
        if (visible(s)) drawCard(s, false);
    moreBelow();
    }
    base = c.px;
    baseKey = key;
    }
    drawSideRows();
    drawBannerButtons();
    // The tab bar's tools: + (a tab of your own) after the tabs, Search and Sort at its right end
    // (chosen: right past the last tab, then along them).
    for (int k = 1; k < 4; k++) Focus().toolW[k] = 0;
    if (Focus().page < 0 && !sp.tabX.empty())
    {
        const int barH = sp.tabsY * 2 + sp.tabsH, bh = (int)(40 * u), by = (barH - bh) / 2;
        auto tool = [&](int k, int bx, int bw, const std::string& label, int icon) {
            GlassFocus& F = Focus();
            F.toolX[k] = bx; F.toolW[k] = bw; F.toolY = by; F.toolH = bh;
            if (onTabs && F.onSort && F.tool == k)
            {
                Glass::Glow(c, bx, by, bw, bh, 12 * u, u);
                Glass::Round(c, bx, by, bw, bh, 12 * u, Menu::FrostChosen, Menu::GlassChosenEdge, 1.5f * u);
            }
            else Glass::Round(c, bx, by, bw, bh, 12 * u, Menu::GlassRow, 0x3DFFFFFF, 1.0f * u);
            if (icon >= 0)
            {
                Glass::DrawIcon(c, icon, bx + 22 * u, by + bh * 0.5f, 9 * u, text);
                GT(c, bx + (int)(40 * u), by + bh * 0.5f, label, 15 * u, text, true);
            }
            else
            {
                const float px = label == "+" ? 24 * u : 15 * u;
                GT(c, bx + (bw - GW(label, px, true)) / 2, by + bh * 0.5f, label, px, text, true);
            }
        };
        const std::string sortL = std::string("Sort: ") + SortLabel(SortKey());
        const int sortW = GW(sortL, 15 * u, true) + (int)(52 * u), sortX = c.w - (int)(28 * u) - sortW;
        std::string q = SearchText();
        if (q.size() > 14) q = q.substr(0, 13) + "...";
        const std::string searchL = q.empty() ? std::string("Search") : "\"" + q + "\"";
        const int searchW = GW(searchL, 15 * u, true) + (int)(36 * u), searchX = sortX - (int)(10 * u) - searchW;
        const int last = (int)sp.tabX.size() - 1;
        tool(1, sp.tabX[last] + sp.tabW[last] + (int)(10 * u), bh, "+", -1);
        tool(2, searchX, searchW, searchL, -1);
        tool(3, sortX, sortW, sortL, Glass::I_LAYERS);
    }
    // The chosen card, over the kept picture: the blurred background put back round it first.
    {
        const int k = v.SpotOf(selected);
        if (k >= 0 && visible(v.spots[k]) && Focus().page < 0)
        {
            const View::Spot& s = v.spots[k];
            const int m = (int)(7 * u), x0 = std::max(0, s.x - m), x1 = std::min(c.w, s.x + grid.cw + m);
            const int y = grid.top + s.y - scroll, y0 = std::max(0, y - m), y1 = std::min(c.h, y + grid.ch + m);
            if (solid)
                for (int yy = y0; yy < y1; yy++)
                    std::copy(c.backdrop.begin() + (size_t)yy * c.w + x0, c.backdrop.begin() + (size_t)yy * c.w + x1, c.px.begin() + (size_t)yy * c.w + x0);
            drawCard(s, true);
            moreBelow();
        }
    }

    // The game's menu (A on a game), beside its card: a glass panel of rows as the sidebar's.
    Focus().popW = 0;
    if (Focus().popup >= 0 && selected >= 0 && selected < (int)games.size())
    {
        const int k = v.SpotOf(selected);
        if (k >= 0)
        {
            static const int icons[] = { Glass::I_PLAY, Glass::I_POKEBALL, Glass::I_SPARKLE, Glass::I_WINDOW, Glass::I_SAVE, Glass::I_LAYERS, Glass::I_BACK };
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
                const uint32_t ink = i == 6 ? 0xFFF2A08Au : text;
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
    // (Rename: the keyboard page. A tab of your own's menu: drawn with no game chosen too, from the tab bar.)
    if (Focus().dialog && Focus().dialog != 3 && ((selected >= 0 && selected < (int)games.size()) || Focus().dialog >= 8))
    {
        GlassFocus& F = Focus();
        static const Game none;
        const Game& g = selected >= 0 && selected < (int)games.size() ? games[selected] : none;
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
        else if (F.dialog >= 5) { title = F.dTitle; msg = F.dMsg; rows = F.dItems; red = F.dialog == 6 ? 0 : -1; }
        else if (F.dialog == 4) { title = "Change Picture"; msg = "Pictures stay on this device. Only use pictures you have permission to use."; rows = { "Import A Picture", "Find Artwork Online", "Plain Card", "Restore Original", "Cancel" }; }
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
        const int ph = pad + (int)(36 * u) + (int)lines.size() * (int)(22 * u) + (int)(16 * u) + (F.dialog == 3 ? fieldH + rg : 0) + (F.dialog == 4 ? (int)(150 * u) + 2 * rg : 0) + (F.dialog == 10 ? (int)(200 * u) + 2 * rg : 0)
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
        if (F.dialog == 4)
        {
            // (The picture it has now.)
            const int ps = (int)(150 * u);
            DrawCover(c, g, px + (pw - ps) / 2, y, ps, ps, 14 * u);
            y += ps + 2 * rg;
        }
        for (const std::string& l : lines) { GT(c, px + pad, y + 10 * u, l, 15 * u, hint, false); y += (int)(22 * u); }
        y += (int)(16 * u);
        if (F.dialog == 10)
        {
            // (The beer message: its code, to scan with a phone.)
            const int qs = (int)(200 * u);
            DrawQr(c, kBeerQr, kBeerQrSize, px + (pw - qs) / 2, y, qs);
            y += qs + 2 * rg;
        }
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
    for (int t = 0; t < TabCount() && t < (int)s.tabX.size(); t++)
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
            if (ok && ec)
            {
                // (The Switch's SD card does not rename over a file that is there: the old one goes first.)
                std::error_code gone;
                std::filesystem::remove(path, gone);
                ec.clear();
                std::filesystem::rename(tmp, path, ec);
            }
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

// The settings page as the pause menu's glass settings pages draw it (Menu::style 1, ui_settings.h):
// a back row, then the groups (each opens its options) or one group's options (On / Off, a choice
// of values, something to do, or something to read), each with what it does beside the rows.
// map: each row's place in p.items (-1: the back row). onBack: the back row chosen.
// A settings description (written in capitals) in sentence case for the glass page: lower case but for
// each sentence's first letter and the names kept as they are written (ROM, SD, HOME, GitHub...). A
// lone A stays a capital only as the button (A:, A PLAYS, A / B, THE A BUTTON); else it is "a".
inline std::string SettingSentence(const std::string& s)
{
    for (char ch : s) if (ch >= 'a' && ch <= 'z') return s;
    static const char* const keep[] = { "FPS", "HUD", "GBA", "DS", "3D", "2D", "PC", "OK", "B", "X", "Y", "L", "R", "ZL", "ZR", "MB", "GPU",
                                        "CPU", "JIT", "LCD", "ROM", "ROMS", "SD", "HOME", "USB", "AUTO", "PS", "NDS", "UI", "4X", "2X", "16:9" };
    static const std::pair<const char*, const char*> spelt[] = { { "POKEMON", "Pok\xC3\xA9mon" }, { "POKEDEX", "Pok\xC3\xA9" "dex" },
        { "GITHUB", "GitHub" }, { "RETROACHIEVEMENTS", "RetroAchievements" }, { "XBOX", "Xbox" }, { "JOY", "Joy" }, { "CON", "Con" },
        { "MISSIONCONTROL", "MissionControl" }, { "SYS", "sys" }, { "FULLDEX", "Fulldex" }, { "WINDOWS", "Windows" }, { "MAC", "Mac" },
        { "LINUX", "Linux" }, { "ANDROID", "Android" }, { "IPHONE", "iPhone" }, { "SWITCH", "Switch" }, { "STEAM", "Steam" } };
    std::string out;
    bool start = true;
    size_t i = 0;
    while (i < s.size())
    {
        size_t j = i;
        while (j < s.size() && ((s[j] >= 'A' && s[j] <= 'Z') || (s[j] >= '0' && s[j] <= '9') || (unsigned char)s[j] >= 0x80)) j++;
        if (j == i) { out += s[i]; if (s[i] == '.' || s[i] == '!' || s[i] == '?') start = true; i++; continue; }
        std::string w = s.substr(i, j - i);
        bool kept = false;
        for (const char* k : keep) if (w == k) kept = true;
        for (const auto& sp : spelt) if (w == sp.first) { w = sp.second; kept = true; }
        if (w == "A")
        {
            // (The button: before ":" or "/", before PLAYS / BUTTON, after "/" or THE.)
            size_t k = j;
            while (k < s.size() && s[k] == ' ') k++;
            const std::string next = s.substr(k, 7);
            const std::string before = out.size() >= 4 ? out.substr(out.size() - 4) : out;
            kept = (k < s.size() && (s[k] == ':' || s[k] == '/')) || next.compare(0, 5, "PLAYS") == 0 || next.compare(0, 6, "BUTTON") == 0 ||
                   (!out.empty() && out.find_last_not_of(' ') != std::string::npos && out[out.find_last_not_of(' ')] == '/') || before == "the ";
        }
        if (!kept)
            for (size_t k = start ? 1 : 0; k < w.size(); k++)
                if (w[k] >= 'A' && w[k] <= 'Z') w[k] += 32;
        out += w;
        start = false;
        i = j;
    }
    return out;
}
// The icon a settings row has on the glass page: its group's for a group, else its own (or its group's).
inline int SettingIcon(const Setting& s, int group)
{
    static const int groupIcon[] = { Glass::I_MONITOR, Glass::I_SPEAKER, Glass::I_LAYERS, Glass::I_PAD, Glass::I_WINDOW,
                                     Glass::I_GRID, Glass::I_TROPHY, Glass::I_INFO, Glass::I_SPARKLE };
    if (s.key && !strncmp(s.key, "!group", 6)) { const int g = atoi(s.key + 6); return g >= 0 && g < 9 ? groupIcon[g] : Glass::I_GEAR; }
    if (!s.key) return Glass::I_INFO;
    static const std::pair<const char*, int> own[] = {
        { "view", Glass::I_LAYOUT }, { "large_screen", Glass::I_EXPAND }, { "fullscreen", Glass::I_FULLSCREEN }, { "smooth", Glass::I_SCREENS },
        { "battle_buttons", Glass::I_PAD }, { "show_fps", Glass::I_GAUGE }, { "fps60", Glass::I_FAST }, { "wide", Glass::I_RATIO },
        { "wide_map", Glass::I_GLOBE }, { "wide_lines", Glass::I_LAYERS }, { "busy_detail", Glass::I_GAUGE }, { "ff_low_res", Glass::I_FAST },
        { "wide_gpu", Glass::I_CHIP }, { "volume", Glass::I_SPEAKER }, { "start_button", Glass::I_PAD }, { "buttons", Glass::I_PAD },
        { "pad_touch", Glass::I_PAD }, { "!keys", Glass::I_PAD }, { "!shortcuts", Glass::I_WINDOW }, { "!forwarders", Glass::I_WINDOW },
        { "list_lang", Glass::I_GLOBE }, { "ra", Glass::I_TROPHY }, { "!ra_login", Glass::I_PEOPLE }, { "!updates", Glass::I_LOAD } };
    for (const auto& o : own)
        if (!strcmp(s.key, o.first)) return o.second;
    if (!strncmp(s.key, "list_", 5)) return Glass::I_GRID;
    if (!strncmp(s.key, "extra_", 6)) return Glass::I_SPARKLE;
    return group >= 0 && group < 9 ? groupIcon[group] : Glass::I_GEAR;
}
// The settings as a page of the launcher: the page drawn (sidebar, title, buttons at the bottom), then
// the menu's rows and preview in it. The launcher's focus is as it was afterwards.
inline void DrawSettingsPage(Canvas& c, Menu& m, const std::vector<Game>& games, int tab, int scroll, const std::string& title,
                             const std::string& hint, const Theme& theme)
{
    const GlassFocus kept = Focus();
    GlassFocus& F = Focus();
    // ("GAME LIST" as "Game List", the names written as they are: RetroAchievements, FPS, ROMs.)
    std::string shown;
    bool start = true;
    for (char ch : title)
    {
        const unsigned char b = (unsigned char)ch;
        shown += b < 128 ? (char)(start ? toupper(b) : tolower(b)) : ch;
        start = b == ' ' || b == '/' || b == '-';
    }
    for (const auto& r : std::vector<std::pair<std::string, std::string>>{ { "Retroachievements", "RetroAchievements" }, { "Fps", "FPS" }, { "Roms", "ROMs" }, { "Rom ", "ROM " }, { "Gba", "GBA" }, { "Ds ", "DS " } })
        for (size_t at = shown.find(r.first); at != std::string::npos; at = shown.find(r.first, at + r.second.size())) shown.replace(at, r.first.size(), r.second);
    F.page = SIDE_SETTINGS; F.side = -1; F.popup = -1; F.dialog = 0; F.onSort = false; F.pageTitle = shown;
    F.sW = 0;
    Draw(c, games, tab, -1, scroll, std::string(), hint, theme, true, false, false);
    // (Where the page put them, kept: when nothing on the page changed it is copied, not drawn, and says nothing.)
    static int lane[6] = {};
    if (F.sW > 0) { lane[0] = F.sX; lane[1] = F.sY; lane[2] = F.sW; lane[3] = F.sH; lane[4] = F.sListW; lane[5] = F.sPvX; }
    m.laneX = lane[0]; m.laneY = lane[1]; m.laneW = lane[2]; m.laneH = lane[3]; m.laneListW = lane[4]; m.lanePvX = lane[5];
    Focus() = kept;
    m.Draw(c, title, std::string());
    m.laneX = -1;
}
inline void BuildSettingsMenu(Menu& m, std::vector<int>& map, const SettingsPage& p, bool onBack)
{
    m.items.clear();
    map.clear();
    m.open = true; m.style = 1; m.keyGrid = false; m.columns = 0; m.pageIcon = Glass::I_GEAR;
    m.items.push_back({ "BACK", "" });
    m.items.back().kind = Menu::K_BACK;
    map.push_back(-1);
    for (size_t i = 0; i < p.items.size(); i++)
    {
        const Setting& s = p.items[i];
        MenuItem it;
        it.label = s.name;
        it.desc = SettingSentence(s.text ? s.text : "");
        it.icon = SettingIcon(s, p.group);
        if (p.group < 0) it.kind = Menu::K_LINK;
        else if (!s.key) it.kind = Menu::K_PLAIN;
        else if (s.key[0] == '!') { it.kind = Menu::K_LINK; it.value = p.Shown(s); }
        else
        {
            const int vi = p.ValueIndex(s);
            bool onOff = s.values.size() == 2;
            for (const auto& v : s.values) onOff = onOff && (v.second == "ON" || v.second == "OFF");
            if (onOff)
            {
                it.kind = Menu::K_TOGGLE;
                it.choices = { "OFF", "ON" };
                it.choice = vi >= 0 && s.values[vi].second == "ON" ? 1 : 0;
            }
            else
            {
                it.kind = Menu::K_PICKER;
                it.value = p.Shown(s);
                for (const auto& v : s.values) it.choices.push_back(v.second);
                it.choice = vi;
            }
        }
        m.items.push_back(it);
        map.push_back((int)i);
    }
    m.selected = onBack ? 0 : std::max(0, std::min((int)m.items.size() - 1, p.selected + 1));
}

}
