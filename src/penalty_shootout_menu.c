/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* penalty_shootout_menu.c — PENALTY SHOOTOUT's start screen (_shootoutStartMenu,
 * menu_data.c), SWOS 2020's layout (asked for 2026-11-01):
 *
 *                       PENALTY SHOOTOUT
 *                        FREE SELECTION
 *                         CHOOSE TEAMS
 *                       RANDOM SELECTION
 *                          ALL RANDOM
 *           EUROPE           AFRICA          NATIONAL
 *         N. AMERICA          ASIA           ORIGINAL
 *         S. AMERICA        OCEANIA           CUSTOM
 *            GOALKEEPER CONTROL   PLAYER / COMPUTER
 *                            EXIT
 *
 * ShowFriendlyMenu shows it IN PLACE of the friendly's own screen while the mode
 * is on, so the friendly flow around it (RNG seed, gameType save/restore, the
 * pristine team-cache reload) is unchanged. CHOOSE TEAMS is the friendly's own
 * ChooseTeamsMenu, wrapped so it comes back to this screen
 * (TrainingChooseTeamsMenu, menus.c), as the RANDOM buttons do. A RANDOM SELECTION button draws two teams of its group --
 * the player's side first, the computer's second -- and plays them exactly as
 * ChooseTeamsMenu plays a chosen pair (InitTeams + PlayFriendly), so the
 * PLAY MATCH screen still comes before the kicks.
 *
 * The groups: the five club continents are the team-selection screen's own
 * country lists (_europeTable ... _oceaniaTable, competition_data.c); NATIONAL
 * is the six national-team files (80..85); ORIGINAL is SWOS's ORIGINAL CUSTOM
 * TEAMS (TEAM.072) and CUSTOM its EDITED CUSTOM TEAMS (CUSTOMS.EDT, file 100),
 * the CHOOSE TEAMS screen's split; ALL RANDOM is every club and national file.
 *
 * Kept out of penalty_shootout.c so the keeper unit test, which links that
 * file, needs no menu or team-file stubs. */

#include "penalty_shootout.h"
#include "game_types.h"
#include "menu_system.h"
#include "random_team.h"     /* the groups and the draw, shared with SHOOTING PRACTICE */

uint16_t g_shootoutKeeperComputer;

extern MenuEntry _menuEntries[];
extern TeamFileEx _selectedTeams[];
extern uint16_t _numSelectedTeams;
extern uint16_t _teamsLoaded;
extern uint16_t _pitchType, _pitchTypeOrSeason, _friendlyPitchType;

extern void    InitTeams(void);
extern void    PlayFriendly(void);
extern void    ShowErrorMenu(uint8_t *text);

#define TEAM_COMPUTER     1      /* TeamFile.teamControls (play_match_menu.h) */
#define TEAM_PLAYER_COACH 2
#define SEASONAL          0xFFFE /* _pitchType */

/* Runtime ordinals (menu_data.c _shootoutStartMenu): 0 title, 1 FREE
 * SELECTION, 2 CHOOSE TEAMS, 3 RANDOM SELECTION, 4 ALL RANDOM, 5..13 the
 * nine groups row by row (in RT_GROUP_* order), 14 GOALKEEPER CONTROL, 15 its
 * PLAYER/COMPUTER button, 16 EXIT. */
#define START_RANDOM_ORDINAL  4
#define START_KEEPER_ORDINAL 15

/* onInit AND onReturn. The friendly's screen sets the match's pitch here
 * (FriendlyMenuReinit): the OPTIONS pitch type, or a random one for SEASONAL. */
__attribute__((used, externally_visible))
void ShootoutStartMenuInit(void)
{
    if (_pitchType != SEASONAL) {
        _pitchTypeOrSeason = 0;
        _friendlyPitchType = _pitchType;
    }
}

/* ALL RANDOM and the nine group buttons (ordinals 4..13). */
__attribute__((used, externally_visible))
void ShootoutRandomTeams(MenuEntry *entry)
{
    uint8_t files[RT_MAX_FILES];
    int16_t group = (int16_t)(entry->ordinal - START_RANDOM_ORDINAL);
    int16_t numFiles, got;

    if (group < 0 || group >= RT_GROUP_COUNT)
        return;
    numFiles = RandomTeam_GroupFiles(group, files);
    if (numFiles == 0)
        return;

    RandomTeam_Seed();
    _teamsLoaded = 0;           /* the files as they are, never a cached career's */
    got = RandomTeam_Draw(files, numFiles, &_selectedTeams[0], (TeamFileEx *)0);
    if (got > 0)
        got = RandomTeam_Draw(files, numFiles, &_selectedTeams[1], &_selectedTeams[0]);
    _teamsLoaded = 0;
    if (got < 0)
        return;                 /* the load error is on screen already */
    if (got == 0) {
        ShowErrorMenu((uint8_t *)"NOT ENOUGH TEAMS");
        return;
    }

    _selectedTeams[0].team.teamControls = TEAM_PLAYER_COACH;
    _selectedTeams[1].team.teamControls = TEAM_COMPUTER;
    _numSelectedTeams = 2;
    InitTeams();
    PlayFriendly();             /* the PLAY MATCH screen, then the kicks */
    /* and back to this start screen, as CHOOSE TEAM(S) comes back
     * (TrainingChooseTeamsMenu, menus.c) -- not out to the PRACTICE submenu. */
}

/* GOALKEEPER CONTROL: PLAYER (you guess the dive) or COMPUTER (the computer
 * guesses for your keeper too). The button draws through its string table. */
__attribute__((used, externally_visible))
void ShootoutKeeperControl(MenuEntry *entry)
{
    g_shootoutKeeperComputer ^= 1;
    CallDrawMenuItem(entry);
}
