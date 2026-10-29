# CHALLENGE_CUP.md — SPECIALS, the two CHALLENGE CUPs, and team star ratings

> Status (2026-10-29): **current**, both targets. Both cups played through their screens on
> SDL and under FS-UAE (Amiga); a drawn tie (extra time, penalties) not yet seen in either.

## 1. What it is

PRESET COMPETITION's world list has a row under CLASSICS: **SPECIALS**. It opens a screen
of special competitions: **CHALLENGE CUP** (clubs) and **NATIONAL CHALLENGE CUP** (national
teams, section 1b), the same game with a different underdog:

    PRESET COMPETITION -> SPECIALS -> CHALLENGE CUP -> the ladder -> PLAY <round> -> PLAY MATCH

**ONCE LOBOS** (El Salvador, fourth division, the game's famous weakest club) plays six cup
ties, one match a round, against stronger and stronger clubs:

| round | opponent |
|---|---|
| FIRST ROUND | another team of Once Lobos' own division |
| SECOND ROUND | a club rated 2 stars |
| LAST 16 | 2.5 stars |
| QUARTER-FINAL | 3 stars |
| SEMI-FINAL | 3.5 stars |
| FINAL | one of the strongest clubs: 4 stars and up (AC Milan, Barcelona, Juventus, Man Utd, Real Madrid, ...) |

- **Six rounds** because a SWOS cup has at most six (`roundLegs[6]` and friends): 64 teams.
- **Every opponent is a club**, never a national side, and the six are **drawn afresh each
  time the cup is started** (PRESET COMPETITION seeds the RNG from the clock).
- **A draw goes to extra time, then penalties.** A defeat ends the run; so does leaving a
  match before the final whistle. Backing out of the PLAY MATCH screen does not play the
  round.
- No REPLAY after a round: it is played once.
- The ladder shows each round's opponent with its stars (the squad screen's value stars),
  played rounds green or red, the next one orange. After a defeat or the final it offers
  NEW DRAW. EXIT gives the run up and goes back to SPECIALS; SPECIALS' EXIT goes back to the
  world list.

Clubs top out at about 4 stars, so "4.5 stars" in the first idea became "4 and up".

## 1a. The bonus match: BEST XI (CHALLENGE CUP only)

Win CHALLENGE CUP's final and a seventh row opens: **BONUS MATCH** against the **BEST XI**, a
ghost team of the most expensive club player at every position of a 4-4-2 (about 4.5 stars --
no real club reaches it), in Juventus's kit with Barcelona's as its second:

| | starting eleven | reserves |
|---|---|---|
| GK | Schmeichel | Taffarel |
| defence | Luisinho, Sammer, Hierro, Maldini | Gullit |
| midfield | Kanchelskis, Savicevic, Juninho, Giggs | Effenberg, Anderton |
| attack | Weah, Del Piero | Shearer |

It is a **surprise**: until the final is won the ladder shows "BONUS MATCH: ???" with no stars,
and only then is the team built -- a dozen team files to load, a wait on the Amiga that nobody
should have at every start for a match most runs never reach. A player of it the database does
not have means no bonus match (logged), and the cup simply ends won. Lose it and you are still
champions; win it and you are LEGENDS. The NATIONAL cup has no bonus match.

**Testing it:** a tester build (`SWOS_TEST_RULES`, every drop but `--release`) has a **SKIP ROUND
(TESTER BUILD)** button under the ladder: the round counts as won, unplayed, and skipping the
final reveals the bonus match. The Amiga makefile does not set `SWOS_TEST_RULES` by itself:
`gnumake -f Makefile.amiga EXTRA_CFLAGS=-DSWOS_TEST_RULES=1` (as `build_dist.sh` does).
`_specialsMenu` has 14 rows for it (`Menu_Specials`).

**Ghost teams** are made by `scripts/ghost_teams.py --c src/ghost_teams_data.c`: by reference
(each player's team file, team, player number and both names), never the players themselves,
since the original game's data is not in this repository. `src/ghost_teams.c` builds one from
the player's own database, checking every name, the players fresh (no cards, injuries, goals or
form), shirts 1..16. One rule so far, best by position: the highest price, ties broken by the
number of headline skills (in this database a player's price IS the sum of his seven skills).
A ghost team is filed as number 200+ of the custom teams' file (72): a REAL file number, because
screens look a team's country up by it (`_countriesTable`), and VIEW OPPO shows "(GHOST TEAM)".

## 1b. NATIONAL CHALLENGE CUP

**BERMUDA**, the weakest national team, the same way:

| round | opponent |
|---|---|
| FIRST ROUND | a weak side of its own continent (North America), at most 1 star: Nicaragua, Jamaica, Guyana, Surinam, Honduras |
| SECOND ROUND | a national team rated 1.5 stars (29 of them) |
| LAST 16 | 2 stars (24) |
| QUARTER-FINAL | 2.5 stars (29) |
| SEMI-FINAL | 3 or 3.5 stars (30) |
| FINAL | the top ten at 4 stars: Italy, Germany, Brazil, Holland, England, Spain, France, Portugal, Russia, Argentina |

## 2. Team star ratings

A team's rating is the **average of its starting eleven's value stars**, the ones the squad
screen draws for each player (`InitPlayerValueDisplayStruct`: the displayed value
`(value - 1) / 8`, frames 113..122, half a star apart), rounded to the nearest half star.

- In the game: `TeamStars_HalfStars()` / `TeamStars_PlayerHalfStars()`, `src/team_stars.c`
  (both targets). The value is the price's `_avgTeamValues` entry, without form: a team file
  is not the career squad.
- Offline: `scripts/team_stars.py` rates every team of the database the same way, from the
  assets folder's `data_json/teams` (`--teams DIR`, `$SWOS_ASSETS`). Equal stars are broken
  by the eleven's summed price.
- The sorted lists, strongest first: `docs/TEAM_STARS.txt` (all 1,711),
  `docs/TEAM_STARS_CLUBS.txt`, `docs/TEAM_STARS_NATIONAL.txt`. Rerun the script after
  changing the database:

      python3 scripts/team_stars.py --out docs/TEAM_STARS.txt
      python3 scripts/team_stars.py --clubs --out docs/TEAM_STARS_CLUBS.txt
      python3 scripts/team_stars.py --nationals --out docs/TEAM_STARS_NATIONAL.txt

Clubs are team files below 72 (CUSTOM); national sides are 80..85 (EUROPE .. OCEANIA). The
EUROCUP / EUROCWC / UEFACUP files only repeat clubs filed under their own country, so they
are left out of both.

Weakest: national **BERMUDA** (83:0; tied with Papua New Guinea at half a star, lower on
price and skills); clubs PREKASH and SV BOXEL (Surinam) are a shade below ONCE LOBOS on
paper -- Once Lobos is the one the cup is built round because it is the one players know.

## 3. The code

| file | what |
|---|---|
| `src/challenge_cup.c` | The two cups (`k_cups`: title, underdog, its team file, round 1's star limit, the pools), the run (underdog + six opponents), the draw (its own generator, seeded from the clock at each draw), a round's match, the match hooks, and `_specialsMenu`'s screens (SPECIALS, the ladder). |
| `src/challenge_cup_pool.c` | **Generated** (`scripts/team_stars.py --challenge-cup src/challenge_cup_pool.c`): each band's teams by team file, number and name, `g_challengeCupPools` (clubs) and `g_nationalCupPools`. The bands are `CHALLENGE_CUP_BANDS` / `NATIONAL_CUP_BANDS` in the script. |
| `src/team_stars.c` | The rating (section 2). |
| `src/ghost_teams.c`, `src/ghost_teams_data.c` | Building a ghost team from the database (`GhostTeams_Build`, `GhostTeams_IsGhost`); the BEST XI by reference, **generated** by `scripts/ghost_teams.py`. |
| `src/teams.c` | `SelectTeamsFinalMenu`: the SPECIALS row (a marker in `leaguesPtrTable`, as CLASSICS'); picking it calls `Specials_Request` and closes the chooser. |
| `src/menus.c` | `PresetCompetitionMenu`: after the chooser, `Specials_Show` while a request is pending, then the chooser again. |
| `src/game_init.c` | `InitAndPlayGame`: `ChallengeCup_TakeMatchRules` after the tie rules are cleared (names the match CHALLENGE CUP / the round, extra time + penalties). `InitializeInGameTeamsAndStartGame`: `ChallengeCup_BeginMatch` beside the training modes' latches. |
| `src/match_setup.c` | `EndOfGame`: `ChallengeCup_FinalWhistle` (goals and winner, after penalties too). |
| `src/game_loop.c` | No REPLAY / EXIT menu after a round (`ChallengeCup_InMatch`). |
| `src/menu_data.c`, `bin/data_json/menus/specialsMenu.json` | `_specialsMenu`: `_challengeMenu`'s shape, 13 rows laid out at run time (`Specials_MenuInit`), registered in `menu_symbols.c` and `s_staticMenus`. |

A round is a friendly in everything but its rules: the two teams go into `_selectedTeams` as
the friendly's selection would put them (Once Lobos PLAYER-COACH, the opponent COMPUTER),
then `InitTeams` + `PlayFriendly`, so the PLAY MATCH screen, tactics and substitutes are the
friendly's. The mode is armed only around that call and latched at kick-off, so no other
match can see it.

Opponents are loaded from their team files (`LoadTeamFile`, as `challenge_gen.c` does) and
checked by name; one the database does not have is redrawn (up to eight tries), and a cup
that cannot be filled says THE CUP'S TEAMS WERE NOT FOUND. The underdog and round 1's
opponent are found at run time in the underdog's team file (TEAM.051, TEAM.083): by name, then
a team of the same division no stronger than the cup's limit (any, for clubs; 1 star for
nations, whose file has one division).

## 4. Changing it

- **Different opponents:** change `CHALLENGE_CUP_BANDS` / `NATIONAL_CUP_BANDS` in
  `scripts/team_stars.py` (or write a hand-picked list in the same `ChallengeCupPoolTeam`
  shape) and regenerate `src/challenge_cup_pool.c`. A band with one team is a fixed opponent.
- **Another cup:** a row in `k_cups` and a pool array; SPECIALS lays out one button per cup
  (room for three in its 13 rows as laid out).

## 5. Not done yet

- A run lives in memory only: leaving SPECIALS gives it up, and nothing is saved.
- Once Lobos is fresh each round (no injuries, cards or form carried over).
- A drawn round (extra time, then penalties) has not been played yet on either target.
- The final's name on the scoreboard is the cup's title; NATIONAL CHALLENGE CUP is long and
  has not been checked on every screen that shows it.
