#pragma once
// The Pokémon hub: moving Pokémon between the player's own games, by the rules the real games
// kept (Pal Park, Poké Transfer, Poké Transporter and Pokémon Bank, and trades within a
// generation). Saves are read and written with PKSM-Core (FlagBrew, GPL-3.0, much of it from
// PKHeX), in third_party/PKSM-Core.
//
//   Gen 1  Red, Blue, Yellow           (Fulldex's own: these two trade both ways, by the Time
//   Gen 2  Gold, Silver, Crystal        Capsule, and only ever went on to Gen 7)
//   Gen 3  Ruby, Sapphire, Emerald, FireRed, LeafGreen
//   Gen 4  Diamond, Pearl, Platinum, HeartGold, SoulSilver
//   Gen 5  Black, White, Black 2, White 2
//   Gen 6  X, Y, Omega Ruby, Alpha Sapphire
//   Gen 7  Sun, Moon, Ultra Sun, Ultra Moon
//
// Within a generation a Pokémon goes either way, as in a trade; between generations only
// forward, as the real games allowed. A Pokémon is moved, never copied: it leaves the first
// game as it arrives in the second. Before either save is written, both are copied into the
// backups folder beside them, where RESTORE BACKUP finds them.
//
// Nothing here draws anything: the launcher's screens (to come) ask, show, and confirm.
#include <memory>
#include <string>
#include <vector>

namespace PokemonHub
{

const int BANK_BOXES = 50;

struct Mon
{
    int box = -1, slot = -1;             // 0-based
    int species = 0, form = 0, level = 0;
    std::string nickname, trainer;
    std::string speciesName;             // in English capitals: "SWAMPERT"
    bool shiny = false, egg = false;
    int heldItem = 0;
    int generation = 0;                  // the format it is kept in (1 to 7)
    // Its origin mark, as Pokémon HOME shows it, from the game it came from: 0 none (Gens 3 to 5
    // have none), 1 Game Boy (Red, Blue, Yellow, Gold, Silver, Crystal), 2 the pentagon (X, Y,
    // Omega Ruby, Alpha Sapphire), 3 the clover (Sun, Moon, Ultra Sun, Ultra Moon), 4 GO.
    int originMark = 0;
};

class Save
{
public:
    ~Save();
    // romHack: the game is a ROM hack (Ultra Violet): read-only until its format is known.
    static std::unique_ptr<Save> Open(const std::string& path, std::string& error, bool romHack = false);
    // The Bank: boxes of the player's own, outside every game, as PKVault and Pokémon Home keep.
    // A folder of BANK_BOXES boxes (box-01 ... ), each Pokémon a file in its own game's format as
    // PKHeX writes it (05.pk3, 12.pk5: its slot and generation), so PKHeX and PKVault read them too.
    // A Pokémon goes in from any game as it is, and out to a game of its generation or a newer one
    // (converted then, as the games did). No party, no trainer; Generation() 0.
    static std::unique_ptr<Save> OpenBank(const std::string& folder, std::string& error);
    bool IsBank() const;
    const std::string& Path() const;
    int Generation() const;              // 1 to 7 (the Bank: 0)
    std::string GameName() const;        // "HeartGold / SoulSilver", "Black 2", "Omega Ruby"
    std::string Trainer() const;
    bool ReadOnly() const;
    int Boxes() const;
    int BoxSlots() const;                // places in a box: 30 (20 in Gen 1 and 2)
    std::string BoxName(int box) const;
    std::vector<Mon> Box(int box) const; // the Pokémon in it (empty slots left out)
    bool FirstEmpty(int& box, int& slot) const;
    bool Changed() const;

    struct Impl;
    std::unique_ptr<Impl> d;

private:
    Save();
};

enum class Verdict
{
    Ok,
    Backwards,      // to an older generation: the real games never allowed it
    NotInGame,      // its species, form, a move, ability, item or ball is not in that game
    Egg,            // eggs only move within a generation
    Empty,          // no Pokémon there
    ReadOnly,       // a ROM hack's save, or a format not written yet
    Full,           // no free box space in the other game
    LastInParty,    // the party's last Pokémon: a game needs one
    Taken,          // that place has a Pokémon in it
    NoEvolution,    // (CanEvolve) not one that evolves by trading, or not holding what it needs
};

struct Check
{
    Verdict verdict = Verdict::Empty;
    std::string reason;                  // for the player, when not Ok
    std::vector<std::string> losses;     // what it gives up on the way ("FORGETS SURF (HM)")
    int evolvesTo = 0;                   // a trade evolution it could take (0: none)
    std::string evolution;               // "KADABRA WILL EVOLVE INTO ALAKAZAM"
};

// The Pokédex of all the player's games and the Bank together (as PKVault's): for each species
// (index = National Dex number, 0 unused), the most any game has (0 not seen, 1 seen, 2 caught by
// its Pokédex), how many the player has now in boxes, parties and the Bank, and a shiny among them.
struct DexMark
{
    int state = 0;
    int owned = 0;
    bool shiny = false;
    std::vector<std::string> where;      // the games (and "Bank") that have one now
};
const int DEX_SPECIES = 809;             // up to the 7th generation's last
// Adds a save's (or the Bank's) Pokédex and Pokémon to dex (sized DEX_SPECIES + 1 when empty).
void AddToDex(const Save& save, std::vector<DexMark>& dex);
std::string SpeciesNameOf(int species);  // "BULBASAUR"
int GenerationOfSpecies(int species);    // 1 to 7
void SpeciesTypes(int species, std::string& type1, std::string& type2); // "GRASS", "POISON" (type2 "" when one)
// A species' base stats (HP, Atk, Def, Spe, SpA, SpD) and types (0 Normal .. 17 Fairy; -1 none), as Sun
// and Moon have them: the arena's bot builds any species from them.
bool SpeciesBase(int species, int base[6], int types[2]);

// One Pokémon as the MY PC tab's details show it (PKVault's): its kinds, nature, ability, moves,
// stats and where it comes from, in English. Gen 1 and 2 have no nature or ability ("").
struct MonDetail
{
    Mon mon;
    std::string type1, type2, nature, ability, item, ball, ot, gender; // gender "M", "F" or ""
    std::string origin;                  // the game it came from ("FireRed"), "" when not kept
    int tid = 0, metLevel = 0;
    std::string moves[4];
    int stats[6] = {}, ivs[6] = {}, evs[6] = {}; // HP, Attack, Defense, Speed, Sp. Atk, Sp. Def
    // (For the arena battles: the moves' numbers, the types' (0 Normal .. 17 Fairy; -1 none) and
    // its stats as they would be at level 50, its IVs, EVs and nature kept.)
    int moveIds[4] = {}, typeIds[2] = { -1, -1 }, stats50[6] = {};
};
bool Detail(const Save& save, int box, int slot, MonDetail& out);
// PKVault's Edit: a Pokémon's effort values (each 0 to 252, 510 in all) and its nickname (in memory,
// until Commit). Not in the first two generations (their stat experience is another thing).
bool SetEvs(Save& save, int box, int slot, const int evs[6], std::string& error);
// Within one game (or the Bank): a Pokémon to a free place of its own boxes or party.
bool Relocate(Save& save, int box, int slot, int toBox, int toSlot, std::string& error);
bool SetNickname(Save& save, int box, int slot, const std::string& name, std::string& error);

// What would happen if the Pokémon at from's box/slot went to `to`.
Check CanMove(const Save& from, int box, int slot, const Save& to);

// Moves it (in memory only, until Commit). evolve: take the trade evolution if there is one.
// toBox/toSlot -1: the first free place.
bool Move(Save& from, int box, int slot, Save& to, bool evolve, std::string& error, int toBox = -1, int toSlot = -1);

// EVOLVE AS IF TRADED, without moving it: a shortcut the real games never had, so the launcher
// offers it only when the player has turned it on. Same rules as a trade: the right held item
// (used up), and an Everstone stops it. (Karrablast and Shelmet need each other: not this way.)
Check CanEvolve(const Save& save, int box, int slot);
bool Evolve(Save& save, int box, int slot, std::string& error);
// Writes one changed save, backed up first.
bool Commit(Save& save, std::string& error, std::vector<std::string>* backups = nullptr);

// Writes the changed saves, each backed up first; when the second cannot be written, the first
// is put back, so a Pokémon is never in both games or in neither.
bool Commit(Save& a, Save& b, std::string& error, std::vector<std::string>* backups = nullptr);


// ---- Added for the launcher's Move Pokémon page and trades -------------------------------------

// A Pokémon's place: a box and its slot, or the party (box PARTY, slot 0 to 5). As a place to go,
// box ANY_BOX means the first free place in the boxes, TO_PARTY the end of the party.
const int PARTY = -1;
const int ANY_BOX = -1;
const int TO_PARTY = -2;

// The party (box PARTY in each Mon), and whether a place is free.
std::vector<Mon> Party(const Save& save);
bool Free(const Save& save, int box, int slot);
bool PartyFull(const Save& save);

// Moving to a chosen place (CanMove above checks the Pokémon; this, the place too).
Check CanMoveTo(const Save& from, int box, int slot, const Save& to, int toBox, int toSlot);

// SWAP: one Pokémon each way between two of the player's games, as a trade (so both games must
// let the other's in: in practice the same generation). Karrablast and Shelmet traded for each
// other both evolve, as they do in Black and White.
Check CanSwap(const Save& a, int boxA, int slotA, const Save& b, int boxB, int slotB);
bool Swap(Save& a, int boxA, int slotA, Save& b, int boxB, int slotB, bool evolve, std::string& error);

// A Pokémon as sent to another device (a friend's trade, or the player's own other device):
// its generation and bytes, and what it is, to show before it is accepted.
struct Packed
{
    int generation = 0;
    std::vector<uint8_t> data;
    Mon mon;
    std::string game; // the game it comes from
};
bool Pack(const Save& save, int box, int slot, Packed& out, std::string& error);
// tradedFor: the species going the other way in a trade (0: none), for Karrablast and Shelmet.
Check CanReceive(const Packed& p, const Save& to, int tradedFor = 0);
bool Receive(Save& to, const Packed& p, bool evolve, int tradedFor, std::string& error, int toBox = ANY_BOX, int toSlot = -1);
// Takes it out (once the other side has it). Not the last Pokémon of the party.
Check CanRemove(const Save& save, int box, int slot);
bool Remove(Save& save, int box, int slot, std::string& error);
// A Pokémon from its bytes as Pack made them (an online trade's): what it is, and its details.
bool PackedFrom(int generation, const std::vector<uint8_t>& data, Packed& out);
bool PackedDetail(const Packed& p, MonDetail& out);

}
