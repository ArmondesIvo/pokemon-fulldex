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
struct ShopItem { const char* name; int price, heal, revive; const char* what; };
inline const std::vector<ShopItem>& Shop()
{
    static const std::vector<ShopItem> s = {
        { "Po\xC3\xA7\xC3\xA3o", 300, 20, 0, "Recupera 20 PS." },
        { "Super Po\xC3\xA7\xC3\xA3o", 700, 60, 0, "Recupera 60 PS." },
        { "Hiper Po\xC3\xA7\xC3\xA3o", 1200, 120, 0, "Recupera 120 PS." },
        { "Po\xC3\xA7\xC3\xA3o M\xC3\xA1xima", 2500, -1, 0, "Recupera todos os PS." },
        { "Reviver", 1500, 0, 1, "Revive um desmaiado com metade dos PS." },
        { "Reviver M\xC3\xA1ximo", 4000, 0, 2, "Revive um desmaiado com todos os PS." } };
    return s;
}
}
