#!/usr/bin/env python3
"""The arena's famous trainers with their teams AND moves as the games have them
(frontend/battle_trainer_teams.h), from the pret decompilations and, for Unova, Serebii's pages
(tools/battle/unova.json, copied by hand).

Usage: gen_trainer_teams.py <data folder> <out.h>
The data folder holds, as downloaded (raw.githubusercontent.com/pret/...):
    hg_trainers.json      pokeheartgold files/poketool/trainer/trainers.json
    pokefirered.h         pokefirered   src/data/trainer_parties.h
    pokeemerald.h         pokeemerald   src/data/trainer_parties.h
    plat/<name>.json      pokeplatinum  res/trainers/data/<name>.json
Which version, as the player chose (09/11/2026): gym leaders their REMATCH team (HeartGold and
SoulSilver's for Kanto and Johto, Emerald's fifth, Platinum's), Koga and Giovanni FireRed's (they
are not Kanto's leaders in HGSS), the Elite Four and champions their first team (Kanto's
FireRed's, Johto's HGSS's, Hoenn's Emerald's, Sinnoh's Platinum's, Unova's Black's), Red HGSS's.
Unova's leaders have no rematch: left out (the arena chooses their moves by type).
"""
import json, os, re, sys

HERE = os.path.dirname(os.path.abspath(__file__))
data, out = sys.argv[1], sys.argv[2]


def norm(s):
    return re.sub(r"[^A-Z0-9]", "", s.upper().replace("♀", "F").replace("♂", "M"))


# Moves and species by name.
rows = re.findall(r'\{ (\d+), (\d+), (\d+), (\d+), "([^"]*)" \}', open(os.path.join(HERE, "../../frontend/battle_moves.h")).read())
MOVES = {norm(r[4]): i for i, r in enumerate(rows) if i}
MOVE_ALIAS = {"VICEGRIP": "VISEGRIP", "HIJUMPKICK": "HIGHJUMPKICK", "FAINTATTACK": "FEINTATTACK", "SMELLINGSALT": "SMELLINGSALTS",
              "SONICBOOM": "SONICBOOM", "SELFDESTRUCT": "SELFDESTRUCT", "ANCIENTPOWER": "ANCIENTPOWER", "DYNAMICPUNCH": "DYNAMICPUNCH",
              "THUNDERPUNCH": "THUNDERPUNCH", "EXTREMESPEED": "EXTREMESPEED", "DRAGONBREATH": "DRAGONBREATH", "BUBBLEBEAM": "BUBBLEBEAM",
              "DOUBLESLAP": "DOUBLESLAP", "SOLARBEAM": "SOLARBEAM", "POISONPOWDER": "POISONPOWDER", "THUNDERSHOCK": "THUNDERSHOCK",
              "SANDATTACK": "SANDATTACK", "WILLOWISP": "WILLOWISP", "XSCISSOR": "XSCISSOR", "UTURN": "UTURN", "GRASSWHISTLE": "GRASSWHISTLE",
              "SOFTBOILED": "SOFTBOILED", "DOUBLEEDGE": "DOUBLEEDGE", "MUDSLAP": "MUDSLAP", "LOCKON": "LOCKON", "SLAMMINGHEAD": "",
              "CONVERSION2": "CONVERSION2", "PSYCHUP": "PSYCHUP", "DIZZYPUNCH": "DIZZYPUNCH"}
sp_names = open(os.path.join(HERE, "../../third_party/PKSM-Core/strings/eng/species.txt"), encoding="utf-8").read().splitlines()
SPECIES = {norm(n): i for i, n in enumerate(sp_names) if i}
SPECIES.update({"NIDORANF": 29, "NIDORANM": 32, "MRMIME": 122, "FARFETCHD": 83, "HOOH": 250, "PORYGONZ": 474, "MIMEJR": 439})
missing = set()


def move(name):
    k = norm(re.sub(r"^MOVE_", "", name))
    if k in ("NONE", ""):
        return 0
    k = MOVE_ALIAS.get(k, k) or k
    if k not in MOVES:
        missing.add(name)
        return 0
    return MOVES[k]


def species(name):
    k = norm(re.sub(r"^SPECIES_", "", name))
    if k not in SPECIES:
        missing.add(name)
        return 0
    return SPECIES[k]


def mon(sp, moves):
    return (species(sp), [move(m) for m in moves][:4] + [0] * (4 - len(moves[:4])))


# HeartGold and SoulSilver.
hg = json.load(open(os.path.join(data, "hg_trainers.json")))["trainers"]
def hg_team(name, cls_part, rematch):
    found = [t for t in hg if t["name"].replace("{TRNAME}", "") == name and cls_part in t["class"] and t["party"]]
    found.sort(key=lambda t: max(p["level"] for p in t["party"]))
    t = found[-1] if rematch else found[0]
    return [mon(p["species"], p.get("moves", [])) for p in t["party"]]


# FireRed and Emerald: sParty_<name>[] = { { ... .species = X, .moves = {A, B, C, D} }, ... };
def c_parties(path):
    src = open(os.path.join(data, path)).read()
    parties = {}
    for m in re.finditer(r"sParty_(\w+)\[\]\s*=\s*\{(.*?)\n\};", src, re.S):
        team = []
        for p in re.finditer(r"\.species\s*=\s*(\w+).*?\.moves\s*=\s*\{([^}]*)\}", m.group(2), re.S):
            team.append(mon(p.group(1), [x.strip() for x in p.group(2).split(",") if x.strip()]))
        if team:
            parties[m.group(1)] = team
    return parties


fr = c_parties("pokefirered.h")
em = c_parties("pokeemerald.h")


def plat(name):
    j = json.load(open(os.path.join(data, "plat", name + ".json")))
    return [mon(p["species"], p.get("moves", [])) for p in j["party"]]


unova = json.load(open(os.path.join(HERE, "unova.json")))
def serebii(name):
    return [mon(p["species"], p["moves"]) for p in unova[name]]


# The champion of Kanto in FireRed: the rival's team for one starter (the one with Charizard).
blue = next(v for k, v in fr.items() if k.startswith("ChampionFirst") and any(s == 6 for s, _ in v))
teams = {
    # (region, sprite): team
    (0, "brock"): hg_team("Brock", "LEADER", True), (0, "misty"): hg_team("Misty", "LEADER", True),
    (0, "ltsurge"): hg_team("Lt. Surge", "LEADER", True), (0, "erika"): hg_team("Erika", "LEADER", True),
    (0, "koga"): fr["LeaderKoga"], (0, "sabrina"): hg_team("Sabrina", "LEADER", True),
    (0, "blaine"): hg_team("Blaine", "LEADER", True), (0, "giovanni"): fr["LeaderGiovanni"],
    (0, "lorelei"): fr["EliteFourLorelei"], (0, "bruno"): fr["EliteFourBruno"], (0, "agatha"): fr["EliteFourAgatha"],
    (0, "lance"): fr["EliteFourLance"], (0, "blue"): blue,
    (1, "falkner"): hg_team("Falkner", "LEADER", True), (1, "bugsy"): hg_team("Bugsy", "LEADER", True),
    (1, "whitney"): hg_team("Whitney", "LEADER", True), (1, "morty"): hg_team("Morty", "LEADER", True),
    (1, "chuck"): hg_team("Chuck", "LEADER", True), (1, "jasmine"): hg_team("Jasmine", "LEADER", True),
    (1, "pryce"): hg_team("Pryce", "LEADER", True), (1, "clair"): hg_team("Clair", "LEADER", True),
    (1, "will"): hg_team("Will", "ELITE_FOUR", False), (1, "koga"): hg_team("Koga", "ELITE_FOUR", False),
    (1, "bruno"): hg_team("Bruno", "ELITE_FOUR", False), (1, "karen"): hg_team("Karen", "ELITE_FOUR", False),
    (1, "lance"): hg_team("Lance", "CHAMPION", False),
    (1, "red"): hg_team("Red", "RED", False),
    (2, "roxanne"): em["Roxanne5"], (2, "brawly"): em["Brawly5"], (2, "wattson"): em["Wattson5"], (2, "flannery"): em["Flannery5"],
    (2, "norman"): em["Norman5"], (2, "winona"): em["Winona5"], (2, "tate"): em["TateAndLiza5"], (2, "juan"): em["Juan5"],
    (2, "sidney"): em["Sidney"], (2, "phoebe"): em["Phoebe"], (2, "glacia"): em["Glacia"], (2, "drake"): em["Drake"], (2, "wallace"): em["Wallace"],
    (3, "roark"): plat("leader_roark_rematch"), (3, "gardenia"): plat("leader_gardenia_rematch"), (3, "fantina"): plat("leader_fantina_rematch"),
    (3, "maylene"): plat("leader_maylene_rematch"), (3, "crasherwake"): plat("leader_wake_rematch"), (3, "byron"): plat("leader_byron_rematch"),
    (3, "candice"): plat("leader_candice_rematch"), (3, "volkner"): plat("leader_volkner_rematch"),
    (3, "aaron"): plat("elite_four_aaron"), (3, "bertha"): plat("elite_four_bertha"), (3, "flint"): plat("elite_four_flint"),
    (3, "lucian"): plat("elite_four_lucian"), (3, "cynthia"): plat("champion_cynthia"),
    (4, "shauntal"): serebii("Shauntal"), (4, "grimsley"): serebii("Grimsley"), (4, "caitlin"): serebii("Caitlin"),
    (4, "marshal"): serebii("Marshal"), (4, "alder"): serebii("Alder"),
}
if missing:
    print("NOT FOUND:", sorted(missing), file=sys.stderr)
    sys.exit(1)
with open(out, "w", encoding="utf-8") as f:
    f.write("#pragma once\n// Generated by tools/battle/gen_trainer_teams.py: do not edit. The famous trainers' teams with\n")
    f.write("// their moves as the games have them (see the script for which game and battle each is from).\n")
    f.write("namespace Arena\n{\nstruct RealMon { short species, moves[4]; };\nstruct RealTeam { int region; const char* sprite; int count; RealMon mons[6]; };\n")
    f.write("inline const RealTeam* FindRealTeam(int region, const char* sprite)\n{\n    static const RealTeam t[] = {\n")
    for (r, s), team in teams.items():
        team = team[:6]
        ms = ", ".join("{ %d, { %s } }" % (sp, ", ".join(map(str, mv))) for sp, mv in team)
        f.write('        { %d, "%s", %d, { %s } },\n' % (r, s, len(team), ms))
    f.write("    };\n    for (const RealTeam& x : t)\n        if (x.region == region && !std::strcmp(x.sprite, sprite)) return &x;\n    return nullptr;\n}\n}\n")
print(len(teams), "teams")
