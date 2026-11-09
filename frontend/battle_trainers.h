#pragma once
// The arena's trainers: each region's eight gym leaders, Elite Four and champion, with their teams
// as the games had them (the species only: the arena gives them moves, Arena::BotMon), and the
// everyday trainers a free battle may draw. Their pictures are Pokémon Showdown's trainer sprites,
// kept in PokeHub's bucket under trainers/<sprite>.png (tools/battle/upload_art.sh).
// (Species without a moving picture, Arena::NoPicture, are left out: Zebstrika, Throh, Bouffalant.)
#include "battle_engine.h"

namespace Arena
{
struct Trainer
{
    const char* name;
    const char* sprite;   // its picture's name in trainers/
    int type;             // the type it is known for (-1 none)
    int team[6];          // species, 0 after the last
};
struct Region
{
    const char* name;
    Trainer gyms[8];
    Trainer elite[5];     // the Elite Four, then the champion
};
inline const std::vector<Region>& Regions()
{
    static const std::vector<Region> r = {
        { "Kanto",
          { { "Brock", "brock", T_ROCK, { 74, 95, 75 } },
            { "Misty", "misty", T_WATER, { 120, 54, 121 } },
            { "Lt. Surge", "ltsurge", T_ELECTRIC, { 100, 25, 26 } },
            { "Erika", "erika", T_GRASS, { 71, 114, 45 } },
            { "Koga", "koga", T_POISON, { 109, 89, 109, 110 } },
            { "Sabrina", "sabrina", T_PSYCHIC, { 64, 122, 49, 65 } },
            { "Blaine", "blaine", T_FIRE, { 58, 77, 78, 59 } },
            { "Giovanni", "giovanni", T_GROUND, { 111, 51, 31, 34, 112 } } },
          { { "Lorelei", "lorelei", T_ICE, { 87, 91, 80, 124, 131 } },
            { "Bruno", "bruno", T_FIGHTING, { 95, 107, 106, 95, 68 } },
            { "Agatha", "agatha", T_GHOST, { 94, 42, 93, 24, 94 } },
            { "Lance", "lance", T_DRAGON, { 130, 148, 148, 142, 149 } },
            { "Blue", "blue", -1, { 18, 65, 112, 103, 59, 9 } } } },
        { "Johto",
          { { "Falkner", "falkner", T_FLYING, { 16, 17, 164 } },
            { "Bugsy", "bugsy", T_BUG, { 11, 14, 123 } },
            { "Whitney", "whitney", T_NORMAL, { 35, 241 } },
            { "Morty", "morty", T_GHOST, { 92, 93, 93, 94 } },
            { "Chuck", "chuck", T_FIGHTING, { 57, 62 } },
            { "Jasmine", "jasmine", T_STEEL, { 81, 81, 208 } },
            { "Pryce", "pryce", T_ICE, { 86, 87, 221 } },
            { "Clair", "clair", T_DRAGON, { 148, 148, 148, 230 } } },
          { { "Will", "will", T_PSYCHIC, { 178, 124, 103, 80, 178 } },
            { "Koga", "koga", T_POISON, { 168, 49, 205, 89, 169 } },
            { "Bruno", "bruno", T_FIGHTING, { 237, 106, 107, 95, 68 } },
            { "Karen", "karen", T_DARK, { 197, 45, 94, 198, 229 } },
            { "Lance", "lance", T_DRAGON, { 130, 149, 149, 142, 6, 149 } } } },
        { "Hoenn",
          { { "Roxanne", "roxanne", T_ROCK, { 74, 74, 299 } },
            { "Brawly", "brawly", T_FIGHTING, { 66, 307, 296 } },
            { "Wattson", "wattson", T_ELECTRIC, { 81, 100, 82, 310 } },
            { "Flannery", "flannery", T_FIRE, { 322, 218, 323, 324 } },
            { "Norman", "norman", T_NORMAL, { 327, 288, 264, 289 } },
            { "Winona", "winona", T_FLYING, { 333, 357, 279, 227, 334 } },
            { "Tate e Liza", "tate", T_PSYCHIC, { 178, 344, 337, 338 } },
            { "Juan", "juan", T_WATER, { 370, 340, 364, 342, 230 } } },
          { { "Sidney", "sidney", T_DARK, { 262, 275, 332, 342, 359 } },
            { "Phoebe", "phoebe", T_GHOST, { 356, 354, 354, 302, 356 } },
            { "Glacia", "glacia", T_ICE, { 364, 362, 364, 362, 365 } },
            { "Drake", "drake", T_DRAGON, { 372, 334, 330, 330, 373 } },
            { "Wallace", "wallace", T_WATER, { 321, 73, 272, 340, 130, 350 } } } },
        { "Sinnoh",
          { { "Roark", "roark", T_ROCK, { 74, 95, 408 } },
            { "Gardenia", "gardenia", T_GRASS, { 387, 421, 407 } },
            { "Fantina", "fantina", T_GHOST, { 355, 93, 429 } },
            { "Maylene", "maylene", T_FIGHTING, { 307, 67, 448 } },
            { "Crasher Wake", "crasherwake", T_WATER, { 130, 195, 419 } },
            { "Byron", "byron", T_STEEL, { 82, 208, 411 } },
            { "Candice", "candice", T_ICE, { 215, 221, 460, 478 } },
            { "Volkner", "volkner", T_ELECTRIC, { 135, 26, 405, 466 } } },
          { { "Aaron", "aaron", T_BUG, { 269, 214, 416, 267, 452 } },
            { "Bertha", "bertha", T_GROUND, { 340, 472, 450, 76, 464 } },
            { "Flint", "flint", T_FIRE, { 229, 136, 78, 392, 467 } },
            { "Lucian", "lucian", T_PSYCHIC, { 122, 196, 437, 65, 475 } },
            { "Cynthia", "cynthia", -1, { 442, 407, 468, 448, 350, 445 } } } },
        { "Unova",
          { { "Cilan", "cilan", T_GRASS, { 506, 511 } },
            { "Lenora", "lenora", T_NORMAL, { 507, 505 } },
            { "Burgh", "burgh", T_BUG, { 544, 557, 542 } },
            { "Elesa", "elesa", T_ELECTRIC, { 587, 180, 596 } },
            { "Clay", "clay", T_GROUND, { 552, 536, 530 } },
            { "Skyla", "skyla", T_FLYING, { 528, 521, 581 } },
            { "Brycen", "brycen", T_ICE, { 583, 615, 614 } },
            { "Drayden", "drayden", T_DRAGON, { 611, 621, 612 } } },
          { { "Shauntal", "shauntal", T_GHOST, { 563, 593, 623, 609 } },
            { "Grimsley", "grimsley", T_DARK, { 560, 510, 553, 625 } },
            { "Caitlin", "caitlin", T_PSYCHIC, { 579, 518, 561, 576 } },
            { "Marshal", "marshal", T_FIGHTING, { 539, 620, 534, 68 } },
            { "Alder", "alder", -1, { 617, 628, 621, 584, 589, 637 } } } },
    };
    return r;
}
// RED, on Mt. Silver as in HeartGold and SoulSilver: open with Johto's eight badges.
inline const Trainer& Red() { static const Trainer r = { "Red", "red", -1, { 25, 131, 143, 3, 6, 9 } }; return r; }
constexpr int RED_LEVEL = 50, RED_PRIZE = 10000;
// The way through, as the games go: a region's gyms open once the one before's Elite Four is beaten
// (Kanto's from the start), its Elite Four with all its eight badges.
// Everyday trainers (a free battle's): their class, as the Portuguese games name it, and picture.
struct Everyday { const char* title; const char* sprite; };
inline const std::vector<Everyday>& EverydayTrainers()
{
    static const std::vector<Everyday> e = {
        { "Treinador \xC3\x81s", "acetrainer" }, { "Treinadora \xC3\x81s", "acetrainerf" }, { "Jovem", "youngster" }, { "Garota", "lass" },
        { "Montanhista", "hiker" }, { "Ca\xC3\xA7" "ador de Insetos", "bugcatcher" }, { "Nadador", "swimmer" }, { "Nadadora", "swimmerf" },
        { "Faixa Preta", "blackbelt" }, { "Beldade", "beauty" }, { "M\xC3\xA9" "dium", "psychic" }, { "Marinheiro", "sailor" },
        { "Pescador", "fisherman" }, { "Cavalheiro", "gentleman" }, { "Cientista", "scientist" }, { "Recruta Rocket", "rocketgrunt" },
        { "Recruta Rocket", "rocketgruntf" }, { "Campista", "picnicker" }, { "Campista", "camper" }, { "F\xC3\xA3 de Pok\xC3\xA9mon", "pokefan" },
        { "Criador", "pokemonbreeder" }, { "Criadora", "pokemonbreederf" }, { "Veterano", "veteran" }, { "Lutadora", "battlegirl" },
        { "Criador de P\xC3\xA1ssaros", "birdkeeper" }, { "Ciclista", "cyclist" }, { "Hex Maniac", "hexmaniac" }, { "Ninja", "ninjaboy" },
        { "Dama", "parasollady" }, { "Ranger", "pokemonranger" }, { "Ranger", "pokemonrangerf" }, { "Ru\xC3\xADnas Man\xC3\xAD" "aco", "ruinmaniac" },
        { "Estudante", "schoolkid" }, { "G\xC3\xAAmeas", "twins" } };
    return e;
}
inline const char* RandomFirstName(unsigned n)
{
    static const char* names[] = { "Rafael", "Bruna", "Diego", "Larissa", "Thiago", "Camila", "Lucas", "Mariana", "Felipe", "Juliana",
        "Gustavo", "Beatriz", "Rodrigo", "Fernanda", "Vin\xC3\xAD" "cius", "Carla", "Mateus", "Aline", "Pedro", "Let\xC3\xAD" "cia" };
    return names[n % (sizeof names / sizeof *names)];
}
// Everyone fights at level 50, the trainers as the player's (as the player asked).
inline int GymLevel(int) { return 50; }
inline int EliteLevel(int) { return 50; }
// Money: what a win pays, and what a loss costs (half of it), as the games' prize money.
inline int GymPrize(int i) { return 600 + 300 * i; }
inline int ElitePrize(int i) { return i < 4 ? 2500 : 6000; }
constexpr int FREE_PRIZE = 300, GYM_TRAINER_PRIZE = 300;
// The shop: what it sells, at what price, and what each does (heal: points, -1 all; revive: 0
// none, 1 half, 2 all).
struct ShopItem { const char* name; int price, heal, revive; const char* what; const char* icon; }; // icon: items/<icon>.png
inline const std::vector<ShopItem>& Shop()
{
    static const std::vector<ShopItem> s = {
        { "Po\xC3\xA7\xC3\xA3o", 300, 20, 0, "Recupera 20 PS.", "potion" },
        { "Super Po\xC3\xA7\xC3\xA3o", 700, 60, 0, "Recupera 60 PS.", "super-potion" },
        { "Hiper Po\xC3\xA7\xC3\xA3o", 1200, 120, 0, "Recupera 120 PS.", "hyper-potion" },
        { "Po\xC3\xA7\xC3\xA3o M\xC3\xA1xima", 2500, -1, 0, "Recupera todos os PS.", "max-potion" },
        { "Reviver", 1500, 0, 1, "Revive um desmaiado com metade dos PS.", "revive" },
        { "Reviver M\xC3\xA1ximo", 4000, 0, 2, "Revive um desmaiado com todos os PS.", "max-revive" } };
    return s;
}
// MEGA EVOLUTION: those with a moving picture in SpriteCollab (29 of the 48), each by its stone.
// form: the Mega's form as Sun and Moon number it (its stats: SpeciesFormBase); sprite: its folder
// in SpriteCollab (the moving picture NNNN_FFFF.pmd); held: the stone's English name, as a 6th or
// 7th generation save holds it; icon: items/<icon>.png.
struct MegaInfo { int species, form, sprite; const char* stone; const char* held; const char* icon; const char* suffix; };
inline const std::vector<MegaInfo>& Megas()
{
    static const std::vector<MegaInfo> m = {
        { 6, 1, 1, "Charizardita X", "CHARIZARDITE X", "charizardite-x", " X" }, { 65, 1, 1, "Alakazita", "ALAKAZITE", "alakazite", "" },
        { 80, 1, 2, "Slowbronita", "SLOWBRONITE", "slowbronite", "" }, { 94, 1, 1, "Gengarita", "GENGARITE", "gengarite", "" },
        { 115, 1, 1, "Kangaskhanita", "KANGASKHANITE", "kangaskhanite", "" }, { 142, 1, 1, "Aerodactylita", "AERODACTYLITE", "aerodactylite", "" },
        { 150, 2, 2, "Mewtwonita Y", "MEWTWONITE Y", "mewtwonite-y", " Y" }, { 208, 1, 1, "Steelixita", "STEELIXITE", "steelixite", "" },
        { 229, 1, 1, "Houndoominita", "HOUNDOOMINITE", "houndoominite", "" }, { 248, 1, 1, "Tyranitarita", "TYRANITARITE", "tyranitarite", "" },
        { 254, 1, 1, "Sceptilita", "SCEPTILITE", "sceptilite", "" }, { 282, 1, 1, "Gardevoirita", "GARDEVOIRITE", "gardevoirite", "" },
        { 302, 1, 1, "Sablenita", "SABLENITE", "sablenite", "" }, { 303, 1, 1, "Mawilita", "MAWILITE", "mawilite", "" },
        { 308, 1, 1, "Medichamita", "MEDICHAMITE", "medichamite", "" }, { 310, 1, 1, "Manectita", "MANECTITE", "manectite", "" },
        { 319, 1, 1, "Sharpedonita", "SHARPEDONITE", "sharpedonite", "" }, { 323, 1, 1, "Cameruptita", "CAMERUPTITE", "cameruptite", "" },
        { 334, 1, 1, "Altarianita", "ALTARIANITE", "altarianite", "" }, { 354, 1, 1, "Banettita", "BANETTITE", "banettite", "" },
        { 359, 1, 1, "Absolita", "ABSOLITE", "absolite", "" }, { 362, 1, 1, "Glalitita", "GLALITITE", "glalitite", "" },
        { 380, 1, 1, "Latiasita", "LATIASITE", "latiasite", "" }, { 381, 1, 1, "Latiosita", "LATIOSITE", "latiosite", "" },
        { 384, 1, 1, "Meteorito", "METEORITE", "meteorite", "" }, { 428, 1, 1, "Lopunnita", "LOPUNNITE", "lopunnite", "" },
        { 448, 1, 1, "Lucarionita", "LUCARIONITE", "lucarionite", "" }, { 475, 1, 1, "Galladita", "GALLADITE", "galladite", "" },
        { 719, 1, 1, "Diancita", "DIANCITE", "diancite", "" } };
    return m;
}
constexpr int MEGA_PRICE = 8000;
inline int MegaFor(int species) { const auto& m = Megas(); for (int i = 0; i < (int)m.size(); i++) if (m[i].species == species) return i; return -1; }
// THE MOVES' LOOKS: PMD Origins' effects (github.com/PMDCollab/RawAsset, Particle and Beam), kept in
// PokeHub's bucket under fx/ (tools/battle/upload_art.sh); each sheet its frames side by side,
// square (a shot's "#a-b": only those frames, looped). Per type: the shot flying, its hit, a ULT's blast, and the beam a ULT fires (a Beam
// folder: <beam>_Body.png, the beam across, a frame a column of pixels; <beam>_Head.png, its end).
struct TypeFx { const char* shot; const char* hit; const char* blast; const char* beam; };
inline const TypeFx& FxOf(int type)
{
    static const TypeFx t[18] = {
        { "Swift_Star.None", "Hit_Neutral.None", "Explosion.None", "beam_Column_White" },                 // Normal
        { "Aura_Sphere.None", "Force_Palm_Hit.None", "Focus_Blast_Hit.None", "beam_Column_Tan" },          // Fighting
        { "Razor_Wind.None", "Air_Slash_Slash.None", "Gust.None", "beam_Column_White" },                   // Flying
        { "Poison_Ball_Ranger.None", "Gunk_Shot_Hit.None", "Gunk_Shot_Hit.None", "beam_Column_Purple" },   // Poison
        { "Mud_Shot_Ball.None#0-0", "Mud_Bomb_Hit.None", "Earthquake_Ranger.None", "beam_Column_Brown" },      // Ground
        { "Stone_Edge_Rock.None#0-3", "Rock_Wrecker_Hit.None", "Rock_Wrecker_Hit.None", "beam_Column_Tan" },   // Rock
        { "Bubbles_Green.None#3-5", "Bug_Bite.None", "Bug_Buzz.None", "beam_Column_Green" },                   // Bug
        { "Shadow_Ball.None", "Shadow_Claw_Hit.None", "Shadow_Force_Hit.None", "beam_Column_Purple" },     // Ghost
        { "Mirror_Shot.None#9-11", "Gyro_Ball_Hit.None", "Gyro_Ball_Hit.None", "beam_Column_White" },           // Steel
        { "Ember.None", "Fire_Fang_Hit.None", "Explosion.None", "beam_Column_Red" },                       // Fire
        { "Bubble_Pop_Blue.None#6-10", "Hydro_Cannon.None", "Hydro_Cannon.None", "beam_Column_Blue" },          // Water
        { "Energy_Ball.None", "Energy_Ball_Hit.None", "Leaf_Storm_Hit.None", "beam_Column_Green" },         // Grass
        { "Charge_Beam_Particle.None#4-7", "Charge_Beam_Hit.None", "Discharge_Hit.None", "beam_Column_Yellow" }, // Electric
        { "Mist_Ball.None", "Psycho_Shift_Hit.None", "Psycho_Shift_Hit.None", "beam_Column_Pink" },        // Psychic
        { "Ice_Ball.None", "Ice_Shard_Hit.None", "Ice_Shard_Hit.None", "beam_Beam_2" },                    // Ice
        { "Dragon_Pulse_Charge.None", "Draco_Meteor_Explosion.None", "Draco_Meteor_Explosion.None", "beam_Column_Purple" }, // Dragon
        { "Dark_Pulse_Particle.None#0-0", "Night_Slash_Hit.None", "Dark_Pulse_Front.None", "beam_Column_Black" }, // Dark
        { "Lucky_Chant_Star_Violet.None#0-3", "Moonlight_Sparkles.None", "Psycho_Shift_Hit.None", "beam_Column_Pink" } }; // Fairy
    return t[type >= 0 && type < 18 ? type : 0];
}
// A move's own shot, when it has one; and what a charging ULT shows (Solar Beam's sunlight).
inline const char* MoveShot(int move)
{
    switch (move)
    {
    case 75: return "Razor_Leaf.None";            // Razor Leaf
    case 345: return "Magical_Leaf.None";         // Magical Leaf
    case 52: case 172: return "Ember.None";       // Ember, Flame Wheel
    case 188: case 482: return "Poison_Ball_Ranger.None";
    case 129: return "Swift_Star.None";
    default: return nullptr;
    }
}
inline const char* MoveCharge(int move) { return move == 76 || move == 669 ? "Solar_Beam_Charge.None" : nullptr; }
// LEGENDARIES and MYTHICALS: never in an everyday trainer's team.
inline bool Legendary(int sp)
{
    static const int l[] = { 144, 145, 146, 150, 151, 243, 244, 245, 249, 250, 251, 377, 378, 379, 380, 381, 382, 383, 384, 385, 386,
        480, 481, 482, 483, 484, 485, 486, 487, 488, 489, 490, 491, 492, 493, 494, 638, 639, 640, 641, 642, 643, 644, 645, 646, 647,
        648, 649, 716, 717, 718, 719, 720, 721, 772, 773, 785, 786, 787, 788, 789, 790, 791, 792, 793, 794, 795, 796, 797, 798, 799,
        800, 801, 802, 803, 804, 805, 806, 807, 808, 809 };
    for (int x : l) if (x == sp) return true;
    return false;
}
}
