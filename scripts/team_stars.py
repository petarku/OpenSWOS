#!/usr/bin/env python3
"""team_stars.py -- every team of the database, rated in value stars, strongest first.

A team's rating is what src/team_stars.c computes in the game: the average of its starting
eleven's value stars (the squad screen's, half a star apart), rounded to the nearest half.
The exact average is listed too, and then the eleven's summed price, so teams of one rating
still sort by strength.

    python3 scripts/team_stars.py                         # every team, sorted, to stdout
    python3 scripts/team_stars.py --clubs                 # clubs only (no national sides)
    python3 scripts/team_stars.py --nationals             # national teams only
    python3 scripts/team_stars.py --out docs/TEAM_STARS.txt
    python3 scripts/team_stars.py --nationals --out docs/TEAM_STARS_NATIONAL.txt
    python3 scripts/team_stars.py --challenge-cup src/challenge_cup_pool.c

--teams DIR is the database's JSON (default: <assets>/data_json/teams, the setup's output in
the per-user game folder, or $SWOS_ASSETS). --challenge-cup writes CHALLENGE CUP's opponent
pool (docs/CHALLENGE_CUP.md): the clubs of each star band. Rerun it when the bands or the
database change.
"""
import argparse
import glob
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Club files are numbered below the custom teams (72); the national sides' files are 80+, and
# the euro cup files (no country number) only repeat clubs that are in their own country's.
CUSTOM_FILE = 72
NATIONAL_FILES = 80     # EUROPE 80, AFRICA, SOUTH_AMERICA, NORTH_AMERICA, ASIA, OCEANIA 85

# CHALLENGE CUP's rounds after the first (which is the weakest team's own division, chosen in
# the game): (round, the band's half stars, lowest, highest). The last band is open above.
CHALLENGE_CUP_BANDS = [
    ("SECOND ROUND", 4, 4, 4),     # 2 stars
    ("LAST 16", 5, 5, 5),          # 2.5
    ("QUARTER-FINAL", 6, 6, 6),    # 3
    ("SEMI-FINAL", 7, 7, 7),       # 3.5
    ("FINAL", 8, 8, 10),           # 4 and up: the strongest clubs
]

# NATIONAL CHALLENGE CUP's, the same way: BERMUDA's first round is a weak side of its own
# continent (chosen in the game), then national teams of each band.
NATIONAL_CUP_BANDS = [
    ("SECOND ROUND", 3, 3, 3),     # 1.5 stars
    ("LAST 16", 4, 4, 4),          # 2
    ("QUARTER-FINAL", 5, 5, 5),    # 2.5
    ("SEMI-FINAL", 6, 6, 7),       # 3 and 3.5
    ("FINAL", 8, 8, 10),           # 4 and up: the strongest nations
]


def avg_team_values():
    """_avgTeamValues as the game has it: the displayed value (1..80) of each price."""
    path = os.path.join(ROOT, "src", "swos_data_bulk_imports.c")
    text = open(path, encoding="latin-1").read()
    m = re.search(r"_avgTeamValues\[\d+\]\s*=\s*\{([^}]*)\}", text)
    if not m:
        sys.exit("team_stars.py: _avgTeamValues not found in " + path)
    return [int(v) for v in m.group(1).split(",") if v.strip()]


def player_half_stars(values, price):
    value = 1 if price < 0 or price >= 128 else values[price]
    value = min(max(value, 1), 80)
    return (value - 1) // 8 + 1


def default_teams_dir():
    assets = os.environ.get("SWOS_ASSETS")
    if not assets:
        assets = os.path.join(os.path.expanduser("~"), "Documents", "SWOS", "assets")
    return os.path.join(assets, "data_json", "teams")


def read_teams(teams_dir, values):
    rows = []
    for path in sorted(glob.glob(os.path.join(teams_dir, "*.json"))):
        data = json.load(open(path, encoding="utf-8"))
        if "teams" not in data:
            continue
        file_number = data.get("country")
        for t in data["teams"]:
            team, players = t["team"], t["players"]
            stars = [player_half_stars(values, players[n]["price"])
                     for n in team["player_numbers"][:11]]
            total = sum(stars)
            xi_price = sum(max(players[n]["price"], 0) for n in team["player_numbers"][:11])
            rows.append({
                "name": team["team_name"].rstrip(),
                "file": os.path.splitext(os.path.basename(path))[0],
                "file_number": file_number,
                "country": team["country_number"],
                "ordinal": team["team_ordinal"],
                "league": team["league"],
                "average": total / 11.0,
                "half_stars": (total * 2 + 11) // 22,     # as src/team_stars.c rounds
                "xi_price": xi_price,
            })
    # Equal stars: the dearer eleven first, so the weakest team in the game sorts last.
    rows.sort(key=lambda r: (-r["average"], -r["xi_price"], r["name"]))
    return rows


def is_club(row):
    n = row["file_number"]
    return n is not None and n < CUSTOM_FILE


def is_national(row):
    n = row["file_number"]
    return n is not None and n >= NATIONAL_FILES


def stars_text(half):
    return "%d%s" % (half // 2, ".5" if half % 2 else "  ")


def listing(rows):
    out = ["# Every team rated in value stars, strongest first (scripts/team_stars.py).",
           "# stars = the starting eleven's average value stars, to the nearest half;",
           "# average = the same in half stars, unrounded; price = the eleven's summed price",
           "# (the tie-break); team = TEAM.nnn:number; div = its division, 0 = the top.",
           "",
           "%5s  %-5s  %7s  %5s  %-17s %-17s %-7s %s" % ("rank", "stars", "average", "price",
                                                      "name", "file", "team", "div"),
           ""]
    for i, r in enumerate(rows, 1):
        out.append("%5d  %-5s  %7.2f  %5d  %-17s %-17s %3d:%-3d %d" % (
            i, stars_text(r["half_stars"]), r["average"], r["xi_price"], r["name"], r["file"],
            r["country"], r["ordinal"], r["league"]))
    return "\n".join(out) + "\n"


def c_string(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def pools(lines, teams, bands, prefix, array, what):
    """One cup's bands as C: a team list per round, then the array the game draws from."""
    counts = []
    for i, (title, band, low, high) in enumerate(bands):
        pool = [r for r in teams if low <= r["half_stars"] <= high]
        if not pool:
            sys.exit("team_stars.py: no %s in the %s band" % (what, title))
        counts.append(len(pool))
        lines.append("static const ChallengeCupPoolTeam %s%d[] = {   /* %s: %s stars */"
                     % (prefix, i + 2, title, stars_text(band).strip()))
        for r in pool:
            lines.append("    { %3d, %3d, %s },   /* %.2f */" % (
                r["country"], r["ordinal"], c_string(r["name"]), r["average"]))
        lines.append("};")
        lines.append("")
    lines.append("const ChallengeCupPool %s[CHALLENGE_CUP_ROUNDS - 1] = {" % array)
    for i, (title, band, low, high) in enumerate(bands):
        lines.append("    { %s%d, %d, %d },   /* %s */" % (prefix, i + 2, counts[i], band, title))
    lines.append("};")
    lines.append("")
    return counts


def challenge_cup_pool(rows):
    lines = [
        "/* challenge_cup_pool.c -- GENERATED by scripts/team_stars.py --challenge-cup; do not",
        " * edit. The opponents after the first round of CHALLENGE CUP (clubs) and NATIONAL",
        " * CHALLENGE CUP (national teams): each round's star band, strongest first",
        " * (docs/CHALLENGE_CUP.md). A team is named by its team file and number, and by its",
        " * name, which the game checks when it loads it. */",
        "",
        '#include "challenge_cup.h"',
        "",
    ]
    counts = pools(lines, [r for r in rows if is_club(r)], CHALLENGE_CUP_BANDS,
                   "k_clubRound", "g_challengeCupPools", "club")
    counts += pools(lines, [r for r in rows if is_national(r)], NATIONAL_CUP_BANDS,
                    "k_nationRound", "g_nationalCupPools", "national team")
    return "\n".join(lines).rstrip("\n") + "\n", counts


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument("--teams", default=default_teams_dir(), help="the database's JSON folder")
    ap.add_argument("--clubs", action="store_true", help="clubs only")
    ap.add_argument("--nationals", action="store_true", help="national teams only")
    ap.add_argument("--out", help="write the listing here instead of stdout")
    ap.add_argument("--challenge-cup", metavar="FILE",
                    help="write both challenge cups' opponent pools (C)")
    args = ap.parse_args()

    if not os.path.isdir(args.teams):
        sys.exit("team_stars.py: no team database at %s (--teams DIR)" % args.teams)
    rows = read_teams(args.teams, avg_team_values())

    if args.challenge_cup:
        text, counts = challenge_cup_pool(rows)
        with open(args.challenge_cup, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("%s: %s teams per band (clubs, then nations)" % (args.challenge_cup, ", ".join(map(str, counts))))
        return

    if args.clubs:
        rows = [r for r in rows if is_club(r)]
    elif args.nationals:
        rows = [r for r in rows if is_national(r)]
    text = listing(rows)
    if args.out:
        with open(args.out, "w", encoding="utf-8", newline="\n") as f:
            f.write(text)
        print("%s: %d teams" % (args.out, len(rows)))
    else:
        sys.stdout.write(text)


if __name__ == "__main__":
    main()
