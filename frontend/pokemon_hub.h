#pragma once
// The Pokémon hub: moving Pokémon between the player's own games, by the rules the real games
// kept (Pal Park, Poké Transfer, Poké Transporter and Pokémon Bank, and trades within a
// generation). Saves are read and written with PKSM-Core (FlagBrew, GPL-3.0, much of it from
// PKHeX), in third_party/PKSM-Core.
//
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

struct Mon
{
    int box = -1, slot = -1;             // 0-based
    int species = 0, form = 0, level = 0;
    std::string nickname, trainer;
    std::string speciesName;             // in English capitals: "SWAMPERT"
    bool shiny = false, egg = false;
    int heldItem = 0;
};

class Save
{
public:
    ~Save();
    // romHack: the game is a ROM hack (Ultra Violet): read-only until its format is known.
    static std::unique_ptr<Save> Open(const std::string& path, std::string& error, bool romHack = false);
    const std::string& Path() const;
    int Generation() const;              // 3 to 7
    std::string GameName() const;        // "HeartGold / SoulSilver", "Black 2", "Omega Ruby"
    std::string Trainer() const;
    bool ReadOnly() const;
    int Boxes() const;
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

}
