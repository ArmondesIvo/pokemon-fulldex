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
    if (g == Generation::THREE) return 3;
    if (g == Generation::FOUR) return 4;
    if (g == Generation::FIVE) return 5;
    if (g == Generation::SIX) return 6;
    if (g == Generation::SEVEN) return 7;
    return 0; // (the first two generations, Let's Go, and the Switch games: not Fulldex's)
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

const TradeEvo* EvolutionFor(const pksm::PKX& pk, const pksm::Sav& to)
{
    if (pk.egg() || pk.heldItem() == kEverstone) return nullptr;
    const int species = (int)pk.species(), item = pk.heldItem();
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
    std::unique_ptr<pksm::Sav> sav;
    bool readOnly = false, changed = false;

    std::unique_ptr<pksm::PKX> At(int box, int slot) const
    {
        if (box < 0 || box >= sav->maxBoxes() || slot < 0 || slot >= 30) return nullptr;
        auto pk = sav->pkm((u8)box, (u8)slot);
        return pk && (int)pk->species() != 0 ? std::move(pk) : nullptr;
    }
};

Save::Save() : d(new Impl) {}
Save::~Save() {}

std::unique_ptr<Save> Save::Open(const std::string& path, std::string& error, bool romHack)
{
    std::unique_ptr<Save> s(new Save);
    if (!ReadFile(path, s->d->original) || s->d->original.empty()) { error = "CANNOT READ THE SAVE"; return nullptr; }
    auto data = std::shared_ptr<u8[]>(new u8[s->d->original.size()]);
    std::copy(s->d->original.begin(), s->d->original.end(), data.get());
    s->d->sav = pksm::Sav::getSave(data, (u32)s->d->original.size());
    if (!s->d->sav || GenNumber(s->d->sav->generation()) == 0) { error = "THIS SAVE IS NOT ONE THE HUB KNOWS"; return nullptr; }
    s->d->path = path;
    s->d->readOnly = romHack;
    return s;
}

const std::string& Save::Path() const { return d->path; }
int Save::Generation() const { return GenNumber(d->sav->generation()); }
std::string Save::GameName() const { return GameNameOf(*d->sav); }
std::string Save::Trainer() const { return d->sav->otName(); }
bool Save::ReadOnly() const { return d->readOnly; }
int Save::Boxes() const { return d->sav->maxBoxes(); }
std::string Save::BoxName(int box) const { return d->sav->boxName((u8)box); }
bool Save::Changed() const { return d->changed; }

std::vector<Mon> Save::Box(int box) const
{
    std::vector<Mon> out;
    for (int slot = 0; slot < 30; slot++)
        if (auto pk = d->At(box, slot))
        {
            Mon m;
            m.box = box;
            m.slot = slot;
            m.species = (int)pk->species();
            m.form = pk->alternativeForm();
            m.level = pk->level();
            m.nickname = pk->nickname();
            m.speciesName = SpeciesName(m.species);
            m.trainer = pk->otName();
            m.shiny = pk->shiny();
            m.egg = pk->egg();
            m.heldItem = pk->heldItem();
            out.push_back(m);
        }
    return out;
}

bool Save::FirstEmpty(int& box, int& slot) const
{
    for (box = 0; box < d->sav->maxBoxes(); box++)
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
// The Pokémon as it would arrive, and the check of it.
std::unique_ptr<pksm::PKX> Arriving(const Save& from, int box, int slot, const Save& to, Check& c)
{
    auto pk = from.d->At(box, slot);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return nullptr; }
    if (from.ReadOnly() || to.ReadOnly())
    {
        c.verdict = Verdict::ReadOnly;
        c.reason = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET";
        return nullptr;
    }
    const int a = from.Generation(), b = to.Generation();
    if (b < a)
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
    if (const TradeEvo* e = EvolutionFor(*arrives, *to.d->sav))
    {
        c.evolvesTo = e->to;
        c.evolution = SpeciesName(e->from) + " CAN EVOLVE INTO " + SpeciesName(e->to) + " ON ARRIVAL, AS IF TRADED" +
                      (e->item ? " (USING ITS " + ItemName(e->item) + ")" : "");
    }
    int fb, fs;
    if (!to.FirstEmpty(fb, fs)) { c.verdict = Verdict::Full; c.reason = "THERE IS NO ROOM IN " + Upper(to.GameName()) + "'S BOXES"; return nullptr; }
    c.verdict = Verdict::Ok;
    return arrives;
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
    Check c;
    auto pk = Arriving(from, box, slot, to, c);
    if (!pk) { error = c.reason; return false; }
    if (toBox < 0 && !to.FirstEmpty(toBox, toSlot)) { error = "NO ROOM"; return false; }
    if (to.d->At(toBox, toSlot)) { error = "THAT PLACE IS TAKEN"; return false; }
    if (evolve)
        if (const TradeEvo* e = EvolutionFor(*pk, *to.d->sav)) Evolve(*pk, *e, to.Generation());
    pk->refreshChecksum();
    to.d->sav->pkm(*pk, (u8)toBox, (u8)toSlot, true);
    from.d->sav->pkm(*from.d->sav->emptyPkm(), (u8)box, (u8)slot, false);
    from.d->changed = to.d->changed = true;
    return true;
}

Check CanEvolve(const Save& save, int box, int slot)
{
    Check c;
    auto pk = save.d->At(box, slot);
    if (!pk) { c.verdict = Verdict::Empty; c.reason = "THERE IS NO POKEMON THERE"; return c; }
    if (save.ReadOnly()) { c.verdict = Verdict::ReadOnly; c.reason = "THE HUB CANNOT CHANGE THIS GAME'S SAVE YET"; return c; }
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
bool WriteSave(Save& s, std::string& error)
{
    s.d->sav->finishEditing();
    const bool ok = WriteAtomic(s.d->path, s.d->sav->rawData().get(), s.d->sav->getLength());
    s.d->sav->beginEditing();
    if (!ok) error = "CANNOT WRITE " + Upper(s.GameName()) + "'S SAVE";
    return ok;
}
}

bool Commit(Save& save, std::string& error, std::vector<std::string>* backups)
{
    if (!save.Changed()) return true;
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

}
