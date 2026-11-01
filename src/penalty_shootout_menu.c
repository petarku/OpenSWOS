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
 * ChooseTeamsMenu. A RANDOM SELECTION button draws two teams of its group --
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

uint16_t g_shootoutKeeperComputer;

extern MenuEntry _menuEntries[];
extern uint8_t  _teamFileBuffer[];
extern TeamFileEx _selectedTeams[];
extern uint16_t _numSelectedTeams;
extern uint16_t _teamsLoaded;
extern uint16_t _pitchType, _pitchTypeOrSeason, _friendlyPitchType;
extern volatile uint16_t _currentTick;
extern QualifyingGroup _europeTable, _africaTable, _southAmericaTable,
                       _northAmericaTable, _asiaTable, _oceaniaTable;

extern int16_t LoadTeamFile(uint16_t extNumber);
extern void    InitTeams(void);
extern void    PlayFriendly(void);
extern void    ChooseTeamsMenu(void);
extern void    SetExitMenuFlag(void);
extern void    ShowErrorMenu(uint8_t *text);

#define TEAM_COMPUTER     1      /* TeamFile.teamControls (play_match_menu.h) */
#define TEAM_PLAYER_COACH 2
#define SEASONAL          0xFFFE /* _pitchType */

/* Runtime ordinals (menu_data.c _shootoutStartMenu): 0 title, 1 FREE
 * SELECTION, 2 CHOOSE TEAMS, 3 RANDOM SELECTION, 4 ALL RANDOM, 5..13 the
 * nine groups row by row, 14 GOALKEEPER CONTROL, 15 its PLAYER/COMPUTER
 * button, 16 EXIT. */
#define START_RANDOM_ORDINAL  4
#define START_KEEPER_ORDINAL 15

enum {
    G_ALL, G_EUROPE, G_AFRICA, G_NATIONAL, G_NORTH_AMERICA, G_ASIA,
    G_ORIGINAL, G_SOUTH_AMERICA, G_OCEANIA, G_CUSTOM, G_COUNT
};

enum {
    kNationalFirst = 80, kNationalLast = 85,
    kOriginalCustomFile = 72, kEditedCustomFile = 100,
    kMaxFiles = 96, kTries = 24,
};

/* The draw's own generator, as the challenge cups': seeded from the clock at
 * each press, leaving the match RNG alone. */
static uint32_t s_rng;

static void seedDraw(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    s_rng ^= (uint32_t)_currentTick << 16 | _currentTick;
    if (s_rng == 0)
        s_rng = 0x2545F491u;
}

static uint16_t randBelow(uint16_t n)
{
    s_rng ^= s_rng << 13;           /* xorshift32 */
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (uint16_t)(s_rng % n);
}

static int16_t addCountries(uint8_t *files, int16_t n, const QualifyingGroup *g)
{
    int16_t i;
    for (i = 0; i < 44 && g->countries[i] != 0xFF && n < kMaxFiles; i++)
        files[n++] = g->countries[i];
    return n;
}

/* The team files a group draws from. */
static int16_t groupFiles(int16_t group, uint8_t *files)
{
    static const QualifyingGroup *const k_continents[] = {
        &_europeTable, &_africaTable, &_northAmericaTable,
        &_asiaTable, &_southAmericaTable, &_oceaniaTable,
    };
    int16_t n = 0, i;

    switch (group) {
    case G_EUROPE:        return addCountries(files, 0, &_europeTable);
    case G_AFRICA:        return addCountries(files, 0, &_africaTable);
    case G_NORTH_AMERICA: return addCountries(files, 0, &_northAmericaTable);
    case G_ASIA:          return addCountries(files, 0, &_asiaTable);
    case G_SOUTH_AMERICA: return addCountries(files, 0, &_southAmericaTable);
    case G_OCEANIA:       return addCountries(files, 0, &_oceaniaTable);
    case G_ORIGINAL:      files[0] = kOriginalCustomFile; return 1;
    case G_CUSTOM:        files[0] = kEditedCustomFile;   return 1;
    case G_NATIONAL:
        for (i = kNationalFirst; i <= kNationalLast; i++)
            files[n++] = (uint8_t)i;
        return n;
    default:                                /* G_ALL: every club and national file */
        for (i = 0; i < (int16_t)(sizeof k_continents / sizeof k_continents[0]); i++)
            n = addCountries(files, n, k_continents[i]);
        for (i = kNationalFirst; i <= kNationalLast && n < kMaxFiles; i++)
            files[n++] = (uint8_t)i;
        return n;
    }
}

static int16_t sameTeam(const TeamFileEx *a, const TeamFileEx *b)
{
    int16_t i;
    if (a->team.countryNumber != b->team.countryNumber ||
        a->team.teamOrdinal != b->team.teamOrdinal)
        return 0;
    for (i = 0; i < (int16_t)sizeof a->team.teamName; i++)  /* no memcmp on the m68k */
        if (a->team.teamName[i] != b->team.teamName[i])
            return 0;
    return 1;
}

/* One team of the group, not `other`: a random file, then a random team of it
 * (a club continent is "a country, then a club"). 1 = drawn, 0 = none found,
 * -1 = a file would not load (LoadTeamFile has shown why). */
static int16_t drawTeam(const uint8_t *files, int16_t numFiles, TeamFileEx *out,
                        const TeamFileEx *other)
{
    int16_t tries;
    for (tries = 0; tries < kTries; tries++) {
        uint8_t file = files[randBelow((uint16_t)numFiles)];
        uint16_t count;
        TeamFileEx *rec;
        if (LoadTeamFile(file) != 0)
            return -1;
        count = *(uint16_t *)_teamFileBuffer;  /* host order once loaded */
        if (count == 0)
            continue;
        rec = (TeamFileEx *)(_teamFileBuffer + 2) + randBelow(count);
        if (rec->team.teamName[0] == 0 || (other && sameTeam(rec, other)))
            continue;
        *out = *rec;
        return 1;
    }
    return 0;
}

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
    uint8_t files[kMaxFiles];
    int16_t group = (int16_t)(entry->ordinal - START_RANDOM_ORDINAL);
    int16_t numFiles, got;

    if (group < 0 || group >= G_COUNT)
        return;
    numFiles = groupFiles(group, files);
    if (numFiles == 0)
        return;

    seedDraw();
    _teamsLoaded = 0;           /* the files as they are, never a cached career's */
    got = drawTeam(files, numFiles, &_selectedTeams[0], (TeamFileEx *)0);
    if (got > 0)
        got = drawTeam(files, numFiles, &_selectedTeams[1], &_selectedTeams[0]);
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
    SetExitMenuFlag();          /* back where CHOOSE TEAMS goes back to */
}

/* GOALKEEPER CONTROL: PLAYER (you guess the dive) or COMPUTER (the computer
 * guesses for your keeper too). The button draws through its string table. */
__attribute__((used, externally_visible))
void ShootoutKeeperControl(MenuEntry *entry)
{
    g_shootoutKeeperComputer ^= 1;
    CallDrawMenuItem(entry);
}
