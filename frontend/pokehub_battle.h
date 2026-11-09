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
#ifdef SSR_POKEMON_HUB
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
    long long held[Arena::SLOTS] = { -1, -1, -1, -1, -1 };
    bool pressed[Arena::SLOTS] = {};
    bool keys4[4] = {}; // (W A S D, on a PC)
    // (Where the buttons were drawn: x, y, r; and the pause, rematch and menu buttons.)
    float bx[Arena::SLOTS] = {}, by[Arena::SLOTS] = {}, br[Arena::SLOTS] = {};
    std::vector<PcHit> overHits;
    float u = 1;
    std::vector<uint32_t> bg; int bgW = 0, bgH = 0;
    float fcx = 0, fcy = 0, fk = 1; // (the field on the screen: its middle, arena units to pixels)
};
inline BattleState& Bat() { static BattleState s; return s; }
enum { BT_PICK = 1, BT_REROLL, BT_DIFF, BT_FIGHT, BT_CARD, BT_BOTCARD, BT_PAUSE, BT_RESUME, BT_QUIT, BT_REMATCH, BT_MENU };
inline std::string BattleTeamFile() { return HubDir() + "/pokehub-battle-team.txt"; }

// ---- The Pokémon: read from their saves -------------------------------------------------------
inline bool BattleSpec(PokemonHub::Save& s, int box, int slot, Arena::MonSpec& out)
{
    PokemonHub::MonDetail d;
    if (!PokemonHub::Detail(s, box, slot, d) || d.mon.egg || !d.mon.species) return false;
    out = Arena::MonSpec();
    out.species = d.mon.species; out.shiny = d.mon.shiny;
    out.name = PcMonName(d.mon);
    out.types[0] = d.typeIds[0]; out.types[1] = d.typeIds[1];
    for (int i = 0; i < 6; i++) out.stats[i] = std::max(1, d.stats50[i]);
    for (int i = 0; i < 4; i++) out.moves[i] = d.moveIds[i];
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
        if (it->second && BattleSpec(*it->second, k.box, k.slot, m)) { out.push_back(m); kept.push_back(k); }
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
// The bot's six: any species of the Pokédex, each with moves chosen for it (Arena::BotMon).
inline void BattleReroll()
{
    BattleState& B = Bat();
    std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    B.bot.clear();
    B.botKeys.clear();
    for (int tries = 0; (int)B.bot.size() < 6 && tries < 200; tries++)
    {
        const int sp = 1 + (int)(rng() % PokemonHub::DEX_SPECIES);
        if (sp == 292) continue; // (Shedinja: 1 HP, gone at the first touch)
        bool dup = false;
        for (const Arena::MonSpec& m : B.bot) dup |= m.species == sp;
        int base[6], types[2];
        if (dup || !PokemonHub::SpeciesBase(sp, base, types)) continue;
        B.bot.push_back(Arena::BotMon(sp, base, types, MonName(PokemonHub::SpeciesNameOf(sp)), rng));
    }
    B.botSel = -1;
    if (B.bot.empty()) B.bot = B.mine;
}
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
    B.mine = BattleRead(B.keys);
    B.sel = B.mine.empty() ? -1 : 0;
    BattleReroll();
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
// Choosing the team: My PC's choosing, the team marked already.
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
    if (B.bot.empty()) BattleReroll();
    B.b.Start(B.mine, B.bot, B.diff, (unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
    B.on = true; B.paused = false; B.acc = 0;
    B.last = std::chrono::steady_clock::now();
    B.stickId = -1; B.stickDX = B.stickDY = 0;
    for (auto& h : B.held) h = -1;
    for (bool& p : B.pressed) p = false;
    for (bool& k : B.keys4) k = false;
    Hub().version++;
}
inline void BattleEnd() { Bat().on = false; Bat().paused = false; Hub().version++; }

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

// ---- The stadium: drawn once a size, then copied each frame ------------------------------------
inline void BattleLayout(const Canvas& c, float u)
{
    BattleState& B = Bat();
    const float top = 74 * u, bottom = c.h - 6 * u;
    B.fk = std::min((c.w * 0.94f) / (2 * Arena::RX), (bottom - top) / (2 * Arena::RY + 40));
    B.fcx = c.w * 0.5f;
    B.fcy = (top + bottom) * 0.5f + 8 * u;
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
    if (k == Arena::S_DASH) { wait = F.dashCd; frac = F.dashCd / 3.5f; }
    else if (k == Arena::S_ULT) { frac = 1 - F.ult / 100; }
    else if (k < 4) { wait = F.sk[k].cd; frac = F.sk[k].cdMax > 0 ? F.sk[k].cd / F.sk[k].cdMax : 0; }
    const bool ready = !none && (k == Arena::S_ULT ? F.ult >= 100 : wait <= 0);
    const uint32_t ring = k == Arena::S_DASH ? 0xFFB0B8C8u : none ? 0xFF606060u : Arena::TypeColour(mi.type);
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
    const float fs = k == Arena::S_ATQ ? 15 * u : k == Arena::S_DASH ? 8.5f * u : 11 * u;
    if (!ready && k != Arena::S_ULT && wait > 0) BGfx::Text(c, x, y, std::to_string((int)std::ceil(wait)), 15 * u, 0xFFFFFFFFu);
    else if (k == Arena::S_ULT && !ready) BGfx::Text(c, x, y, std::to_string((int)F.ult) + "%", 10 * u, 0xFFFFFFFFu);
    else BGfx::Text(c, x, y, label, fs, none ? 0xFF909090u : 0xFFFFFFFFu);
    // (The move's name under it.)
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
        BGfx::Mon(c, F.spec.species, F.spec.shiny, x, y + 4 * k - hop, monH, F.face > 0, F.flash > 0 ? 0.85f : 0, alpha, squash);
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
        }
        else
        {
            const float t = (bm.t - bm.windup) / bm.dur, w = bm.width * k * (1 - t * 0.6f);
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 1.3f, BGfx::Alpha(tc, 0.5f));
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 0.8f, tc);
            BGfx::Seg(c, x0, y0 - 30 * k, x1, y1 - 30 * k, w * 0.35f, 0xFFFFFFF0u);
        }
    }
    for (const Arena::Particle& p : b.parts)
        BGfx::Rect(c, (int)(SX(p.x) - p.size * k / 2), (int)(SY(p.y) - 40 * k - p.size * k / 2), std::max(1, (int)(p.size * k)), std::max(1, (int)(p.size * k)), BGfx::Alpha(p.col, 1 - p.t / p.life));
    for (const Arena::Proj& p : b.projs)
    {
        const float x = SX(p.x), y = SY(p.y) - 40 * k;
        BGfx::Disc(c, x, y, p.r * k * 1.6f, BGfx::Alpha(Arena::TypeColour(p.type), 0.35f));
        BGfx::Disc(c, x, y, p.r * k, Arena::TypeColour(p.type));
        BGfx::Disc(c, x, y, p.r * k * 0.45f, 0xFFFFFFF0u);
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
            BGfx::Text(c, c.w / 2.0f, y0 + 78 * u, "Voc\xC3\xAA " + std::to_string(b.kos[0]) + " x " + std::to_string(b.kos[1]) + " Bot", 15 * u, 0xFFE0E0E0u);
            const float cx0 = c.w / 2.0f - 110 * u, cx1 = c.w / 2.0f + 110 * u, ry = y0 + 118 * u;
            BGfx::Text(c, cx0, ry, "Voc\xC3\xAA", 13 * u, 0xFF8CB8FFu);
            BGfx::Text(c, cx1, ry, "Bot", 13 * u, 0xFFFF9090u);
            BGfx::Text(c, c.w / 2.0f, ry + 26 * u, "Dano", 11 * u, 0xFFA0A8B8u, false);
            BGfx::Text(c, cx0, ry + 26 * u, std::to_string(b.dealt[0]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, cx1, ry + 26 * u, std::to_string(b.dealt[1]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, c.w / 2.0f, ry + 50 * u, "Acertos", 11 * u, 0xFFA0A8B8u, false);
            BGfx::Text(c, cx0, ry + 50 * u, std::to_string(b.hits[0]), 14 * u, 0xFFFFFFFFu);
            BGfx::Text(c, cx1, ry + 50 * u, std::to_string(b.hits[1]), 14 * u, 0xFFFFFFFFu);
            button("Revanche", BT_REMATCH, c.w / 2.0f - 90 * u, true);
            button("Menu", BT_MENU, c.w / 2.0f + 90 * u, false);
        }
    }
}

// ---- The fight's input: fingers (id), the mouse (-2), keys ---------------------------------------
inline void BattleOver(int kind)
{
    BattleState& B = Bat();
    if (kind == BT_PAUSE) B.paused = true;
    else if (kind == BT_RESUME) { B.paused = false; B.last = std::chrono::steady_clock::now(); }
    else if (kind == BT_QUIT || kind == BT_MENU) BattleEnd();
    else if (kind == BT_REMATCH) BattleBegin();
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
inline void BattleSheet(Canvas& c, float u, int x, int y, int w, int h, const Arena::MonSpec& m)
{
    PcCard(c, x, y, w, h, 8 * u, u);
    const int pad = (int)(10 * u);
    int tx = x + pad, ty = y + pad;
    GTFit(c, tx, ty + 8 * u, m.name + "  Lv.50", 13 * u, w / 2, PcPal::Ink, true);
    int cx = tx + std::min(GW(m.name + "  Lv.50", 13 * u, true), w / 2) + (int)(8 * u);
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
    const int gap = (int)(10 * u), footH = (int)(52 * u);
    const int pw = (R - L - gap) / 2, ph = bottom - top - footH - gap;
    auto panel = [&](int side, int x)
    {
        const bool me = side == 0;
        const std::vector<Arena::MonSpec>& team = me ? B.mine : B.bot;
        int& sel = me ? B.sel : B.botSel;
        PcCard(c, x, top, pw, ph, 10 * u, u);
        GT(c, x + (int)(12 * u), top + 18 * u, me ? "Seu time" : "Advers\xC3\xA1rio (bot)", 15 * u, PcPal::Ink, true);
        const std::string bl = me ? "Escolher no My PC" : "Sortear outro";
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
                BattleCard(c, u, x0, gy, cw, ch, team[i], i == sel);
                B.hits.push_back({ x0, gy, cw, ch, me ? BT_CARD : BT_BOTCARD, side, i });
            }
            else Glass::Round(c, x0, gy, cw, ch, 8 * u, PcPal::Slot, PcPal::Line, 1.0f * u);
        }
        const int sy = gy + ch + (int)(8 * u), sh = top + ph - (int)(10 * u) - sy;
        if (sel >= 0 && sel < (int)team.size()) BattleSheet(c, u, gx, sy, gw, sh, team[sel]);
        else if (team.empty() && me)
            GTFit(c, gx, sy + 20 * u, "Toque em \"Escolher no My PC\" e marque at\xC3\xA9 6 Pok\xC3\xA9mon (das caixas, da equipe ou do Banco).", 11.5f * u, gw, PcPal::Hint, false);
        else GTFit(c, gx, sy + 20 * u, "Toque num Pok\xC3\xA9mon para ver os golpes de cada bot\xC3\xA3o.", 11.5f * u, gw, PcPal::Hint, false);
    };
    panel(0, L);
    panel(1, L + pw + gap);
    // The foot: the difficulty, the words, FIGHT.
    const int fy = bottom - footH;
    PcCard(c, L, fy, R - L, footH, 10 * u, u);
    int x = L + (int)(12 * u);
    GT(c, x, fy + footH / 2.0f, "Bot:", 12 * u, PcPal::Ink, true);
    x += GW("Bot:", 12 * u, true) + (int)(8 * u);
    static const char* dn[3] = { "F\xC3\xA1" "cil", "Normal", "Dif\xC3\xAD" "cil" };
    for (int d = 0; d < 3; d++)
    {
        const int bw = GW(dn[d], 12 * u, true) + (int)(22 * u), bh = footH - (int)(16 * u);
        PcButton(c, x, fy + (int)(8 * u), bw, bh, 7 * u, B.diff == d, u);
        GT(c, x + (int)(11 * u), fy + footH / 2.0f, dn[d], 12 * u, B.diff == d ? PcPal::OnPrimary : PcPal::Ink, true);
        B.hits.push_back({ x, fy + (int)(8 * u), bw, bh, BT_DIFF, 0, d });
        x += bw + (int)(6 * u);
    }
    const std::string go = "Lutar!";
    const int gw2 = GW(go, 16 * u, true) + (int)(48 * u), gh = footH - (int)(12 * u), gx2 = R - gw2 - (int)(8 * u);
    PcButton(c, gx2, fy + (int)(6 * u), gw2, gh, 9 * u, !B.mine.empty(), u);
    GT(c, gx2 + (int)(24 * u), fy + footH / 2.0f, go, 16 * u, B.mine.empty() ? PcPal::Hint : PcPal::OnPrimary, true);
    B.hits.push_back({ gx2, fy + (int)(6 * u), gw2, gh, BT_FIGHT, 0, 0 });
    const std::string note = !B.msg.empty() ? B.msg : "Todos lutam no n\xC3\xADvel 50. Nada muda nos seus saves.";
    GTFit(c, x + (int)(10 * u), fy + footH / 2.0f, note, 11 * u, gx2 - x - (int)(20 * u), PcPal::Hint, false);
}
inline void BattlePagePointer(int phase, int x, int y)
{
    if (phase != 2) return;
    BattleState& B = Bat();
    for (size_t i = B.hits.size(); i-- > 0;)
    {
        const PcHit& h = B.hits[i];
        if (x < h.x || x >= h.x + h.w || y < h.y || y >= h.y + h.h) continue;
        B.msg.clear();
        switch (h.kind)
        {
        case BT_PICK: BattleStartPick(); break;
        case BT_REROLL: BattleReroll(); break;
        case BT_DIFF: B.diff = h.value; break;
        case BT_FIGHT: BattleBegin(); break;
        case BT_CARD: B.sel = h.value; break;
        case BT_BOTCARD: B.botSel = h.value; break;
        }
        Hub().version++;
        return;
    }
}
#else
inline bool BattleOn() { return false; }
inline void DrawBattle(Canvas&, float) {}
inline void BattlePicked(bool) {}
#endif
