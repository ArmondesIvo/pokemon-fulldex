#pragma once
// THE ARENA's pages (PokeHub's battles against the bot; battle_engine.h is the fight itself).
//  - The BATTLE tab (HubMode 9): your team (up to 6, chosen in My PC as for a trade, Pc().pick
//    with purpose 1), the bot's (6 of any species, at random, moves chosen for them: Arena::BotMon),
//    the difficulty, and FIGHT.
//  - The fight, over everything (BattleOn): the stadium, the two Pokémon, a stick on the left,
//    the five buttons on the right, the bars at the top; at the end the score, REMATCH and MENU.
// Nothing is written to any save: the Pokémon are read, never moved or changed.
// (Included inside namespace Launcher, by launcher.h, after the trades.)
#include "battle_engine.h"
#include "battle_trainers.h"
#ifdef SSR_POKEMON_HUB
// (PokeHub's bucket: the moving pictures under pmd/, the trainers under trainers/.)
#ifndef SSR_PMD_URL
#define SSR_PMD_URL "https://pub-9b8023f699314ac588c93abe6032db61.r2.dev/pmd"
#endif

struct BattleKey { std::string path; int box = 0, slot = 0; bool operator==(const BattleKey& o) const { return path == o.path && box == o.box && slot == o.slot; } };
struct BattleState
{
    std::vector<BattleKey> keys, botKeys;
    std::vector<Arena::MonSpec> mine, bot;
    bool loaded = false, fromTab = false;
    int diff = 1, sel = -1, botSel = -1;
    std::string msg;
    std::vector<PcHit> hits;
    // (The fight.)
    bool on = false, paused = false;
    Arena::Battle b;
    double acc = 0;
    std::chrono::steady_clock::time_point last;
    // (Fingers: the stick's and the buttons'; -1 none. The mouse is finger -2.)
    long long stickId = -1; float stickX = 0, stickY = 0, stickDX = 0, stickDY = 0;
    long long held[Arena::SLOTS] = { -1, -1, -1, -1, -1, -1 };
    bool pressed[Arena::SLOTS] = {};
    bool keys4[4] = {}; // (W A S D, on a PC)
    // (Where the buttons were drawn: x, y, r; and the pause, rematch and menu buttons.)
    float bx[Arena::SLOTS] = {}, by[Arena::SLOTS] = {}, br[Arena::SLOTS] = {};
    std::vector<PcHit> overHits;
    float u = 1;
    std::vector<uint32_t> bg; int bgW = 0, bgH = 0;
    float fcx = 0, fcy = 0, fk = 1; // (the field on the screen: its middle, arena units to pixels)
    // (Choosing the team: every Pokémon of each save together, a tab a save, no boxes.)
    struct PickMon { BattleKey k; int species = 0, level = 0; bool shiny = false; std::string name; };
    struct PickGame { std::string name; std::vector<PickMon> mons; };
    bool picking = false;
    std::vector<PickGame> pg;
    int pgTab = 0;
    std::vector<BattleKey> pickSel;
    float pickScroll = 0, pickMax = 0;
    bool down = false, dragged = false;
    int downX = 0, downY = 0;
    float scroll0 = 0;
    // (A team card held a moment, then dragged to another place: the order changed.)
    int downCard = -1, dragCard = -1, dragX = 0, dragY = 0;
    std::chrono::steady_clock::time_point downAt;
    // (Whom it fights: 0 a free battle, 1 the gyms, 2 the Elite Four; the region of the last two.)
    int mode = 0, region = 0;
    // (kind: 0 a free battle's, 1 a gym leader, 2 the Elite Four, 3 Red, 4 a gym's trainer.)
    struct Opp { std::string name, sprite; int kind = 0, index = 0, level = 50; } opp;
    // (A run, a gym's or the Elite Four's: its battles one after another.)
    struct Step { Opp opp; std::vector<Arena::MonSpec> team; int prize = 0; };
    std::vector<Step> steps;
    int runKind = 0, gymSel = 0;
    // (Choosing a famous one already beaten, for a free battle.)
    bool oppPick = false;
    int oppRegion = 0;
    // (Kept in pokehub-arena.txt: money, the bag, each region's badges and Elite wins.)
    bool arenaLoaded = false;
    int money = 3000;
    int bag[6] = {};
    int badges[8] = {}, champion[8] = {};
    bool redBeaten = false;
    int eliteBeat[8] = {}; // (each region's Elite Four and champion beaten, a bit each)
    // (Mega Stones: how many in the bag, each Megas() one; and which of the team holds one here, by
    // "path|box|slot". The arena's only: nothing goes into a save.)
    int stones[64] = {};
    std::map<std::string, int> equip;
    int shopTab = 0;
    // (The Elite Four's run: its step, the team's HP between battles; the item chosen to use.)
    bool eliteOn = false;
    int eliteStep = 0, itemSel = -1;
    std::vector<int> runHp, fightIdx;
    bool shop = false;
    // (The end of a fight, settled once: what it paid or cost, and a line about it.)
    bool settled = false;
    int delta = 0;
    std::string endNote;
};
inline BattleState& Bat() { static BattleState s; return s; }
enum { BT_PICK = 1, BT_REROLL, BT_DIFF, BT_FIGHT, BT_CARD, BT_BOTCARD, BT_PAUSE, BT_RESUME, BT_QUIT, BT_REMATCH, BT_MENU,
    BT_PG_TAB, BT_PG_MON, BT_PG_SLOT, BT_PG_CANCEL, BT_PG_DONE, BT_PG_CLEAR,
    BT_MODE, BT_REGION, BT_SHOP, BT_SHOP_BACK, BT_BUY, BT_ITEM, BT_PREP_MON, BT_NEXT, BT_GIVEUP, BT_GYM_RESET,
    BT_GYM_SEL, BT_OPP_OPEN, BT_OPP_BACK, BT_OPP_REGION, BT_OPP_TAKE, BT_EQUIP, BT_SHOP_TAB, BT_BUY_STONE };
inline std::string BattleTeamFile() { return HubDir() + "/pokehub-battle-team.txt"; }
inline void PmdWantTeams(); // (both teams' moving pictures fetched: below)
inline void PmdWant(const std::vector<int>& species);

// ---- What the arena keeps: money, the bag, badges ------------------------------------------------
inline std::string ArenaFile() { return HubDir() + "/pokehub-arena.txt"; }
inline void ArenaSave()
{
    const BattleState& B = Bat();
    const std::string tmp = ArenaFile() + ".tmp";
    if (FILE* f = fopen(tmp.c_str(), "wb"))
    {
        fprintf(f, "money\t%d\n", B.money);
        for (int i = 0; i < 6; i++) fprintf(f, "item\t%d\t%d\n", i, B.bag[i]);
        for (int r = 0; r < 8; r++) fprintf(f, "region\t%d\t%d\t%d\n", r, B.badges[r], B.champion[r]);
        fprintf(f, "red\t%d\n", B.redBeaten ? 1 : 0);
        for (int r = 0; r < 8; r++) fprintf(f, "elite\t%d\t%d\n", r, B.eliteBeat[r]);
        for (int i = 0; i < 64; i++) if (B.stones[i]) fprintf(f, "stone\t%d\t%d\n", i, B.stones[i]);
        for (const auto& e : B.equip) fprintf(f, "equip\t%d\t%s\n", e.second, e.first.c_str());
        fclose(f);
        std::rename(tmp.c_str(), ArenaFile().c_str());
    }
}
inline void ArenaLoad()
{
    BattleState& B = Bat();
    if (B.arenaLoaded) return;
    B.arenaLoaded = true;
    if (FILE* f = fopen(ArenaFile().c_str(), "rb"))
    {
        char line[256];
        while (fgets(line, sizeof line, f))
        {
            int a = 0, b = 0, c = 0;
            if (sscanf(line, "money\t%d", &a) == 1) B.money = std::max(0, a);
            else if (sscanf(line, "item\t%d\t%d", &a, &b) == 2 && a >= 0 && a < 6) B.bag[a] = std::max(0, b);
            else if (sscanf(line, "region\t%d\t%d\t%d", &a, &b, &c) == 3 && a >= 0 && a < 8) { B.badges[a] = b; B.champion[a] = c; }
            else if (sscanf(line, "red\t%d", &a) == 1) B.redBeaten = a != 0;
            else if (sscanf(line, "elite\t%d\t%d", &a, &b) == 2 && a >= 0 && a < 8) B.eliteBeat[a] = b;
            else if (sscanf(line, "stone\t%d\t%d", &a, &b) == 2 && a >= 0 && a < 64) B.stones[a] = std::max(0, b);
            else if (sscanf(line, "equip\t%d\t", &a) == 1 && a >= 0)
            {
                std::string l = line;
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                const size_t t2 = l.find('\t', 6);
                if (t2 != std::string::npos) B.equip[l.substr(t2 + 1)] = a;
            }
        }
        fclose(f);
    }
}
inline std::string Money(int v)
{
    std::string d = std::to_string(std::abs(v)), o;
    for (size_t i = 0; i < d.size(); i++) { if (i && (d.size() - i) % 3 == 0) o += '.'; o += d[i]; }
    return std::string(v < 0 ? "-" : "") + "P$ " + o;
}

// ---- The trainers' pictures (Showdown's, in PokeHub's bucket under trainers/), fetched when needed --
struct ArtImg { int state = 0, w = 0, h = 0; std::vector<uint32_t> px; }; // state 0 asked, 1 ready, 2 none
struct ArtStore { std::mutex m; std::map<std::string, ArtImg> imgs; std::vector<std::string> queue; bool busy = false; };
inline ArtStore& TArt() { static ArtStore s; return s; }
inline void ArtWork()
{
    ArtStore& S = TArt();
    for (;;)
    {
        std::string key;
        { std::lock_guard<std::mutex> l(S.m); if (S.queue.empty()) { S.busy = false; return; } key = S.queue.back(); S.queue.pop_back(); }
        std::string flat = key;
        std::replace(flat.begin(), flat.end(), '/', '_');
        const std::string dir = HubDir() + "/arena-art", path = dir + "/" + flat + ".png";
        std::string data;
        if (FILE* f = fopen(path.c_str(), "rb")) { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n); fclose(f); }
        if (data.empty())
        {
            Achievements::HttpCall c;
            c.url = std::string(SSR_PMD_URL).substr(0, std::string(SSR_PMD_URL).rfind('/')) + "/" + key + ".png";
            c.userAgent = "PokeHub"; c.timeout = 20;
            int status = 0; std::string err;
            if (Achievements::HttpRequest(c, status, data, err) && status == 200)
            {
                std::error_code ec; std::filesystem::create_directories(dir, ec);
                if (FILE* f = fopen(path.c_str(), "wb")) { fwrite(data.data(), 1, data.size(), f); fclose(f); }
            }
            else data.clear();
        }
        ArtImg img;
        int n = 0;
        if (!data.empty())
            if (unsigned char* px = stbi_load_from_memory((const unsigned char*)data.data(), (int)data.size(), &img.w, &img.h, &n, 4))
            {
                img.px.resize((size_t)img.w * img.h);
                for (int k = 0; k < img.w * img.h; k++) img.px[k] = (uint32_t)px[k * 4 + 3] << 24 | (uint32_t)px[k * 4] << 16 | (uint32_t)px[k * 4 + 1] << 8 | px[k * 4 + 2];
                stbi_image_free(px);
            }
        std::lock_guard<std::mutex> l(S.m);
        img.state = img.px.empty() ? 2 : 1;
        S.imgs[key] = std::move(img);
        Hub().version++;
    }
}
// A picture of the bucket's ("trainers/brock", "items/potion"), or null while it is fetched (asked
// for then).
inline const ArtImg* ArenaArt(const std::string& key)
{
    if (key.empty()) return nullptr;
    ArtStore& S = TArt();
    std::lock_guard<std::mutex> l(S.m);
    auto it = S.imgs.find(key);
    if (it != S.imgs.end()) return it->second.state == 1 ? &it->second : nullptr;
    S.imgs[key].state = 0;
    S.queue.push_back(key);
    if (!S.busy) { S.busy = true; std::thread(ArtWork).detach(); }
    return nullptr;
}
inline const ArtImg* TrainerArt(const std::string& key) { return key.empty() ? nullptr : ArenaArt("trainers/" + key); }
inline const ArtImg* ItemArt(const std::string& key) { return key.empty() ? nullptr : ArenaArt("items/" + key); }
// Drawn as large as fits in the box, its pixels square, standing on the box's foot.
inline void DrawArt(Canvas& c, const ArtImg& a, int x, int y, int w, int h, float alpha = 1)
{
    int x0 = a.w, y0 = a.h, x1 = -1, y1 = -1;
    for (int yy = 0; yy < a.h; yy++) for (int xx = 0; xx < a.w; xx++) if ((a.px[(size_t)yy * a.w + xx] >> 24) >= 128) { x0 = std::min(x0, xx); y0 = std::min(y0, yy); x1 = std::max(x1, xx); y1 = std::max(y1, yy); }
    if (x1 < 0) return;
    const int cw = x1 - x0 + 1, ch = y1 - y0 + 1;
    float sc = std::min((float)w / cw, (float)h / ch);
    if (sc > 1) sc = std::floor(sc);
    const int dw = (int)(cw * sc), dh = (int)(ch * sc), ox = x + (w - dw) / 2, oy = y + h - dh;
    for (int j = 0; j < dh; j++)
    {
        const int yy = oy + j;
        if (yy < 0 || yy >= c.h) continue;
        for (int i = 0; i < dw; i++)
        {
            const int xx = ox + i;
            if (xx < 0 || xx >= c.w) continue;
            const uint32_t p = a.px[(size_t)(y0 + (int)(j / sc)) * a.w + x0 + (int)(i / sc)];
            if ((p >> 24) < 128) continue;
            uint32_t& d = c.px[(size_t)yy * c.w + xx];
            if (alpha >= 1) d = p | 0xFF000000u;
            else { const float t = alpha; d = 0xFF000000u | (uint32_t)(((p >> 16) & 255) * t + ((d >> 16) & 255) * (1 - t)) << 16 | (uint32_t)(((p >> 8) & 255) * t + ((d >> 8) & 255) * (1 - t)) << 8 | (uint32_t)((p & 255) * t + (d & 255) * (1 - t)); }
        }
    }
}

// ---- The Pokémon: read from their saves -------------------------------------------------------
inline bool BattleSpec(PokemonHub::Save& s, int box, int slot, Arena::MonSpec& out, std::string* item = nullptr)
{
    PokemonHub::MonDetail d;
    if (!PokemonHub::Detail(s, box, slot, d) || d.mon.egg || !d.mon.species || Arena::NoPicture(d.mon.species)) return false;
    out = Arena::MonSpec();
    out.species = d.mon.species; out.shiny = d.mon.shiny;
    out.name = PcMonName(d.mon);
    out.types[0] = d.typeIds[0]; out.types[1] = d.typeIds[1];
    for (int i = 0; i < 6; i++) out.stats[i] = std::max(1, d.stats50[i]);
    for (int i = 0; i < 4; i++) out.moves[i] = d.moveIds[i];
    if (item) *item = d.item;
    return true;
}
inline std::string EquipKey(const BattleKey& k) { return k.path + "|" + std::to_string(k.box) + "|" + std::to_string(k.slot); }
// Its Mega (Megas()[idx]) given: stats as its Mega's (each the difference of the base stats on, at
// level 50; HP as it is), types, name and moving picture.
inline bool ApplyMega(Arena::MonSpec& m, int idx)
{
    if (idx < 0 || idx >= (int)Arena::Megas().size()) return false;
    const Arena::MegaInfo& mg = Arena::Megas()[idx];
    int b0[6], t0[2], b1[6], t1[2];
    if (mg.species != m.species || !PokemonHub::SpeciesFormBase(m.species, 0, b0, t0) || !PokemonHub::SpeciesFormBase(m.species, mg.form, b1, t1)) return false;
    m.mega = idx;
    for (int i = 0; i < 6; i++) m.megaStats[i] = i == 0 ? m.stats[0] : std::max(1, m.stats[i] + b1[i] - b0[i]);
    m.megaTypes[0] = t1[0]; m.megaTypes[1] = t1[1];
    m.megaName = "Mega " + MonName(PokemonHub::SpeciesNameOf(m.species)) + mg.suffix;
    m.megaKey = m.species + 10000 * mg.sprite;
    return true;
}
// (Each key's Pokémon, a save opened once for all of its; those gone since are left out.)
inline std::vector<Arena::MonSpec> BattleRead(std::vector<BattleKey>& keys)
{
    std::map<std::string, std::unique_ptr<PokemonHub::Save>> open;
    std::vector<Arena::MonSpec> out;
    std::vector<BattleKey> kept;
    for (const BattleKey& k : keys)
    {
        auto it = open.find(k.path);
        if (it == open.end()) { std::string err; it = open.emplace(k.path, TradeOpen(k.path, err)).first; }
        Arena::MonSpec m;
        std::string item;
        if (it->second && BattleSpec(*it->second, k.box, k.slot, m, &item))
        {
            // (Its Mega: by a stone given here, or one it holds in its save.)
            const auto eq = Bat().equip.find(EquipKey(k));
            if (eq != Bat().equip.end()) ApplyMega(m, eq->second);
            else
                for (int i = 0; i < (int)Arena::Megas().size(); i++)
                    if (item == Arena::Megas()[i].held) ApplyMega(m, i);
            out.push_back(m); kept.push_back(k);
        }
    }
    keys = kept;
    return out;
}
inline void BattleSaveTeam()
{
    if (FILE* f = fopen(BattleTeamFile().c_str(), "wb"))
    {
        for (const BattleKey& k : Bat().keys) fprintf(f, "%s\t%d\t%d\n", k.path.c_str(), k.box, k.slot);
        fclose(f);
    }
}
// A trainer's team, from species: moves chosen for each (Arena::BotMon), at its level.
inline std::vector<Arena::MonSpec> BattleTeamOf(const int* species, int n, int level, std::mt19937& rng)
{
    std::vector<Arena::MonSpec> t;
    for (int i = 0; i < n; i++)
    {
        const int sp = species[i];
        int base[6], types[2];
        if (sp <= 0 || Arena::NoPicture(sp) || !PokemonHub::SpeciesBase(sp, base, types)) continue;
        t.push_back(Arena::BotMon(sp, base, types, MonName(PokemonHub::SpeciesNameOf(sp)), rng, level));
    }
    return t;
}
// (An everyday trainer's Pokémon, theirs and a gym's: not too strong, as the player asked: base
// stats 500 at most in all, none above 130, so no walls like Doublade.)
inline bool EverydayOk(int sp)
{
    int base[6], types[2];
    if (sp <= 0 || sp == 292 || Arena::NoPicture(sp) || Arena::Legendary(sp) || !PokemonHub::SpeciesBase(sp, base, types)) return false;
    int total = 0;
    for (int b : base) { if (b > 130) return false; total += b; }
    return total <= 500;
}
inline int TeamSize(const Arena::Trainer& t) { int n = 0; while (n < 6 && t.team[n]) n++; return n; }
// A famous trainer's team: as the games have it, its moves too, when that is known
// (battle_trainer_teams.h); else its species with moves chosen by type. All at level 50.
inline std::vector<Arena::MonSpec> TrainerTeam(int region, const Arena::Trainer& t, std::mt19937& rng)
{
    const Arena::RealTeam* real = Arena::FindRealTeam(region, t.sprite);
    if (!real) { auto v = BattleTeamOf(t.team, TeamSize(t), 50, rng); for (auto& m : v) m.shiny = false; return v; }
    std::vector<Arena::MonSpec> out;
    for (int i = 0; i < real->count; i++)
    {
        const Arena::RealMon& rm = real->mons[i];
        int base[6], types[2];
        if (rm.species <= 0 || Arena::NoPicture(rm.species) || !PokemonHub::SpeciesBase(rm.species, base, types)) continue;
        Arena::MonSpec m = Arena::BotMon(rm.species, base, types, MonName(PokemonHub::SpeciesNameOf(rm.species)), rng, 50);
        bool any = false;
        for (int k = 0; k < 4; k++) any |= rm.moves[k] > 0;
        if (any) for (int k = 0; k < 4; k++) m.moves[k] = rm.moves[k];
        m.shiny = false; // (a famous one's team as the games have it: never shiny)
        out.push_back(m);
    }
    if (out.empty()) { out = BattleTeamOf(t.team, TeamSize(t), 50, rng); for (auto& m : out) m.shiny = false; }
    return out;
}
// The gym to fight now in a region: the first without its badge (-1: all eight won).
inline int NextGym(int region) { for (int i = 0; i < 8; i++) if (!(Bat().badges[region] >> i & 1)) return i; return -1; }
// (The way through: gyms open after the region before's Elite Four; an Elite Four with all eight
// badges; Red with Johto's eight.)
inline bool GymsOpen(int r) { return r == 0 || Bat().champion[r - 1] > 0; }
inline bool EliteOpen(int r) { return GymsOpen(r) && (Bat().badges[r] & 255) == 255; }
inline bool RedOpen() { return (Bat().badges[1] & 255) == 255; }
// (A famous one beaten is free to fight in a free battle: k 0..7 a gym, 8..12 the Elite Four.)
inline bool Beaten(int r, int k) { const BattleState& B = Bat(); return k < 8 ? (B.badges[r] >> k & 1) : (B.eliteBeat[r] >> (k - 8) & 1); }
// The species of a type, for a gym's trainers: no legendaries (base stats 580 or more), all with pictures.
inline const std::vector<int>& TypePool(int type)
{
    static std::map<int, std::vector<int>> cache;
    auto it = cache.find(type);
    if (it != cache.end()) return it->second;
    std::vector<int> v;
    for (int sp = 1; sp <= 649; sp++)
    {
        int base[6], types[2];
        if (Arena::NoPicture(sp) || !PokemonHub::SpeciesBase(sp, base, types)) continue;
        int total = 0;
        for (int b : base) total += b;
        if (total >= 300 && EverydayOk(sp) && (types[0] == type || types[1] == type)) v.push_back(sp);
    }
    return cache[type] = v;
}
inline int BadgeCount(int r) { int n = 0; for (int i = 0; i < 8; i++) n += Bat().badges[r] >> i & 1; return n; }
// (Why a fight is not open yet; "" when it is.)
inline std::string LockReason(int mode, int r)
{
    const auto& regs = Arena::Regions();
    if (mode == 0) return "";
    if (!GymsOpen(r)) return std::string("Bloqueado: ven\xC3\xA7" "a a Elite dos 4 de ") + regs[r - 1].name + " primeiro.";
    if (mode == 2 && !EliteOpen(r)) return std::string("Precisa das 8 ins\xC3\xADgnias de ") + regs[r].name + " (" + std::to_string(BadgeCount(r)) + "/8).";
    return "";
}
// Whom the bot is, by the mode: a free battle's (drawn at random), the next gym's leader, or the
// Elite Four's member of this step.
inline void BattleSetOpp(bool reroll = false)
{
    BattleState& B = Bat();
    std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    const auto& regs = Arena::Regions();
    B.region = std::max(0, std::min(B.region, (int)regs.size() - 1));
    const Arena::Region& R = regs[B.region];
    B.botSel = -1;
    if (B.eliteOn && B.eliteStep < (int)B.steps.size())
    {
        B.opp = B.steps[B.eliteStep].opp;
        B.bot = B.steps[B.eliteStep].team;
    }
    else if (B.mode == 1)
    {
        // (The gym chosen; 8 is Red, once Johto's eight are won.)
        const bool red = B.region == 1 && RedOpen();
        B.gymSel = std::max(0, std::min(B.gymSel, red ? 8 : 7));
        const Arena::Trainer& t = B.gymSel == 8 ? Arena::Red() : R.gyms[B.gymSel];
        B.opp = { t.name, t.sprite, B.gymSel == 8 ? 3 : 1, B.gymSel, 50 };
        B.bot = TrainerTeam(B.gymSel == 8 ? 1 : B.region, t, rng);
    }
    else if (B.mode == 2)
    {
        const Arena::Trainer& t = R.elite[0];
        B.opp = { t.name, t.sprite, 2, 0, 50 };
        B.bot = TrainerTeam(B.region, t, rng);
    }
    else if (reroll || B.opp.kind != 0 || B.bot.empty())
    {
        // (Three in ten a famous one already beaten, with their team; else an everyday trainer with
        // six at random.)
        std::vector<std::pair<int, const Arena::Trainer*>> famous;
        for (int r = 0; r < (int)regs.size(); r++)
            for (int k = 0; k < 13; k++) if (Beaten(r, k)) famous.push_back({ r, k < 8 ? &regs[r].gyms[k] : &regs[r].elite[k - 8] });
        if (B.redBeaten) famous.push_back({ 1, &Arena::Red() });
        if (!famous.empty() && rng() % 10 < 3)
        {
            const auto& f = famous[rng() % famous.size()];
            B.opp = { f.second->name, f.second->sprite, 0, 0, 50 };
            B.bot = TrainerTeam(f.first, *f.second, rng);
        }
        else
        {
            const auto& ev = Arena::EverydayTrainers();
            const Arena::Everyday& e = ev[rng() % ev.size()];
            B.opp = { std::string(e.title) + " " + Arena::RandomFirstName(rng()), e.sprite, 0, 0, 50 };
            // (Two to six, never a legendary.)
            const int want = 2 + (int)(rng() % 5);
            int sp[6], n = 0;
            for (int tries = 0; n < want && tries < 200; tries++)
            {
                const int x = 1 + (int)(rng() % PokemonHub::DEX_SPECIES);
                bool dup = !EverydayOk(x);
                for (int i = 0; i < n; i++) dup |= sp[i] == x;
                if (!dup) sp[n++] = x;
            }
            B.bot = BattleTeamOf(sp, n, 50, rng);
        }
    }
    TrainerArt(B.opp.sprite);
    PmdWantTeams();
    Hub().version++;
}
inline void BattleReroll() { BattleSetOpp(true); }
inline void BattleLoad(bool again = false)
{
    BattleState& B = Bat();
    if (B.loaded && !again) return;
    B.loaded = true;
    B.keys.clear();
    if (FILE* f = fopen(BattleTeamFile().c_str(), "rb"))
    {
        char line[2048];
        while (fgets(line, sizeof line, f))
        {
            std::string l = line;
            while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
            const size_t a = l.find('\t'), b = a == std::string::npos ? a : l.find('\t', a + 1);
            if (b == std::string::npos) continue;
            B.keys.push_back({ l.substr(0, a), atoi(l.c_str() + a + 1), atoi(l.c_str() + b + 1) });
        }
        fclose(f);
    }
    ArenaLoad();
    B.mine = BattleRead(B.keys);
    B.sel = B.mine.empty() ? -1 : 0;
    if (!B.eliteOn) BattleSetOpp(B.bot.empty());
    PmdWantTeams();
}
inline void OpenBattleTab()
{
    GlassFocus& F = Focus();
    OpenSide(SIDE_HUB);
    HubMode() = 9;
    Bat().fromTab = true;
    F.prow = 0; F.pact = -1; F.rScroll = 0;
    BattleLoad(true);
    Hub().version++;
}
// Choosing the team, the arena's own way: a tab for each save (the Bank first), all its Pokémon
// together (boxes and party), up to 6 marked, as few as 1.
inline void BattleOpenPicker()
{
    BattleState& B = Bat();
    B.pg.clear();
    for (const MoverGame& g : MoverGames())
    {
        std::string err;
        auto s = OpenMoverGame(g, err);
        if (!s) continue;
        BattleState::PickGame G;
        G.name = g.bank ? std::string("Banco") : PcShortName(g.name);
        auto add = [&](const PokemonHub::Mon& m, int box) {
            if (m.egg || !m.species) return;
            BattleState::PickMon p;
            p.k = { g.path, box, m.slot }; p.species = m.species; p.level = m.level; p.shiny = m.shiny; p.name = PcMonName(m);
            G.mons.push_back(p);
        };
        if (!s->IsBank()) for (const PokemonHub::Mon& m : PokemonHub::Party(*s)) add(m, PokemonHub::PARTY);
        for (int b = 0; b < s->Boxes(); b++) for (const PokemonHub::Mon& m : s->Box(b)) add(m, b);
        if (!G.mons.empty()) B.pg.push_back(std::move(G));
    }
    B.pgTab = 0; B.pickScroll = 0;
    B.pickSel = B.keys;
    B.picking = true;
    B.msg.clear();
    Hub().version++;
}
// (A page over the tab's, which the back button closes: the team's choosing, the shop, the opponents.)
inline bool BattlePicking() { const BattleState& B = Bat(); return B.picking || B.shop || B.oppPick; }
inline void BattlePickerDone(bool use)
{
    BattleState& B = Bat();
    if (!B.picking) { B.shop = false; B.oppPick = false; Hub().version++; return; }
    B.picking = false;
    if (use && !B.pickSel.empty())
    {
        // (A stone given to one that leaves the team: back to the bag.)
        for (auto it = B.equip.begin(); it != B.equip.end();)
        {
            bool kept = false;
            for (const BattleKey& k : B.pickSel) kept |= EquipKey(k) == it->first;
            if (!kept) { B.stones[it->second]++; it = B.equip.erase(it); } else ++it;
        }
        ArenaSave();
        B.keys = B.pickSel;
        B.mine = BattleRead(B.keys);
        B.sel = B.mine.empty() ? -1 : 0;
        BattleSaveTeam();
        PmdWantTeams();
    }
    Hub().version++;
}
// (The old way, through My PC's choosing: kept for a Pick of purpose 1.)
inline void BattleStartPick()
{
    if (Pc().pending) { Bat().msg = "Salve ou desfa\xC3\xA7" "a as mudan\xC3\xA7" "as do My PC antes."; return; }
    OpenMyPc();
    PcState& P = Pc();
    P.pick = PcState::Pick();
    P.pick.on = true; P.pick.need = 0; P.pick.purpose = 1;
    P.pick.title = "Escolha at\xC3\xA9 6 para a batalha";
    for (const BattleKey& k : Bat().keys) P.pick.chosen.emplace_back(k.path, k.box, k.slot);
    P.msg.clear();
}
inline void BattlePicked(bool use)
{
    PcState& P = Pc();
    const auto chosen = P.pick.chosen;
    P.pick = PcState::Pick();
    P.selSide = -1;
    BattleState& B = Bat();
    if (use)
    {
        B.keys.clear();
        for (const auto& c : chosen) B.keys.push_back({ std::get<0>(c), std::get<1>(c), std::get<2>(c) });
        B.mine = BattleRead(B.keys);
        B.sel = B.mine.empty() ? -1 : 0;
        BattleSaveTeam();
        PmdWantTeams();
    }
    const bool fromTab = B.fromTab;
    GlassFocus& F = Focus();
    OpenSide(SIDE_HUB);
    HubMode() = 9;
    B.fromTab = fromTab;
    F.prow = 0; F.pact = -1; F.rScroll = 0;
    Hub().version++;
}
inline bool BattleOn() { return Bat().on; }
inline void BattleBegin()
{
    BattleState& B = Bat();
    if (B.mine.empty()) { B.msg = "Escolha seu time primeiro."; return; }
    if (B.bot.empty()) BattleSetOpp(true);
    // (In the Elite Four's run, each with the HP it kept; the fainted stay out.)
    std::vector<Arena::MonSpec> team;
    B.fightIdx.clear();
    for (int i = 0; i < (int)B.mine.size(); i++)
    {
        Arena::MonSpec m = B.mine[i];
        if (B.eliteOn && i < (int)B.runHp.size()) { if (B.runHp[i] <= 0) continue; m.hp = B.runHp[i]; }
        team.push_back(m);
        B.fightIdx.push_back(i);
    }
    if (team.empty()) { B.msg = "Todos desmaiados: use um Reviver."; return; }
    const int diff = B.mode == 0 ? B.diff : B.opp.kind == 4 ? 1 : 2; // (a gym's trainers Normal; leaders, the Elite and Red Hard)
    B.b.Start(team, B.bot, diff, (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    B.on = true; B.paused = false; B.acc = 0; B.settled = false; B.delta = 0; B.endNote.clear();
    B.last = std::chrono::steady_clock::now();
    B.stickId = -1; B.stickDX = B.stickDY = 0;
    for (auto& h : B.held) h = -1;
    for (bool& p : B.pressed) p = false;
    for (bool& k : B.keys4) k = false;
    Hub().version++;
}
// A RUN, as the games' gyms and Elite Four: its battles one after another, the team's HP kept
// between them (items in the preparation). A gym: two of its trainers (Pokémon of its type), then
// its leader; the Elite Four: its four, then the champion.
inline void BattleStartRun(int kind)
{
    BattleState& B = Bat();
    std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    const Arena::Region& R = Arena::Regions()[B.region];
    B.steps.clear();
    if (kind == 1)
    {
        const Arena::Trainer& L = R.gyms[std::max(0, std::min(7, B.gymSel))];
        const std::vector<int>& pool = TypePool(L.type);
        const auto& ev = Arena::EverydayTrainers();
        for (int k = 0; k < 2; k++)
        {
            const Arena::Everyday& e = ev[rng() % ev.size()];
            int sp[6], n = 0;
            for (int tries = 0; n < 2 + k && tries < 100 && !pool.empty(); tries++)
            {
                const int x = pool[rng() % pool.size()];
                bool dup = false;
                for (int i = 0; i < n; i++) dup |= sp[i] == x;
                if (!dup) sp[n++] = x;
            }
            BattleState::Step st;
            st.opp = { std::string(e.title) + " " + Arena::RandomFirstName(rng()), e.sprite, 4, k, 50 };
            st.team = BattleTeamOf(sp, n, 50, rng);
            st.prize = Arena::GYM_TRAINER_PRIZE;
            if (!st.team.empty()) B.steps.push_back(st);
        }
        BattleState::Step st;
        st.opp = { L.name, L.sprite, 1, B.gymSel, 50 };
        st.team = TrainerTeam(B.region, L, rng);
        st.prize = Arena::GymPrize(B.gymSel);
        B.steps.push_back(st);
    }
    else
        for (int e = 0; e < 5; e++)
        {
            const Arena::Trainer& t = R.elite[e];
            BattleState::Step st;
            st.opp = { t.name, t.sprite, 2, e, 50 };
            st.team = TrainerTeam(B.region, t, rng);
            st.prize = Arena::ElitePrize(e);
            B.steps.push_back(st);
        }
    B.eliteOn = true; B.runKind = kind; B.eliteStep = 0; B.itemSel = -1;
    B.runHp.clear();
    for (const auto& m : B.mine) B.runHp.push_back(m.stats[0]);
    std::vector<int> want;
    for (const auto& st : B.steps) { for (const auto& m : st.team) want.push_back(m.species); TrainerArt(st.opp.sprite); }
    PmdWant(want);
    BattleSetOpp();
    BattleBegin();
}
// A fight's end, once: money; in a run the next step, or its end (a badge, a champion).
inline void BattleSettle()
{
    BattleState& B = Bat();
    if (B.settled) return;
    B.settled = true;
    const bool won = B.b.winner == 0;
    const auto& R = Arena::Regions()[B.region];
    const bool run = B.eliteOn && B.eliteStep < (int)B.steps.size();
    const int prize = run ? B.steps[B.eliteStep].prize : B.opp.kind == 3 ? Arena::RED_PRIZE : Arena::FREE_PRIZE;
    B.delta = won ? prize : -std::min(B.money, prize / 2);
    B.money += B.delta;
    if (B.opp.kind == 3 && won) { B.redBeaten = true; B.endNote = "Voc\xC3\xAA venceu o Red no Monte Prateado!"; }
    if (run)
    {
        for (size_t k = 0; k < B.fightIdx.size() && B.fightIdx[k] < (int)B.runHp.size(); k++) B.runHp[B.fightIdx[k]] = B.b.HpAfter(0, (int)k);
        if (!won) { B.eliteOn = false; B.endNote = B.runKind == 1 ? "O desafio do gin\xC3\xA1sio acabou." : "O desafio da Elite dos 4 acabou."; }
        else
        {
            if (B.opp.kind == 2) B.eliteBeat[B.region] |= 1 << B.opp.index;
            if (B.eliteStep + 1 >= (int)B.steps.size())
            {
                B.eliteOn = false;
                if (B.runKind == 1)
                {
                    B.badges[B.region] |= 1 << B.gymSel;
                    B.endNote = (B.badges[B.region] & 255) == 255 ? std::string("Todas as ins\xC3\xADgnias de ") + R.name + "! A Elite dos 4 est\xC3\xA1 liberada." : std::string("Ins\xC3\xADgnia de ") + B.opp.name + " conquistada!";
                }
                else { B.champion[B.region]++; B.endNote = std::string("Voc\xC3\xAA \xC3\xA9 o campe\xC3\xA3o de ") + R.name + "!"; }
            }
            else { B.eliteStep++; B.endNote = std::string("Pr\xC3\xB3ximo: ") + B.steps[B.eliteStep].opp.name + ". Use itens na prepara\xC3\xA7\xC3\xA3o."; }
        }
    }
    ArenaSave();
}
inline void BattleEnd()
{
    BattleState& B = Bat();
    B.on = false; B.paused = false;
    // (Back on the tab: the next one to fight, by the mode.)
    BattleSetOpp(false);
    Hub().version++;
}

// ---- Moving pictures: PMD SpriteCollab's (tools/battle/fetch_pmd.py), fetched when needed ----------
// One file a species (NNNN.pmd) from PokeHub's bucket, kept in <saves>/pmd; four animations (Idle,
// Walk, Attack, Hurt), each eight rows (the ways it can face) of frames. Until a species' file is
// here, its still picture is drawn instead.
enum { PA_IDLE, PA_WALK, PA_ATTACK, PA_HURT, PMD_ANIMS };
struct PmdAnim { int fw = 0, fh = 0, rows = 1, w = 0, h = 0, total = 0; std::vector<int> durs; std::vector<uint32_t> px; };
struct PmdMon { int state = 0; PmdAnim a[PMD_ANIMS]; int foot = 0; }; // state: 0 asked, 1 ready, 2 none
struct PmdStore
{
    std::mutex m;
    std::map<int, PmdMon> mons;
    std::vector<int> queue;
    bool busy = false;
};
inline PmdStore& Pmd() { static PmdStore s; return s; }
inline bool PmdParse(const std::string& data, PmdMon& out)
{
    if (data.compare(0, 5, "PMD1\n") != 0) return false;
    const size_t end = data.find("\n\n");
    if (end == std::string::npos) return false;
    size_t at = 5, png = end + 2;
    static const char* names[PMD_ANIMS] = { "Idle", "Walk", "Attack", "Hurt" };
    for (int i = 0; i < PMD_ANIMS; i++)
    {
        const size_t nl = data.find('\n', at);
        if (nl == std::string::npos || nl > end) return false;
        char name[16] = {}, durs[512] = {};
        int fw = 0, fh = 0, rows = 0;
        long len = 0;
        if (sscanf(data.substr(at, nl - at).c_str(), "%15s %d %d %d %511s %ld", name, &fw, &fh, &rows, durs, &len) != 6 || strcmp(name, names[i]) || len <= 0 || png + (size_t)len > data.size()) return false;
        PmdAnim& a = out.a[i];
        a.fw = fw; a.fh = fh; a.rows = std::max(1, rows);
        for (const char* p = durs; *p;) { a.durs.push_back(std::max(1, atoi(p))); a.total += a.durs.back(); while (*p && *p != ',') p++; if (*p) p++; }
        int n = 0;
        if (unsigned char* px = stbi_load_from_memory((const unsigned char*)data.data() + png, (int)len, &a.w, &a.h, &n, 4))
        {
            a.px.resize((size_t)a.w * a.h);
            for (int k = 0; k < a.w * a.h; k++) a.px[k] = (uint32_t)px[k * 4 + 3] << 24 | (uint32_t)px[k * 4] << 16 | (uint32_t)px[k * 4 + 1] << 8 | px[k * 4 + 2];
            stbi_image_free(px);
        }
        if (a.px.empty() || a.fw <= 0 || a.fh <= 0 || a.durs.empty()) return false;
        png += (size_t)len;
        at = nl + 1;
    }
    // (Where its feet are: the lowest drawn row of its first Idle frame, facing down, from the middle.)
    const PmdAnim& id = out.a[PA_IDLE];
    int low = id.fh / 2;
    for (int y = 0; y < id.fh && y < id.h; y++)
        for (int x = 0; x < id.fw && x < id.w; x++)
            if ((id.px[(size_t)y * id.w + x] >> 24) >= 128) low = y;
    out.foot = low - id.fh / 2;
    return true;
}
inline void PmdWork()
{
    PmdStore& S = Pmd();
    for (;;)
    {
        int sp = 0;
        {
            std::lock_guard<std::mutex> l(S.m);
            if (S.queue.empty()) { S.busy = false; return; }
            sp = S.queue.back();
            S.queue.pop_back();
        }
        char file[24];
        // (A form's key: species + 10000 x its folder, as Mega Charizard X 6 + 10000: 0006_0001.pmd.)
        if (sp >= 10000) snprintf(file, sizeof file, "%04d_%04d.pmd", sp % 10000, sp / 10000);
        else snprintf(file, sizeof file, "%04d.pmd", sp);
        const std::string dir = HubDir() + "/pmd", path = dir + "/" + file;
        std::string data;
        if (FILE* f = fopen(path.c_str(), "rb")) { char buf[65536]; size_t n; while ((n = fread(buf, 1, sizeof buf, f)) > 0) data.append(buf, n); fclose(f); }
        PmdMon mon;
        bool ok = !data.empty() && PmdParse(data, mon);
        if (!ok)
        {
            Achievements::HttpCall c;
            c.url = std::string(SSR_PMD_URL) + "/" + file;
            c.userAgent = "PokeHub";
            c.timeout = 20;
            int status = 0;
            std::string err;
            data.clear();
            if (Achievements::HttpRequest(c, status, data, err) && status == 200 && PmdParse(data, mon))
            {
                ok = true;
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                if (FILE* f = fopen((path + ".tmp").c_str(), "wb")) { fwrite(data.data(), 1, data.size(), f); fclose(f); std::rename((path + ".tmp").c_str(), path.c_str()); }
            }
            else fprintf(stderr, "[arena] no moving picture for %d (%d %s)\n", sp, status, err.c_str());
        }
        std::lock_guard<std::mutex> l(S.m);
        mon.state = ok ? 1 : 2;
        S.mons[sp] = std::move(mon);
        Hub().version++;
    }
}
// These species' pictures wanted: fetched (or read) one after another, off the screen's thread.
inline void PmdWant(const std::vector<int>& species)
{
    PmdStore& S = Pmd();
    std::lock_guard<std::mutex> l(S.m);
    for (int sp : species)
        if (sp > 0 && !S.mons.count(sp) && std::find(S.queue.begin(), S.queue.end(), sp) == S.queue.end()) { S.queue.insert(S.queue.begin(), sp); S.mons[sp].state = 0; }
    if (!S.busy && !S.queue.empty())
    {
        S.busy = true;
        std::thread(PmdWork).detach(); // (detached: a static joinable thread would end the app at exit)
    }
}
inline const PmdMon* PmdGet(int sp)
{
    PmdStore& S = Pmd();
    std::lock_guard<std::mutex> l(S.m);
    auto it = S.mons.find(sp);
    return it != S.mons.end() && it->second.state == 1 ? &it->second : nullptr; // (never changed once ready)
}
inline void PmdWantTeams()
{
    std::vector<int> sp;
    for (const Arena::MonSpec& m : Bat().mine) { sp.push_back(m.species); if (m.mega >= 0) sp.push_back(m.megaKey); }
    for (const Arena::MonSpec& m : Bat().bot) sp.push_back(m.species);
    PmdWant(sp);
}

// ---- Drawing helpers: plain pixels, blended -----------------------------------------------------
namespace BGfx
{
inline void Put(Canvas& c, int x, int y, uint32_t col)
{
    if ((unsigned)x >= (unsigned)c.w || (unsigned)y >= (unsigned)c.h) return;
    const uint32_t a = col >> 24;
    if (!a) return;
    uint32_t& d = c.px[(size_t)y * c.w + x];
    if (a >= 255) { d = col; return; }
    const uint32_t ia = 255 - a;
    const uint32_t r = (((col >> 16) & 255) * a + ((d >> 16) & 255) * ia) / 255, g = (((col >> 8) & 255) * a + ((d >> 8) & 255) * ia) / 255, b = ((col & 255) * a + (d & 255) * ia) / 255;
    d = 0xFF000000u | r << 16 | g << 8 | b;
}
inline uint32_t Alpha(uint32_t col, float a) { return ((uint32_t)std::max(0.0f, std::min(255.0f, a * (col >> 24))) << 24) | (col & 0xFFFFFFu); }
inline uint32_t Mix(uint32_t a, uint32_t b, float t)
{
    auto ch = [&](int s) { return (uint32_t)(((a >> s) & 255) * (1 - t) + ((b >> s) & 255) * t) << s; };
    return 0xFF000000u | ch(16) | ch(8) | ch(0);
}
inline void Rect(Canvas& c, int x, int y, int w, int h, uint32_t col)
{
    const int x0 = std::max(0, x), y0 = std::max(0, y), x1 = std::min(c.w, x + w), y1 = std::min(c.h, y + h);
    for (int yy = y0; yy < y1; yy++) for (int xx = x0; xx < x1; xx++) Put(c, xx, yy, col);
}
inline void Ellipse(Canvas& c, float cx, float cy, float rx, float ry, uint32_t col)
{
    if (rx <= 0 || ry <= 0) return;
    const int y0 = std::max(0, (int)(cy - ry)), y1 = std::min(c.h - 1, (int)(cy + ry));
    for (int y = y0; y <= y1; y++)
    {
        const float t = (y + 0.5f - cy) / ry;
        if (t * t > 1) continue;
        const float hw = rx * std::sqrt(1 - t * t);
        const int x0 = std::max(0, (int)(cx - hw + 0.5f)), x1 = std::min(c.w - 1, (int)(cx + hw - 0.5f));
        for (int x = x0; x <= x1; x++) Put(c, x, y, col);
    }
}
inline void Disc(Canvas& c, float cx, float cy, float r, uint32_t col) { Ellipse(c, cx, cy, r, r, col); }
// (A ring: an ellipse's edge, w wide.)
inline void Ring(Canvas& c, float cx, float cy, float rx, float ry, float w, uint32_t col)
{
    const int y0 = std::max(0, (int)(cy - ry - w)), y1 = std::min(c.h - 1, (int)(cy + ry + w));
    const int x0 = std::max(0, (int)(cx - rx - w)), x1 = std::min(c.w - 1, (int)(cx + rx + w));
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
        {
            const float dx = (x + 0.5f - cx) / rx, dy = (y + 0.5f - cy) / ry;
            const float d = (std::sqrt(dx * dx + dy * dy) - 1) * std::min(rx, ry);
            if (std::fabs(d) <= w * 0.5f) Put(c, x, y, col);
        }
}
// (A thick line, rounded at its ends: each row only where the line crosses it.)
inline void Seg(Canvas& c, float x0, float y0, float x1, float y1, float w, uint32_t col)
{
    const float r = w * 0.5f;
    const float dx = x1 - x0, dy = y1 - y0, l2 = std::max(1e-3f, dx * dx + dy * dy), L = std::sqrt(l2);
    const int by0 = std::max(0, (int)(std::min(y0, y1) - r)), by1 = std::min(c.h - 1, (int)(std::max(y0, y1) + r));
    const float minX = std::min(x0, x1) - r, maxX = std::max(x0, x1) + r;
    for (int y = by0; y <= by1; y++)
    {
        const float py = y + 0.5f - y0;
        float a = minX, b = maxX;
        if (std::fabs(dy) > 1e-3f)
        {
            // (Within r of the line: |px*dy - py*dx| <= r*L.)
            const float e0 = (py * dx - r * L) / dy + x0, e1 = (py * dx + r * L) / dy + x0;
            a = std::max(a, std::min(e0, e1)); b = std::min(b, std::max(e0, e1));
        }
        const int xa = std::max(0, (int)a), xb = std::min(c.w - 1, (int)b + 1);
        for (int x = xa; x <= xb; x++)
        {
            const float px = x + 0.5f - x0;
            const float t = std::max(0.0f, std::min(1.0f, (px * dx + py * dy) / l2));
            const float ex = px - t * dx, ey = py - t * dy;
            if (ex * ex + ey * ey <= r * r) Put(c, x, y, col);
        }
    }
}
// Text with a dark edge, centred on x.
inline void Text(Canvas& c, float x, float y, const std::string& s, float px, uint32_t col, bool bold = true, uint32_t edge = 0xFF101418u)
{
    const int w = GW(s, px, bold);
    const int x0 = (int)(x - w / 2.0f);
    const int o = std::max(1, (int)(px / 12));
    for (int dy = -o; dy <= o; dy += o) for (int dx = -o; dx <= o; dx += o) if (dx || dy) GT(c, x0 + dx, y + dy, s, px, edge, bold);
    GT(c, x0, y, s, px, col, bold);
}
// A Pokémon's picture standing on (cx, foot), h its picture's full height in pixels; flipped to
// face right (the pictures face left); white while hit; see-through while dashing.
inline void Mon(Canvas& c, int species, bool shiny, float cx, float foot, float h, bool faceRight, float white, float alpha, float squash = 1)
{
    int w = 0, hh = 0, bb[4] = {};
    const std::vector<uint32_t>* px = PcSprite(species, shiny, w, hh, bb, true);
    if (!px || w <= 0 || hh <= 0) { PcDrawMon(c, species, shiny, false, (int)(cx - h / 2), (int)(foot - h), (int)h, (int)h); return; }
    const float sc = h / hh;
    const float sx = sc / squash, sy = sc * squash;
    const int dw = std::max(1, (int)(bb[2] * sx)), dh = std::max(1, (int)(bb[3] * sy));
    // (Where its drawn part sits in the full picture, kept: a small one stays small.)
    const float midX = bb[0] + bb[2] * 0.5f - w * 0.5f;
    const int ox = (int)(cx + (faceRight ? -midX : midX) * sx - dw / 2.0f), oy = (int)(foot - dh);
    for (int j = 0; j < dh; j++)
    {
        const int yy = oy + j;
        if (yy < 0 || yy >= c.h) continue;
        const int syy = bb[1] + std::min(bb[3] - 1, (int)(j / sy));
        for (int i = 0; i < dw; i++)
        {
            const int xx = ox + i;
            if (xx < 0 || xx >= c.w) continue;
            int sxx = std::min(bb[2] - 1, (int)(i / sx));
            if (faceRight) sxx = bb[2] - 1 - sxx;
            uint32_t p = (*px)[(size_t)syy * w + bb[0] + sxx];
            if ((p >> 24) < 128) continue;
            p |= 0xFF000000u;
            if (white > 0) p = Mix(p, 0xFFFFFFFFu, std::min(1.0f, white));
            Put(c, xx, yy, alpha >= 1 ? p : Alpha(p, alpha));
        }
    }
}
}

// A PMD frame: anim, its row (way), the time into it (seconds); feet at (cx, foot), s pixels a pixel.
inline void PmdDraw(Canvas& c, const PmdMon& m, int anim, int row, float t, bool loop, float cx, float foot, float s, float white, float alpha)
{
    const PmdAnim& a = m.a[anim];
    int tick = (int)(t * 60), frame = 0;
    if (loop) tick %= std::max(1, a.total);
    for (int acc = 0; frame < (int)a.durs.size() - 1; frame++) { acc += a.durs[frame]; if (tick < acc) break; }
    const int r = a.rows > 1 ? row : 0;
    const int sx0 = frame * a.fw, sy0 = r * a.fh;
    if (sx0 + a.fw > a.w || sy0 + a.fh > a.h) return;
    const int dw = (int)(a.fw * s), dh = (int)(a.fh * s);
    const int ox = (int)(cx - dw / 2.0f), oy = (int)(foot - m.foot * s - dh / 2.0f);
    for (int j = 0; j < dh; j++)
    {
        const int yy = oy + j;
        if (yy < 0 || yy >= c.h) continue;
        const int syy = sy0 + std::min(a.fh - 1, (int)(j / s));
        for (int i = 0; i < dw; i++)
        {
            const int xx = ox + i;
            if (xx < 0 || xx >= c.w) continue;
            uint32_t p = a.px[(size_t)syy * a.w + sx0 + std::min(a.fw - 1, (int)(i / s))];
            if ((p >> 24) < 128) continue;
            p |= 0xFF000000u;
            if (white > 0) p = BGfx::Mix(p, 0xFFFFFFFFu, std::min(1.0f, white));
            BGfx::Put(c, xx, yy, alpha >= 1 ? p : BGfx::Alpha(p, alpha));
        }
    }
}
// (The eight ways, as SpriteCollab's rows: down, down-right, right, up-right, up, up-left, left, down-left.)
inline int PmdRow(float dx, float dy)
{
    if (std::fabs(dx) + std::fabs(dy) < 1e-3f) return 0;
    const float a = std::atan2(dx, dy);
    return ((int)std::lround(a / 0.785398f) + 8) % 8;
}

// ---- The moves' looks, a kind for each type (drawn, in the pictures' blocky way) ------------------
namespace BFx
{
using namespace Arena;
// (A blocky disc: squares of b pixels, as the sprites' pixels are.)
inline void Blob(Canvas& c, float cx, float cy, float r, uint32_t col, float b)
{
    b = std::max(1.0f, b);
    const int n = (int)std::ceil(r / b);
    for (int j = -n; j <= n; j++)
        for (int i = -n; i <= n; i++)
        {
            const float dx = (i + 0.5f) * b, dy = (j + 0.5f) * b;
            if (dx * dx + dy * dy > r * r) continue;
            BGfx::Rect(c, (int)(cx + i * b), (int)(cy + j * b), (int)b + 1, (int)b + 1, col);
        }
}
inline void Leaf(Canvas& c, float cx, float cy, float len, float ang, uint32_t col, float b)
{
    const float ca = std::cos(ang), sa = std::sin(ang);
    for (float t = -1; t <= 1; t += 0.25f)
    {
        const float w = (1 - t * t) * len * 0.35f;
        Blob(c, cx + ca * t * len, cy + sa * t * len, std::max(b * 0.6f, w), col, b);
    }
    BGfx::Seg(c, cx - ca * len, cy - sa * len, cx + ca * len, cy + sa * len, std::max(1.0f, b * 0.5f), 0xFF2E6B1Eu);
}
inline void Bolt(Canvas& c, float x0, float y0, float x1, float y1, float w, float seed, uint32_t col, float amp = 0.18f)
{
    float px = x0, py = y0;
    const float dx = x1 - x0, dy = y1 - y0, l = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    const float nx = -dy / l, ny = dx / l;
    // (Long ones in more, smaller steps: a jagged line, not a saw.)
    const int n = std::max(6, std::min(24, (int)(l / (amp < 0.1f ? 30.0f : 1e9f))));
    for (int i = 1; i <= n; i++)
    {
        const float t = (float)i / n, off = i == n ? 0 : std::sin(seed * 13.7f + i * 2.3f) * l * amp;
        const float qx = x0 + dx * t + nx * off, qy = y0 + dy * t + ny * off;
        BGfx::Seg(c, px, py, qx, qy, w * 1.8f, BGfx::Alpha(col, 0.5f));
        BGfx::Seg(c, px, py, qx, qy, w, 0xFFFFFFE0u);
        px = qx; py = qy;
    }
}
inline void Star(Canvas& c, float cx, float cy, float r, float ang, uint32_t col)
{
    for (int i = 0; i < 4; i++)
    {
        const float a = ang + i * 1.5708f;
        BGfx::Seg(c, cx, cy, cx + std::cos(a) * r, cy + std::sin(a) * r, std::max(2.0f, r * 0.28f), col);
    }
    BGfx::Disc(c, cx, cy, r * 0.3f, 0xFFFFFFFFu);
}
// A shot flying: type, where, its size, how long it has flown, which way.
inline void Shot(Canvas& c, int type, float x, float y, float r, float t, float dx, float dy, float px)
{
    const uint32_t tc = Arena::TypeColour(type);
    const float l = std::max(1e-3f, std::sqrt(dx * dx + dy * dy)), ux = dx / l, uy = dy / l;
    const float b = std::max(2.0f, px);
    switch (type)
    {
    case T_FIRE: case T_DRAGON:
    {
        const uint32_t outer = type == T_FIRE ? 0xFFE84818u : 0xFF5A30C8u, mid = type == T_FIRE ? 0xFFF89830u : 0xFF8C68F8u, in = type == T_FIRE ? 0xFFFFF0A0u : 0xFFE0D0FFu;
        for (int i = 3; i >= 0; i--)
        {
            const float f = 1 + 0.15f * std::sin(t * 40 + i * 2);
            Blob(c, x - ux * r * i * 0.7f, y - uy * r * i * 0.7f, r * (1.0f - i * 0.2f) * f, BGfx::Alpha(outer, 1 - i * 0.2f), b);
        }
        Blob(c, x, y, r * 0.7f, mid, b);
        Blob(c, x + ux * r * 0.2f, y + uy * r * 0.2f, r * 0.38f, in, b);
        break;
    }
    case T_WATER:
        for (int i = 0; i < 3; i++) Blob(c, x - ux * r * (1.2f + i * 0.9f) + std::sin(t * 20 + i) * r * 0.3f, y - uy * r * (1.2f + i * 0.9f), r * (0.35f - i * 0.08f), 0xC0A8D8FFu, b);
        Blob(c, x, y, r, 0xFF3878E8u, b);
        Blob(c, x - r * 0.25f, y - r * 0.25f, r * 0.55f, 0xFF78B0F8u, b);
        Blob(c, x - r * 0.35f, y - r * 0.4f, r * 0.2f, 0xFFFFFFFFu, b);
        break;
    case T_GRASS: case T_BUG:
        for (int i = 0; i < 3; i++)
        {
            const float a = t * 14 + i * 2.094f;
            Leaf(c, x + std::cos(a) * r * 0.6f, y + std::sin(a) * r * 0.6f, r * 0.75f, a + 0.8f, type == T_GRASS ? 0xFF58C040u : 0xFFA8C020u, b);
        }
        break;
    case T_ELECTRIC:
        Blob(c, x, y, r * 0.8f, 0xFFF8D030u, b);
        Blob(c, x, y, r * 0.4f, 0xFFFFFFF0u, b);
        for (int i = 0; i < 3; i++)
        {
            const float a = t * 30 + i * 2.1f;
            Bolt(c, x, y, x + std::cos(a) * r * 2, y + std::sin(a) * r * 2, std::max(2.0f, b * 0.7f), t * 9 + i, 0xFFF8D030u);
        }
        break;
    case T_ICE:
    {
        const float a = t * 10;
        for (int i = 0; i < 2; i++)
        {
            const float aa = a + i * 0.785f, rr = r * (1.1f - i * 0.4f);
            BGfx::Seg(c, x + std::cos(aa) * rr, y + std::sin(aa) * rr, x - std::cos(aa) * rr, y - std::sin(aa) * rr, rr * 0.7f, i ? 0xFFFFFFFFu : 0xFF88D8F0u);
        }
        break;
    }
    case T_ROCK: case T_GROUND:
    {
        const uint32_t a = type == T_ROCK ? 0xFF8C7040u : 0xFFB89048u, d = type == T_ROCK ? 0xFF5A4828u : 0xFF7A5C2Cu;
        Blob(c, x, y, r, d, b);
        Blob(c, x - r * 0.15f, y - r * 0.15f, r * 0.75f, a, b);
        Blob(c, x - r * 0.35f, y - r * 0.35f, r * 0.25f, 0xFFE8D8B0u, b);
        for (int i = 0; i < 2; i++) Blob(c, x - ux * r * (1.6f + i) + std::sin(t * 25 + i) * r * 0.4f, y - uy * r * (1.6f + i), r * 0.3f, d, b);
        break;
    }
    case T_PSYCHIC: case T_FAIRY:
        for (int i = 0; i < 3; i++)
        {
            const float rr = r * (0.5f + std::fmod(t * 3 + i / 3.0f, 1.0f));
            BGfx::Ring(c, x, y, rr, rr, std::max(2.0f, b * 0.8f), BGfx::Alpha(type == T_PSYCHIC ? 0xFFF85888u : 0xFFF8A0C8u, 1.2f - rr / r * 0.7f));
        }
        Blob(c, x, y, r * 0.45f, 0xFFFFE0F0u, b);
        if (type == T_FAIRY) Star(c, x, y, r * 0.9f, t * 6, 0xFFFFFFFFu);
        break;
    case T_GHOST: case T_DARK: case T_POISON:
    {
        const uint32_t o = type == T_GHOST ? 0xFF6040A0u : type == T_DARK ? 0xFF302838u : 0xFF9040B0u, i2 = type == T_GHOST ? 0xFFB088F0u : type == T_DARK ? 0xFF8050A0u : 0xFFD088F0u;
        for (int i = 1; i <= 4; i++) Blob(c, x - ux * r * i * 0.6f + std::sin(t * 18 + i * 1.7f) * r * 0.35f, y - uy * r * i * 0.6f + std::cos(t * 18 + i) * r * 0.35f, r * (0.7f - i * 0.12f), BGfx::Alpha(o, 0.8f - i * 0.15f), b);
        Blob(c, x, y, r, o, b);
        Blob(c, x - r * 0.2f, y - r * 0.2f, r * 0.5f, i2, b);
        if (type == T_POISON) for (int i = 0; i < 3; i++) BGfx::Ring(c, x + std::cos(t * 9 + i * 2) * r, y + std::sin(t * 9 + i * 2) * r, r * 0.25f, r * 0.25f, b * 0.6f, 0xFFD088F0u);
        break;
    }
    case T_FLYING:
        for (int i = 0; i < 2; i++)
        {
            const float o = (i - 0.5f) * r * 0.9f;
            const float cx = x - uy * o - ux * r * 0.3f * i, cy = y + ux * o - uy * r * 0.3f * i;
            // (A crescent of wind: an arc facing the way it goes.)
            for (float a = -1.1f; a <= 1.1f; a += 0.22f)
            {
                const float ca = std::cos(a), sa = std::sin(a);
                const float qx = cx + (ux * ca - uy * sa) * r, qy = cy + (uy * ca + ux * sa) * r;
                Blob(c, qx, qy, std::max(b, r * 0.18f * (1.2f - std::fabs(a) * 0.6f)), 0xE0F0F8FFu, b);
            }
        }
        break;
    case T_STEEL:
        BGfx::Seg(c, x - ux * r * 1.3f, y - uy * r * 1.3f, x + ux * r * 1.3f, y + uy * r * 1.3f, r * 0.9f, 0xFF9098A8u);
        BGfx::Seg(c, x - ux * r * 1.1f, y - uy * r * 1.1f, x + ux * r * 1.1f, y + uy * r * 1.1f, r * 0.45f, 0xFFE0E4F0u);
        Star(c, x + ux * r * 0.6f, y + uy * r * 0.6f, r * 0.6f, t * 12, 0xFFFFFFFFu);
        break;
    case T_FIGHTING:
        Blob(c, x, y, r, 0xFFC03028u, b);
        Blob(c, x - r * 0.2f, y - r * 0.2f, r * 0.6f, 0xFFF07858u, b);
        for (int i = 0; i < 4; i++) { const float a = t * 8 + i * 1.57f; BGfx::Seg(c, x + std::cos(a) * r * 1.2f, y + std::sin(a) * r * 1.2f, x + std::cos(a) * r * 1.7f, y + std::sin(a) * r * 1.7f, b, 0xFFFFFFFFu); }
        break;
    default:
        Blob(c, x, y, r, tc, b);
        Star(c, x, y, r * 1.1f, t * 10, 0xFFFFFFFFu);
        break;
    }
}
// A particle, by its kind (battle_engine.h's PK_...), at (x, y) on the screen.
inline void Part(Canvas& c, const Arena::Particle& p, float x, float y, float k, float px)
{
    const float f = std::min(1.0f, p.t / std::max(0.01f, p.life));
    const float sz = std::max(1.0f, p.size * k * (1 + p.grow * f));
    switch (p.kind)
    {
    case Arena::PK_FLAME:
    {
        // (Hot to cold: white, yellow, orange, red, then smoke; it swells, then shrinks and fades,
        // a tongue of flame with its tip up.)
        const float swell = 1 + p.grow * std::min(f, 0.35f) / 0.35f, shrink = f > 0.55f ? 1 - (f - 0.55f) / 0.45f * 0.75f : 1.0f;
        const float z = std::max(1.0f, p.size * k * swell * shrink);
        uint32_t col = f < 0.12f ? 0xFFFFFFF0u : f < 0.3f ? 0xFFFFF07Au : f < 0.5f ? 0xFFFFA234u : f < 0.68f ? 0xFFF05A1Eu : 0xFF5A4842u;
        if (p.col != Arena::TypeColour(Arena::T_FIRE)) col = BGfx::Mix(col, p.col, 0.55f); // (a dragon's: purple flames)
        const float a = f < 0.68f ? 1.0f : (1 - f) / 0.32f * 0.55f;
        const float flick = std::sin(p.rot * 3) * z * 0.08f;
        Blob(c, x, y, z * 0.5f, BGfx::Alpha(col, a), px);
        Blob(c, x + flick, y - z * 0.42f, z * 0.3f, BGfx::Alpha(col, a), px);
        if (f < 0.5f) Blob(c, x, y - z * 0.05f, z * 0.24f, BGfx::Alpha(0xFFFFF8D0u, 1 - f * 1.6f), px);
        break;
    }
    case Arena::PK_SMOKE: Blob(c, x, y, sz * 0.5f, BGfx::Alpha(p.col, (1 - f) * 0.8f), px); break;
    case Arena::PK_LEAF: Leaf(c, x, y, sz * 0.6f, p.rot, BGfx::Alpha(p.col, f > 0.8f ? (1 - f) * 5 : 1.0f), px); break;
    case Arena::PK_SPARK:
        Star(c, x, y, sz * 0.6f * (1 - f * 0.5f), p.rot, BGfx::Alpha(BGfx::Mix(p.col, 0xFFFFFFFFu, 0.5f), 1 - f));
        break;
    case Arena::PK_DROP:
        Blob(c, x, y, sz * 0.45f, BGfx::Alpha(BGfx::Mix(p.col, 0xFF2858C8u, 0.3f), 1 - f * 0.7f), px);
        Blob(c, x - sz * 0.12f, y - sz * 0.15f, sz * 0.14f, BGfx::Alpha(0xFFFFFFFFu, 1 - f), px);
        break;
    case Arena::PK_ROCK:
        Blob(c, x, y, sz * 0.45f, BGfx::Alpha(BGfx::Mix(p.col, 0xFF3A2C1Cu, 0.4f), 1 - f * 0.5f), px);
        Blob(c, x - sz * 0.1f, y - sz * 0.1f, sz * 0.22f, BGfx::Alpha(p.col, 1 - f * 0.5f), px);
        break;
    case Arena::PK_ORB:
        Blob(c, x, y, sz * 0.7f, BGfx::Alpha(p.col, 0.3f * (1 - f)), px);
        Blob(c, x, y, sz * 0.4f, BGfx::Alpha(p.col, 1 - f), px);
        Blob(c, x, y, sz * 0.15f, BGfx::Alpha(0xFFFFFFFFu, 1 - f), px);
        break;
    default:
        BGfx::Rect(c, (int)(x - sz / 2), (int)(y - sz / 2), std::max(1, (int)sz), std::max(1, (int)sz), BGfx::Alpha(p.col, 1 - f));
        break;
    }
}
// A patch of ground: scorched, frozen..., and for Grass and Bug thorny vines coming up from it.
inline void ZoneDraw(Canvas& c, const Arena::Zone& z, float x, float y, float k, float px)
{
    const float in = std::min(1.0f, z.t * 5), out = std::min(1.0f, (z.life - z.t) * 3), a = in * out;
    const uint32_t tc = Arena::TypeColour(z.type);
    const float rx = z.r * k, ry = z.r * k * 0.6f;
    BGfx::Ellipse(c, x, y, rx, ry, BGfx::Alpha(BGfx::Mix(tc, 0xFF201810u, z.type == Arena::T_FIRE ? 0.55f : 0.3f), 0.45f * a));
    BGfx::Ring(c, x, y, rx, ry, std::max(2.0f, 3 * k), BGfx::Alpha(tc, 0.7f * a));
    if (z.type == Arena::T_GRASS || z.type == Arena::T_BUG)
    {
        // (Vines: curling up from the ground, thorns on them.)
        unsigned s = z.seed;
        for (int i = 0; i < 10; i++)
        {
            s = s * 1103515245u + 12345u;
            const float ang = (s % 628) / 100.0f, d = ((s >> 10) % 100) / 100.0f * 0.8f;
            float vx = x + std::cos(ang) * rx * d, vy = y + std::sin(ang) * ry * d;
            const float hgt = (60 + (s >> 20) % 45) * k * in;
            float lx = vx, ly = vy;
            for (int j = 1; j <= 6; j++)
            {
                const float t = j / 6.0f, sw = std::sin(t * 4 + i + z.t * 3) * 8 * k;
                const float nx = vx + sw, ny = vy - hgt * t;
                BGfx::Seg(c, lx, ly, nx, ny, std::max(3.0f, 9 * k * (1.3f - t)), BGfx::Alpha(0xFF1E5A1Eu, a));
                BGfx::Seg(c, lx, ly, nx, ny, std::max(2.0f, 5 * k * (1.3f - t)), BGfx::Alpha(0xFF3E9A32u, a));
                if (j % 2 == 0) { Blob(c, nx + 6 * k, ny, std::max(px, 5 * k), BGfx::Alpha(0xFF7CD050u, a), px); Blob(c, nx - 5 * k, ny + 3 * k, std::max(px * 0.7f, 2.5f * k), BGfx::Alpha(0xFFC04040u, a), px); }
                lx = nx; ly = ny;
            }
        }
    }
}
// A wave (sound, wind): wide arcs of its type's colour, facing the way it goes.
inline void Wave(Canvas& c, int type, float x, float y, float r, float dx, float dy, float t, float px)
{
    const float l = std::max(1e-3f, std::sqrt(dx * dx + dy * dy)), ux = dx / l, uy = dy / l;
    const uint32_t tc = Arena::TypeColour(type);
    for (int k = 0; k < 3; k++)
    {
        const float rr = r * (0.6f + 0.2f * k), back = r * 0.45f * k;
        const float cx = x - ux * back, cy = y - uy * back;
        for (float a = -1.0f; a <= 1.0f; a += 0.12f)
        {
            const float ca = std::cos(a), sa = std::sin(a);
            const float qx = cx + (ux * ca - uy * sa) * rr, qy = cy + (uy * ca + ux * sa) * rr;
            Blob(c, qx, qy, std::max(px, rr * 0.13f * (1.3f - std::fabs(a))), k == 1 ? 0xF0FFFFFFu : BGfx::Alpha(BGfx::Mix(tc, 0xFFFFFFFFu, 0.25f), 0.95f - k * 0.2f), px);
        }
    }
    (void)t;
}
// Where a move landed: a flash and the type's own bits flying out; a physical one a slash too.
inline void Hit(Canvas& c, const Impact& m, float x, float y, float k, float px)
{
    const float t = std::min(1.0f, m.t / 0.35f), sz = (m.big ? 90 : 55) * k;
    const uint32_t tc = Arena::TypeColour(m.type);
    BGfx::Disc(c, x, y, sz * (0.3f + 0.7f * t), BGfx::Alpha(0xFFFFFFFFu, 0.55f * (1 - t)));
    BGfx::Ring(c, x, y, sz * (0.4f + t), sz * (0.4f + t) * 0.8f, std::max(2.0f, 6 * k * (1 - t)), BGfx::Alpha(tc, 1 - t));
    if (m.phys)
    {
        // (Three claw lines across it.)
        const float l = sz * 0.9f, sh = std::min(1.0f, t * 3);
        for (int i = -1; i <= 1; i++)
        {
            const float ox = i * sz * 0.22f;
            BGfx::Seg(c, x - l * 0.6f + ox, y - l * 0.6f, x - l * 0.6f + ox + l * 1.2f * sh, y - l * 0.6f + l * 1.2f * sh, std::max(2.0f, 5 * k * (1 - t)), BGfx::Alpha(0xFFFFFFFFu, 1 - t));
        }
    }
    for (int i = 0; i < 6; i++)
    {
        const float a = i * 1.047f + m.x * 0.01f, d = sz * (0.3f + t * 0.9f);
        const float bx = x + std::cos(a) * d, by = y + std::sin(a) * d * 0.8f;
        if (m.type == T_GRASS || m.type == T_BUG) Leaf(c, bx, by, 9 * k, a + t * 6, tc, px);
        else if (m.type == T_ELECTRIC) Bolt(c, x, y, bx, by, std::max(2.0f, 3 * k), t * 7 + i, tc);
        else if (m.type == T_FAIRY || m.type == T_NORMAL) Star(c, bx, by, 10 * k * (1 - t * 0.5f), t * 8, BGfx::Alpha(m.type == T_FAIRY ? 0xFFF8B8D8u : 0xFFFFFFFFu, 1 - t));
        else Blob(c, bx, by, 8 * k * (1 - t * 0.6f), BGfx::Alpha(tc, 1 - t * 0.5f), px);
    }
}
}

// ---- PMD Origins' effect sheets (fx/, battle_trainers.h's FxOf): frames side by side, square ---------
inline const ArtImg* FxImg(const char* name)
{
    if (!name) return nullptr;
    std::string n = name;
    const size_t h = n.find('#');
    if (h != std::string::npos) n.resize(h);
    return ArenaArt("fx/" + n);
}
// (A shot's frames to loop: "Name#a-b" those, else all.)
inline void FxRange(const char* name, int nf, int& a, int& b)
{
    a = 0; b = nf - 1;
    if (const char* h = name ? strchr(name, '#') : nullptr) { a = std::max(0, atoi(h + 1)); const char* d = strchr(h, '-'); b = d ? std::min(nf - 1, atoi(d + 1)) : a; }
    if (a > b) a = b;
}
// One frame of a sheet, centred on (cx, cy), s screen pixels a pixel.
inline void FxFrame(Canvas& c, const ArtImg& a, int frame, float cx, float cy, float s, float alpha = 1)
{
    const int fw = a.h, nf = std::max(1, a.w / std::max(1, fw));
    frame = std::max(0, std::min(nf - 1, frame));
    const int dw = std::max(1, (int)(fw * s)), ox = (int)(cx - dw / 2.0f), oy = (int)(cy - dw / 2.0f);
    for (int j = 0; j < dw; j++)
    {
        const int yy = oy + j;
        if (yy < 0 || yy >= c.h) continue;
        const int sy = std::min(a.h - 1, (int)(j / s));
        for (int i = 0; i < dw; i++)
        {
            const int xx = ox + i;
            if (xx < 0 || xx >= c.w) continue;
            const uint32_t p = a.px[(size_t)sy * a.w + frame * fw + std::min(fw - 1, (int)(i / s))];
            const uint32_t al = p >> 24;
            if (al < 16) continue;
            BGfx::Put(c, xx, yy, alpha >= 1 && al == 255 ? p : BGfx::Alpha(p, alpha));
        }
    }
}
// A beam of a Beam folder's: its body (a frame: the beam across, one pixel along, stretched) from
// (x0, y0) along (ux, uy), len pixels, and its head at the end; frame f; s pixels a pixel.
inline void FxBeam(Canvas& c, const ArtImg& body, const ArtImg& head, int f, float x0, float y0, float ux, float uy, float len, float s)
{
    const int fw = head.h, nf = std::max(1, head.w / std::max(1, fw));
    f = std::max(0, std::min(nf - 1, f));
    const float half = fw * s * 0.5f, headLen = head.h * s;
    const float nx = -uy, ny = ux;
    const float x1 = x0 + ux * len, y1 = y0 + uy * len;
    const int by0 = std::max(0, (int)(std::min(y0, y1) - half)), by1 = std::min(c.h - 1, (int)(std::max(y0, y1) + half));
    const float minX = std::min(x0, x1) - half, maxX = std::max(x0, x1) + half;
    for (int y = by0; y <= by1; y++)
    {
        const float py = y + 0.5f - y0;
        float a = minX, b = maxX;
        if (std::fabs(uy) > 1e-3f)
        {
            // (Within half of the line: |px * ny ... |; the row's span, not the whole box.)
            const float e0 = (py * ux - half) / uy + x0, e1 = (py * ux + half) / uy + x0;
            a = std::max(a, std::min(e0, e1)); b = std::min(b, std::max(e0, e1));
        }
        for (int x = std::max(0, (int)a); x <= std::min(c.w - 1, (int)b + 1); x++)
        {
            const float px = x + 0.5f - x0;
            const float along = px * ux + py * uy, across = px * nx + py * ny;
            if (along < 0 || along > len || std::fabs(across) > half) continue;
            const int col = std::min(fw - 1, (int)((across + half) / s));
            uint32_t p;
            if (along > len - headLen) { const int row = std::min(head.h - 1, (int)((along - (len - headLen)) / s)); p = head.px[(size_t)row * head.w + f * fw + col]; }
            else p = body.px[f * fw + col];
            if ((p >> 24) < 16) continue;
            BGfx::Put(c, x, y, p);
        }
    }
}

// ---- The stadium: drawn once a size, then copied each frame ------------------------------------
inline void BattleLayout(const Canvas& c, float u)
{
    BattleState& B = Bat();
    // (As big as the screen lets it: under the bars at the top, a little past the foot.)
    const float top = 52 * u, bottom = c.h + 4 * u;
    B.fk = std::min((c.w * 0.97f) / (2 * Arena::RX), (bottom - top) / (2 * Arena::RY + 40));
    B.fcx = c.w * 0.5f;
    B.fcy = (top + bottom) * 0.5f + 6 * u;
}
inline void BattleBackground(Canvas& c, float u)
{
    BattleState& B = Bat();
    if (B.bgW == c.w && B.bgH == c.h && B.bg.size() == c.px.size()) { std::copy(B.bg.begin(), B.bg.end(), c.px.begin()); return; }
    BattleLayout(c, u);
    const float k = B.fk, cx = B.fcx, cy = B.fcy, rx = Arena::RX * k, ry = Arena::RY * k;
    std::mt19937 rng(7);
    // (The stands: rows of people, darker towards the top.)
    const int cell = std::max(3, (int)(5 * u));
    static const uint32_t shirts[] = { 0xFFB84040u, 0xFF4068B8u, 0xFFE0B040u, 0xFF48A060u, 0xFFE8E8E8u, 0xFF8050B0u, 0xFF303848u, 0xFFE07830u };
    for (int y = 0; y < c.h; y++)
        for (int x = 0; x < c.w; x++)
            c.px[(size_t)y * c.w + x] = BGfx::Mix(0xFF1A2236u, 0xFF2A3550u, (float)y / c.h);
    for (int gy = 0; gy < c.h; gy += cell * 2)
        for (int gx = (gy / (cell * 2)) % 2 * cell; gx < c.w; gx += cell * 2)
        {
            if (rng() % 7 == 0) continue;
            const uint32_t s = shirts[rng() % 8];
            const float dark = 0.25f + 0.6f * (float)gy / c.h;
            BGfx::Rect(c, gx, gy + cell, cell, cell, BGfx::Mix(0xFF101420u, s, dark));
            BGfx::Rect(c, gx + cell / 4, gy + cell / 3, cell / 2 + 1, cell * 2 / 3, BGfx::Mix(0xFF101420u, 0xFFE8B890u, dark));
        }
    // (Lights in the top corners.)
    for (int side = 0; side < 2; side++)
    {
        const float lx = side ? c.w - 70 * u : 70 * u, ly = 20 * u;
        BGfx::Rect(c, (int)(lx - 26 * u), (int)ly, (int)(52 * u), (int)(26 * u), 0xFF606878u);
        for (int i = 0; i < 6; i++) BGfx::Disc(c, lx - 18 * u + (i % 3) * 18 * u, ly + 7 * u + (i / 3) * 12 * u, 5 * u, 0xFFFFFBE0u);
        BGfx::Disc(c, lx, ly + 13 * u, 60 * u, 0x18FFFFE0u);
    }
    // (The wall round the field: red and blue boards.)
    const float wall = 22 * k + 8 * u;
    for (int y = std::max(0, (int)(cy - ry - wall)); y < std::min(c.h, (int)(cy + ry + wall)); y++)
        for (int x = std::max(0, (int)(cx - rx - wall)); x < std::min(c.w, (int)(cx + rx + wall)); x++)
        {
            const float dx = (x - cx) / (rx + wall), dy = (y - cy) / (ry + wall);
            if (dx * dx + dy * dy > 1) continue;
            const float ang = std::atan2((y - cy) / ry, (x - cx) / rx);
            const int panel = (int)std::floor((ang + 3.14159f) / 0.2f);
            c.px[(size_t)y * c.w + x] = panel % 2 ? 0xFFB83838u : 0xFF2F55B8u;
        }
    BGfx::Ring(c, cx, cy, rx + wall, ry + wall, 3 * u, 0xFF141820u);
    // (The grass, mown in stripes, a white line round it, the halfway line and a Poké Ball.)
    const float stripe = 60 * k;
    for (int y = std::max(0, (int)(cy - ry)); y < std::min(c.h, (int)(cy + ry + 1)); y++)
        for (int x = std::max(0, (int)(cx - rx)); x < std::min(c.w, (int)(cx + rx + 1)); x++)
        {
            const float dx = (x - cx) / rx, dy = (y - cy) / ry;
            if (dx * dx + dy * dy > 1) continue;
            const bool light = ((int)std::floor((x - cx) / stripe + 100)) % 2;
            uint32_t g = light ? 0xFF5DAE3Eu : 0xFF4E9E35u;
            if ((rng() & 31) == 0) g = BGfx::Mix(g, 0xFF2F7A22u, 0.5f);
            // (Darker at the edge, as under the stands' shade.)
            g = BGfx::Mix(g, 0xFF2E6A24u, std::max(0.0f, (dx * dx + dy * dy) - 0.75f) * 1.6f);
            c.px[(size_t)y * c.w + x] = g;
        }
    BGfx::Ring(c, cx, cy, rx - 26 * k, ry - 26 * k, std::max(2.0f, 4 * k), 0xD0FFFFFFu);
    BGfx::Rect(c, (int)(cx - 2 * k), (int)(cy - ry + 26 * k), std::max(2, (int)(4 * k)), (int)(2 * ry - 52 * k), 0xD0FFFFFFu);
    const float pr = 70 * k;
    BGfx::Disc(c, cx, cy, pr + 4 * k, 0xFF202020u);
    for (int y = (int)(cy - pr); y <= (int)(cy + pr); y++)
        for (int x = (int)(cx - pr); x <= (int)(cx + pr); x++)
        {
            const float dx = x - cx, dy = y - cy;
            if (dx * dx + dy * dy > pr * pr) continue;
            BGfx::Put(c, x, y, dy < -5 * k ? 0xFFD83A3Au : dy > 5 * k ? 0xFFF2F2F2u : 0xFF202020u);
        }
    BGfx::Disc(c, cx, cy, 22 * k, 0xFF202020u);
    BGfx::Disc(c, cx, cy, 15 * k, 0xFFF2F2F2u);
    B.bg = c.px; B.bgW = c.w; B.bgH = c.h;
}

// ---- The fight's picture -----------------------------------------------------------------------
inline void BattleHud(Canvas& c, float u, int side)
{
    BattleState& B = Bat();
    const Arena::Battle& b = B.b;
    const Arena::Fighter& F = b.f[side];
    const float w = std::min(330 * u, c.w * 0.36f), h = 58 * u, y = 8 * u;
    const float x = side == 0 ? 10 * u : c.w - 10 * u - w;
    Glass::Round(c, (int)x, (int)y, (int)w, (int)h, 8 * u, 0xE0101828u, side == 0 ? 0xFF3C7BE0u : 0xFFE04848u, 2 * u);
    // (Its picture in a square at the outer end.)
    const float ps = h - 10 * u, px = side == 0 ? x + 5 * u : x + w - 5 * u - ps;
    Glass::Round(c, (int)px, (int)(y + 5 * u), (int)ps, (int)ps, 6 * u, side == 0 ? 0xFF5B8FD8u : 0xFFD86060u);
    BGfx::Mon(c, F.spec.species, F.spec.shiny, px + ps / 2, y + 5 * u + ps - 2 * u, ps * 1.05f, side == 1, 0, 1);
    const float tx0 = side == 0 ? px + ps + 8 * u : x + 8 * u, tw = w - ps - 21 * u;
    // (Its name, its status, its HP.)
    static const char* st[] = { "", "QUE", "ENV", "ENV", "PAR", "DOR", "CON" };
    static const uint32_t stc[] = { 0, 0xFFFF9040u, 0xFFC070E0u, 0xFFA040C0u, 0xFFE0B020u, 0xFF9090A8u, 0xFF70C8E0u };
    GTFit(c, (int)tx0, y + 12 * u, F.spec.name, 13 * u, (int)(tw * 0.62f), 0xFFFFFFFFu, true);
    const std::string hp = std::to_string(F.hp) + "/" + std::to_string(F.maxHp);
    GT(c, (int)(tx0 + tw - GW(hp, 12 * u, true)), y + 12 * u, hp, 12 * u, 0xFFFFFFFFu, true);
    if (F.status)
    {
        const int sw = GW(st[F.status], 9 * u, true) + (int)(8 * u);
        const int sx = (int)(tx0 + tw - GW(hp, 12 * u, true) - sw - 6 * u);
        Glass::Round(c, sx, (int)(y + 5 * u), sw, (int)(14 * u), 4 * u, stc[F.status]);
        GT(c, sx + (int)(4 * u), y + 12 * u, st[F.status], 9 * u, 0xFFFFFFFFu, true);
    }
    // (Bars: HP, then ULT.)
    const float hy = y + 23 * u, bw = tw;
    BGfx::Rect(c, (int)tx0, (int)hy, (int)bw, (int)(10 * u), 0xFF303846u);
    const float frac = (float)F.hp / std::max(1, F.maxHp);
    const uint32_t hc = frac > 0.5f ? 0xFF48D058u : frac > 0.2f ? 0xFFF0C030u : 0xFFF04838u;
    const int fw = (int)(bw * frac);
    BGfx::Rect(c, side == 0 ? (int)tx0 : (int)(tx0 + bw - fw), (int)hy, fw, (int)(10 * u), hc);
    BGfx::Rect(c, (int)tx0, (int)(hy + 14 * u), (int)bw, (int)(5 * u), 0xFF303846u);
    const int uw = (int)(bw * F.ult / 100);
    BGfx::Rect(c, side == 0 ? (int)tx0 : (int)(tx0 + bw - uw), (int)(hy + 14 * u), uw, (int)(5 * u), F.ult >= 100 ? 0xFFFFD040u : 0xFFF08A30u);
    // (The team: a ball each, grey when fainted; under the card.)
    const int n = (int)b.team[side].size();
    for (int i = 0; i < n; i++)
    {
        const bool down = i < b.cur[side] || (i == b.cur[side] && F.fainted);
        const float bx = side == 0 ? x + 10 * u + i * 16 * u : x + w - 10 * u - i * 16 * u, byy = y + h + 9 * u;
        BGfx::Disc(c, bx, byy, 6 * u, 0xFF101418u);
        BGfx::Ellipse(c, bx, byy - 1.5f * u, 5 * u, 3.5f * u, down ? 0xFF606060u : 0xFFE04040u);
        BGfx::Ellipse(c, bx, byy + 1.5f * u, 5 * u, 3.5f * u, down ? 0xFF8A8A8Au : 0xFFF0F0F0u);
        BGfx::Disc(c, bx, byy, 1.8f * u, 0xFF101418u);
    }
}
inline void BattleButton(Canvas& c, float u, int k, float x, float y, float r)
{
    BattleState& B = Bat();
    const Arena::Fighter& F = B.b.f[0];
    B.bx[k] = x; B.by[k] = y; B.br[k] = r;
    const int move = k < 4 ? F.sk[k].move : 0;
    const bool none = k < 4 && !move;
    const BattleData::MoveInfo& mi = BattleData::Move(move);
    float wait = 0, frac = 0;
    if (k == Arena::S_DASH) { wait = F.dashCharges > 0 ? 0 : F.dashCd; frac = F.dashCharges > 0 ? 0 : F.dashCd / 3.5f; }
    else if (k == Arena::S_ULT) { frac = 1 - F.ult / 100; }
    else if (k < 4) { wait = F.sk[k].cd; frac = F.sk[k].cdMax > 0 ? F.sk[k].cd / F.sk[k].cdMax : 0; }
    const bool ready = !none && (k == Arena::S_ULT ? F.ult >= 100 : wait <= 0);
    const uint32_t ring = k == Arena::S_EVO ? BGfx::Mix(0xFFFF60D0u, 0xFF60D0FFu, 0.5f + 0.5f * std::sin(B.b.time * 4)) : k == Arena::S_DASH ? 0xFFB0B8C8u : none ? 0xFF606060u : Arena::TypeColour(mi.type);
    const bool down = B.held[k] != -1;
    BGfx::Disc(c, x, y, r, down ? 0xA0FFFFFFu : ready ? 0x70203020u : 0x90101418u);
    BGfx::Ring(c, x, y, r, r, std::max(2.0f, 3 * u), ready ? ring : BGfx::Alpha(ring, 0.45f));
    if (k == Arena::S_ULT && ready) BGfx::Ring(c, x, y, r + 4 * u + 2 * u * std::sin(B.b.time * 8), r + 4 * u + 2 * u * std::sin(B.b.time * 8), 2 * u, 0xC0FFD040u);
    // (Waiting: the part left shaded, as a clock.)
    if (!ready && !none && frac > 0)
    {
        const float a0 = -1.5708f;
        for (int yy = (int)(y - r); yy <= (int)(y + r); yy++)
            for (int xx = (int)(x - r); xx <= (int)(x + r); xx++)
            {
                const float dx = xx - x, dy = yy - y;
                if (dx * dx + dy * dy > r * r) continue;
                float a = std::atan2(dy, dx) - a0;
                if (a < 0) a += 6.2832f;
                if (a < frac * 6.2832f) BGfx::Put(c, xx, yy, 0x70000000u);
            }
    }
    const char* label = Arena::SlotLabel(k);
    const float fs = k == Arena::S_ATQ ? 15 * u : k == Arena::S_DASH ? 8.5f * u : k == Arena::S_EVO ? 10 * u : 11 * u;
    if (!ready && k != Arena::S_ULT && wait > 0) BGfx::Text(c, x, y, std::to_string((int)std::ceil(wait)), 15 * u, 0xFFFFFFFFu);
    else if (k == Arena::S_ULT && !ready) BGfx::Text(c, x, y, std::to_string((int)F.ult) + "%", 10 * u, 0xFFFFFFFFu);
    else BGfx::Text(c, x, y, label, fs, none ? 0xFF909090u : 0xFFFFFFFFu);
    // (The move's name under it.)
    if (k == Arena::S_DASH && F.dashMax > 1)
    {
        // (Two dashes: how many are ready, as dots under it.)
        for (int i = 0; i < F.dashMax; i++) BGfx::Disc(c, x - 6 * u + i * 12 * u, y + r + 7 * u, 4 * u, i < F.dashCharges ? 0xFFFFFFFFu : 0x60FFFFFFu);
    }
    if (k < 4 && move)
    {
        std::string n = mi.name;
        if (GW(n, 8.5f * u, true) > r * 2.6f) { while (n.size() > 3 && GW(n + ".", 8.5f * u, true) > r * 2.6f) n.pop_back(); n += "."; }
        BGfx::Text(c, x, y + r + 8 * u, n, 8.5f * u, 0xFFFFFFFFu);
    }
}
inline void BattleStep()
{
    BattleState& B = Bat();
    const auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - B.last).count();
    B.last = now;
    if (B.paused) return;
    B.acc += std::min(0.1, dt);
    const float step = 1.0f / 60;
    while (B.acc >= step)
    {
        B.acc -= step;
        Arena::Input in;
        float mx = B.stickDX, my = B.stickDY;
        if (B.stickId == -1) { mx = (float)(B.keys4[3] - B.keys4[1]); my = (float)(B.keys4[2] - B.keys4[0]); }
        in.mx = mx; in.my = my;
        for (int k = 0; k < Arena::SLOTS; k++)
        {
            in.press[k] = B.pressed[k] || (k == Arena::S_ATQ && B.held[k] != -1);
            B.pressed[k] = false;
        }
        B.b.Step(step, in);
    }
}
inline void DrawBattle(Canvas& c, float u)
{
    BattleState& B = Bat();
    if (!B.on) return;
    B.u = u;
    BattleStep();
    if (B.b.phase == Arena::Battle::END) BattleSettle();
    Hub().version++; // (a new frame each time, while it is up)
    BattleBackground(c, u);
    BattleLayout(c, u);
    const Arena::Battle& b = B.b;
    const float k = B.fk;
    // (The screen shakes on a big hit.)
    const float sh = b.shake * 6 * u;
    const float ox = sh > 0 ? std::sin(b.time * 70) * sh : 0, oy = sh > 0 ? std::cos(b.time * 53) * sh * 0.6f : 0;
    auto SX = [&](float x) { return B.fcx + x * k + ox; };
    auto SY = [&](float y) { return B.fcy + y * k + oy; };
    // Rings on the ground, then shadows, then the Pokémon (the one further up first).
    for (const Arena::Ring& r : b.rings)
    {
        const float t = r.t / r.dur, rr = r.rmax * k * (0.2f + 0.8f * t);
        BGfx::Ring(c, SX(r.x), SY(r.y), rr, rr / 1.6f, 10 * k, BGfx::Alpha(Arena::TypeColour(r.type), 1 - t * 0.7f));
        BGfx::Ellipse(c, SX(r.x), SY(r.y), rr, rr / 1.6f, BGfx::Alpha(Arena::TypeColour(r.type), 0.25f * (1 - t)));
    }
    for (const Arena::Zone& z : b.zones) BFx::ZoneDraw(c, z, SX(z.x), SY(z.y), k, std::max(2.0f, 3.4f * k));
    int order[2] = { 0, 1 };
    if (b.f[1].y < b.f[0].y) std::swap(order[0], order[1]);
    const float monH = 215 * k;
    for (int i : order)
    {
        const Arena::Fighter& F = b.f[i];
        if (F.fainted && b.phase != Arena::Battle::KO) continue;
        const float x = SX(F.x), y = SY(F.y);
        BGfx::Ellipse(c, x, y, 34 * k, 12 * k, 0x60000000u);
        BGfx::Ring(c, x, y, 38 * k, 14 * k, 3 * k, i == 0 ? 0xC03C7BE0u : 0xC0E04848u);
        if (F.castT > 0) BGfx::Ellipse(c, x, y, 50 * k, 18 * k, BGfx::Alpha(Arena::TypeColour(F.castType), F.castT * 1.5f));
        float hop = F.moving ? std::fabs(std::sin(F.walk)) * 10 * k : 0;
        float squash = F.moving ? 1 + 0.04f * std::sin(F.walk * 2) : 1 + 0.025f * std::sin(b.time * 3 + i);
        float alpha = F.dashT > 0 ? 0.45f : 1;
        if (F.fainted) { const float t = std::min(1.0f, b.phaseT / 0.8f); alpha = 1 - t; squash = 1 - 0.5f * t; }
        if ((F.status == Arena::SLP || F.status == Arena::FRZ) && !F.fainted) hop = 0;
        if (const PmdMon* pm = PmdGet(F.mega ? F.spec.megaKey : F.spec.species))
        {
            // (Its moving picture: hurt, attacking, walking or standing, facing where it looks.)
            const int row = PmdRow(F.dirX, F.dirY);
            const float ps = 3.4f * k * (F.fainted ? squash : 1);
            if (F.hurtT > 0 || F.fainted) PmdDraw(c, *pm, PA_HURT, row, 0.3f - F.hurtT, false, x, y, ps, F.flash > 0 ? 0.6f : 0, alpha);
            else if (F.atkT > 0) { const float at = (F.atkDur - F.atkT) / F.atkDur * pm->a[PA_ATTACK].total / 60.0f; PmdDraw(c, *pm, PA_ATTACK, row, at, false, x, y, ps, 0, alpha); }
            else if (F.moving || F.dashT > 0 || F.lungeT > 0) PmdDraw(c, *pm, PA_WALK, row, F.walk * 0.25f, true, x, y, ps, 0, alpha);
            else PmdDraw(c, *pm, PA_IDLE, row, b.time + i * 0.37f, true, x, y, ps, 0, alpha);
        }
        else BGfx::Mon(c, F.spec.species, F.spec.shiny, x, y + 4 * k - hop, monH, F.face > 0, F.flash > 0 ? 0.85f : 0, alpha, squash);
        if (F.status == Arena::FRZ) BGfx::Ellipse(c, x, y - monH * 0.25f, 40 * k, 50 * k, 0x5098E0F0u);
        if (F.status == Arena::SLP) BGfx::Text(c, x + 30 * k, y - monH * 0.5f - std::fmod(b.time, 1.0f) * 20 * k, "z", 14 * u, 0xFFE0E0FFu);
        if (F.protectT > 0) BGfx::Ring(c, x, y - monH * 0.25f, 60 * k, 70 * k, 4 * k, 0xB090D0FFu);
        // (A little HP bar over it.)
        if (!F.fainted)
        {
            const float bw = 60 * k, by = y - monH * 0.62f - 10 * k;
            BGfx::Rect(c, (int)(x - bw / 2 - 1), (int)(by - 1), (int)(bw + 2), (int)(6 * k + 2), 0xC0101418u);
            const float frac = (float)F.hp / std::max(1, F.maxHp);
            BGfx::Rect(c, (int)(x - bw / 2), (int)by, (int)(bw * frac), (int)(6 * k), i == 0 ? 0xFF48A0F0u : 0xFFF05050u);
        }
    }
    // Shots and the beams.
    for (const Arena::Beam& bm : b.beams)
    {
        const float x0 = SX(bm.x0), y0 = SY(bm.y0), x1 = SX(bm.x0 + bm.dx * bm.len), y1 = SY(bm.y0 + bm.dy * bm.len);
        const uint32_t tc = Arena::TypeColour(bm.type);
        if (bm.t < bm.windup)
        {
            // (Where it will go: a thin line, blinking.)
            const float a = 0.3f + 0.3f * std::sin(bm.t * 40);
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, bm.width * k, BGfx::Alpha(tc, a * 0.35f));
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, 3 * k, BGfx::Alpha(0xFFFFFFFFu, a));
            BGfx::Disc(c, x0, y0 - 30 * k, (20 + 30 * bm.t / bm.windup) * k, BGfx::Alpha(tc, 0.7f));
            if (const ArtImg* ch = FxImg(Arena::MoveCharge(bm.move)))
                FxFrame(c, *ch, (int)(bm.t / bm.windup * std::max(1, ch->w / std::max(1, ch->h))), x0, y0 - 30 * k, 3.4f * k);
            // (Charging: sparks drawn in to it.)
            for (int i = 0; i < 10; i++)
            {
                const float ang = i * 0.628f + bm.t * 5, rr = (1 - std::fmod(bm.t * 2.5f + i * 0.1f, 1.0f)) * 110 * k;
                BFx::Star(c, x0 + std::cos(ang) * rr, y0 - 30 * k + std::sin(ang) * rr * 0.7f, 7 * k, bm.t * 9 + i, BGfx::Alpha(BGfx::Mix(tc, 0xFFFFFFFFu, 0.5f), 0.9f));
            }
        }
        else
        {
            const float t = (bm.t - bm.windup) / bm.dur, w = bm.width * k * (1 - t * 0.6f);
            if (bm.ult)
            {
                // (A ULT's beam: its type's Beam, growing and dying away along its frames.)
                const std::string base = std::string("fx/") + Arena::FxOf(bm.type).beam;
                const ArtImg* bd = ArenaArt(base + "_Body");
                const ArtImg* hd = ArenaArt(base + "_Head");
                if (bd && hd)
                {
                    const float ux = x1 - x0, uy = y1 - y0, ul = std::max(1.0f, std::sqrt(ux * ux + uy * uy));
                    const int nf = std::max(1, hd->w / std::max(1, hd->h));
                    const float sc = std::max(2.0f, bm.width * k * 1.5f / std::max(1, hd->h));
                    FxBeam(c, *bd, *hd, (int)(t * nf), x0, y0 - 30 * k, ux / ul, uy / ul, ul, sc);
                    continue;
                }
            }
            if (!bm.ult)
            {
                // (A jet is its particles; Electric's lightning too, three bolts that flicker.)
                if (bm.type == Arena::T_ELECTRIC)
                    for (int i = 0; i < 3; i++) BFx::Bolt(c, x0, y0 - 30 * k, x1, y1 - 30 * k, std::max(2.0f, w * 0.14f), bm.t * 23 + i * 3.1f, tc, 0.035f);
                continue;
            }
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 1.3f, BGfx::Alpha(tc, 0.5f));
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 0.8f, tc);
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 0.35f, 0xFFFFFFF0u);
            // (The type's own bits along it.)
            for (int i = 1; i < 10; i++)
            {
                const float f = std::fmod(i / 10.0f + bm.t * 2.5f, 1.0f);
                BFx::Shot(c, bm.type, x0 + (x1 - x0) * f, y0 - 30 * k + (y1 - y0) * f, w * 0.32f, bm.t * 3 + i, bm.dx, bm.dy, std::max(2.0f, 3.4f * k));
            }
        }
    }
    for (const Arena::Particle& p : b.parts) BFx::Part(c, p, SX(p.x), SY(p.y) - 40 * k, k, std::max(2.0f, 3.4f * k));
    const float fxPx = std::max(2.0f, 3.4f * k); // (the moves' blocks as big as the sprites' pixels)
    for (const Arena::Proj& p : b.projs)
    {
        const float x = SX(p.x), y = SY(p.y) - 40 * k;
        if (!p.wave) BGfx::Disc(c, x, y, p.r * k * 2.3f, BGfx::Alpha(Arena::TypeColour(p.type), 0.22f));
        const char* sn = Arena::MoveShot(p.move) ? Arena::MoveShot(p.move) : Arena::FxOf(p.type).shot;
        const ArtImg* si = p.wave ? nullptr : FxImg(sn);
        if (p.wave) BFx::Wave(c, p.type, x, y, p.r * k * 2.4f, p.vx, p.vy, b.time, fxPx);
        else if (si)
        {
            // (Its sheet, looped; as large as the shot.)
            int a0, a1;
            FxRange(sn, std::max(1, si->w / std::max(1, si->h)), a0, a1);
            const int fr = a0 + (int)(b.time * 14 + p.x * 0.01f) % (a1 - a0 + 1);
            // (At the sprites' own scale, as PMD draws them: a bigger shot a little bigger.)
            FxFrame(c, *si, fr, x, y, k * (p.r >= 20 ? 3.2f : 2.6f) * (si->h <= 12 ? 1.6f : 1.0f));
        }
        else BFx::Shot(c, p.type, x, y, p.r * k * 1.6f, b.time + p.x * 0.001f, p.vx, p.vy, fxPx);
    }
    for (const Arena::Impact& m : b.impacts)
    {
        // (A hit: its type's sheet played once; a ULT's blast bigger; a touch of the type's
        // slash or fist; drawn by hand while the sheet is fetched.)
        const Arena::TypeFx& fx = Arena::FxOf(m.type);
        const char* hn = m.blast ? fx.blast : m.phys && (m.type == Arena::T_NORMAL || m.type == Arena::T_DARK || m.type == Arena::T_STEEL || m.type == Arena::T_DRAGON) ? "Slash.None" : fx.hit;
        const ArtImg* hi = FxImg(hn);
        if (!hi) { if (!m.blast) BFx::Hit(c, m, SX(m.x), SY(m.y) - 40 * k, k, fxPx); continue; }
        const float life = m.blast ? 0.9f : 0.6f;
        const int nf = std::max(1, hi->w / std::max(1, hi->h));
        FxFrame(c, *hi, (int)(m.t / life * nf), SX(m.x), SY(m.y) - (m.blast ? 30 : 40) * k, 3.4f * k * (m.blast ? 1.6f : m.big ? 1.15f : 0.85f));
    }
    // Words floating up.
    for (const Arena::FloatText& t : b.texts)
    {
        const float a = t.t / t.life;
        BGfx::Text(c, SX(t.x), SY(t.y) - 50 * k - a * 40 * u, t.s, 13 * u * t.size * (a < 0.12f ? 0.7f + a * 2.5f : 1), t.col);
    }
    // The bars at the top, the time between them.
    BattleHud(c, u, 0);
    BattleHud(c, u, 1);
    {
        // (This pair's time left, as the site's 99.)
        const int s = std::max(0, (int)std::ceil(Arena::Battle::DUEL_TIME - b.duel));
        char tm[16];
        snprintf(tm, sizeof tm, "%d", s);
        Glass::Round(c, (int)(c.w / 2 - 34 * u), (int)(8 * u), (int)(68 * u), (int)(30 * u), 6 * u, 0xE0101828u, 0xFF606878u, 1.5f * u);
        BGfx::Text(c, c.w / 2.0f, 23 * u, tm, 17 * u, s <= 10 ? 0xFFFF6060u : 0xFFFFFFFFu);
        const std::string sc = std::to_string(b.kos[0]) + "  x  " + std::to_string(b.kos[1]);
        BGfx::Text(c, c.w / 2.0f, 48 * u, sc, 11 * u, 0xFFE0E0E0u);
    }
    // The controls (not over the end).
    B.overHits.clear();
    const float pr = 17 * u, ppx = c.w - 26 * u, ppy = 92 * u;
    if (b.phase != Arena::Battle::END)
    {
        BGfx::Disc(c, ppx, ppy, pr, 0x90101418u);
        BGfx::Ring(c, ppx, ppy, pr, pr, 2 * u, 0xFFE0E0E0u);
        BGfx::Rect(c, (int)(ppx - 6 * u), (int)(ppy - 7 * u), (int)(4 * u), (int)(14 * u), 0xFFFFFFFFu);
        BGfx::Rect(c, (int)(ppx + 2 * u), (int)(ppy - 7 * u), (int)(4 * u), (int)(14 * u), 0xFFFFFFFFu);
        B.overHits.push_back({ (int)(ppx - pr - 6 * u), (int)(ppy - pr - 6 * u), (int)(2 * pr + 12 * u), (int)(2 * pr + 12 * u), BT_PAUSE, 0, 0 });
        // (The stick: where the finger went down, else its usual place.)
        const float sr = 54 * u;
        const float scx = B.stickId != -1 ? B.stickX : 100 * u, scy = B.stickId != -1 ? B.stickY : c.h - 95 * u;
        BGfx::Disc(c, scx, scy, sr, 0x40FFFFFFu);
        BGfx::Ring(c, scx, scy, sr, sr, 2 * u, 0x80FFFFFFu);
        BGfx::Disc(c, scx + B.stickDX * sr * 0.6f, scy + B.stickDY * sr * 0.6f, 24 * u, 0xB0FFFFFFu);
        // (The buttons, as the site lays them: ATQ big in the corner, the rest round it.)
        const float ax = c.w - 82 * u, ay = c.h - 88 * u;
        for (float& r : B.br) r = 0; // (only those drawn this frame are touched)
        if (b.f[0].spec.mega >= 0 && !b.megaUsed[0] && !b.f[0].mega) BattleButton(c, u, Arena::S_EVO, ax - 172 * u, ay - 62 * u, 25 * u);
        BattleButton(c, u, Arena::S_ATQ, ax, ay, 44 * u);
        BattleButton(c, u, Arena::S_HAB, ax - 98 * u, ay + 28 * u, 29 * u);
        BattleButton(c, u, Arena::S_Q, ax - 84 * u, ay - 74 * u, 29 * u);
        BattleButton(c, u, Arena::S_ULT, ax + 4 * u, ay - 118 * u, 30 * u);
        BattleButton(c, u, Arena::S_DASH, ax - 182 * u, ay + 36 * u, 26 * u);
    }
    // The big words: who comes in, FIGHT!, K.O.!, a ULT's name.
    if (b.banner.life > 0 && b.banner.t < b.banner.life)
    {
        const float t = b.banner.t / b.banner.life;
        const float in = std::min(1.0f, t * 6), out = t > 0.8f ? (1 - t) / 0.2f : 1;
        const float bh = 64 * u * in, cy = c.h * 0.42f;
        for (int y = (int)(cy - bh / 2); y < (int)(cy + bh / 2); y++)
        {
            const int shift = (int)((y - cy) * 0.25f);
            BGfx::Rect(c, shift, y, c.w, 1, BGfx::Alpha(Arena::TypeColour(b.banner.type), 0.75f * out));
        }
        BGfx::Text(c, c.w / 2.0f + (1 - in) * c.w * 0.3f, cy, b.banner.text, 34 * u, BGfx::Alpha(0xFFFFFFFFu, out), true, BGfx::Alpha(0xFF101418u, out));
    }
    if (b.phase == Arena::Battle::INTRO && b.cur[1] == 0 && b.kos[0] == 0 && b.kos[1] == 0)
    {
        // (The first time: the trainer, who wants to fight.)
        const float t = std::min(1.0f, b.phaseT * 3);
        if (const ArtImg* a = TrainerArt(B.opp.sprite)) DrawArt(c, *a, (int)(c.w - (40 + 220 * t) * u), (int)(c.h * 0.22f), (int)(200 * u), (int)(200 * u));
        BGfx::Text(c, c.w / 2.0f, c.h * 0.2f, B.opp.name + " quer lutar!", 20 * u, 0xFFFFD040u);
    }
    if (b.phase == Arena::Battle::INTRO)
    {
        const bool first = b.phaseT < 0.8f;
        const std::string who = b.f[0].spec.name + "  vs  " + b.f[1].spec.name;
        BGfx::Text(c, c.w / 2.0f, c.h * 0.3f, who, 18 * u, 0xFFFFFFFFu);
        BGfx::Text(c, c.w / 2.0f, c.h * 0.42f, first ? "Prontos..." : "Lutar!", (first ? 30 : 44) * u, first ? 0xFFFFFFFFu : 0xFFFFD040u);
    }
    if (b.phase == Arena::Battle::KO) BGfx::Text(c, c.w / 2.0f, c.h * 0.42f, "K.O.!", 48 * u * std::min(1.0f, 0.6f + b.phaseT * 2), 0xFFFF4040u);
    // The end: who won, each side's damage and hits; REMATCH, MENU.
    if (b.phase == Arena::Battle::END || B.paused)
    {
        for (auto& p : c.px) p = 0xFF000000u | ((p >> 1) & 0x7F7F7Fu);
        const float w = std::min(520 * u, c.w - 40 * u), h = B.paused ? 190 * u : 280 * u;
        const float x0 = (c.w - w) / 2, y0 = (c.h - h) / 2;
        Glass::Round(c, (int)x0, (int)y0, (int)w, (int)h, 14 * u, 0xF0121A2Au, 0xFF4E8A56u, 3 * u);
        auto button = [&](const std::string& l, int kind, float cx, bool strong) {
            const float bw = std::max(140 * u, (float)GW(l, 15 * u, true) + 40 * u), bh = 42 * u, by = y0 + h - 62 * u;
            Glass::Round(c, (int)(cx - bw / 2), (int)by, (int)bw, (int)bh, 10 * u, strong ? 0xFF4E8A56u : 0xFF2A3446u, strong ? 0xFF3A6C41u : 0xFF606878u, 1.5f * u);
            BGfx::Text(c, cx, by + bh / 2, l, 15 * u, 0xFFFFFFFFu);
            B.overHits.push_back({ (int)(cx - bw / 2), (int)by, (int)bw, (int)bh, kind, 0, 0 });
        };
        if (B.paused)
        {
            BGfx::Text(c, c.w / 2.0f, y0 + 50 * u, "Pausado", 28 * u, 0xFFFFFFFFu);
            button("Continuar", BT_RESUME, c.w / 2.0f - 90 * u, true);
            button("Desistir", BT_QUIT, c.w / 2.0f + 90 * u, false);
        }
        else
        {
            const bool won = b.winner == 0;
            BGfx::Text(c, c.w / 2.0f, y0 + 42 * u, won ? "Vit\xC3\xB3ria!" : "Derrota", 32 * u, won ? 0xFFFFD040u : 0xFFFF6060u);
            BGfx::Text(c, c.w / 2.0f, y0 + 72 * u, "Voc\xC3\xAA " + std::to_string(b.kos[0]) + " x " + std::to_string(b.kos[1]) + " " + B.opp.name, 14 * u, 0xFFE0E0E0u);
            BGfx::Text(c, c.w / 2.0f, y0 + 94 * u, (B.delta >= 0 ? "+" : "") + Money(B.delta) + (B.endNote.empty() ? std::string() : "   \xC2\xB7   " + B.endNote), 12.5f * u, B.delta >= 0 ? 0xFF8CF08Cu : 0xFFFF9090u);
            const float cx0 = c.w / 2.0f - 110 * u, cx1 = c.w / 2.0f + 110 * u, ry = y0 + 126 * u;
            BGfx::Text(c, cx0, ry, "Voc\xC3\xAA", 13 * u, 0xFF8CB8FFu);
            BGfx::Text(c, cx1, ry, B.opp.name, 13 * u, 0xFFFF9090u);
            BGfx::Text(c, c.w / 2.0f, ry + 26 * u, "Dano", 11 * u, 0xFFA0A8B8u, false);
            BGfx::Text(c, cx0, ry + 26 * u, std::to_string(b.dealt[0]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, cx1, ry + 26 * u, std::to_string(b.dealt[1]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, c.w / 2.0f, ry + 50 * u, "Acertos", 11 * u, 0xFFA0A8B8u, false);
            BGfx::Text(c, cx0, ry + 50 * u, std::to_string(b.hits[0]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, cx1, ry + 50 * u, std::to_string(b.hits[1]), 14 * u, 0xFFFFFFFFu);
            if (B.opp.kind == 0) { button("Revanche", BT_REMATCH, c.w / 2.0f - 90 * u, true); button("Menu", BT_MENU, c.w / 2.0f + 90 * u, false); }
            else if (B.opp.kind == 3 && !won) { button("Tentar de novo", BT_REMATCH, c.w / 2.0f - 100 * u, true); button("Menu", BT_MENU, c.w / 2.0f + 100 * u, false); }
            else button("Continuar", BT_MENU, c.w / 2.0f, true);
        }
    }
}

// ---- The fight's input: fingers (id), the mouse (-2), keys ---------------------------------------
inline void BattleOver(int kind)
{
    BattleState& B = Bat();
    if (kind == BT_PAUSE) B.paused = true;
    else if (kind == BT_RESUME) { B.paused = false; B.last = std::chrono::steady_clock::now(); }
    else if (kind == BT_QUIT || kind == BT_MENU)
    {
        // (Given up mid-fight: a loss.)
        if (!B.settled && B.b.phase != Arena::Battle::END) { B.b.winner = 1; BattleSettle(); }
        BattleEnd();
    }
    else if (kind == BT_REMATCH) { BattleSetOpp(false); BattleBegin(); }
    Hub().version++;
}
// phase 0 down, 1 moved, 2 up; x, y in the canvas's pixels.
inline void BattlePointer(int phase, long long id, float x, float y)
{
    BattleState& B = Bat();
    const float u = B.u;
    if (phase == 0)
    {
        for (const PcHit& h : B.overHits)
            if (x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) { BattleOver(h.kind); return; }
        if (B.paused || B.b.phase == Arena::Battle::END) return;
        for (int k = 0; k < Arena::SLOTS; k++)
        {
            const float dx = x - B.bx[k], dy = y - B.by[k], r = B.br[k] + 8 * u;
            if (B.br[k] > 0 && dx * dx + dy * dy < r * r) { B.held[k] = id; B.pressed[k] = true; return; }
        }
        // (Down on the left half: the stick, from there.)
        if (B.stickId == -1 && x < B.bx[Arena::S_DASH] - B.br[Arena::S_DASH] - 20 * u) { B.stickId = id; B.stickX = x; B.stickY = y; B.stickDX = B.stickDY = 0; }
        return;
    }
    if (id == B.stickId)
    {
        if (phase == 2) { B.stickId = -1; B.stickDX = B.stickDY = 0; return; }
        const float r = 54 * u;
        float dx = (x - B.stickX) / r, dy = (y - B.stickY) / r;
        const float l = std::sqrt(dx * dx + dy * dy);
        // (Past the edge: the stick follows the finger, as the site's does.)
        if (l > 1.3f) { B.stickX += (dx / l) * (l - 1.3f) * r; B.stickY += (dy / l) * (l - 1.3f) * r; dx = dx / l * 1.3f; dy = dy / l * 1.3f; }
        const float m = std::min(1.0f, l);
        B.stickDX = l > 0 ? dx / std::max(1.0f, l) : 0; B.stickDY = l > 0 ? dy / std::max(1.0f, l) : 0;
        (void)m;
        return;
    }
    if (phase == 2) for (long long& h : B.held) if (h == id) h = -1;
}
// A key, on a PC: W A S D or the arrows move; J Atq, K Hab, L Q, U ULT, space the dash; Esc pause.
// which: 0..3 the moves (up, left, down, right), 10 + a slot, 20 pause.
inline void BattleKeyIn(int which, bool down)
{
    BattleState& B = Bat();
    if (which >= 0 && which < 4) { B.keys4[which] = down; return; }
    if (which >= 10 && which < 10 + Arena::SLOTS)
    {
        const int k = which - 10;
        if (down && B.held[k] != -3) B.pressed[k] = true;
        B.held[k] = down ? -3 : -1;
        return;
    }
    if (which == 20 && down)
    {
        if (B.b.phase == Arena::Battle::END) BattleEnd();
        else if (B.paused) BattleOver(BT_RESUME);
        else B.paused = true;
    }
}

// ---- The BATTLE tab: the teams and FIGHT ----------------------------------------------------------
inline void BattleCard(Canvas& c, float u, int x, int y, int w, int h, const Arena::MonSpec& m, bool sel)
{
    Glass::Round(c, x, y, w, h, 8 * u, sel ? PcPal::Body : PcPal::Btn, sel ? PcPal::Primary : PcPal::Line, sel ? 3.0f * u : 1.0f * u);
    PcDrawMon(c, m.species, m.shiny, false, x + (int)(6 * u), y + (int)(4 * u), w - (int)(12 * u), h - (int)(26 * u), 0, true);
    GTFit(c, x + (int)(6 * u), y + h - 11 * u, m.name, 10.5f * u, w - (int)(12 * u), PcPal::Ink, true);
    if (m.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, x + w - 9 * u, y + 9 * u, 5 * u, PcPal::Shiny);
}
// The chosen one's sheet: types, its stats at 50, and what each button does.
inline void BattleSheet(Canvas& c, float u, int x, int y, int w, int h, const Arena::MonSpec& m, int mine = -1)
{
    const std::string lvl = "  Lv." + std::to_string(m.level);
    PcCard(c, x, y, w, h, 8 * u, u);
    const int pad = (int)(10 * u);
    int tx = x + pad, ty = y + pad;
    GTFit(c, tx, ty + 8 * u, m.name + lvl, 13 * u, w / 2, PcPal::Ink, true);
    int cx = tx + std::min(GW(m.name + lvl, 13 * u, true), w / 2) + (int)(8 * u);
    for (int t : m.types)
    {
        if (t < 0) continue;
        const std::string n = Arena::TypeNamePt(t);
        const int cw = GW(n, 9.5f * u, true) + (int)(12 * u);
        Glass::Round(c, cx, ty, cw, (int)(17 * u), 6 * u, Arena::TypeColour(t));
        GT(c, cx + (int)(6 * u), ty + 8.5f * u, n, 9.5f * u, 0xFFFFFFFFu, true);
        cx += cw + (int)(5 * u);
    }
    ty += (int)(24 * u);
    static const char* sn[6] = { "PS", "Atq", "Def", "Vel", "AtE", "DfE" };
    const int sw = (w - 2 * pad) / 6;
    for (int i = 0; i < 6; i++)
    {
        GT(c, tx + i * sw, ty + 6 * u, sn[i], 9.5f * u, PcPal::Stat[i], true);
        GT(c, tx + i * sw, ty + 20 * u, std::to_string(m.stats[i]), 11.5f * u, PcPal::Ink, true);
    }
    ty += (int)(32 * u);
    Arena::Battle tmp;
    Arena::Fighter f;
    f.spec = m;
    tmp.MakeKit(f);
    for (int k = 0; k < 4 && ty + 14 * u < y + h; k++)
    {
        const int mv = f.sk[k].move;
        const BattleData::MoveInfo& mi = BattleData::Move(mv);
        GT(c, tx, ty + 6 * u, Arena::SlotLabel(k), 10 * u, PcPal::Hint, true);
        const std::string kind = mi.cat == 2 ? "status" : mi.cat == 1 ? "especial" : "f\xC3\xADsico";
        const std::string line = mv ? std::string(mi.name) + "  \xC2\xB7  " + Arena::TypeNamePt(mi.type) + ", " + kind + (mi.power ? ", " + std::to_string(mi.power) : std::string()) : std::string("-");
        GTFit(c, tx + (int)(40 * u), ty + 6 * u, line, 10.5f * u, w - 2 * pad - (int)(40 * u), PcPal::Ink, false);
        ty += (int)(15 * u);
    }
    // (Its Mega Stone: here, in your bag, or held in its save.)
    const int mi = Arena::MegaFor(m.species);
    if (mine >= 0 && mi >= 0 && ty + 30 * u <= y + h)
    {
        BattleState& B = Bat();
        const Arena::MegaInfo& mg = Arena::Megas()[mi];
        const bool here = mine < (int)B.keys.size() && B.equip.count(EquipKey(B.keys[mine]));
        const bool held = m.mega >= 0 && !here;
        const int iy = ty + (int)(4 * u), is = (int)(24 * u);
        if (const ArtImg* a = ItemArt(mg.icon)) DrawArt(c, *a, tx, iy, is, is);
        const std::string what = std::string(mg.stone) + (held ? "  \xC2\xB7  segura no save" : here ? "  \xC2\xB7  equipada" : "  \xC2\xB7  voc\xC3\xAA tem " + std::to_string(B.stones[mi]));
        GTFit(c, tx + is + (int)(8 * u), iy + is / 2.0f, what, 10.5f * u, w - 2 * pad - is - (int)(110 * u), PcPal::Ink, true);
        if (!held)
        {
            const std::string l = here ? "Tirar" : "Equipar";
            const bool can = here || B.stones[mi] > 0;
            const int bw = GW(l, 10.5f * u, true) + (int)(20 * u), bh = (int)(24 * u), bx = x + w - pad - bw;
            PcButton(c, bx, iy, bw, bh, 6 * u, can && !here, u);
            GT(c, bx + (int)(10 * u), iy + bh / 2.0f, l, 10.5f * u, can && !here ? PcPal::OnPrimary : can ? PcPal::Ink : PcPal::Hint, true);
            B.hits.push_back({ bx, iy, bw, bh, BT_EQUIP, 0, mine });
        }
    }
}
// (A Pokémon of the choosing's: as chosen it is the green frame and its number in the team.)
inline const BattleState::PickMon* BattlePickFind(const BattleKey& k)
{
    for (const auto& g : Bat().pg) for (const auto& m : g.mons) if (m.k == k) return &m;
    return nullptr;
}
inline void DrawBattlePicker(Canvas& c, float u, int L, int R, int top, int bottom)
{
    BattleState& B = Bat();
    // The saves, a tab each.
    int x = L, ty = top;
    const int th = (int)(32 * u);
    for (int i = 0; i < (int)B.pg.size(); i++)
    {
        const std::string l = B.pg[i].name + "  " + std::to_string(B.pg[i].mons.size());
        const int w = GW(l, 12 * u, true) + (int)(24 * u);
        if (x + w > R) break;
        PcButton(c, x, ty, w, th, 8 * u, i == B.pgTab, u);
        GT(c, x + (int)(12 * u), ty + th / 2.0f, l, 12 * u, i == B.pgTab ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ x, ty, w, th, BT_PG_TAB, 0, i });
        x += w + (int)(6 * u);
    }
    // The foot: the team so far, CLEAR, CANCEL, DONE.
    const int footH = (int)(70 * u), fy = bottom - footH;
    PcCard(c, L, fy, R - L, footH, 10 * u, u);
    const int ss = footH - (int)(16 * u);
    GT(c, L + (int)(12 * u), fy + footH / 2.0f, "Time " + std::to_string(B.pickSel.size()) + "/6", 13 * u, PcPal::Ink, true);
    int sx = L + (int)(12 * u) + GW("Time 6/6", 13 * u, true) + (int)(12 * u);
    for (int i = 0; i < 6; i++)
    {
        const int x0 = sx + i * (ss + (int)(6 * u));
        const BattleState::PickMon* m = i < (int)B.pickSel.size() ? BattlePickFind(B.pickSel[i]) : nullptr;
        Glass::Round(c, x0, fy + (int)(8 * u), ss, ss, 8 * u, m ? PcPal::Body : PcPal::Slot, m ? PcPal::Primary : PcPal::Line, m ? 2.0f * u : 1.0f * u);
        if (m) { PcDrawMon(c, m->species, m->shiny, false, x0 + (int)(4 * u), fy + (int)(12 * u), ss - (int)(8 * u), ss - (int)(8 * u), 0, true); B.hits.push_back({ x0, fy + (int)(8 * u), ss, ss, BT_PG_SLOT, 0, i }); }
    }
    int bx = R - (int)(10 * u);
    auto button = [&](const std::string& l, int kind, bool strong) {
        const int bw = GW(l, 13 * u, true) + (int)(28 * u), bh = footH - (int)(22 * u);
        bx -= bw;
        PcButton(c, bx, fy + (int)(11 * u), bw, bh, 8 * u, strong, u);
        GT(c, bx + (int)(14 * u), fy + footH / 2.0f, l, 13 * u, strong ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ bx, fy + (int)(11 * u), bw, bh, kind, 0, 0 });
        bx -= (int)(8 * u);
    };
    if (!B.pickSel.empty()) button("Pronto", BT_PG_DONE, true);
    button("Cancelar", BT_PG_CANCEL, false);
    if (!B.pickSel.empty()) button("Limpar", BT_PG_CLEAR, false);
    if (!B.msg.empty()) GTFit(c, sx + 6 * (ss + (int)(6 * u)) + (int)(6 * u), fy + footH / 2.0f, B.msg, 11 * u, bx - sx - 6 * (ss + (int)(6 * u)) - (int)(10 * u), PcPal::Hint, false);
    // Its Pokémon: a grid, dragged up and down; those cut by the edges are left out.
    const int gt = ty + th + (int)(10 * u), gb = fy - (int)(10 * u);
    PcCard(c, L, gt, R - L, gb - gt, 10 * u, u);
    if (B.pg.empty()) { GT(c, L + (int)(16 * u), gt + 24 * u, "Nenhum Pok\xC3\xA9mon nos seus saves.", 13 * u, PcPal::Hint, false); B.pickMax = 0; return; }
    B.pgTab = std::max(0, std::min(B.pgTab, (int)B.pg.size() - 1));
    const auto& mons = B.pg[B.pgTab].mons;
    const int pad = (int)(10 * u), gw = R - L - 2 * pad;
    const int cols = std::max(4, gw / (int)(92 * u)), gap = (int)(6 * u);
    const int cw = (gw - (cols - 1) * gap) / cols, ch = (int)(cw * 1.1f);
    const int rows = ((int)mons.size() + cols - 1) / cols;
    B.pickMax = std::max(0.0f, (float)(rows * (ch + gap) - (gb - gt - 2 * pad)));
    B.pickScroll = std::min(B.pickScroll, B.pickMax);
    // (Cells cut by the edges drawn whole, then what went past the edges put back as it was.)
    const int clipTop = gt + (int)(3 * u), clipBottom = gb - (int)(3 * u);
    const std::vector<uint32_t> keep(c.px.begin(), c.px.end());
    for (int i = 0; i < (int)mons.size(); i++)
    {
        const int x0 = L + pad + (i % cols) * (cw + gap), y0 = gt + pad + (i / cols) * (ch + gap) - (int)B.pickScroll;
        if (y0 + ch < clipTop || y0 > clipBottom) continue;
        const auto& m = mons[i];
        const auto it = std::find(B.pickSel.begin(), B.pickSel.end(), m.k);
        const bool on = it != B.pickSel.end();
        Glass::Round(c, x0, y0, cw, ch, 8 * u, on ? PcPal::Body : PcPal::Btn, on ? PcPal::Primary : PcPal::Line, on ? 3.0f * u : 1.0f * u);
        PcDrawMon(c, m.species, m.shiny, false, x0 + (int)(6 * u), y0 + (int)(4 * u), cw - (int)(12 * u), ch - (int)(30 * u), 0, true);
        GTFit(c, x0 + (int)(6 * u), y0 + ch - 19 * u, m.name, 10 * u, cw - (int)(12 * u), PcPal::Ink, true);
        GT(c, x0 + (int)(6 * u), y0 + ch - 7 * u, "Lv." + std::to_string(m.level), 9 * u, PcPal::Hint, false);
        if (on)
        {
            const std::string n = std::to_string(it - B.pickSel.begin() + 1);
            BGfx::Disc(c, x0 + cw - 12 * u, y0 + 12 * u, 9 * u, PcPal::Primary);
            GT(c, x0 + cw - 12 * u - GW(n, 10 * u, true) / 2, y0 + 12 * u, n, 10 * u, PcPal::OnPrimary, true);
        }
        if (m.shiny) Glass::DrawIcon(c, Glass::I_SPARKLE, x0 + 10 * u, y0 + 10 * u, 5 * u, PcPal::Shiny);
        if (Arena::NoPicture(m.species))
        {
            // (No moving picture: faded, and not to be chosen.)
            BGfx::Rect(c, x0, y0, cw, ch, 0xB0EAF1E2u);
            GTFit(c, x0 + (int)(6 * u), y0 + ch * 0.45f, "sem sprite", 9.5f * u, cw - (int)(12 * u), PcPal::Hint, true);
        }
        const int hy0 = std::max(y0, clipTop), hy1 = std::min(y0 + ch, clipBottom);
        if (hy1 > hy0) B.hits.push_back({ x0, hy0, cw, hy1 - hy0, BT_PG_MON, 0, i });
    }
    for (int y = 0; y < c.h; y++)
        if (y < clipTop || y >= clipBottom)
            std::copy(keep.begin() + (size_t)y * c.w + L, keep.begin() + (size_t)y * c.w + R, c.px.begin() + (size_t)y * c.w + L);
    if (B.pickMax > 0)
    {
        // (How far down: a thin bar at the right.)
        const float vis = (float)(gb - gt), total = vis + B.pickMax, bh = std::max(20 * u, vis * vis / total);
        const float by = gt + (vis - bh) * (B.pickScroll / B.pickMax);
        BGfx::Rect(c, R - (int)(6 * u), (int)by, (int)(3 * u), (int)bh, 0x60000000u);
    }
}
// (A team card held: it lifts after a moment, then goes where the finger goes; the team's page and
// the preparation's.)
inline void BattleDragDraw(Canvas& c, float u)
{
    BattleState& B = Bat();
    if (B.down && B.downCard >= 0 && B.dragCard < 0)
    {
        Hub().version++; // (frames while it is held, to show it lift)
        if (std::chrono::duration<float>(std::chrono::steady_clock::now() - B.downAt).count() >= 0.35f && !B.dragged) { B.dragCard = B.downCard; B.dragX = B.downX; B.dragY = B.downY; }
    }
    if (B.dragCard >= 0 && B.dragCard < (int)B.mine.size())
    {
        const int cw = (int)(100 * u), ch = (int)(118 * u);
        BattleCard(c, u, B.dragX - cw / 2, B.dragY - ch / 2, cw, ch, B.mine[B.dragCard], true);
    }
}
// A small trainer tile: picture, name, the type chip; done (a check) or the one now (framed).
inline void BattleTrainerTile(Canvas& c, float u, int x, int y, int w, int h, const Arena::Trainer& t, bool done, bool now, const std::string& tag)
{
    Glass::Round(c, x, y, w, h, 8 * u, now ? PcPal::Body : PcPal::Btn, now ? PcPal::Primary : PcPal::Line, now ? 3.0f * u : 1.0f * u);
    if (const ArtImg* a = TrainerArt(t.sprite)) DrawArt(c, *a, x + (int)(4 * u), y + (int)(4 * u), w - (int)(8 * u), h - (int)(34 * u), done ? 0.45f : 1);
    GTFit(c, x + (int)(5 * u), y + h - 22 * u, t.name, 10 * u, w - (int)(10 * u), PcPal::Ink, true);
    if (t.type >= 0)
    {
        const std::string n = Arena::TypeNamePt(t.type);
        const int cw = std::min(w - (int)(10 * u), GW(n, 8 * u, true) + (int)(8 * u));
        Glass::Round(c, x + (int)(5 * u), y + h - (int)(15 * u), cw, (int)(12 * u), 4 * u, Arena::TypeColour(t.type));
        GT(c, x + (int)(9 * u), y + h - 9 * u, n, 8 * u, 0xFFFFFFFFu, true);
    }
    else GT(c, x + (int)(5 * u), y + h - 9 * u, tag, 8.5f * u, PcPal::Hint, true);
    if (done) { BGfx::Disc(c, x + w - 11 * u, y + 11 * u, 8 * u, PcPal::Primary); GT(c, (int)(x + w - 15 * u), y + 11 * u, "ok", 7.5f * u, PcPal::OnPrimary, true); }
}
// The right side: whom you fight. A free battle: the trainer drawn and its team; the gyms and the
// Elite Four: the region, the eight (or five) in their order, and the next one's team.
inline void BattleOppPanel(Canvas& c, float u, int x, int top, int pw, int ph)
{
    BattleState& B = Bat();
    PcCard(c, x, top, pw, ph, 10 * u, u);
    const auto& regs = Arena::Regions();
    int y = top + (int)(8 * u);
    if (B.mode != 0)
    {
        int cx = x + (int)(10 * u);
        for (int r = 0; r < (int)regs.size(); r++)
        {
            const std::string l = regs[r].name;
            const int w = GW(l, 11 * u, true) + (int)(18 * u), h = (int)(26 * u);
            const bool open = B.mode == 1 ? GymsOpen(r) : EliteOpen(r);
            if (open || r == B.region) PcButton(c, cx, y, w, h, 7 * u, r == B.region, u);
            else Glass::Round(c, cx, y, w, h, 7 * u, PcPal::Slot, PcPal::Line, 1.0f * u);
            GT(c, cx + (int)(9 * u), y + h / 2.0f, l, 11 * u, r == B.region ? PcPal::OnPrimary : open ? PcPal::Ink : PcPal::Hint, true);
            if (!B.eliteOn) B.hits.push_back({ cx, y, w, h, BT_REGION, 0, r });
            cx += w + (int)(5 * u);
        }
        y += (int)(34 * u);
        const Arena::Region& R = regs[B.region];
        // (The gyms: any of the eight to choose, Red too once Johto's are won; the Elite Four's five.)
        const bool red = B.mode == 1 && B.region == 1 && RedOpen();
        const int n = B.mode == 1 ? (red ? 9 : 8) : 5, per = n;
        const int gw = pw - (int)(20 * u), tw = (gw - (per - 1) * (int)(5 * u)) / per, th = std::min((int)(tw * 1.55f), (int)(ph * 0.42f));
        for (int i = 0; i < n; i++)
        {
            const Arena::Trainer& t = B.mode == 1 ? (i == 8 ? Arena::Red() : R.gyms[i]) : R.elite[i];
            const bool done = B.mode == 1 ? (i == 8 ? B.redBeaten : (B.badges[B.region] >> i & 1) != 0) : (B.eliteBeat[B.region] >> i & 1) != 0;
            const int tx = x + (int)(10 * u) + i * (tw + (int)(5 * u));
            BattleTrainerTile(c, u, tx, y, tw, th, t, done, B.mode == 1 ? i == B.gymSel : i == 0, i == 8 ? "Monte Prateado" : i == 4 && B.mode == 2 ? "Campe\xC3\xA3o" : "");
            if (B.mode == 1) B.hits.push_back({ tx, y, tw, th, BT_GYM_SEL, 0, i });
        }
        y += th + (int)(8 * u);
        std::string line;
        const std::string lock = LockReason(B.mode, B.region);
        if (!lock.empty()) line = lock;
        else if (B.mode == 1 && B.gymSel == 8)
            line = std::string(B.redBeaten ? "Revanche: " : "Liberado: ") + "Red, no Monte Prateado  \xC2\xB7  " + Money(Arena::RED_PRIZE);
        else if (B.mode == 1)
            line = std::string("Gin\xC3\xA1sio de ") + R.gyms[B.gymSel].name + ": 2 treinadores e o l\xC3\xAD" "der, sem curar  \xC2\xB7  " + Money(Arena::GymPrize(B.gymSel)) + "  \xC2\xB7  " + std::to_string(BadgeCount(B.region)) + "/8 ins\xC3\xADgnias";
        else
            line = std::string("5 batalhas seguidas, sem curar entre elas  \xC2\xB7  ") + (B.champion[B.region] ? "Campe\xC3\xA3o " + std::to_string(B.champion[B.region]) + "x" : std::string("Itens na prepara\xC3\xA7\xC3\xA3o"));
        GTFit(c, x + (int)(12 * u), y + 8 * u, line, 11.5f * u, pw - (int)(24 * u), lock.empty() ? PcPal::Ink : 0xFFB04040u, true);
        y += (int)(20 * u);
    }
    else
    {
        // (The trainer drawn: big, its name; another one.)
        const int ts = std::min((int)(ph * 0.42f), (int)(pw * 0.3f));
        if (const ArtImg* a = TrainerArt(B.opp.sprite)) DrawArt(c, *a, x + (int)(10 * u), y, ts, ts);
        GTFit(c, x + ts + (int)(20 * u), y + 22 * u, B.opp.name, 16 * u, pw - ts - (int)(30 * u), PcPal::Ink, true);
        GT(c, x + ts + (int)(20 * u), y + 44 * u, "Todos no n\xC3\xADvel 50  \xC2\xB7  vit\xC3\xB3ria: " + Money(Arena::FREE_PRIZE), 11 * u, PcPal::Hint, false);
        const std::string bl = "Sortear outro";
        const int bw = GW(bl, 11.5f * u, true) + (int)(22 * u), bh = (int)(28 * u), bxx = x + ts + (int)(20 * u);
        PcButton(c, bxx, y + (int)(60 * u), bw, bh, 7 * u, false, u);
        GT(c, bxx + (int)(11 * u), y + 60 * u + bh / 2.0f, bl, 11.5f * u, PcPal::Ink, true);
        B.hits.push_back({ bxx, y + (int)(60 * u), bw, bh, BT_REROLL, 1, 0 });
        const std::string pl = "Escolher advers\xC3\xA1rio";
        const int pw2 = GW(pl, 11.5f * u, true) + (int)(22 * u), px2 = bxx + bw + (int)(8 * u);
        PcButton(c, px2, y + (int)(60 * u), pw2, bh, 7 * u, true, u);
        GT(c, px2 + (int)(11 * u), y + 60 * u + bh / 2.0f, pl, 11.5f * u, PcPal::OnPrimary, true);
        B.hits.push_back({ px2, y + (int)(60 * u), pw2, bh, BT_OPP_OPEN, 1, 0 });
        y += ts + (int)(8 * u);
    }
    // (Its team: six places, the one tapped's sheet under them.)
    const int gx = x + (int)(10 * u), gw = pw - (int)(20 * u);
    const int cw = (gw - 5 * (int)(6 * u)) / 6, ch = std::min((int)(cw * 1.2f), std::max((int)(40 * u), top + ph - y - (int)(10 * u)));
    for (int i = 0; i < 6; i++)
    {
        const int x0 = gx + i * (cw + (int)(6 * u));
        if (i < (int)B.bot.size()) { BattleCard(c, u, x0, y, cw, ch, B.bot[i], i == B.botSel); B.hits.push_back({ x0, y, cw, ch, BT_BOTCARD, 1, i }); }
        else Glass::Round(c, x0, y, cw, ch, 8 * u, PcPal::Slot, PcPal::Line, 1.0f * u);
    }
    const int sy = y + ch + (int)(8 * u), sh = top + ph - (int)(10 * u) - sy;
    if (B.botSel >= 0 && B.botSel < (int)B.bot.size() && sh > 60 * u) BattleSheet(c, u, gx, sy, gw, sh, B.bot[B.botSel]);
}
// THE SHOP: what it sells, its price, how many you have; your money. Two tabs: the items (for the
// runs' preparation) and the Mega Stones (given to one of the team in its sheet).
inline void DrawBattleShop(Canvas& c, float u, int L, int R, int top, int bottom)
{
    BattleState& B = Bat();
    PcCard(c, L, top, R - L, bottom - top, 10 * u, u);
    GT(c, L + (int)(16 * u), top + 24 * u, "Loja", 20 * u, PcPal::Ink, true);
    int tx = L + (int)(16 * u) + GW("Loja", 20 * u, true) + (int)(20 * u);
    GT(c, tx, top + 24 * u, Money(B.money), 15 * u, PcPal::Primary, true);
    tx += GW(Money(B.money), 15 * u, true) + (int)(24 * u);
    static const char* tabs[2] = { "Itens", "Mega Pedras" };
    for (int t = 0; t < 2; t++)
    {
        const int w = GW(tabs[t], 12 * u, true) + (int)(24 * u), h = (int)(30 * u);
        PcButton(c, tx, top + (int)(9 * u), w, h, 7 * u, B.shopTab == t, u);
        GT(c, tx + (int)(12 * u), top + 9 * u + h / 2.0f, tabs[t], 12 * u, B.shopTab == t ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ tx, top + (int)(9 * u), w, h, BT_SHOP_TAB, 0, t });
        tx += w + (int)(6 * u);
    }
    {
        const std::string l = "Voltar";
        const int w = GW(l, 12 * u, true) + (int)(26 * u), h = (int)(32 * u), bx = R - w - (int)(12 * u);
        PcButton(c, bx, top + (int)(8 * u), w, h, 7 * u, false, u);
        GT(c, bx + (int)(13 * u), top + 8 * u + h / 2.0f, l, 12 * u, PcPal::Ink, true);
        B.hits.push_back({ bx, top + (int)(8 * u), w, h, BT_SHOP_BACK, 0, 0 });
    }
    const int gap = (int)(10 * u);
    const int gx = L + (int)(14 * u), gy = top + (int)(52 * u), gw = R - L - (int)(28 * u), gh = bottom - gy - (int)(40 * u);
    if (B.shopTab == 0)
    {
        const auto& shop = Arena::Shop();
        const int rows = 3, cols = 2;
        const int cw = (gw - gap) / cols, ch = (gh - (rows - 1) * gap) / rows;
        for (int i = 0; i < (int)shop.size(); i++)
        {
            const int x0 = gx + (i % cols) * (cw + gap), y0 = gy + (i / cols) * (ch + gap);
            Glass::Round(c, x0, y0, cw, ch, 8 * u, PcPal::Btn, PcPal::Line, 1.0f * u);
            const int is = std::min(ch - (int)(16 * u), (int)(64 * u));
            if (const ArtImg* a = ItemArt(shop[i].icon)) DrawArt(c, *a, x0 + (int)(10 * u), y0 + (ch - is) / 2, is, is);
            const int tx0 = x0 + is + (int)(22 * u);
            GT(c, tx0, y0 + ch * 0.32f, shop[i].name, 14 * u, PcPal::Ink, true);
            GT(c, tx0, y0 + ch * 0.62f, shop[i].what, 11 * u, PcPal::Hint, false);
            GT(c, tx0, y0 + ch * 0.85f, "Voc\xC3\xAA tem " + std::to_string(B.bag[i]), 10.5f * u, PcPal::Ink, false);
            const std::string l = Money(shop[i].price);
            const bool can = B.money >= shop[i].price;
            const int w = GW(l, 12 * u, true) + (int)(26 * u), h = std::min((int)(36 * u), ch - (int)(16 * u)), bx = x0 + cw - w - (int)(12 * u);
            PcButton(c, bx, y0 + (ch - h) / 2, w, h, 7 * u, can, u);
            GT(c, bx + (int)(13 * u), y0 + ch / 2.0f, l, 12 * u, can ? PcPal::OnPrimary : PcPal::Hint, true);
            B.hits.push_back({ bx, y0 + (ch - h) / 2, w, h, BT_BUY, 0, i });
        }
    }
    else
    {
        // (The Mega Stones: a tile each, its picture, its Pokémon, how many you have; a tap buys one.)
        const auto& megas = Arena::Megas();
        const int cols = 6, rows = ((int)megas.size() + cols - 1) / cols, g2 = (int)(6 * u);
        const int cw = (gw - (cols - 1) * g2) / cols, ch = (gh - (rows - 1) * g2) / rows;
        for (int i = 0; i < (int)megas.size(); i++)
        {
            const int x0 = gx + (i % cols) * (cw + g2), y0 = gy + (i / cols) * (ch + g2);
            const bool can = B.money >= Arena::MEGA_PRICE;
            Glass::Round(c, x0, y0, cw, ch, 7 * u, PcPal::Btn, PcPal::Line, 1.0f * u);
            const int is = std::min(ch - (int)(10 * u), (int)(40 * u));
            if (const ArtImg* a = ItemArt(megas[i].icon)) DrawArt(c, *a, x0 + (int)(5 * u), y0 + (ch - is) / 2, is, is);
            const int tx0 = x0 + is + (int)(12 * u), tw = cw - is - (int)(16 * u);
            GTFit(c, tx0, y0 + ch * 0.28f, megas[i].stone, 10.5f * u, tw, PcPal::Ink, true);
            GTFit(c, tx0, y0 + ch * 0.55f, Money(Arena::MEGA_PRICE), 9.5f * u, tw, can ? PcPal::Primary : PcPal::Hint, true);
            GTFit(c, tx0, y0 + ch * 0.8f, "tem " + std::to_string(B.stones[i]), 9.5f * u, tw, PcPal::Hint, false);
            B.hits.push_back({ x0, y0, cw, ch, BT_BUY_STONE, 0, i });
        }
    }
    GTFit(c, gx, bottom - 18 * u, !B.msg.empty() ? B.msg : B.shopTab == 0 ? std::string("Os itens servem entre as batalhas dos Gin\xC3\xA1sios e da Elite dos 4, na prepara\xC3\xA7\xC3\xA3o.") : std::string("Toque numa pedra para comprar. Equipe na ficha do Pok\xC3\xA9mon (aba Batalha). S\xC3\xB3 vale na arena."), 11 * u, gw, PcPal::Hint, false);
}
// BETWEEN THE ELITE FOUR's battles: your team as it is (HP), the bag; an item, then a Pokémon, to
// use it; then the next one, or give up.
inline void DrawBattlePrep(Canvas& c, float u, int L, int R, int top, int bottom)
{
    BattleState& B = Bat();
    const Arena::Region& Rg = Arena::Regions()[B.region];
    PcCard(c, L, top, R - L, bottom - top, 10 * u, u);
    if (B.eliteStep >= (int)B.steps.size()) return;
    const BattleState::Opp& nx = B.steps[B.eliteStep].opp;
    const std::string what = B.runKind == 1 ? std::string("Gin\xC3\xA1sio de ") + Rg.gyms[B.gymSel].name : std::string("Elite dos 4 de ") + Rg.name;
    GT(c, L + (int)(16 * u), top + 22 * u, what + "  \xC2\xB7  " + std::to_string(B.eliteStep + 1) + "/" + std::to_string(B.steps.size()), 16 * u, PcPal::Ink, true);
    GT(c, L + (int)(16 * u), top + 44 * u, std::string("Pr\xC3\xB3ximo: ") + nx.name + (nx.kind == 1 ? " (l\xC3\xAD" "der)" : nx.kind == 2 && nx.index == 4 ? " (campe\xC3\xA3o)" : "") + "  \xC2\xB7  " + Money(B.money), 12 * u, PcPal::Hint, true);
    if (const ArtImg* a = TrainerArt(nx.sprite)) DrawArt(c, *a, R - (int)(130 * u), top + (int)(6 * u), (int)(110 * u), (int)(110 * u));
    // (The team: a card each with its HP; tap one to use the chosen item on it.)
    const int gx = L + (int)(14 * u), gy = top + (int)(64 * u), gw = R - L - (int)(150 * u);
    const int cw = (gw - 5 * (int)(8 * u)) / 6, ch = (int)(cw * 1.35f);
    for (int i = 0; i < (int)B.mine.size() && i < 6; i++)
    {
        const int x0 = gx + i * (cw + (int)(8 * u));
        const int mx = B.mine[i].stats[0], hp = i < (int)B.runHp.size() ? B.runHp[i] : mx;
        if (i == B.dragCard) Glass::Round(c, x0, gy, cw, ch - (int)(18 * u), 8 * u, PcPal::Slot, PcPal::Primary, 2.0f * u);
        else BattleCard(c, u, x0, gy, cw, ch - (int)(18 * u), B.mine[i], false);
        if (hp <= 0) { BGfx::Rect(c, x0, gy, cw, ch - (int)(18 * u), 0xA0EAF1E2u); GTFit(c, x0 + (int)(6 * u), gy + ch * 0.4f, "Desmaiado", 10 * u, cw - (int)(12 * u), 0xFFC04040u, true); }
        const float f = (float)std::max(0, hp) / std::max(1, mx);
        BGfx::Rect(c, x0, gy + ch - (int)(14 * u), cw, (int)(6 * u), 0xFFC8D4C0u);
        BGfx::Rect(c, x0, gy + ch - (int)(14 * u), (int)(cw * f), (int)(6 * u), f > 0.5f ? 0xFF48B058u : f > 0.2f ? 0xFFE0A820u : 0xFFE04838u);
        GT(c, x0, gy + ch - 1 * u, std::to_string(std::max(0, hp)) + "/" + std::to_string(mx), 9.5f * u, PcPal::Ink, true);
        B.hits.push_back({ x0, gy, cw, ch, BT_PREP_MON, 0, i });
    }
    // (The bag.)
    const auto& shop = Arena::Shop();
    const int by = gy + ch + (int)(18 * u);
    GT(c, gx, by + 8 * u, "Mochila (toque num item e depois no Pok\xC3\xA9mon)", 12 * u, PcPal::Ink, true);
    int ix = gx;
    const int iy = by + (int)(22 * u), ih = (int)(40 * u);
    for (int i = 0; i < (int)shop.size(); i++)
    {
        const std::string l = std::string(shop[i].name) + "  x" + std::to_string(B.bag[i]);
        const int w = GW(l, 11.5f * u, true) + (int)(52 * u);
        if (ix + w > R - (int)(14 * u)) break;
        const bool on = B.itemSel == i;
        Glass::Round(c, ix, iy, w, ih, 8 * u, on ? PcPal::Primary : B.bag[i] ? PcPal::Btn : PcPal::Slot, on ? PcPal::PrimaryDark : PcPal::Line, 1.2f * u);
        if (const ArtImg* a = ItemArt(shop[i].icon)) DrawArt(c, *a, ix + (int)(6 * u), iy + (int)(5 * u), (int)(30 * u), ih - (int)(10 * u));
        GT(c, ix + (int)(41 * u), iy + ih / 2.0f, l, 11.5f * u, on ? PcPal::OnPrimary : B.bag[i] ? PcPal::Ink : PcPal::Hint, true);
        B.hits.push_back({ ix, iy, w, ih, BT_ITEM, 0, i });
        ix += w + (int)(6 * u);
    }
    // (The foot: give up, or the next battle.)
    const int fh = (int)(46 * u), fy = bottom - fh - (int)(10 * u);
    int bx = R - (int)(14 * u);
    auto button = [&](const std::string& l, int kind, bool strong) {
        const int w = GW(l, 14 * u, true) + (int)(36 * u);
        bx -= w;
        PcButton(c, bx, fy, w, fh, 9 * u, strong, u);
        GT(c, bx + (int)(18 * u), fy + fh / 2.0f, l, 14 * u, strong ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ bx, fy, w, fh, kind, 0, 0 });
        bx -= (int)(10 * u);
    };
    button(std::string("Lutar contra ") + nx.name, BT_NEXT, true);
    button("Desistir", BT_GIVEUP, false);
    GTFit(c, gx, fy + fh / 2.0f, !B.msg.empty() ? B.msg : "Os desmaiados s\xC3\xB3 voltam com Reviver. Segure um card e arraste para mudar a ordem.", 11.5f * u, bx - gx - (int)(10 * u), PcPal::Hint, false);
    BattleDragDraw(c, u);
}
// Choosing a famous opponent for a free battle: a tab a region, its leaders, Elite Four and
// champion (Red under Johto); those not beaten yet faded.
inline void DrawOppPicker(Canvas& c, float u, int L, int R, int top, int bottom)
{
    BattleState& B = Bat();
    const auto& regs = Arena::Regions();
    PcCard(c, L, top, R - L, bottom - top, 10 * u, u);
    GT(c, L + (int)(16 * u), top + 22 * u, "Escolher advers\xC3\xA1rio", 17 * u, PcPal::Ink, true);
    {
        const std::string l = "Voltar";
        const int w = GW(l, 12 * u, true) + (int)(26 * u), h = (int)(32 * u), bx = R - w - (int)(12 * u);
        PcButton(c, bx, top + (int)(8 * u), w, h, 7 * u, false, u);
        GT(c, bx + (int)(13 * u), top + 8 * u + h / 2.0f, l, 12 * u, PcPal::Ink, true);
        B.hits.push_back({ bx, top + (int)(8 * u), w, h, BT_OPP_BACK, 0, 0 });
    }
    int cx = L + (int)(16 * u);
    const int ty = top + (int)(48 * u);
    for (int r = 0; r < (int)regs.size(); r++)
    {
        const std::string l = regs[r].name;
        const int w = GW(l, 12 * u, true) + (int)(22 * u), h = (int)(30 * u);
        PcButton(c, cx, ty, w, h, 7 * u, r == B.oppRegion, u);
        GT(c, cx + (int)(11 * u), ty + h / 2.0f, l, 12 * u, r == B.oppRegion ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ cx, ty, w, h, BT_OPP_REGION, 0, r });
        cx += w + (int)(6 * u);
    }
    const Arena::Region& Rg = regs[std::max(0, std::min(B.oppRegion, (int)regs.size() - 1))];
    const int n = B.oppRegion == 1 ? 14 : 13, cols = 7;
    const int gx = L + (int)(16 * u), gy = ty + (int)(42 * u), gw = R - L - (int)(32 * u), gh = bottom - gy - (int)(36 * u);
    const int tw = (gw - (cols - 1) * (int)(6 * u)) / cols, th = std::min((int)(tw * 1.3f), (gh - (int)(6 * u)) / 2);
    for (int i = 0; i < n; i++)
    {
        const int k = i == 13 ? 99 : i;
        const Arena::Trainer& t = k == 99 ? Arena::Red() : k < 8 ? Rg.gyms[k] : Rg.elite[k - 8];
        const bool open = k == 99 ? B.redBeaten : Beaten(B.oppRegion, k);
        const int x0 = gx + (i % cols) * (tw + (int)(6 * u)), y0 = gy + (i / cols) * (th + (int)(6 * u));
        BattleTrainerTile(c, u, x0, y0, tw, th, t, false, false, k == 99 ? "Monte Prateado" : k == 12 ? "Campe\xC3\xA3o" : "");
        if (!open) { BGfx::Rect(c, x0, y0, tw, th, 0xB8EAF1E2u); GTFit(c, x0 + (int)(6 * u), y0 + th * 0.45f, "N\xC3\xA3o derrotado", 9.5f * u, tw - (int)(12 * u), PcPal::Hint, true); }
        B.hits.push_back({ x0, y0, tw, th, BT_OPP_TAKE, 0, B.oppRegion * 100 + k });
    }
    GTFit(c, gx, bottom - 20 * u, !B.msg.empty() ? B.msg : "Quem voc\xC3\xAA derrota nos Gin\xC3\xA1sios e na Elite dos 4 fica liberado aqui, com o time dos jogos.", 11 * u, gw, PcPal::Hint, false);
}
inline void DrawBattlePage(Canvas& c, float u, int L, int R, int top, int bottom)
{
    BattleState& B = Bat();
    BattleLoad();
    B.u = u;
    B.hits.clear();
    GlassFocus& F = Focus();
    F.rVis = 0; F.pdY = c.h; F.prow = 0; F.pact = -1;
    Glass::Round(c, L, top, R - L, bottom - top, 12 * u, PcPal::Body, PcPal::Primary, 4 * u);
    { const int in = (int)(10 * u); L += in; R -= in; top += in; bottom -= in; }
    if (B.picking) { DrawBattlePicker(c, u, L, R, top, bottom); return; }
    if (B.shop) { DrawBattleShop(c, u, L, R, top, bottom); return; }
    if (B.oppPick) { DrawOppPicker(c, u, L, R, top, bottom); return; }
    if (B.eliteOn) { DrawBattlePrep(c, u, L, R, top, bottom); return; }
    const int gap = (int)(10 * u), footH = (int)(52 * u);
    const int pw = (R - L - gap) / 2, ph = bottom - top - footH - gap;
    auto panel = [&](int side, int x)
    {
        const bool me = side == 0;
        const std::vector<Arena::MonSpec>& team = me ? B.mine : B.bot;
        int& sel = me ? B.sel : B.botSel;
        PcCard(c, x, top, pw, ph, 10 * u, u);
        GT(c, x + (int)(12 * u), top + 18 * u, "Seu time", 15 * u, PcPal::Ink, true);
        const std::string bl = me ? "Escolher time" : "Sortear outro";
        const int bw = GW(bl, 11.5f * u, true) + (int)(22 * u), bh = (int)(28 * u), bxx = x + pw - bw - (int)(10 * u);
        PcButton(c, bxx, top + (int)(5 * u), bw, bh, 7 * u, me, u);
        GT(c, bxx + (int)(11 * u), top + 5 * u + bh / 2.0f, bl, 11.5f * u, me ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ bxx, top + (int)(5 * u), bw, bh, me ? BT_PICK : BT_REROLL, side, 0 });
        // (Six places in two rows; then the chosen one's sheet.)
        const int gx = x + (int)(10 * u), gy = top + (int)(42 * u), gw = pw - (int)(20 * u);
        const int cw = (gw - 5 * (int)(6 * u)) / 6, ch = std::min((int)(cw * 1.25f), (int)(ph * 0.34f));
        for (int i = 0; i < 6; i++)
        {
            const int x0 = gx + i * (cw + (int)(6 * u));
            if (i < (int)team.size())
            {
                if (me && i == B.dragCard) Glass::Round(c, x0, gy, cw, ch, 8 * u, PcPal::Slot, PcPal::Primary, 2.0f * u);
                else BattleCard(c, u, x0, gy, cw, ch, team[i], i == sel);
                B.hits.push_back({ x0, gy, cw, ch, me ? BT_CARD : BT_BOTCARD, side, i });
            }
            else Glass::Round(c, x0, gy, cw, ch, 8 * u, PcPal::Slot, PcPal::Line, 1.0f * u);
        }
        const int sy = gy + ch + (int)(8 * u), sh = top + ph - (int)(10 * u) - sy;
        if (sel >= 0 && sel < (int)team.size()) BattleSheet(c, u, gx, sy, gw, sh, team[sel], me ? sel : -1);
        else if (team.empty() && me)
            GTFit(c, gx, sy + 20 * u, "Toque em \"Escolher time\" e marque de 1 a 6 Pok\xC3\xA9mon dos seus saves ou do Banco.", 11.5f * u, gw, PcPal::Hint, false);
        else GTFit(c, gx, sy + 20 * u, "Toque num Pok\xC3\xA9mon para ver os golpes de cada bot\xC3\xA3o.", 11.5f * u, gw, PcPal::Hint, false);
    };
    panel(0, L);
    BattleOppPanel(c, u, L + pw + gap, top, pw, ph);
    BattleDragDraw(c, u);
    // The foot: the mode, the difficulty (a free battle's), your money and the shop, FIGHT.
    const int fy = bottom - footH;
    PcCard(c, L, fy, R - L, footH, 10 * u, u);
    int x = L + (int)(10 * u);
    const int bh = footH - (int)(16 * u);
    static const char* mn[3] = { "Livre", "Gin\xC3\xA1sios", "Elite dos 4" };
    for (int m = 0; m < 3; m++)
    {
        const int bw = GW(mn[m], 12 * u, true) + (int)(20 * u);
        PcButton(c, x, fy + (int)(8 * u), bw, bh, 7 * u, B.mode == m, u);
        GT(c, x + (int)(10 * u), fy + footH / 2.0f, mn[m], 12 * u, B.mode == m ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ x, fy + (int)(8 * u), bw, bh, BT_MODE, 0, m });
        x += bw + (int)(5 * u);
    }
    x += (int)(10 * u);
    if (B.mode == 0)
    {
        static const char* dn[3] = { "F\xC3\xA1" "cil", "Normal", "Dif\xC3\xAD" "cil" };
        for (int d = 0; d < 3; d++)
        {
            const int bw = GW(dn[d], 11 * u, true) + (int)(16 * u);
            PcButton(c, x, fy + (int)(10 * u), bw, bh - (int)(4 * u), 7 * u, B.diff == d, u);
            GT(c, x + (int)(8 * u), fy + footH / 2.0f, dn[d], 11 * u, B.diff == d ? PcPal::OnPrimary : PcPal::Ink, true);
            B.hits.push_back({ x, fy + (int)(8 * u), bw, bh, BT_DIFF, 0, d });
            x += bw + (int)(4 * u);
        }
        x += (int)(10 * u);
    }
    const std::string go = B.mode == 2 || (B.mode == 1 && B.gymSel < 8) ? "Come\xC3\xA7" "ar desafio" : "Lutar!";
    const bool canGo = !B.mine.empty() && LockReason(B.mode, B.region).empty();
    const int gw2 = GW(go, 15 * u, true) + (int)(40 * u), gh = footH - (int)(12 * u), gx2 = R - gw2 - (int)(8 * u);
    PcButton(c, gx2, fy + (int)(6 * u), gw2, gh, 9 * u, canGo, u);
    GT(c, gx2 + (int)(20 * u), fy + footH / 2.0f, go, 15 * u, canGo ? PcPal::OnPrimary : PcPal::Hint, true);
    B.hits.push_back({ gx2, fy + (int)(6 * u), gw2, gh, BT_FIGHT, 0, 0 });
    const std::string sl = "Loja";
    const int sw = GW(sl, 12 * u, true) + (int)(24 * u), sx = gx2 - sw - (int)(8 * u);
    PcButton(c, sx, fy + (int)(8 * u), sw, bh, 7 * u, false, u);
    GT(c, sx + (int)(12 * u), fy + footH / 2.0f, sl, 12 * u, PcPal::Ink, true);
    B.hits.push_back({ sx, fy + (int)(8 * u), sw, bh, BT_SHOP, 0, 0 });
    const std::string mo = Money(B.money);
    const int mw = GW(mo, 13 * u, true);
    GT(c, sx - mw - (int)(12 * u), fy + footH / 2.0f, mo, 13 * u, PcPal::Primary, true);
    const std::string note = !B.msg.empty() ? B.msg : "Nada muda nos seus saves  \xC2\xB7  Sprites: PMD SpriteCollab (CC BY-NC 4.0) e Pok\xC3\xA9mon Showdown";
    GTFit(c, x, fy + footH / 2.0f, note, 10.5f * u, sx - mw - (int)(24 * u) - x, PcPal::Hint, false);
}
inline void BattlePagePointer(int phase, int x, int y)
{
    BattleState& B = Bat();
    if (phase == 0)
    {
        B.down = true; B.dragged = false; B.downX = x; B.downY = y; B.scroll0 = B.pickScroll;
        B.downAt = std::chrono::steady_clock::now(); B.downCard = -1; B.dragCard = -1;
        if (!B.picking)
            for (const PcHit& h : B.hits) if ((h.kind == BT_CARD || h.kind == BT_PREP_MON) && x >= h.x && x < h.x + h.w && y >= h.y && y < h.y + h.h) B.downCard = h.value;
        Hub().version++;
        return;
    }
    if (phase == 1)
    {
        if (!B.down) return;
        if (B.downCard >= 0 && B.dragCard < 0 && std::chrono::duration<float>(std::chrono::steady_clock::now() - B.downAt).count() >= 0.35f) B.dragCard = B.downCard;
        if (B.dragCard >= 0) { B.dragX = x; B.dragY = y; B.dragged = true; Hub().version++; return; }
        if (!B.dragged && std::abs(y - B.downY) > 10 * B.u) B.dragged = true;
        if (B.dragged && B.picking) { B.pickScroll = std::max(0.0f, std::min(B.pickMax, B.scroll0 - (y - B.downY))); Hub().version++; }
        return;
    }
    const bool tap = B.down && !B.dragged && B.dragCard < 0;
    B.down = false;
    if (B.dragCard >= 0)
    {
        // (Dropped: before the card under the finger, or the nearest one along.)
        const int from = B.dragCard;
        B.dragCard = -1; B.downCard = -1;
        int to = -1, best = INT_MAX;
        for (const PcHit& h : B.hits)
            if (h.kind == BT_CARD || h.kind == BT_PREP_MON)
            {
                const int d = std::abs(x - (h.x + h.w / 2)) + (y >= h.y - h.h && y < h.y + 2 * h.h ? 0 : 100000);
                if (d < best) { best = d; to = h.value; }
            }
        if (to >= 0 && to != from && from < (int)B.mine.size() && from < (int)B.keys.size())
        {
            const Arena::MonSpec m = B.mine[from];
            const BattleKey k = B.keys[from];
            B.mine.erase(B.mine.begin() + from); B.keys.erase(B.keys.begin() + from);
            B.mine.insert(B.mine.begin() + to, m); B.keys.insert(B.keys.begin() + to, k);
            if (from < (int)B.runHp.size() && to < (int)B.runHp.size())
            {
                // (In a run: its HP goes with it.)
                const int hp = B.runHp[from];
                B.runHp.erase(B.runHp.begin() + from);
                B.runHp.insert(B.runHp.begin() + to, hp);
            }
            B.sel = to;
            BattleSaveTeam();
        }
        Hub().version++;
        return;
    }
    B.downCard = -1;
    if (!tap) return;
    for (size_t i = B.hits.size(); i-- > 0;)
    {
        const PcHit& h = B.hits[i];
        if (x < h.x || x >= h.x + h.w || y < h.y || y >= h.y + h.h) continue;
        B.msg.clear();
        switch (h.kind)
        {
        case BT_PICK: BattleOpenPicker(); break;
        case BT_REROLL: BattleReroll(); break;
        case BT_DIFF: B.diff = h.value; break;
        case BT_FIGHT:
            if (B.mine.empty()) { B.msg = "Escolha seu time primeiro."; break; }
            if (!LockReason(B.mode, B.region).empty()) { B.msg = LockReason(B.mode, B.region); break; }
            if (B.mode == 1 && B.gymSel < 8) { BattleStartRun(1); break; }
            if (B.mode == 2) { BattleStartRun(2); break; }
            BattleBegin();
            break;
        case BT_MODE: B.mode = h.value; BattleSetOpp(false); break;
        case BT_REGION: B.region = h.value; BattleSetOpp(false); break;
        case BT_GYM_RESET: B.badges[B.region] = 0; ArenaSave(); BattleSetOpp(false); break;
        case BT_SHOP: B.shop = true; break;
        case BT_SHOP_BACK: B.shop = false; break;
        case BT_BUY:
        {
            const auto& it = Arena::Shop()[h.value];
            if (B.money < it.price) { B.msg = "Dinheiro insuficiente."; break; }
            B.money -= it.price; B.bag[h.value]++; ArenaSave();
            B.msg = std::string("Comprou ") + it.name + ".";
            break;
        }
        case BT_ITEM: B.itemSel = B.itemSel == h.value ? -1 : (B.bag[h.value] ? h.value : -1); if (B.itemSel < 0 && !B.bag[h.value]) B.msg = "Voc\xC3\xAA n\xC3\xA3o tem esse item."; break;
        case BT_PREP_MON:
        {
            if (B.itemSel < 0) { B.msg = "Escolha um item primeiro."; break; }
            const auto& it = Arena::Shop()[B.itemSel];
            if (h.value >= (int)B.runHp.size() || h.value >= (int)B.mine.size()) break;
            int& hp = B.runHp[h.value];
            const int mx = B.mine[h.value].stats[0];
            if (it.revive) { if (hp > 0) { B.msg = "Reviver s\xC3\xB3 serve para um desmaiado."; break; } hp = it.revive == 2 ? mx : mx / 2; }
            else { if (hp <= 0) { B.msg = "Desmaiado: use um Reviver."; break; } if (hp >= mx) { B.msg = "Os PS j\xC3\xA1 est\xC3\xA3o cheios."; break; } hp = it.heal < 0 ? mx : std::min(mx, hp + it.heal); }
            B.bag[B.itemSel]--;
            B.msg = std::string(it.name) + " em " + B.mine[h.value].name + ".";
            if (!B.bag[B.itemSel]) B.itemSel = -1;
            ArenaSave();
            break;
        }
        case BT_NEXT: BattleSetOpp(); BattleBegin(); break;
        case BT_GYM_SEL: B.gymSel = h.value; BattleSetOpp(false); break;
        case BT_OPP_OPEN: B.oppPick = true; break;
        case BT_OPP_BACK: B.oppPick = false; break;
        case BT_OPP_REGION: B.oppRegion = h.value; break;
        case BT_OPP_TAKE:
        {
            // (A famous one beaten: chosen for the free battle, with their team.)
            const int r = h.value / 100, k = h.value % 100;
            if (!(k == 99 ? B.redBeaten : Beaten(r, k))) { B.msg = "Derrote esse primeiro no modo Gin\xC3\xA1sios ou na Elite dos 4."; break; }
            const Arena::Trainer& t = k == 99 ? Arena::Red() : k < 8 ? Arena::Regions()[r].gyms[k] : Arena::Regions()[r].elite[k - 8];
            std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
            B.opp = { t.name, t.sprite, 0, 0, 50 };
            B.bot = TrainerTeam(k == 99 ? 1 : r, t, rng);
            B.botSel = -1; B.oppPick = false;
            PmdWantTeams();
            break;
        }
        case BT_SHOP_TAB: B.shopTab = h.value; break;
        case BT_BUY_STONE:
        {
            if (B.money < Arena::MEGA_PRICE) { B.msg = "Dinheiro insuficiente."; break; }
            B.money -= Arena::MEGA_PRICE; B.stones[h.value]++; ArenaSave();
            B.msg = std::string("Comprou ") + Arena::Megas()[h.value].stone + ".";
            break;
        }
        case BT_EQUIP:
        {
            if (h.value < 0 || h.value >= (int)B.keys.size() || h.value >= (int)B.mine.size()) break;
            const std::string key = EquipKey(B.keys[h.value]);
            const int mi = Arena::MegaFor(B.mine[h.value].species);
            auto it = B.equip.find(key);
            if (it != B.equip.end()) { B.stones[it->second]++; B.equip.erase(it); }
            else if (mi < 0) break;
            else if (!B.stones[mi]) { B.msg = std::string("Voc\xC3\xAA n\xC3\xA3o tem ") + Arena::Megas()[mi].stone + ": compre na Loja."; break; }
            else { B.stones[mi]--; B.equip[key] = mi; }
            ArenaSave();
            const int sel = B.sel;
            B.mine = BattleRead(B.keys);
            B.sel = sel;
            PmdWantTeams();
            break;
        }
        case BT_GIVEUP: B.eliteOn = false; B.itemSel = -1; B.msg = "Desafio abandonado."; BattleSetOpp(false); break;
        case BT_CARD: B.sel = h.value; break;
        case BT_BOTCARD: B.botSel = h.value; break;
        case BT_PG_TAB: B.pgTab = h.value; B.pickScroll = 0; break;
        case BT_PG_MON:
        {
            if (B.pgTab < 0 || B.pgTab >= (int)B.pg.size() || h.value < 0 || h.value >= (int)B.pg[B.pgTab].mons.size()) break;
            if (Arena::NoPicture(B.pg[B.pgTab].mons[h.value].species)) { B.msg = "Esse ainda n\xC3\xA3o tem sprite animado: n\xC3\xA3o luta por enquanto."; break; }
            const BattleKey k = B.pg[B.pgTab].mons[h.value].k;
            auto it = std::find(B.pickSel.begin(), B.pickSel.end(), k);
            if (it != B.pickSel.end()) B.pickSel.erase(it);
            else if (B.pickSel.size() >= 6) B.msg = "J\xC3\xA1 s\xC3\xA3o 6: toque num escolhido para tirar.";
            else B.pickSel.push_back(k);
            break;
        }
        case BT_PG_SLOT: if (h.value >= 0 && h.value < (int)B.pickSel.size()) B.pickSel.erase(B.pickSel.begin() + h.value); break;
        case BT_PG_CLEAR: B.pickSel.clear(); break;
        case BT_PG_CANCEL: BattlePickerDone(false); break;
        case BT_PG_DONE: BattlePickerDone(true); break;
        }
        Hub().version++;
        return;
    }
}
#else
inline bool BattlePicking() { return false; }
inline void BattlePickerDone(bool) {}
inline bool BattleOn() { return false; }
inline void DrawBattle(Canvas&, float) {}
inline void BattlePicked(bool) {}
#endif
