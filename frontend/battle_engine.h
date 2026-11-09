#pragma once
// THE ARENA (PokeHub's battles, in the manner of pokearena2d.com): two Pokémon at a time in a
// stadium, in real time. Each moves freely (a stick) and has five buttons: ATQ, HAB, Q and ULT,
// made from its own four moves (the weakest damaging one ATQ, the strongest ULT, the rest HAB and
// Q), and DISPARADA, a dash. A special move flies (a ULT one: a beam across the field), a physical
// one is a lunge (a ground one: a ring round it), a status move works at once. ULT charges by
// fighting. Up to six a side: when one faints the next comes in; the other keeps what it has.
// Every Pokémon fights at level 50 (its stats from its IVs, EVs and nature). No drawing here: the
// page (pokehub_battle.h) draws what this holds, so it can run (and be tested) on its own.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include "battle_moves.h"

namespace Arena
{
// (The field: an ellipse round 0,0, in arena units; the screen fits it in.)
constexpr float RX = 900, RY = 430;
enum { T_NORMAL, T_FIGHTING, T_FLYING, T_POISON, T_GROUND, T_ROCK, T_BUG, T_GHOST, T_STEEL, T_FIRE, T_WATER, T_GRASS, T_ELECTRIC, T_PSYCHIC, T_ICE, T_DRAGON, T_DARK, T_FAIRY };
// (Attacking type by defending type: 0 none, h half, 1, 2 double. PKSM's order.)
inline float TypeEff(int a, int d)
{
    static const char* chart[18] = {
        "11111h10h111111111", "21hh12h021111h212h", "12111h21h112h11111", "111hhh1h0112111112",
        "110212h1221h211111", "1h21h121h211112111", "1hhh111hhh1212112h", "0111111211111211h1",
        "11111211hhh1h12112", "11111h212hh2112h11", "1111221112hh111h11", "11hh22h1hh2h111h11",
        "11210111112hh11h11", "12121111h1111h1101", "11212111hhh211h211", "11111111h111111210",
        "1h11111211111211hh", "121h1111hh11111221" };
    if (a < 0 || a > 17 || d < 0 || d > 17) return 1;
    const char ch = chart[a][d];
    return ch == '0' ? 0.0f : ch == 'h' ? 0.5f : ch == '2' ? 2.0f : 1.0f;
}
// Species with no moving picture in SpriteCollab (tools/battle/fetch_pmd.py found none): they do
// not fight, as the player asked; the bot never draws them, the choosing shows them faded.
inline bool NoPicture(int species)
{
    static const int none[] = { 514, 516, 520, 522, 523, 538, 558, 564, 565, 591, 592, 616, 618, 626, 668, 683, 732, 733, 735, 756, 765 };
    if (species <= 0 || species > 809) return true;
    for (int n : none) if (n == species) return true;
    return false;
}
inline const char* TypeNamePt(int t)
{
    static const char* n[18] = { "Normal", "Lutador", "Voador", "Venenoso", "Terrestre", "Pedra", "Inseto", "Fantasma", "A\xC3\xA7o",
        "Fogo", "\xC3\x81gua", "Planta", "El\xC3\xA9trico", "Ps\xC3\xADquico", "Gelo", "Drag\xC3\xA3o", "Sombrio", "Fada" };
    return t >= 0 && t < 18 ? n[t] : "";
}
inline uint32_t TypeColour(int t)
{
    static const uint32_t c[18] = { 0xFFA8A878u, 0xFFC03028u, 0xFFA890F0u, 0xFFA040A0u, 0xFFE0C068u, 0xFFB8A038u, 0xFFA8B820u, 0xFF705898u, 0xFFB8B8D0u,
        0xFFF08030u, 0xFF6890F0u, 0xFF78C850u, 0xFFF8D030u, 0xFFF85888u, 0xFF98D8D8u, 0xFF7038F8u, 0xFF705848u, 0xFFEE99ACu };
    return t >= 0 && t < 18 ? c[t] : 0xFFFFFFFFu;
}

// One Pokémon as it comes from a save (MonDetail's): stats HP, Atk, Def, Spe, SpA, SpD at level 50.
struct MonSpec
{
    int species = 0;
    bool shiny = false;
    std::string name;
    int types[2] = { 0, -1 };
    int stats[6] = { 100, 50, 50, 50, 50, 50 };
    int moves[4] = {};
    int level = 50;
    // (Its Mega, when it holds the stone: -1 none; what it becomes; its moving picture's key.)
    int mega = -1, megaStats[6] = {}, megaTypes[2] = { 0, -1 }, megaKey = 0;
    std::string megaName;
    int hp = -1;                       // the HP it starts with (-1 all: the Elite Four's keep theirs)
};
enum { S_ATQ, S_HAB, S_Q, S_ULT, S_DASH, S_EVO, SLOTS };
inline const char* SlotLabel(int s) { static const char* n[SLOTS] = { "Atq", "Hab", "Q", "ULT", "Disparada", "Mega" }; return s >= 0 && s < SLOTS ? n[s] : ""; }
// (Stat stages: as the games' -6 to +6.)
enum { ST_ATK, ST_DEF, ST_SPA, ST_SPD, ST_SPE, ST_ACC, ST_EVA, STAGES };
enum { NONE, BRN, PSN, TOX, PAR, SLP, FRZ };
inline float StageMul(int n) { return n >= 0 ? (2.0f + n) / 2.0f : 2.0f / (2.0f - n); }

struct Skill { int move = 0; float cd = 0, cdMax = 1; };
struct Fighter
{
    MonSpec spec;
    int hp = 1, maxHp = 1;
    float x = 0, y = 0, face = 1, walk = 0;  // walk: a clock for its steps, while it moves
    bool moving = false;
    Skill sk[4];
    float dashCd = 0, dashT = 0, dashX = 0, dashY = 0;
    float ult = 0, ultLock = 0;           // ultLock: after a ULT, seconds it does not charge
    int stage[STAGES] = {};
    int status = NONE;
    float statusT = 0, tick = 0, toxN = 0;
    float confT = 0, seedT = 0, protectT = 0, regenT = 0, focusT = 0;
    float busy = 0;                          // using a move: cannot start another
    float lungeT = 0, lungeX = 0, lungeY = 0, lungeScale = 0; int lungeMove = 0; bool lungeHit = false, lungeUlt = false;
    float flash = 0, castT = 0;              // hit (white), casting (a glow)
    float dirX = 1, dirY = 0;                // where it looks (its picture's eight ways)
    float atkT = 0, atkDur = 0.45f, hurtT = 0; // attacking (time left of it), hurt (time left)
    int castType = 0;
    bool fainted = false, mega = false;
    float boomT = 0;                         // Self-Destruct, Explosion: when it faints
};
struct Proj { int owner = 0, move = 0, type = 0; float x = 0, y = 0, vx = 0, vy = 0, life = 0, r = 16, scale = 1; bool ult = false, wave = false; };
struct Beam { int owner = 0, move = 0, type = 0; float x0 = 0, y0 = 0, dx = 1, dy = 0, len = 1500, width = 90, t = 0, windup = 0.5f, dur = 0.3f, scale = 1; bool hit = false, ult = true; };
struct Ring { int owner = 0, move = 0, type = 0; float x = 0, y = 0, rmax = 240, t = 0, dur = 0.35f, scale = 1; bool hit = false; };
struct FloatText { std::string s; float x = 0, y = 0, t = 0, life = 1.1f; uint32_t col = 0xFFFFFFFFu; float size = 1; };
// (A hit's flash where it landed, drawn in its type's way.)
struct Impact { float x = 0, y = 0, t = 0; int type = 0; bool big = false, phys = false; };
struct Particle { float x = 0, y = 0, vx = 0, vy = 0, t = 0, life = 0.5f, size = 6; uint32_t col = 0xFFFFFFFFu; };
struct Banner { std::string text; int type = 0; float t = 0, life = 0; };
// What a side does this step: where it goes (a length up to 1), and the buttons pressed now.
struct Input { float mx = 0, my = 0; bool press[SLOTS] = {}; };

// (Moves the arena knows by number, beyond their kind and type.)
namespace M
{
inline bool In(int id, std::initializer_list<int> l) { for (int v : l) if (v == id) return true; return false; }
inline bool Ground(int id) { return In(id, { 89, 222, 523, 414, 90 }); } // Earthquake, Magnitude, Bulldoze, Earth Power(no: special flies), Fissure
inline bool Drain(int id) { return In(id, { 71, 72, 202, 141, 409, 532, 577, 138, 570, 613 }); }
inline bool Recoil(int id) { return In(id, { 36, 38, 66, 344, 394, 413, 452, 457, 528, 543, 617 }); }
inline bool Priority(int id) { return In(id, { 98, 245, 183, 453, 418, 420, 425, 389, 410, 709, 252, 183 }); }
inline bool Boom(int id) { return In(id, { 120, 153 }); }             // Self-Destruct, Explosion: it faints after
inline bool Recharge(int id) { return In(id, { 63, 416, 307, 308, 338, 439, 459 }); } // Hyper Beam, Giga Impact, Blast Burn...: a rest after
inline bool Heal(int id) { return In(id, { 105, 135, 208, 303, 355, 235, 236, 234, 456, 273, 461, 666 }); }
}

// A Pokémon the bot fights with, of any species: its stats at 50 (IVs 31, 85 EVs each, a neutral
// nature) from its base stats, and four moves chosen for it (the real moves it could learn are not
// built in): a weak, a middling and a strong one of its own types, physical or special as its
// Attack or Special Attack is higher, and a boost or a status move.
inline MonSpec BotMon(int species, const int base[6], const int types[2], const std::string& name, std::mt19937& rng, int level = 50)
{
    MonSpec m;
    m.species = species; m.name = name; m.level = level;
    m.shiny = rng() % 64 == 0;
    m.types[0] = types[0]; m.types[1] = types[1];
    for (int i = 0; i < 6; i++) m.stats[i] = (2 * base[i] + 31 + 21) * level / 100 + (i == 0 ? level + 10 : 5);
    // (Per type: physical weak, middling, strong; then special the same.)
    static const int moves[18][6] = {
        { 98, 34, 38, 129, 304, 63 },     { 183, 280, 370, 410, 396, 411 }, { 17, 332, 413, 16, 403, 542 },
        { 40, 398, 441, 51, 188, 482 },   { 523, 91, 89, 189, 426, 414 },   { 88, 157, 444, 246, 408, 800 },
        { 210, 404, 224, 522, 324, 405 }, { 425, 421, 566, 466, 506, 247 }, { 418, 442, 231, 429, 430, 796 },
        { 424, 7, 394, 52, 53, 126 },     { 453, 127, 401, 55, 57, 56 },    { 331, 402, 452, 345, 412, 76 },
        { 209, 9, 528, 84, 85, 87 },      { 427, 428, 706, 93, 60, 94 },    { 420, 8, 556, 181, 58, 59 },
        { 525, 337, 200, 239, 406, 434 }, { 44, 400, 242, 555, 399, 539 },  { 98, 583, 583, 574, 605, 585 } };
    const bool phys = m.stats[1] >= m.stats[4];
    const int o = phys ? 0 : 3;
    const int t1 = types[0] >= 0 && types[0] < 18 ? types[0] : 0, t2 = types[1] >= 0 && types[1] < 18 ? types[1] : t1;
    m.moves[0] = moves[t1][o];
    m.moves[1] = moves[t2][o + 1];
    m.moves[2] = moves[t1][o + 2];
    if (m.moves[1] == m.moves[2]) m.moves[1] = moves[t1][o + 1] != m.moves[2] ? moves[t1][o + 1] : moves[t2][o];
    static const int physBoost[] = { 14, 339, 349, 86, 92, 261 }, specBoost[] = { 347, 417, 105, 86, 92, 261 };
    m.moves[3] = (phys ? physBoost : specBoost)[rng() % 6];
    return m;
}

class Battle
{
public:
    enum Phase { INTRO, FIGHT, KO, END };
    std::vector<MonSpec> team[2];
    int cur[2] = { 0, 0 };
    Fighter f[2];
    std::vector<Proj> projs;
    std::vector<Beam> beams;
    std::vector<Ring> rings;
    std::vector<FloatText> texts;
    std::vector<Particle> parts;
    std::vector<Impact> impacts;
    Banner banner;
    Phase phase = INTRO;
    float phaseT = 0, time = 0, shake = 0;
    float duel = 0;                   // this pair's time; at DUEL_TIME the one with less HP (as a share) falls
    static constexpr float DUEL_TIME = 99;
    int winner = -1, difficulty = 1; // 0 easy, 1 normal, 2 hard
    int dealt[2] = {}, hits[2] = {}, kos[2] = {};
    bool megaUsed[2] = {};           // (one Mega Evolution a side a battle, as the games)
    std::string intro;                // "Vai, Charizard!"
    std::mt19937 rng{ 12345 };

    void Start(const std::vector<MonSpec>& mine, const std::vector<MonSpec>& theirs, int diff, unsigned seed)
    {
        *this = Battle();
        rng.seed(seed);
        team[0] = mine; team[1] = theirs; difficulty = diff;
        for (int s = 0; s < 2; s++) Enter(s, 0);
        SetPhase(INTRO, "Lutar!");
    }
    int Left(int side) const
    {
        int n = 0;
        for (int i = cur[side]; i < (int)team[side].size(); i++) n += !(i == cur[side] && f[side].fainted);
        return n;
    }
    float R01() { return std::uniform_real_distribution<float>(0, 1)(rng); }
    // The HP each of a side's team has now: those before the one fighting fainted, those after it
    // untouched.
    int HpAfter(int side, int i) const
    {
        if (i < cur[side]) return 0;
        if (i == cur[side]) return f[side].fainted ? 0 : f[side].hp;
        const MonSpec& m = team[side][i];
        return m.hp >= 0 ? m.hp : m.stats[0];
    }

    // A step of dt seconds (the page calls it at 60 a second).
    void Step(float dt, Input in0)
    {
        time += dt;
        phaseT += dt;
        shake = std::max(0.0f, shake - dt * 3);
        if (banner.life > 0) banner.t += dt;
        for (auto& t : texts) t.t += dt;
        texts.erase(std::remove_if(texts.begin(), texts.end(), [](const FloatText& t) { return t.t >= t.life; }), texts.end());
        for (auto& p : parts) { p.t += dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vx *= 0.92f; p.vy *= 0.92f; }
        parts.erase(std::remove_if(parts.begin(), parts.end(), [](const Particle& p) { return p.t >= p.life; }), parts.end());
        for (auto& m : impacts) m.t += dt;
        impacts.erase(std::remove_if(impacts.begin(), impacts.end(), [](const Impact& m) { return m.t >= 0.35f; }), impacts.end());
        if (phase == INTRO) { if (phaseT > 1.4f) SetPhase(FIGHT); return; }
        if (phase == END) return;
        if (phase == KO)
        {
            UpdateShots(dt * 0.3f);
            if (phaseT > 1.5f)
            {
                for (int s = 0; s < 2; s++)
                    if (f[s].fainted)
                    {
                        if (cur[s] + 1 < (int)team[s].size()) Enter(s, cur[s] + 1);
                        else { winner = 1 - s; SetPhase(END); return; }
                    }
                projs.clear(); beams.clear(); rings.clear();
                duel = 0;
                SetPhase(INTRO, "Lutar!");
            }
            return;
        }
        // (Time: the one with less of its HP left falls; both, when even.)
        duel += dt;
        if (duel >= DUEL_TIME)
        {
            const float a = (float)f[0].hp / f[0].maxHp, b = (float)f[1].hp / f[1].maxHp;
            for (int s = 0; s < 2; s++)
                if ((s == 0 ? a <= b : b <= a) && !f[s].fainted) { f[s].hp = 0; f[s].fainted = true; Say(f[s].x, f[s].y - 70, "Tempo!", 0xFFFFD040u, 1.2f); }
        }
        Input in1 = BotThink(1, dt);
        Act(0, in0, dt);
        Act(1, in1, dt);
        UpdateShots(dt);
        for (int s = 0; s < 2; s++) Tick(s, dt);
        // (Those that fainted: K.O.)
        if (f[0].fainted || f[1].fainted)
        {
            for (int s = 0; s < 2; s++) if (f[s].fainted) kos[1 - s]++;
            SetPhase(KO);
            shake = 1;
        }
    }
    // The bot's own think, open so the page can show it and the test can run both sides.
    Input BotThink(int s, float dt);

private:
    float botTimer[2] = {}, botStrafe[2] = { 1, -1 };
    Input botHeld[2];

    void SetPhase(Phase p, const std::string& say = std::string())
    {
        phase = p; phaseT = 0;
        if (!say.empty()) intro = say;
    }
    void Say(float x, float y, const std::string& s, uint32_t col, float size = 1, float life = 1.1f)
    {
        // (Several at once over the same one: each a line higher, never on top of another.)
        int near = 0;
        for (const FloatText& o : texts) if (std::fabs(o.x - x) < 120 && o.t < 0.4f) near++;
        FloatText t; t.s = s; t.x = x + (R01() - 0.5f) * 30; t.y = y - near * 30; t.col = col; t.size = size; t.life = life;
        texts.push_back(t);
    }
    void Burst(float x, float y, uint32_t col, int n, float speed, float size = 6, float life = 0.45f)
    {
        for (int i = 0; i < n; i++)
        {
            const float a = R01() * 6.2832f, v = speed * (0.3f + R01());
            Particle p; p.x = x; p.y = y; p.vx = std::cos(a) * v; p.vy = std::sin(a) * v * 0.7f; p.col = col; p.size = size * (0.6f + R01() * 0.8f); p.life = life * (0.6f + R01() * 0.8f);
            parts.push_back(p);
        }
    }
    static float Power(int id)
    {
        const BattleData::MoveInfo& m = BattleData::Move(id);
        if (m.cat == 2) return 0;
        return m.power ? (float)m.power : 60.0f; // (fixed-damage ones: Seismic Toss, Night Shade... as 60)
    }
    bool Stab(const MonSpec& s, int type) const { return s.types[0] == type || s.types[1] == type; }
    // The buttons from its four moves (see the top).
public:
    void MakeKit(Fighter& F)
    {
        std::vector<int> dmg, other;
        for (int m : F.spec.moves)
        {
            if (m <= 0 || m >= BattleData::MOVE_COUNT) continue;
            if (BattleData::Move(m).cat == 2) other.push_back(m); else dmg.push_back(m);
        }
        auto score = [&](int m) { const auto& i = BattleData::Move(m); return Power(m) * (Stab(F.spec, i.type) ? 1.5f : 1.0f) * (i.acc ? i.acc / 100.0f : 1.0f); };
        std::stable_sort(dmg.begin(), dmg.end(), [&](int a, int b) { return score(a) < score(b); });
        int atq = 0, ult = 0;
        std::vector<int> rest;
        if (dmg.empty()) { atq = 33; ult = 33; } // (none that hurts: Tackle, as all have)
        else if (dmg.size() == 1) { atq = ult = dmg[0]; }
        else { atq = dmg.front(); ult = dmg.back(); rest.assign(dmg.begin() + 1, dmg.end() - 1); }
        for (int m : other) rest.push_back(m);
        F.sk[S_ATQ].move = atq; F.sk[S_ULT].move = ult;
        F.sk[S_HAB].move = rest.size() > 0 ? rest[0] : 0;
        F.sk[S_Q].move = rest.size() > 1 ? rest[1] : 0;
        for (int s = 0; s < 4; s++)
        {
            Skill& k = F.sk[s];
            const auto& i = BattleData::Move(k.move);
            if (s == S_ATQ) k.cdMax = 0.5f + (Power(k.move) >= 80 ? 0.25f : 0);
            else if (s == S_ULT) k.cdMax = 0;
            else if (i.cat == 2) k.cdMax = k.move == 156 ? 25.0f : M::Heal(k.move) ? 15.0f : 8.0f; // (Rest a long wait; healing ones a while)
            else k.cdMax = 2.4f + Power(k.move) / 40.0f;
            if (s != S_ULT && M::Priority(k.move)) k.cdMax *= 0.6f;
            k.cd = s == S_ATQ ? 0 : k.cdMax * 0.4f;
        }
    }
private:
    void Enter(int s, int index)
    {
        cur[s] = index;
        Fighter F;
        F.spec = team[s][index];
        F.maxHp = std::max(1, F.spec.stats[0]);
        F.hp = F.spec.hp >= 0 ? std::max(1, std::min(F.maxHp, F.spec.hp)) : F.maxHp;
        F.x = (s == 0 ? -1.0f : 1.0f) * RX * 0.55f;
        F.y = 0;
        F.face = s == 0 ? 1.0f : -1.0f;
        F.dashCd = 1;
        MakeKit(F);
        f[s] = F;
        Burst(F.x, F.y, 0xFFFFFFFFu, 18, 260, 7, 0.5f);
    }
    float Speed(const Fighter& F) const
    {
        float v = (150.0f + F.spec.stats[3] * 1.2f) * StageMul(F.stage[ST_SPE]);
        if (F.status == PAR) v *= 0.5f;
        return v;
    }
    bool Stunned(const Fighter& F) const { return F.status == SLP || F.status == FRZ; }
    static void Clamp(Fighter& F)
    {
        const float k = (F.x * F.x) / (RX * RX) + (F.y * F.y) / (RY * RY);
        if (k > 1) { const float r = 1 / std::sqrt(k); F.x *= r; F.y *= r; }
    }
    void Act(int s, const Input& in, float dt)
    {
        Fighter& F = f[s];
        Fighter& O = f[1 - s];
        for (auto& k : F.sk) k.cd = std::max(0.0f, k.cd - dt * (F.status == PAR ? 0.75f : 1.0f));
        F.dashCd = std::max(0.0f, F.dashCd - dt);
        F.busy = std::max(0.0f, F.busy - dt);
        F.flash = std::max(0.0f, F.flash - dt);
        F.castT = std::max(0.0f, F.castT - dt);
        F.atkT = std::max(0.0f, F.atkT - dt);
        F.hurtT = std::max(0.0f, F.hurtT - dt);
        F.protectT = std::max(0.0f, F.protectT - dt);
        F.ultLock = std::max(0.0f, F.ultLock - dt);
        if (F.ultLock <= 0) F.ult = std::min(100.0f, F.ult + dt * 1.5f);
        // (A lunge or a dash under way: it goes on by itself.)
        if (F.lungeT > 0)
        {
            const float st = std::min(dt, F.lungeT);
            F.x += F.lungeX * st; F.y += F.lungeY * st; F.lungeT -= st;
            Clamp(F);
            if (!F.lungeHit && Dist(F, O) < (F.lungeUlt ? 120.0f : 80.0f))
            {
                F.lungeHit = true;
                Hit(s, F.lungeMove, F.lungeScale, F.lungeUlt, F.x, F.y);
                F.lungeT = std::min(F.lungeT, 0.05f);
            }
            if (F.lungeT <= 0 && !F.lungeHit && F.lungeUlt) Say(F.x, F.y - 70, "Errou", 0xFFDDDDDDu);
            return;
        }
        if (F.dashT > 0)
        {
            const float st = std::min(dt, F.dashT);
            F.x += F.dashX * st; F.y += F.dashY * st; F.dashT -= st;
            Clamp(F);
            if (R01() < 0.6f) { Particle p; p.x = F.x; p.y = F.y + 20; p.size = 8; p.col = 0x88FFFFFFu; p.life = 0.3f; parts.push_back(p); }
            return;
        }
        if (Stunned(F)) { F.moving = false; return; }
        // Moving.
        float mx = in.mx, my = in.my;
        const float len = std::sqrt(mx * mx + my * my);
        if (len > 1) { mx /= len; my /= len; }
        if (F.confT > 0) { mx = -mx; my = -my; }
        F.moving = len > 0.15f && F.busy <= 0.12f;
        if (F.moving)
        {
            const float v = Speed(F);
            F.x += mx * v * dt; F.y += my * v * dt;
            Clamp(F);
            F.walk += dt * (4 + v / 80);
            if (std::fabs(mx) > 0.2f) F.face = mx > 0 ? 1.0f : -1.0f;
            F.dirX = mx; F.dirY = my;
        }
        else
        {
            if (O.x != F.x) F.face = O.x > F.x ? 1.0f : -1.0f;
            if (F.atkT <= 0) { F.dirX = O.x - F.x; F.dirY = O.y - F.y; }
        }
        // Its buttons.
        if (in.press[S_EVO]) MegaEvolve(s);
        if (in.press[S_DASH] && F.dashCd <= 0)
        {
            float dx = mx, dy = my;
            if (std::sqrt(dx * dx + dy * dy) < 0.2f) { dx = F.x - O.x; dy = F.y - O.y; const float l = std::max(1.0f, std::sqrt(dx * dx + dy * dy)); dx /= l; dy /= l; }
            else { const float l = std::sqrt(dx * dx + dy * dy); dx /= l; dy /= l; }
            const float v = 1500;
            F.dashX = dx * v; F.dashY = dy * v; F.dashT = 0.2f; F.dashCd = 3.5f;
            return;
        }
        if (F.busy > 0) return;
        for (int k : { S_ULT, S_HAB, S_Q, S_ATQ })
            if (in.press[k] && Use(s, k)) break;
    }
    // MEGA EVOLUTION: its Mega's stats and types from now on (its HP as it is), once a battle a side.
    void MegaEvolve(int s)
    {
        Fighter& F = f[s];
        if (F.spec.mega < 0 || megaUsed[s] || F.mega || F.busy > 0 || F.fainted) return;
        megaUsed[s] = true;
        F.mega = true;
        for (int i = 1; i < 6; i++) F.spec.stats[i] = F.spec.megaStats[i];
        F.spec.types[0] = F.spec.megaTypes[0]; F.spec.types[1] = F.spec.megaTypes[1];
        F.spec.name = F.spec.megaName;
        F.busy = 0.8f; F.protectT = std::max(F.protectT, 0.8f);
        banner.text = "Mega Evolu\xC3\xA7\xC3\xA3o!"; banner.type = F.spec.types[0]; banner.t = 0; banner.life = 1.2f;
        static const uint32_t rainbow[] = { 0xFFFF6060u, 0xFFFFB040u, 0xFFFFF060u, 0xFF60F080u, 0xFF60B0FFu, 0xFFC070FFu };
        for (uint32_t col : rainbow) Burst(F.x, F.y, col, 8, 300, 9, 0.7f);
        shake = std::max(shake, 0.6f);
    }
    static float Dist(const Fighter& a, const Fighter& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y)); }
    bool Use(int s, int k)
    {
        Fighter& F = f[s];
        Fighter& O = f[1 - s];
        Skill& sk = F.sk[k];
        if (!sk.move) return false;
        if (k == S_ULT ? F.ult < 100 : sk.cd > 0) return false;
        const BattleData::MoveInfo& mi = BattleData::Move(sk.move);
        const bool ult = k == S_ULT;
        if (ult) { F.ult = 0; F.ultLock = 8; banner.text = std::string(mi.name) + "!"; banner.type = mi.type; banner.t = 0; banner.life = 1.1f; }
        else sk.cd = sk.cdMax;
        float dx = O.x - F.x, dy = O.y - F.y;
        const float d = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
        dx /= d; dy /= d;
        if (std::fabs(dx) > 0.1f) F.face = dx > 0 ? 1.0f : -1.0f;
        F.dirX = dx; F.dirY = dy;
        F.atkT = F.atkDur = mi.cat == 2 ? 0.35f : ult ? 0.6f : k == S_ATQ ? 0.3f : 0.45f;
        F.castT = 0.3f; F.castType = mi.type;
        const float scale = k == S_ATQ ? 0.24f : ult ? 1.3f : 0.48f;
        if (k != S_ATQ) Say(F.x, F.y - 90, mi.name, 0xFFFFFFFFu, 0.85f, 0.9f);
        if (M::Boom(sk.move)) F.boomT = 0.45f;
        const float rest = M::Recharge(sk.move) ? 1.0f : 0.0f;
        if (rest > 0) Say(F.x, F.y - 110, "Recarregando...", 0xFFDDDDDDu, 0.75f, 1.2f);
        if (mi.cat == 2) { StatusMove(s, sk.move); F.busy = 0.35f; return true; }
        // (Its shape, as Showdown describes the move: battle_moves.h.)
        const int shape = M::Ground(sk.move) ? BattleData::SH_RING : mi.shape;
        if (shape == BattleData::SH_RING)
        {
            Ring r; r.owner = s; r.move = sk.move; r.type = mi.type; r.x = F.x; r.y = F.y; r.rmax = ult ? 420.0f : 240.0f; r.scale = scale;
            rings.push_back(r);
            F.busy = 0.35f + rest; shake = std::max(shake, ult ? 1.0f : 0.5f);
            return true;
        }
        if (shape == BattleData::SH_LUNGE)
        {
            // (A lunge: as far as the other, or its reach.)
            const float reach = ult ? 480.0f : k == S_ATQ ? 200.0f : 280.0f;
            const float go = std::min(reach, std::max(40.0f, d - 30));
            const float dur = ult ? 0.3f : M::Priority(sk.move) ? 0.08f : 0.16f;
            F.lungeX = dx * go / dur; F.lungeY = dy * go / dur; F.lungeT = dur;
            F.lungeMove = sk.move; F.lungeScale = scale; F.lungeHit = false; F.lungeUlt = ult;
            F.busy = dur + (k == S_ATQ ? 0.12f : 0.25f) + rest;
            return true;
        }
        if (ult || shape == BattleData::SH_JET)
        {
            // (A beam: a ULT's across the field after its warning; a jet, short and quick.)
            Beam b; b.owner = s; b.move = sk.move; b.type = mi.type; b.x0 = F.x; b.y0 = F.y; b.dx = dx; b.dy = dy; b.scale = scale; b.ult = ult;
            if (!ult) { b.windup = 0.12f; b.dur = 0.3f; b.len = 720; b.width = k == S_ATQ ? 44.0f : 62.0f; }
            beams.push_back(b);
            F.busy = b.windup + b.dur + rest;
            return true;
        }
        const float v = shape == BattleData::SH_WAVE ? 620.0f : k == S_ATQ ? 900.0f : 760.0f;
        auto shot = [&](float ax, float ay, float sc, float r, bool wave) {
            Proj p; p.owner = s; p.move = sk.move; p.type = mi.type; p.x = F.x + ax * 30; p.y = F.y + ay * 30;
            p.vx = ax * v; p.vy = ay * v; p.life = (wave ? 680.0f : 1300.0f) / v; p.r = r; p.scale = sc; p.wave = wave;
            projs.push_back(p);
        };
        if (shape == BattleData::SH_SPREAD)
        {
            // (Several at once, in a fan: the middle one where it aims.)
            for (int i = -1; i <= 1; i++)
            {
                const float a = i * 0.22f, ca = std::cos(a), sa = std::sin(a);
                shot(dx * ca - dy * sa, dy * ca + dx * sa, scale * 0.55f, k == S_ATQ ? 13.0f : 18.0f, false);
            }
        }
        else if (shape == BattleData::SH_WAVE) shot(dx, dy, scale, 55, true);
        else shot(dx, dy, scale, k == S_ATQ ? 16.0f : 28.0f, false);
        F.busy = (k == S_ATQ ? 0.22f : 0.35f) + rest;
        return true;
    }
    void Stage(int s, int which, int by)
    {
        static const char* n[STAGES] = { "ATK", "DEF", "SP.ATK", "SP.DEF", "VEL", "PRECIS\xC3\x83O", "ESQUIVA" };
        Fighter& F = f[s];
        const int was = F.stage[which];
        F.stage[which] = std::max(-6, std::min(6, was + by));
        if (F.stage[which] == was) { Say(F.x, F.y - 60, std::string(n[which]) + " n\xC3\xA3o muda", 0xFFDDDDDDu, 0.8f); return; }
        Say(F.x, F.y - 60 - (float)texts.size() * 3, std::string(n[which]) + (by > 0 ? " up" : " down"), by > 0 ? 0xFF7CFC7Cu : 0xFFFF8080u, 0.9f);
        Burst(F.x, F.y, by > 0 ? 0xFF9CFF9Cu : 0xFFFF9C9Cu, 10, 120, 5);
    }
    bool Inflict(int s, int st, bool say = true)
    {
        Fighter& F = f[s];
        if (F.status != NONE) return false;
        const auto has = [&](int t) { return F.spec.types[0] == t || F.spec.types[1] == t; };
        if ((st == BRN && has(T_FIRE)) || (st == PAR && has(T_ELECTRIC)) || ((st == PSN || st == TOX) && (has(T_POISON) || has(T_STEEL))) || (st == FRZ && has(T_ICE))) return false;
        F.status = st; F.tick = 0; F.toxN = 0;
        F.statusT = st == SLP ? 2.2f : st == FRZ ? 1.5f : 6.0f;
        static const char* n[] = { "", "Queimado!", "Envenenado!", "Muito envenenado!", "Paralisado!", "Dormiu!", "Congelado!" };
        static const uint32_t col[] = { 0, 0xFFFF9040u, 0xFFC070E0u, 0xFFA040C0u, 0xFFF8D030u, 0xFFB0B0C8u, 0xFF98E0F0u };
        if (say) Say(F.x, F.y - 75, n[st], col[st], 0.95f);
        return true;
    }
    void StatusMove(int s, int id)
    {
        Fighter& F = f[s];
        Fighter& O = f[1 - s];
        using M::In;
        // (On itself.)
        if (In(id, { 14 })) { Stage(s, ST_ATK, 2); return; }
        if (In(id, { 74, 526 })) { Stage(s, ST_ATK, 1); Stage(s, ST_SPA, 1); return; }
        if (In(id, { 347 })) { Stage(s, ST_SPA, 1); Stage(s, ST_SPD, 1); return; }
        if (In(id, { 339, 489 })) { Stage(s, ST_ATK, 1); Stage(s, ST_DEF, 1); return; }
        if (In(id, { 349, 508 })) { Stage(s, ST_ATK, 1); Stage(s, ST_SPE, 1); return; }
        if (In(id, { 483 })) { Stage(s, ST_SPA, 1); Stage(s, ST_SPD, 1); Stage(s, ST_SPE, 1); return; }
        if (In(id, { 504 })) { Stage(s, ST_ATK, 2); Stage(s, ST_SPA, 2); Stage(s, ST_SPE, 2); Stage(s, ST_DEF, -1); Stage(s, ST_SPD, -1); return; }
        if (In(id, { 417, 294 })) { Stage(s, ST_SPA, 2); return; }
        if (In(id, { 97, 397, 475 })) { Stage(s, ST_SPE, 2); return; }
        if (In(id, { 96, 159, 336, 468 })) { Stage(s, ST_ATK, 1); return; }
        if (In(id, { 334, 112, 151, 538 })) { Stage(s, ST_DEF, 2); return; }
        if (In(id, { 106, 110, 111 })) { Stage(s, ST_DEF, 1); return; }
        if (In(id, { 133 })) { Stage(s, ST_SPD, 2); return; }
        if (In(id, { 104, 107 })) { Stage(s, ST_EVA, 1); return; }
        if (In(id, { 116 })) { F.focusT = 15; Say(F.x, F.y - 60, "Foco!", 0xFFFFE070u); return; }
        if (In(id, { 182, 197, 203, 588, 596 })) { F.protectT = 1.3f; Say(F.x, F.y - 60, "Protegido!", 0xFF90D0FFu); return; }
        if (In(id, { 156 }))
        {
            // (Rest: half its HP back, its status gone, and asleep, open to all, a long while.)
            F.status = NONE; Heal(s, F.maxHp / 2); Inflict(s, SLP, false); F.statusT = 4.0f;
            Say(F.x, F.y - 60, "Dormiu!", 0xFFB0B0C8u); Burst(F.x, F.y, 0xFF9CFF9Cu, 16, 140);
            return;
        }
        if (M::Heal(id)) { Heal(s, F.maxHp * 2 / 5); return; }
        if (In(id, { 392, 275 })) { F.regenT = 10; Say(F.x, F.y - 60, "Enraizado!", 0xFF7CFC7Cu); return; }
        // (On the other, wherever it is.)
        if (O.protectT > 0) { Say(O.x, O.y - 60, "Protegeu!", 0xFF90D0FFu); return; }
        const float acc = BattleData::Move(id).acc;
        if (acc > 0 && R01() * 100 >= acc * StageMul(F.stage[ST_ACC]) / StageMul(O.stage[ST_EVA])) { Say(O.x, O.y - 60, "Errou", 0xFFDDDDDDu); return; }
        Burst(O.x, O.y, 0xFFE0C0FFu, 10, 140, 5);
        const int o = 1 - s;
        if (In(id, { 45 })) { Stage(o, ST_ATK, -1); return; }
        if (In(id, { 43, 39 })) { Stage(o, ST_DEF, -1); return; }
        if (In(id, { 103 })) { Stage(o, ST_DEF, -2); return; }
        if (In(id, { 28, 108, 148, 134 })) { Stage(o, ST_ACC, -1); return; }
        if (In(id, { 81, 184, 178 })) { Stage(o, ST_SPE, -2); return; }
        if (In(id, { 204, 297 })) { Stage(o, ST_ATK, -2); return; }
        if (In(id, { 313, 319 })) { Stage(o, ST_SPD, -2); return; }
        if (In(id, { 321 })) { Stage(o, ST_ATK, -1); Stage(o, ST_DEF, -1); return; }
        if (In(id, { 86, 78, 137 })) { if (!Inflict(o, PAR)) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); return; }
        if (In(id, { 92 })) { if (!Inflict(o, TOX)) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); return; }
        if (In(id, { 77, 139 })) { if (!Inflict(o, PSN)) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); return; }
        if (In(id, { 261 })) { if (!Inflict(o, BRN)) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); return; }
        if (In(id, { 79, 95, 47, 147, 142, 281, 320, 464 })) { if (!Inflict(o, SLP)) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); return; }
        if (In(id, { 109, 48, 207, 186, 298 })) { O.confT = 3; Say(O.x, O.y - 60, "Confuso!", 0xFFFFA0E0u); return; }
        if (In(id, { 73 })) { if (O.spec.types[0] == T_GRASS || O.spec.types[1] == T_GRASS) Say(O.x, O.y - 60, "N\xC3\xA3o afetou", 0xFFDDDDDDu); else { O.seedT = 8; Say(O.x, O.y - 60, "Semente!", 0xFF7CFC7Cu); } return; }
        // (Any other: a short boost, so no button is ever dead.)
        Stage(s, (int)(R01() * 2) ? ST_DEF : ST_SPD, 1);
    }
    void Heal(int s, int amount)
    {
        Fighter& F = f[s];
        const int was = F.hp;
        F.hp = std::min(F.maxHp, F.hp + std::max(1, amount));
        if (F.hp > was) { Say(F.x, F.y - 70, "+" + std::to_string(F.hp - was), 0xFF7CFC7Cu, 1.0f); Burst(F.x, F.y, 0xFF9CFF9Cu, 8, 90, 5); }
    }
    void Hurt(int s, int amount)
    {
        Fighter& F = f[s];
        if (F.fainted) return;
        F.hp = std::max(0, F.hp - amount);
        if (F.hp == 0) { F.fainted = true; Burst(F.x, F.y, 0xFFFFFFFFu, 26, 320, 8, 0.7f); }
    }
    // A move of s's reaching the other.
    void Hit(int s, int move, float scale, bool ult, float hx, float hy)
    {
        Fighter& A = f[s];
        Fighter& D = f[1 - s];
        if (D.fainted) return;
        if (D.dashT > 0) { Say(D.x, D.y - 60, "Esquivou!", 0xFFDDDDDDu, 0.85f); return; }
        if (D.protectT > 0) { Say(D.x, D.y - 60, "Protegeu!", 0xFF90D0FFu); Burst(D.x, D.y, 0xFF90D0FFu, 10, 160); return; }
        const BattleData::MoveInfo& mi = BattleData::Move(move);
        if (!ult && mi.acc > 0 && R01() * 100 >= mi.acc * StageMul(A.stage[ST_ACC]) / StageMul(D.stage[ST_EVA])) { Say(D.x, D.y - 60, "Errou", 0xFFDDDDDDu); return; }
        const bool phys = mi.cat == 0;
        float atk = (phys ? A.spec.stats[1] * StageMul(A.stage[ST_ATK]) : A.spec.stats[4] * StageMul(A.stage[ST_SPA]));
        if (phys && A.status == BRN) atk *= 0.5f;
        const float def = phys ? D.spec.stats[2] * StageMul(D.stage[ST_DEF]) : D.spec.stats[5] * StageMul(D.stage[ST_SPD]);
        const float eff = TypeEff(mi.type, D.spec.types[0]) * (D.spec.types[1] >= 0 ? TypeEff(mi.type, D.spec.types[1]) : 1.0f);
        if (eff == 0) { Say(D.x, D.y - 60, "Imune", 0xFFBBBBBBu); return; }
        const bool crit = R01() < (A.focusT > 0 ? 0.25f : 1.0f / 16);
        float dmg = (22.0f * Power(move) * atk / std::max(1.0f, def)) / 50.0f + 2;
        dmg *= (Stab(A.spec, mi.type) ? 1.5f : 1.0f) * eff * (0.85f + 0.15f * R01()) * (crit ? 1.5f : 1.0f) * scale;
        if (s == 1) dmg *= difficulty == 0 ? 0.75f : difficulty == 2 ? 1.1f : 1.0f;
        // (A ULT takes at most 60% of a full bar: never one that wipes a team alone.)
        if (ult) dmg = std::min(dmg, D.maxHp * 0.6f);
        const int n = std::max(1, (int)std::lround(dmg));
        Hurt(1 - s, n);
        dealt[s] += n; hits[s]++;
        D.flash = 0.15f;
        D.hurtT = 0.3f;
        // (Knocked back a little.)
        float kx = D.x - hx, ky = D.y - hy;
        const float kl = std::max(1.0f, std::sqrt(kx * kx + ky * ky));
        const float kb = ult ? 110.0f : scale > 0.5f ? 45.0f : 18.0f;
        D.x += kx / kl * kb; D.y += ky / kl * kb;
        Clamp(D);
        const uint32_t tc = TypeColour(mi.type);
        { Impact im; im.x = D.x; im.y = D.y; im.type = mi.type; im.big = ult || scale > 0.5f; im.phys = phys; impacts.push_back(im); }
        Burst(D.x, D.y, tc, ult ? 30 : scale > 0.5f ? 16 : 8, ult ? 380.0f : 220.0f, ult ? 10.0f : 6.0f);
        Say(D.x, D.y - 50, std::to_string(n), eff > 1 ? 0xFFFFD040u : eff < 1 ? 0xFFC8C8C8u : 0xFFFFFFFFu, ult ? 1.5f : scale > 0.5f ? 1.15f : 0.9f, 0.9f);
        if (eff > 1) Say(D.x, D.y - 80, "Super eficaz!", 0xFFFFD040u, 0.85f);
        else if (eff < 1) Say(D.x, D.y - 80, "Pouco eficaz", 0xFFC8C8C8u, 0.8f);
        if (crit) Say(D.x, D.y - 100, "Cr\xC3\xADtico!", 0xFFFF7050u, 0.85f);
        if (ult) shake = std::max(shake, 0.8f);
        // (Its ULT charges by fighting.)
        // (Its ULT charges by fighting; not by the ULT's own hit, nor while locked after one.)
        if (!ult && A.ultLock <= 0) A.ult = std::min(100.0f, A.ult + 45.0f * n / std::max(1, D.maxHp));
        if (D.ultLock <= 0) D.ult = std::min(100.0f, D.ult + 40.0f * n / std::max(1, D.maxHp));
        // (What else it does.)
        if (M::Drain(move)) Heal(s, n / 2);
        if (M::Recoil(move)) Hurt(s, std::max(1, n / 3));
        if (!D.fainted)
        {
            const float k = ult ? 2.0f : 1.0f;
            const float r = R01();
            if (mi.type == T_FIRE && r < 0.12f * k) Inflict(1 - s, BRN);
            else if (mi.type == T_ELECTRIC && r < 0.12f * k) Inflict(1 - s, PAR);
            else if (mi.type == T_ICE && r < 0.10f * k) Inflict(1 - s, FRZ);
            else if (mi.type == T_POISON && r < 0.2f * k) Inflict(1 - s, PSN);
            else if (move == 503 && r < 0.3f) Inflict(1 - s, BRN);
        }
    }
    void UpdateShots(float dt)
    {
        for (Proj& p : projs)
        {
            p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt;
            if (R01() < 0.7f) { Particle q; q.x = p.x; q.y = p.y; q.size = p.r * 0.5f; q.col = TypeColour(p.type); q.life = 0.25f; parts.push_back(q); }
            Fighter& D = f[1 - p.owner];
            if (p.life > 0 && phase == FIGHT && !D.fainted && D.dashT <= 0)
            {
                const float dx = D.x - p.x, dy = D.y - p.y;
                if (dx * dx + dy * dy < (p.r + 34) * (p.r + 34)) { Hit(p.owner, p.move, p.scale, p.ult, p.x, p.y); p.life = 0; }
            }
            if ((p.x * p.x) / ((RX + 120) * (RX + 120)) + (p.y * p.y) / ((RY + 120) * (RY + 120)) > 1) p.life = 0;
        }
        projs.erase(std::remove_if(projs.begin(), projs.end(), [](const Proj& p) { return p.life <= 0; }), projs.end());
        for (Beam& b : beams)
        {
            b.t += dt;
            if (!b.hit && b.t >= b.windup && phase == FIGHT)
            {
                b.hit = true;
                Fighter& D = f[1 - b.owner];
                // (Along the line, within its width.)
                const float px = D.x - b.x0, py = D.y - b.y0;
                const float along = px * b.dx + py * b.dy, across = std::fabs(px * -b.dy + py * b.dx);
                if (along > 0 && along < b.len && across < b.width * 0.5f + 30) Hit(b.owner, b.move, b.scale, b.ult, D.x - b.dx * 50, D.y - b.dy * 50);
                else Say(D.x, D.y - 60, "Errou", 0xFFDDDDDDu);
                if (b.ult) shake = std::max(shake, 0.6f);
            }
        }
        beams.erase(std::remove_if(beams.begin(), beams.end(), [](const Beam& b) { return b.t >= b.windup + b.dur; }), beams.end());
        for (Ring& r : rings)
        {
            r.t += dt;
            if (!r.hit && r.t >= r.dur * 0.5f && phase == FIGHT)
            {
                r.hit = true;
                const Fighter& D = f[1 - r.owner];
                const float dx = D.x - r.x, dy = (D.y - r.y) * 1.6f;
                if (dx * dx + dy * dy < r.rmax * r.rmax) Hit(r.owner, r.move, r.scale, r.scale > 1, r.x, r.y);
                else Say(D.x, D.y - 60, "Errou", 0xFFDDDDDDu);
            }
        }
        rings.erase(std::remove_if(rings.begin(), rings.end(), [](const Ring& r) { return r.t >= r.dur; }), rings.end());
    }
    void Tick(int s, float dt)
    {
        Fighter& F = f[s];
        if (F.fainted) return;
        if (F.boomT > 0 && (F.boomT -= dt) <= 0) { Burst(F.x, F.y, 0xFFFFE0A0u, 40, 420, 12, 0.8f); shake = 1; Hurt(s, F.hp); return; }
        F.confT = std::max(0.0f, F.confT - dt);
        F.focusT = std::max(0.0f, F.focusT - dt);
        if (F.status != NONE)
        {
            F.statusT -= dt;
            F.tick += dt;
            if (F.tick >= 1)
            {
                F.tick -= 1;
                if (F.status == BRN) { const int n = std::max(1, F.maxHp / 50); Hurt(s, n); Say(F.x, F.y - 40, std::to_string(n), 0xFFFF9040u, 0.75f, 0.7f); }
                if (F.status == PSN) { const int n = std::max(1, F.maxHp / 40); Hurt(s, n); Say(F.x, F.y - 40, std::to_string(n), 0xFFC070E0u, 0.75f, 0.7f); }
                if (F.status == TOX) { F.toxN++; const int n = std::max(1, (int)(F.maxHp * 0.01f * F.toxN)); Hurt(s, n); Say(F.x, F.y - 40, std::to_string(n), 0xFFA040C0u, 0.75f, 0.7f); }
            }
            if (F.statusT <= 0) F.status = NONE;
        }
        if (F.seedT > 0)
        {
            F.seedT -= dt;
            static float acc[2] = {};
            acc[s] += dt;
            if (acc[s] >= 1) { acc[s] -= 1; const int n = std::max(1, F.maxHp / 50); Hurt(s, n); Heal(1 - s, n); }
        }
        if (F.regenT > 0)
        {
            F.regenT -= dt;
            static float acc[2] = {};
            acc[s] += dt;
            if (acc[s] >= 1) { acc[s] -= 1; Heal(s, std::max(1, F.maxHp / 30)); }
        }
    }
};

// THE BOT: it keeps where its moves work best (close for a physical ATQ, away for a special one),
// circles, uses what is ready, dodges with the dash. Harder: it sees sooner and dodges more.
inline Input Battle::BotThink(int s, float dt)
{
    Fighter& F = f[s];
    Fighter& O = f[1 - s];
    const float react = difficulty == 0 ? 0.55f : difficulty == 1 ? 0.3f : 0.12f;
    const float dodge = difficulty == 0 ? 0.08f : difficulty == 1 ? 0.35f : 0.7f;
    Input in = botHeld[s];
    for (bool& b : in.press) b = false;
    botTimer[s] -= dt;
    const float dx = O.x - F.x, dy = O.y - F.y;
    const float d = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
    // (Something coming: the dash, sometimes.)
    if (F.dashCd <= 0)
    {
        bool danger = false;
        for (const Proj& p : projs)
        {
            if (p.owner == s) continue;
            const float px = F.x - p.x, py = F.y - p.y, pl = std::sqrt(px * px + py * py);
            const float vl = std::max(1.0f, std::sqrt(p.vx * p.vx + p.vy * p.vy));
            if (pl < 260 && (px * p.vx + py * p.vy) / (pl * vl + 1) > 0.85f) danger = true;
        }
        for (const Beam& b : beams) if (b.owner != s && !b.hit && b.t > b.windup * 0.4f) danger = true;
        if (O.lungeT > 0 && d < 260) danger = true;
        if (danger && R01() < dodge * dt * 8)
        {
            in.mx = -dy / d * botStrafe[s]; in.my = dx / d * botStrafe[s];
            in.press[S_DASH] = true;
            return in;
        }
    }
    if (botTimer[s] > 0) return in;
    botTimer[s] = react * (0.7f + 0.6f * R01());
    const BattleData::MoveInfo& atq = BattleData::Move(F.sk[S_ATQ].move);
    const bool close = atq.shape == BattleData::SH_LUNGE || atq.shape == BattleData::SH_RING;
    const float want = close ? 90.0f : 380.0f;
    if (R01() < 0.15f) botStrafe[s] = -botStrafe[s];
    float mx = 0, my = 0;
    if (d > want + 60) { mx = dx / d; my = dy / d; }
    else if (d < want - 80) { mx = -dx / d; my = -dy / d; }
    mx += -dy / d * botStrafe[s] * 0.7f; my += dx / d * botStrafe[s] * 0.7f;
    // (Off the walls: back towards the middle.)
    if ((F.x * F.x) / (RX * RX) + (F.y * F.y) / (RY * RY) > 0.7f) { mx -= F.x / RX; my -= F.y / RY; }
    in.mx = mx; in.my = my;
    botHeld[s] = in;
    if (F.busy > 0 || F.lungeT > 0) return in;
    auto reach = [&](int k) {
        const BattleData::MoveInfo& m = BattleData::Move(F.sk[k].move);
        if (m.cat == 2) return true;
        const int sh = M::Ground(F.sk[k].move) ? BattleData::SH_RING : m.shape;
        if (sh == BattleData::SH_RING) return d < (k == S_ULT ? 380.0f : 220.0f);
        if (sh == BattleData::SH_LUNGE) return d < (k == S_ULT ? 470.0f : k == S_ATQ ? 210.0f : 290.0f);
        if (k != S_ULT && sh == BattleData::SH_JET) return d < 680.0f;
        if (sh == BattleData::SH_WAVE) return d < 650.0f;
        return d < 1000.0f;
    };
    // (Healing when low, boosting early, the rest when in reach.)
    for (int k : { S_HAB, S_Q })
    {
        const int m = F.sk[k].move;
        if (!m || F.sk[k].cd > 0 || BattleData::Move(m).cat != 2) continue;
        if (M::Heal(m) && F.hp > F.maxHp * 0.55f) continue;
        if (R01() < (difficulty == 0 ? 0.3f : 0.7f)) { in.press[k] = true; return in; }
    }
    if (F.ult >= 100 && reach(S_ULT) && R01() < 0.8f) { in.press[S_ULT] = true; return in; }
    for (int k : { S_HAB, S_Q })
        if (F.sk[k].move && F.sk[k].cd <= 0 && BattleData::Move(F.sk[k].move).cat != 2 && reach(k) && R01() < 0.75f) { in.press[k] = true; return in; }
    if (reach(S_ATQ) && R01() < (difficulty == 0 ? 0.45f : difficulty == 1 ? 0.75f : 0.95f)) in.press[S_ATQ] = true;
    return in;
}
}
