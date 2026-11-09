// The Pokémon hub (pokemon_hub.h) on PKSM-Core.
#include "pokemon_hub.h"

#include "pkx/PKX.hpp"
#include "sav/Sav.hpp"
#include "utils/i18n.hpp"

#include <algorithm>
#include <cstdio>
#include <ctime>
#include <filesystem>

namespace PokemonHub
{
namespace fs = std::filesystem;
using pksm::Generation;
using pksm::GameVersion;

namespace
{

// UTF-8 paths, whichever C++ this builds as (PKSM-Core wants C++20, where u8string changed).
fs::path P(const std::string& s)
{
#if defined(__cpp_char8_t)
    return fs::path(std::u8string(s.begin(), s.end()));
#else
    return fs::u8path(s);
#endif
}
std::string S(const fs::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

int GenNumber(Generation g)
{
    if (g == Generation::ONE) return 1;
    if (g == Generation::TWO) return 2;
    if (g == Generation::THREE) return 3;
    if (g == Generation::FOUR) return 4;
    if (g == Generation::FIVE) return 5;
    if (g == Generation::SIX) return 6;
    if (g == Generation::SEVEN) return 7;
    return 0; // (Let's Go and the Switch games: not Fulldex's)
}

std::string Upper(std::string s)
{
    for (char& c : s)
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
    return s;
}

std::string SpeciesName(int n) { return Upper(i18n::species(pksm::Language::ENG, pksm::Species{(u16)n})); }
std::string MoveName(int n) { return Upper(i18n::move(pksm::Language::ENG, pksm::Move{(u16)n})); }
std::string ItemName(int n) { return Upper(i18n::item(pksm::Language::ENG, (u16)n)); }

bool IsHm(int move)
{
    for (int m : { 15, 19, 57, 70, 127, 148, 249, 250, 291, 431, 432 })
        if (m == move) return true;
    return false;
}

// Trade evolutions: species, item it must hold (0: none), what it becomes, the generation the
// item and evolution first existed in. (Karrablast and Shelmet need each other: not here.)
struct TradeEvo { int from, item, to, gen; };
const TradeEvo kTradeEvos[] = {
    { 64, 0, 65, 1 },   { 67, 0, 68, 1 },     { 75, 0, 76, 1 },     { 93, 0, 94, 1 },
    { 61, 221, 186, 2 }, { 79, 221, 199, 2 }, { 95, 233, 208, 2 }, { 123, 233, 212, 2 },
    { 117, 235, 230, 2 }, { 137, 252, 233, 2 }, { 366, 226, 367, 3 }, { 366, 227, 368, 3 },
    { 233, 324, 474, 4 }, { 112, 321, 464, 4 }, { 125, 322, 466, 4 }, { 126, 323, 467, 4 },
    { 356, 325, 477, 4 }, { 349, 537, 350, 5 }, { 525, 0, 526, 5 },  { 533, 0, 534, 5 },
    { 682, 647, 683, 6 }, { 684, 646, 685, 6 }, { 708, 0, 709, 6 },  { 710, 0, 711, 6 },
};
const int kEverstone = 229;

// (Karrablast and Shelmet: only when traded for each other.)
const TradeEvo kPairEvos[] = { { 588, 0, 589, 5 }, { 616, 0, 617, 5 } };

const TradeEvo* EvolutionFor(const pksm::PKX& pk, const pksm::Sav& to, int tradedFor = 0)
{
    if (pk.egg() || pk.heldItem() == kEverstone) return nullptr;
    const int species = (int)pk.species(), item = pk.heldItem();
    if ((species == 588 && tradedFor == 616) || (species == 616 && tradedFor == 588))
        for (const TradeEvo& e : kPairEvos)
            if (e.from == species && e.to <= (int)to.maxSpecies() && GenNumber(to.generation()) >= 5) return &e;
    for (const TradeEvo& e : kTradeEvos)
        if (e.from == species && (e.item == 0 || e.item == item) && e.to <= (int)to.maxSpecies() && GenNumber(to.generation()) >= std::max(e.gen, 3))
            return &e;
    return nullptr;
}

std::string GameNameOf(const pksm::Sav& s)
{
    const GameVersion v = s.version();
    if (v == GameVersion::S || v == GameVersion::R) return "Ruby / Sapphire";
    if (v == GameVersion::E) return "Emerald";
    if (v == GameVersion::FR || v == GameVersion::LG) return "FireRed / LeafGreen";
    if (v == GameVersion::D || v == GameVersion::P) return "Diamond / Pearl";
    if (v == GameVersion::Pt) return "Platinum";
    if (v == GameVersion::HG || v == GameVersion::SS) return "HeartGold / SoulSilver";
    if (v == GameVersion::B) return "Black";
    if (v == GameVersion::W) return "White";
    if (v == GameVersion::B2) return "Black 2";
    if (v == GameVersion::W2) return "White 2";
    if (v == GameVersion::X) return "X";
    if (v == GameVersion::Y) return "Y";
    if (v == GameVersion::OR) return "Omega Ruby";
    if (v == GameVersion::AS) return "Alpha Sapphire";
    if (v == GameVersion::SN) return "Sun";
    if (v == GameVersion::MN) return "Moon";
    if (v == GameVersion::US) return "Ultra Sun";
    if (v == GameVersion::UM) return "Ultra Moon";
    return "?";
}

std::string Why(pksm::Sav::BadTransferReason r, const std::string& game)
{
    using R = pksm::Sav::BadTransferReason;
    switch (r)
    {
        case R::SPECIES: return "THIS POKEMON IS NOT IN " + Upper(game);
        case R::FORM: return "THIS FORM IS NOT IN " + Upper(game);
        case R::MOVE: return "IT KNOWS A MOVE " + Upper(game) + " DOES NOT HAVE";
        case R::ABILITY: return "ITS ABILITY IS NOT IN " + Upper(game);
        case R::ITEM: return "ITS HELD ITEM IS NOT IN " + Upper(game);
        case R::BALL: return "ITS POKE BALL IS NOT IN " + Upper(game);
        default: return "IT CANNOT GO TO " + Upper(game);
    }
}

bool ReadFile(const std::string& path, std::vector<u8>& out)
{
    out.clear();
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return false;
    u8 buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) out.insert(out.end(), buf, buf + n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
}

bool WriteAtomic(const std::string& path, const u8* data, size_t size)
{
    const std::string tmp = path + ".hub-tmp";
    FILE* f = fopen(tmp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, size, f) == size;
    ok = fflush(f) == 0 && ok;
    ok = fclose(f) == 0 && ok;
    if (!ok) { remove(tmp.c_str()); return false; }
#ifdef __SWITCH__
    const std::string old = path + ".hub-old";
    remove(old.c_str());
    const bool hadOld = rename(path.c_str(), old.c_str()) == 0;
    if (rename(tmp.c_str(), path.c_str()) != 0)
    {
        if (hadOld) rename(old.c_str(), path.c_str());
        remove(tmp.c_str());
        return false;
    }
    if (hadOld) remove(old.c_str());
#else
    std::error_code ec;
    fs::rename(P(tmp), P(path), ec);
    if (ec) { remove(tmp.c_str()); return false; }
#endif
    return true;
}

// Into the backups folder beside the save, named as the game's own (black2-20261105-091500.sav).
std::string BackUp(const std::string& path, const std::vector<u8>& data)
{
    const fs::path p = P(path);
    const fs::path dir = (p.has_parent_path() ? p.parent_path() : fs::path(".")) / "backups";
    std::error_code ec;
    fs::create_directories(dir, ec);
    const std::string stem = S(p.stem()).empty() ? S(p.filename()) : S(p.stem());
    time_t now = time(nullptr);
    for (int tries = 0; tries < 10; tries++, now++)
    {
        char when[32];
        strftime(when, sizeof when, "%Y%m%d-%H%M%S", localtime(&now));
        const fs::path out = dir / (stem + "-" + when + ".sav");
        if (fs::exists(out, ec)) continue;
        return WriteAtomic(S(out), data.data(), data.size()) ? S(out) : std::string();
    }
    return std::string();
}

}

// ---------------------------------------------------------------------------------------------

struct Save::Impl
{
    std::string path;
    std::vector<u8> original;            // the file as read, for putting back
    std::vector<u8> tail;                // what an emulator keeps after a GBA save (mGBA: 16 bytes of clock), kept as it was
    std::unique_ptr<pksm::Sav> sav;
    bool readOnly = false, changed = false;
    // The Bank (Save::OpenBank): its folder, each place's Pokémon (nullptr: free), and the places
    // changed since it was last written, with what their file held before (to put it back).
    bool bank = false;
    std::vector<std::unique_ptr<pksm::PKX>> slots;
    std::vector<std::pair<int, std::vector<u8>>> touched; // (place, its file before; empty: none)

    std::unique_ptr<pksm::PKX> At(int box, int slot) const
    {
        if (bank)
        {
            if (box < 0 || box >= BANK_BOXES || slot < 0 || slot >= 30) return nullptr;
            const auto& pk = slots[(size_t)box * 30 + slot];
            return pk ? pk->clone() : nullptr;
        }
        if (box == PARTY)
        {
            if (slot < 0 || slot >= 6 || slot >= (int)sav->partyCount()) return nullptr;
            auto pk = sav->pkm((u8)slot);
            return pk && (int)pk->species() != 0 ? std::move(pk) : nullptr;
        }
        if (box < 0 || box >= sav->maxBoxes() || slot < 0 || slot >= 30) return nullptr;
        auto pk = sav->pkm((u8)box, (u8)slot);
        return pk && (int)pk->species() != 0 ? std::move(pk) : nullptr;
    }
};

namespace
{
pksm::Generation GenOf(int n)
{
    return n == 1 ? Generation::ONE : n == 2 ? Generation::TWO : n == 3 ? Generation::THREE : n == 4 ? Generation::FOUR : n == 5 ? Generation::FIVE : n == 6 ? Generation::SIX : Generation::SEVEN;
}
// A party Pokémon as a box one (the first part of the party form is the box form).
std::unique_ptr<pksm::PKX> BoxForm(const pksm::PKX& pk)
{
    const int g = GenNumber(pk.generation());
    // (The first two generations' are kept with their names, party or not, as PKHeX's .pk1 and .pk2.)
    if (!pk.isParty() || g <= 2) return pk.clone();
    const size_t len = g == 3 ? 80 : g <= 5 ? 136 : 232;
    std::vector<u8> raw(pk.rawData().begin(), pk.rawData().begin() + std::min<size_t>(len, pk.rawData().size()));
    return pksm::PKX::getPKM(pk.generation(), raw.data(), raw.size());
}
Mon MonOf(const pksm::PKX& pk, int box, int slot)
{
    Mon m;
    m.box = box;
    m.slot = slot;
    m.species = (int)pk.species();
    m.form = pk.alternativeForm();
    m.level = pk.level();
    m.nickname = pk.nickname();
    m.speciesName = SpeciesName(m.species);
    m.trainer = pk.otName();
    m.shiny = pk.shiny();
    m.egg = pk.egg();
    m.heldItem = pk.heldItem();
    m.generation = GenNumber(pk.generation());
    return m;
}
}

Save::Save() : d(new Impl) {}
Save::~Save() {}

std::unique_ptr<Save> Save::Open(const std::string& path, std::string& error, bool romHack)
{
    std::unique_ptr<Save> s(new Save);
    if (!ReadFile(path, s->d->original) || s->d->original.empty()) { error = "CANNOT READ THE SAVE"; return nullptr; }
    // (A GBA save with something after its 128 KB, as mGBA's clock: read without it, written back with it.)
    size_t size = s->d->original.size();
    if (size > 0x20000 && size <= 0x20000 + 0x100) size = 0x20000;
    // (A Game Boy save with its clock after it, as mGBA writes Gold, Silver and Crystal's: the same.)
    if (size > 0x8000 && size <= 0x8000 + 0x100) size = 0x8000;
    s->d->tail.assign(s->d->original.begin() + size, s->d->original.end());
    auto data = std::shared_ptr<u8[]>(new u8[size]);
    std::copy(s->d->original.begin(), s->d->original.begin() + size, data.get());
    s->d->sav = pksm::Sav::getSave(data, (u32)size);
    if (!s->d->sav || GenNumber(s->d->sav->generation()) == 0) { error = "THIS SAVE IS NOT ONE THE HUB KNOWS"; return nullptr; }
    s->d->path = path;
    s->d->readOnly = romHack;
    return s;
}

namespace
{
// A Bank place's file: <folder>/box-07/12.pk4 (box 7, slot 12, a Gen 4 Pokémon), as PKHeX names its kinds.
std::string BankBoxDir(const std::string& folder, int box)
{
    char b[16];
    snprintf(b, sizeof b, "box-%02d", box + 1);
    return S(P(folder) / b);
}
std::string BankFile(const std::string& folder, int box, int slot, int gen)
{
    char b[16];
    snprintf(b, sizeof b, "%02d.pk%d", slot + 1, gen);
    return S(P(BankBoxDir(folder, box)) / b);
}
// The place's file now, whichever generation it is (empty path: none).
std::string BankFileFound(const std::string& folder, int box, int slot)
{
    for (int g = 1; g <= 7; g++)
    {
        const std::string f = BankFile(folder, box, slot, g);
        std::error_code ec;
        if (fs::exists(P(f), ec)) return f;
    }
    return std::string();
}
}

std::unique_ptr<Save> Save::OpenBank(const std::string& folder, std::string& error)
{
    std::unique_ptr<Save> s(new Save);
    s->d->bank = true;
    s->d->path = folder;
    s->d->slots.resize((size_t)BANK_BOXES * 30);
    std::error_code ec;
    fs::create_directories(P(folder), ec);
    if (ec) { error = "CANNOT MAKE THE BANK'S FOLDER"; return nullptr; }
    for (int box = 0; box < BANK_BOXES; box++)
        for (int slot = 0; slot < 30; slot++)
        {
            const std::string f = BankFileFound(folder, box, slot);
            if (f.empty()) continue;
            std::vector<u8> raw;
            if (!ReadFile(f, raw) || raw.empty() || raw.size() > 512) continue;
            const int gen = f.back() - '0';
            auto pk = pksm::PKX::getPKM(GenOf(gen), raw.data(), raw.size());
            if (pk && (int)pk->species() != 0) s->d->slots[(size_t)box * 30 + slot] = std::move(pk);
        }
    return s;
}

bool Save::IsBank() const { return d->bank; }
const std::string& Save::Path() const { return d->path; }
int Save::Generation() const { return d->bank ? 0 : GenNumber(d->sav->generation()); }
std::string Save::GameName() const { return d->bank ? std::string("Bank") : GameNameOf(*d->sav); }
std::string Save::Trainer() const { return d->bank ? std::string() : d->sav->otName(); }
bool Save::ReadOnly() const { return d->readOnly; }
int Save::Boxes() const { return d->bank ? BANK_BOXES : d->sav->maxBoxes(); }
std::string Save::BoxName(int box) const { return d->bank ? "Bank " + std::to_string(box + 1) : d->sav->boxName((u8)box); }
bool Save::Changed() const { return d->changed; }

std::vector<Mon> Save::Box(int box) const
{
    std::vector<Mon> out;
    for (int slot = 0; slot < 30; slot++)
        if (auto pk = d->At(box, slot)) out.push_back(MonOf(*pk, box, slot));
    return out;
}

bool Save::FirstEmpty(int& box, int& slot) const
{
    for (box = 0; box < Boxes(); box++)
        for (slot = 0; slot < 30; slot++)
            if (!d->At(box, slot)) return true;
    return false;
}

namespace
{
// The trade evolution itself: species, ability by its slot, the item used up, the name if it had
// none of its own (Gens 3 and 4 write names in capitals, from 5 on as words: "GOLEM", "Golem").
void Evolve(pksm::PKX& pk, const TradeEvo& e, int generation)
{
    const bool named = pk.nicknamed();
    pk.species(pksm::Species{(u16)e.to});
    pk.setAbility(pk.abilityNumber());
    if (e.item) pk.heldItem((u16)0);
    if (!named)
    {
        std::string name = i18n::species(pk.language(), pksm::Species{(u16)e.to});
        if (generation <= 4) name = Upper(name);
        pk.nickname(name);
    }
    pk.updatePartyData();
    pk.refreshChecksum(); // (a changed Pokémon whose own checksum is not redone is a Bad Egg in Gen 3)
}
}

// ---------------------------------------------------------------------------------------------

namespace
{
// The Pokémon as it would arrive, and the check of it. needRoom: a free place in the boxes too.
std::unique_ptr<pksm::PKX> ArrivingPk(const pksm::PKX& source, int a, bool readOnly, const Save& to, Check& c, bool needRoom, int tradedFor)
{
    auto pk = BoxForm(source);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return nullptr; }
    if (readOnly || to.ReadOnly())
    {
        c.verdict = Verdict::ReadOnly;
        c.reason = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET";
        return nullptr;
    }
    if (to.IsBank())
    {
        // (The Bank keeps it as it is, in its own game's format: nothing converted, nothing lost.)
        int fb, fs;
        if (needRoom && !to.FirstEmpty(fb, fs)) { c.verdict = Verdict::Full; c.reason = "THE BANK IS FULL"; return nullptr; }
        c.verdict = Verdict::Ok;
        return pk;
    }
    const int b = to.Generation();
    // (Red, Blue, Yellow and Gold, Silver, Crystal traded both ways, by the Time Capsule; from them,
    // Pokémon only ever went on to Sun and Moon, by Poké Transporter.)
    if (a <= 2 && b >= 3 && b < 7)
    {
        c.verdict = Verdict::NotInGame;
        c.reason = "GEN 1 AND 2 POKEMON ONLY WENT ON TO SUN AND MOON, AS IN THE ORIGINAL GAMES";
        return nullptr;
    }
    if (b < a && !(a <= 2 && b <= 2))
    {
        c.verdict = Verdict::Backwards;
        c.reason = "POKEMON ONLY MOVE TO NEWER GAMES, AS IN THE ORIGINAL GAMES";
        return nullptr;
    }
    if (pk->egg() && b != a)
    {
        c.verdict = Verdict::Egg;
        c.reason = "EGGS CAN ONLY MOVE BETWEEN GAMES OF THE SAME GENERATION";
        return nullptr;
    }
    auto arrives = to.d->sav->transfer(*pk);
    if (!arrives) { c.verdict = Verdict::NotInGame; c.reason = "IT CANNOT GO TO " + Upper(to.GameName()); return nullptr; }
    const auto why = to.d->sav->invalidTransferReason(*arrives);
    if (why != pksm::Sav::BadTransferReason::OKAY)
    {
        c.verdict = Verdict::NotInGame;
        c.reason = Why(why, to.GameName());
        return nullptr;
    }
    // What it gives up on the way.
    for (int i = 0; i < 4; i++)
    {
        const int m = (int)pk->move((u8)i);
        if (m == 0) continue;
        bool kept = false;
        for (int j = 0; j < 4; j++) kept = kept || (int)arrives->move((u8)j) == m;
        if (!kept) c.losses.push_back("FORGETS " + MoveName(m) + (IsHm(m) ? " (HM)" : ""));
    }
    if (pk->heldItem() != 0 && arrives->heldItem() != pk->heldItem())
        c.losses.push_back("LEAVES ITS " + ItemName(pk->heldItem()) + " BEHIND");
    if (const TradeEvo* e = EvolutionFor(*arrives, *to.d->sav, tradedFor))
    {
        c.evolvesTo = e->to;
        c.evolution = SpeciesName(e->from) + " CAN EVOLVE INTO " + SpeciesName(e->to) + " ON ARRIVAL, AS IF TRADED" +
                      (e->item ? " (USING ITS " + ItemName(e->item) + ")" : "");
    }
    int fb, fs;
    if (needRoom && !to.FirstEmpty(fb, fs)) { c.verdict = Verdict::Full; c.reason = "THERE IS NO ROOM IN " + Upper(to.GameName()) + "'S BOXES"; return nullptr; }
    c.verdict = Verdict::Ok;
    return arrives;
}
std::unique_ptr<pksm::PKX> Arriving(const Save& from, int box, int slot, const Save& to, Check& c, bool needRoom = true, int tradedFor = 0)
{
    auto pk = from.d->At(box, slot);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return nullptr; }
    return ArrivingPk(*pk, GenNumber(pk->generation()), from.ReadOnly(), to, c, needRoom, tradedFor);
}
// (Taking the party's last Pokémon that is not an egg would leave a game nothing to play with.)
bool LastInParty(const Save& s, int box, int slot)
{
    if (box != PARTY || s.IsBank()) return false;
    int others = 0;
    for (int i = 0; i < (int)s.d->sav->partyCount(); i++)
        if (i != slot)
            if (auto pk = s.d->At(PARTY, i)) others += !pk->egg();
    return others == 0;
}
// Puts a Pokémon at a place (the party, or a box), and takes one away.
// (In the Bank: the place's Pokémon, and the place noted with its file as it was, for Commit.)
void BankSet(Save& s, int box, int slot, std::unique_ptr<pksm::PKX> pk)
{
    const int place = box * 30 + slot;
    bool noted = false;
    for (const auto& t : s.d->touched) noted = noted || t.first == place;
    if (!noted)
    {
        std::vector<u8> before;
        const std::string f = BankFileFound(s.d->path, box, slot);
        if (!f.empty()) ReadFile(f, before);
        s.d->touched.push_back({ place, before });
    }
    s.d->slots[(size_t)place] = std::move(pk);
    s.d->changed = true;
}
void Put(Save& s, const pksm::PKX& pk, int box, int slot)
{
    if (s.IsBank()) { BankSet(s, box, slot, pk.clone()); return; }
    if (box == PARTY)
    {
        s.d->sav->pkm(pk, (u8)slot);
        s.d->sav->fixParty();
    }
    else s.d->sav->pkm(pk, (u8)box, (u8)slot, true);
    s.d->changed = true;
}
void Take(Save& s, int box, int slot)
{
    if (s.IsBank()) { BankSet(s, box, slot, nullptr); return; }
    if (box == PARTY)
    {
        s.d->sav->pkm(*s.d->sav->emptyPkm(), (u8)slot);
        s.d->sav->fixParty();
    }
    else s.d->sav->pkm(*s.d->sav->emptyPkm(), (u8)box, (u8)slot, false);
    s.d->changed = true;
}
// Where it goes: ANY_BOX the first free place, TO_PARTY the end of the party. False: no room there.
bool Place(const Save& to, int& box, int& slot)
{
    if (box == TO_PARTY)
    {
        if (to.IsBank() || (int)to.d->sav->partyCount() >= 6) return false;
        box = PARTY;
        slot = (int)to.d->sav->partyCount();
        return true;
    }
    if (box == ANY_BOX) return to.FirstEmpty(box, slot);
    if (box < 0 || box >= to.Boxes()) return false;
    if (slot < 0)
    {
        // (A box chosen, not a slot: its first free one.)
        for (slot = 0; slot < 30; slot++)
            if (!to.d->At(box, slot)) return true;
        return false;
    }
    return !to.d->At(box, slot) && box >= 0 && box < to.Boxes() && slot >= 0 && slot < 30;
}
}

Check CanMove(const Save& from, int box, int slot, const Save& to)
{
    Check c;
    Arriving(from, box, slot, to, c);
    return c;
}

bool Move(Save& from, int box, int slot, Save& to, bool evolve, std::string& error, int toBox, int toSlot)
{
    if (&from == &to) { error = "CHOOSE TWO DIFFERENT GAMES"; return false; }
    const Check c = CanMoveTo(from, box, slot, to, toBox, toSlot);
    if (c.verdict != Verdict::Ok) { error = c.reason; return false; }
    Check again;
    auto pk = Arriving(from, box, slot, to, again, false);
    if (!pk || !Place(to, toBox, toSlot)) { error = "THERE IS NO ROOM THERE"; return false; }
    if (evolve && !to.IsBank())
        if (const TradeEvo* e = EvolutionFor(*pk, *to.d->sav)) Evolve(*pk, *e, to.Generation());
    pk->refreshChecksum();
    Take(from, box, slot);
    Put(to, *pk, toBox, toSlot);
    return true;
}

Check CanEvolve(const Save& save, int box, int slot)
{
    Check c;
    auto pk = save.d->At(box, slot);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return c; }
    if (save.ReadOnly()) { c.verdict = Verdict::ReadOnly; c.reason = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return c; }
    if (save.IsBank()) { c.verdict = Verdict::NoEvolution; c.reason = "POKEMON EVOLVE IN A GAME, NOT IN THE BANK"; return c; }
    const TradeEvo* e = EvolutionFor(*pk, *save.d->sav);
    if (!e)
    {
        c.verdict = Verdict::NoEvolution;
        c.reason = pk->heldItem() == kEverstone ? "IT IS HOLDING AN EVERSTONE" : "IT DOES NOT EVOLVE BY TRADING, OR NEEDS TO HOLD AN ITEM";
        return c;
    }
    c.verdict = Verdict::Ok;
    c.evolvesTo = e->to;
    c.evolution = SpeciesName(e->from) + " WILL EVOLVE INTO " + SpeciesName(e->to) + (e->item ? " (USING ITS " + ItemName(e->item) + ")" : "");
    return c;
}

bool Evolve(Save& save, int box, int slot, std::string& error)
{
    const Check c = CanEvolve(save, box, slot);
    if (c.verdict != Verdict::Ok) { error = c.reason; return false; }
    auto pk = save.d->At(box, slot);
    Evolve(*pk, *EvolutionFor(*pk, *save.d->sav), save.Generation());
    save.d->sav->pkm(*pk, (u8)box, (u8)slot, false);
    save.d->changed = true;
    return true;
}

namespace
{
// The Bank's changed places written: adding (the places that now hold a Pokémon) or taking away
// (the files of places now free). Adding goes before the game's save is written and taking away
// after it, so that whatever fails, a Pokémon is never in neither place.
bool BankWrite(Save& s, bool adding, std::string& error)
{
    for (const auto& t : s.d->touched)
    {
        const int box = t.first / 30, slot = t.first % 30;
        const auto& pk = s.d->slots[(size_t)t.first];
        if (adding != (pk != nullptr)) continue;
        std::error_code ec;
        fs::create_directories(P(BankBoxDir(s.d->path, box)), ec);
        // (Whatever file the place had, of any generation, goes first.)
        const std::string old = BankFileFound(s.d->path, box, slot);
        if (!pk)
        {
            if (!old.empty() && !fs::remove(P(old), ec)) { error = "CANNOT TAKE IT OUT OF THE BANK'S FOLDER"; return false; }
            continue;
        }
        const std::string f = BankFile(s.d->path, box, slot, GenNumber(pk->generation()));
        if (!old.empty() && old != f) fs::remove(P(old), ec);
        auto kept = BoxForm(*pk);
        if (!kept || !WriteAtomic(f, kept->rawData().data(), kept->rawData().size())) { error = "CANNOT WRITE TO THE BANK'S FOLDER"; return false; }
    }
    return true;
}
// (The places added put back as their files were, when the game's save could not be written.)
void BankUndoAdding(Save& s)
{
    for (const auto& t : s.d->touched)
    {
        const int box = t.first / 30, slot = t.first % 30;
        if (!s.d->slots[(size_t)t.first]) continue;
        std::error_code ec;
        const std::string now = BankFileFound(s.d->path, box, slot);
        if (!now.empty()) fs::remove(P(now), ec);
        if (!t.second.empty())
        {
            // (Its old file's generation, from its length: as PKHeX's box forms are.)
            const size_t n = t.second.size();
            const int g = n == 69 || n == 59 ? 1 : n == 73 || n == 63 ? 2 : n == 80 ? 3 : n == 232 ? 6 : 4;
            WriteAtomic(BankFile(s.d->path, box, slot, g), t.second.data(), n);
        }
    }
}
void BankDone(Save& s)
{
    s.d->touched.clear();
    s.d->changed = false;
}
bool WriteSave(Save& s, std::string& error)
{
    s.d->sav->finishEditing();
    std::vector<u8> out(s.d->sav->rawData().get(), s.d->sav->rawData().get() + s.d->sav->getLength());
    out.insert(out.end(), s.d->tail.begin(), s.d->tail.end());
    const bool ok = WriteAtomic(s.d->path, out.data(), out.size());
    s.d->sav->beginEditing();
    if (!ok) error = "CANNOT WRITE " + Upper(s.GameName()) + "'S SAVE";
    return ok;
}
}

bool Commit(Save& save, std::string& error, std::vector<std::string>* backups)
{
    if (!save.Changed()) return true;
    if (save.IsBank())
    {
        if (!BankWrite(save, true, error) || !BankWrite(save, false, error)) return false;
        BankDone(save);
        return true;
    }
    const std::string kept = BackUp(save.Path(), save.d->original);
    if (kept.empty()) { error = "CANNOT BACK UP " + Upper(save.GameName()) + "'S SAVE - NOTHING CHANGED"; return false; }
    if (backups) backups->push_back(kept);
    if (!WriteSave(save, error)) return false;
    ReadFile(save.Path(), save.d->original);
    save.d->changed = false;
    return true;
}

bool Commit(Save& a, Save& b, std::string& error, std::vector<std::string>* backups)
{
    // With the Bank: what it gains first, then the game's save (backed up), then what it gives up.
    if (a.IsBank() || b.IsBank())
    {
        Save& bank = a.IsBank() ? a : b;
        Save& game = a.IsBank() ? b : a;
        if (game.IsBank()) { error = "CHOOSE A GAME"; return false; }
        if (bank.Changed() && !BankWrite(bank, true, error)) { BankUndoAdding(bank); return false; }
        if (!Commit(game, error, backups))
        {
            BankUndoAdding(bank);
            return false;
        }
        if (bank.Changed() && !BankWrite(bank, false, error))
        {
            error += " - IT IS NOW IN BOTH: TAKE IT OUT OF ONE";
            BankDone(bank);
            return false;
        }
        BankDone(bank);
        return true;
    }
    Save* both[2] = { &a, &b };
    for (Save* s : both)
        if (s->Changed())
        {
            const std::string kept = BackUp(s->Path(), s->d->original);
            if (kept.empty()) { error = "CANNOT BACK UP " + Upper(s->GameName()) + "'S SAVE - NOTHING CHANGED"; return false; }
            if (backups) backups->push_back(kept);
        }
    if (a.Changed() && !WriteSave(a, error)) return false;
    if (b.Changed() && !WriteSave(b, error))
    {
        // (The first is put back as it was: the Pokémon stays where it was.)
        if (a.Changed() && !WriteAtomic(a.Path(), a.d->original.data(), a.d->original.size()))
            error += " - AND " + Upper(a.GameName()) + "'S SAVE COULD NOT BE PUT BACK: RESTORE ITS BACKUP";
        return false;
    }
    for (Save* s : both)
        if (s->Changed())
        {
            ReadFile(s->Path(), s->d->original);
            s->d->changed = false;
        }
    return true;
}

// ---------------------------------------------------------------------------------------------

std::vector<Mon> Party(const Save& save)
{
    std::vector<Mon> out;
    if (save.IsBank()) return out;
    for (int slot = 0; slot < 6; slot++)
        if (auto pk = save.d->At(PARTY, slot)) out.push_back(MonOf(*pk, PARTY, slot));
    return out;
}

bool Free(const Save& save, int box, int slot) { return !save.d->At(box, slot); }
bool PartyFull(const Save& save) { return save.IsBank() || (int)save.d->sav->partyCount() >= 6; }

Check CanMoveTo(const Save& from, int box, int slot, const Save& to, int toBox, int toSlot)
{
    Check c;
    if (&from == &to) { c.verdict = Verdict::Empty; c.reason = "CHOOSE TWO DIFFERENT GAMES"; return c; }
    if (!Arriving(from, box, slot, to, c, false)) return c;
    if (LastInParty(from, box, slot))
    {
        c.verdict = Verdict::LastInParty;
        c.reason = "IT IS THE LAST POKEMON IN YOUR PARTY: A GAME NEEDS ONE";
        return c;
    }
    int b = toBox, s = toSlot;
    if (!Place(to, b, s))
    {
        c.verdict = toBox == ANY_BOX ? Verdict::Full : Verdict::Taken;
        c.reason = toBox == ANY_BOX ? "THERE IS NO ROOM IN " + Upper(to.GameName()) + "'S BOXES"
                 : toBox == TO_PARTY ? "THE PARTY IN " + Upper(to.GameName()) + " IS FULL" : "THAT PLACE IS TAKEN";
    }
    return c;
}

Check CanSwap(const Save& a, int boxA, int slotA, const Save& b, int boxB, int slotB)
{
    Check c;
    if (&a == &b) { c.verdict = Verdict::Empty; c.reason = "CHOOSE TWO DIFFERENT GAMES"; return c; }
    if (a.IsBank() || b.IsBank()) { c.verdict = Verdict::NotInGame; c.reason = "THE BANK DOES NOT TRADE: MOVE THEM ONE BY ONE"; return c; }
    auto pa = a.d->At(boxA, slotA), pb = b.d->At(boxB, slotB);
    if (!pa || !pb) { c.verdict = Verdict::Empty; c.reason = "CHOOSE A POKEMON IN EACH GAME"; return c; }
    Check ca, cb;
    if (!Arriving(a, boxA, slotA, b, ca, false, (int)pb->species())) return ca;
    if (!Arriving(b, boxB, slotB, a, cb, false, (int)pa->species())) return cb;
    c.verdict = Verdict::Ok;
    for (const std::string& l : ca.losses) c.losses.push_back(SpeciesName((int)pa->species()) + ": " + l);
    for (const std::string& l : cb.losses) c.losses.push_back(SpeciesName((int)pb->species()) + ": " + l);
    if (ca.evolvesTo || cb.evolvesTo)
    {
        c.evolvesTo = ca.evolvesTo ? ca.evolvesTo : cb.evolvesTo;
        c.evolution = ca.evolution + (ca.evolvesTo && cb.evolvesTo ? "; " : "") + cb.evolution;
    }
    return c;
}

bool Swap(Save& a, int boxA, int slotA, Save& b, int boxB, int slotB, bool evolve, std::string& error)
{
    const Check c = CanSwap(a, boxA, slotA, b, boxB, slotB);
    if (c.verdict != Verdict::Ok) { error = c.reason; return false; }
    const int sa = (int)a.d->At(boxA, slotA)->species(), sb = (int)b.d->At(boxB, slotB)->species();
    Check x, y;
    auto toB = Arriving(a, boxA, slotA, b, x, false, sb);
    auto toA = Arriving(b, boxB, slotB, a, y, false, sa);
    if (!toB || !toA) { error = "THE TRADE COULD NOT BE MADE"; return false; }
    if (evolve)
    {
        if (const TradeEvo* e = EvolutionFor(*toB, *b.d->sav, sb)) Evolve(*toB, *e, b.Generation());
        if (const TradeEvo* e = EvolutionFor(*toA, *a.d->sav, sa)) Evolve(*toA, *e, a.Generation());
    }
    toB->refreshChecksum();
    toA->refreshChecksum();
    Put(b, *toB, boxB, slotB);
    Put(a, *toA, boxA, slotA);
    return true;
}

bool Detail(const Save& save, int box, int slot, MonDetail& out)
{
    auto pk = save.d->At(box, slot);
    if (!pk) return false;
    out = MonDetail();
    out.mon = MonOf(*pk, box, slot);
    const int g = GenNumber(pk->generation());
    auto type = [](pksm::Type t) { return Upper(i18n::type(pksm::Language::ENG, t)); };
    out.type1 = type(pk->type1());
    out.type2 = pk->type2() == pk->type1() ? std::string() : type(pk->type2());
    if (g >= 3)
    {
        out.nature = Upper(i18n::nature(pksm::Language::ENG, pk->nature()));
        out.ability = Upper(i18n::ability(pksm::Language::ENG, pk->ability()));
        out.ball = Upper(i18n::ball(pksm::Language::ENG, pk->ball()));
        out.metLevel = pk->metLevel();
    }
    if (pk->heldItem()) out.item = ItemName(pk->heldItem());
    out.ot = pk->otName();
    out.tid = pk->TID();
    if (g >= 2) out.gender = pk->gender() == pksm::Gender::Male ? "M" : pk->gender() == pksm::Gender::Female ? "F" : "";
    for (int i = 0; i < 4; i++)
    {
        const int m = (int)pk->move((u8)i);
        out.moves[i] = m ? MoveName(m) : std::string();
    }
    const pksm::Stat st[6] = { pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPD, pksm::Stat::SPATK, pksm::Stat::SPDEF };
    for (int i = 0; i < 6; i++)
    {
        out.stats[i] = pk->stat(st[i]);
        out.ivs[i] = pk->iv(st[i]);
        out.evs[i] = pk->ev(st[i]);
    }
    return true;
}

bool SetEvs(Save& save, int box, int slot, const int evs[6], std::string& error)
{
    auto pk = save.d->At(box, slot);
    if (!pk) { error = "THERE IS NO POKEMON THERE"; return false; }
    if (save.ReadOnly()) { error = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return false; }
    if (GenNumber(pk->generation()) < 3) { error = "GEN 1 AND 2 POKEMON HAVE NO EFFORT VALUES TO SET"; return false; }
    int total = 0;
    for (int i = 0; i < 6; i++)
    {
        if (evs[i] < 0 || evs[i] > 252) { error = "EACH IS 0 TO 252"; return false; }
        total += evs[i];
    }
    if (total > 510) { error = "510 IN ALL AT MOST"; return false; }
    const pksm::Stat st[6] = { pksm::Stat::HP, pksm::Stat::ATK, pksm::Stat::DEF, pksm::Stat::SPD, pksm::Stat::SPATK, pksm::Stat::SPDEF };
    for (int i = 0; i < 6; i++) pk->ev(st[i], (u16)evs[i]);
    if (box == PARTY) pk->updatePartyData();
    pk->refreshChecksum();
    Put(save, *pk, box, slot);
    return true;
}

bool Relocate(Save& save, int box, int slot, int toBox, int toSlot, std::string& error)
{
    auto pk = save.d->At(box, slot);
    if (!pk) { error = "THERE IS NO POKEMON THERE"; return false; }
    if (save.ReadOnly()) { error = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return false; }
    if (box == toBox && slot == toSlot) return true;
    if (toBox == PARTY && (save.IsBank() || toSlot != (int)save.d->sav->partyCount())) { error = "A PARTY FILLS FROM THE FIRST PLACE"; return false; }
    if (toBox != PARTY && save.d->At(toBox, toSlot)) { error = "THAT PLACE IS TAKEN"; return false; }
    if (toBox != PARTY && (toBox < 0 || toBox >= save.Boxes() || toSlot < 0 || toSlot >= 30)) { error = "NO SUCH PLACE"; return false; }
    if (box == PARTY && toBox != PARTY && LastInParty(save, box, slot)) { error = "IT IS THE LAST POKEMON IN YOUR PARTY: A GAME NEEDS ONE"; return false; }
    auto kept = box == PARTY ? BoxForm(*pk) : pk->clone();
    if (!kept) { error = "IT CANNOT BE MOVED"; return false; }
    Take(save, box, slot);
    if (box == PARTY && toBox == PARTY && toSlot > slot) toSlot--; // (the party closed up)
    Put(save, *kept, toBox, toSlot);
    return true;
}

bool SetNickname(Save& save, int box, int slot, const std::string& name, std::string& error)
{
    auto pk = save.d->At(box, slot);
    if (!pk) { error = "THERE IS NO POKEMON THERE"; return false; }
    if (save.ReadOnly()) { error = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return false; }
    const int g = GenNumber(pk->generation());
    const size_t most = g <= 2 ? 10 : g == 3 ? 10 : 10;
    if (name.empty() || name.size() > most) { error = "1 TO 10 LETTERS"; return false; }
    pk->nickname(name);
    pk->nicknamed(Upper(name) != SpeciesName((int)pk->species()));
    pk->refreshChecksum();
    Put(save, *pk, box, slot);
    return true;
}

std::string SpeciesNameOf(int species) { return species > 0 ? SpeciesName(species) : std::string(); }
int GenerationOfSpecies(int n)
{
    return n <= 151 ? 1 : n <= 251 ? 2 : n <= 386 ? 3 : n <= 493 ? 4 : n <= 649 ? 5 : n <= 721 ? 6 : 7;
}

void AddToDex(const Save& save, std::vector<DexMark>& dex)
{
    if (dex.size() < (size_t)DEX_SPECIES + 1) dex.resize((size_t)DEX_SPECIES + 1);
    const std::string name = save.GameName();
    auto own = [&](const Mon& m) {
        if (m.egg || m.species <= 0 || m.species > DEX_SPECIES) return;
        DexMark& d = dex[(size_t)m.species];
        d.owned++;
        d.shiny = d.shiny || m.shiny;
        if (std::find(d.where.begin(), d.where.end(), name) == d.where.end()) d.where.push_back(name);
        // (Having one is having caught one, whatever a game's Pokédex says.)
        d.state = std::max(d.state, 2);
    };
    for (int box = 0; box < save.Boxes(); box++)
        for (const Mon& m : save.Box(box)) own(m);
    for (const Mon& m : Party(save)) own(m);
    if (save.IsBank()) return;
    const int top = std::min<int>(DEX_SPECIES, (int)save.d->sav->maxSpecies());
    for (int n = 1; n <= top; n++)
        dex[(size_t)n].state = std::max(dex[(size_t)n].state, save.d->sav->dexState(pksm::Species{(u16)n}));
}

bool Pack(const Save& save, int box, int slot, Packed& out, std::string& error)
{
    auto pk = save.d->At(box, slot);
    if (!pk) { error = "THERE IS NO POKEMON THERE"; return false; }
    auto bx = BoxForm(*pk);
    if (!bx) { error = "THIS POKEMON CANNOT BE SENT"; return false; }
    out.generation = GenNumber(bx->generation());
    out.mon = MonOf(*bx, box, slot);
    out.game = save.GameName();
    bx->encrypt();
    out.data.assign(bx->rawData().begin(), bx->rawData().end());
    return true;
}

namespace
{
std::unique_ptr<pksm::PKX> Unpack(const Packed& p)
{
    if (p.generation < 1 || p.generation > 7 || p.data.empty() || p.data.size() > 512) return nullptr;
    std::vector<u8> raw = p.data;
    auto pk = pksm::PKX::getPKM(GenOf(p.generation), raw.data(), raw.size());
    return pk && (int)pk->species() != 0 ? std::move(pk) : nullptr;
}
}

Check CanReceive(const Packed& p, const Save& to, int tradedFor)
{
    Check c;
    auto pk = Unpack(p);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "WHAT ARRIVED IS NOT A POKEMON THE HUB CAN READ"; return c; }
    ArrivingPk(*pk, p.generation, false, to, c, true, tradedFor);
    return c;
}

bool Receive(Save& to, const Packed& p, bool evolve, int tradedFor, std::string& error, int toBox, int toSlot)
{
    auto src = Unpack(p);
    if (!src) { error = "WHAT ARRIVED IS NOT A POKEMON THE HUB CAN READ"; return false; }
    Check c;
    auto pk = ArrivingPk(*src, p.generation, false, to, c, false, tradedFor);
    if (!pk) { error = c.reason; return false; }
    if (!Place(to, toBox, toSlot)) { error = "THERE IS NO ROOM THERE"; return false; }
    if (evolve && !to.IsBank())
        if (const TradeEvo* e = EvolutionFor(*pk, *to.d->sav, tradedFor)) Evolve(*pk, *e, to.Generation());
    pk->refreshChecksum();
    Put(to, *pk, toBox, toSlot);
    return true;
}

Check CanRemove(const Save& save, int box, int slot)
{
    Check c;
    if (!save.d->At(box, slot)) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return c; }
    if (save.ReadOnly()) { c.verdict = Verdict::ReadOnly; c.reason = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return c; }
    if (LastInParty(save, box, slot)) { c.verdict = Verdict::LastInParty; c.reason = "IT IS THE LAST POKEMON IN YOUR PARTY: A GAME NEEDS ONE"; return c; }
    c.verdict = Verdict::Ok;
    return c;
}

bool Remove(Save& save, int box, int slot, std::string& error)
{
    const Check c = CanRemove(save, box, slot);
    if (c.verdict != Verdict::Ok) { error = c.reason; return false; }
    Take(save, box, slot);
    return true;
}

}
