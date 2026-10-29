/* challenge_cup.c -- SPECIALS, CHALLENGE CUP and NATIONAL CHALLENGE CUP. See challenge_cup.h,
 * docs/CHALLENGE_CUP.md.
 *
 * A side at the very bottom of the game plays six cup ties against stronger and stronger
 * opponents, drawn afresh each time the cup is started (k_cups):
 *
 *   CHALLENGE CUP           ONCE LOBOS: a club of its own division, then clubs of two stars,
 *                           two and a half, three, three and a half, and one of the strongest
 *   NATIONAL CHALLENGE CUP  BERMUDA: a weak side of its own continent, then national teams of
 *                           one and a half stars, two, two and a half, three or three and a
 *                           half, and one of the strongest
 *
 * Rounds 2..6 come from src/challenge_cup_pool.c (scripts/team_stars.py).
 *
 * Each round is a friendly in everything but its rules: InitAndPlayGame with the two teams
 * the friendly's selection would have put in _selectedTeams, and extra time then penalties
 * on a draw (the tie rules, which InitAndPlayGame clears and ChallengeCup_TakeMatchRules
 * puts back). So the PLAY MATCH screen, tactics and substitutes work as in a friendly. The
 * mode is latched at kick-off (ChallengeCup_BeginMatch) like the training modes'; a round
 * whose match was left before the final whistle is lost, one backed out of on the PLAY MATCH
 * screen is not played.
 *
 * The screens are one classic packed menu, _specialsMenu (13 rows laid out here, as
 * challenge_collection.c lays out _challengeMenu), shown from PresetCompetitionMenu once the
 * chooser has closed for SPECIALS: so, as for the collection, a match is played from under
 * the main menu and the menus are put back after it (restore_menus). */

#include <string.h>
#ifdef SWOS_SDL_BUILD
#include <stdio.h>
#include <time.h>
#endif

#include "challenge_cup.h"
#include "game_types.h"
#include "menu_system.h"
#include "menus.h"
#include "play_match_menu.h"    /* TEAM_COMPUTER, TEAM_PLAYER_COACH */
#include "sprites.h"
#include "string_utils.h"
#include "swos_io.h"
#include "team_stars.h"

extern uint8_t  _specialsMenu[];
extern uint8_t *_spriteGraphicsPtr;
extern uint8_t  _teamFileBuffer[];
extern uint8_t  _gameName[], _gameRound[];
extern TeamFileEx _selectedTeams[];
extern uint16_t _numSelectedTeams;
extern uint16_t _teamsLoaded;
extern uint16_t _pitchType, _pitchTypeOrSeason, _friendlyPitchType, _friendlySeason;
extern uint16_t _minSubstitutes, _maxSubstitutes;
extern int16_t  _extraTimeState, _penaltiesState;
extern uint16_t _quitToDos, _fadeInitFlag;
extern volatile uint16_t _fadeState;
extern int16_t  LoadTeamFile(uint16_t extNumber);
extern void     InitTeams(void);
extern void     PlayFriendly(void);
extern volatile uint16_t _currentTick;
extern void     ShowErrorMenu(uint8_t *text);
extern void     ResetMenuViewState(void);

enum {
    kNameBytes  = 17,
    kRows       = 13,               /* _specialsMenu's entries */
    kSeasonal   = 0xFFFE,           /* _pitchType: SEASONAL */
    kRandomPitch = 0xFFFF,
    kSeasonSep  = 7,
    kPoolTries  = 8,
};
/* A cup: who starts, where round 1's opponent comes from, and the pools after it. */
typedef struct CupDef {
    const char *title;              /* the menus' and the match's name */
    const char *underdog;           /* the starting side, by name ... */
    uint8_t     underdogFile;       /* ... in this team file */
    int8_t      firstMaxHalfStars;  /* round 1: a team of the underdog's file and division,
                                     * no stronger than this */
    const ChallengeCupPool *pools;  /* rounds 2..6 */
    const char *about;              /* SPECIALS' line about it */
    const char *log;                /* the screen's name in the [menu] log */
} CupDef;

enum { CUP_CLUB, CUP_NATIONAL, kCups };
static const CupDef k_cups[kCups] = {
    { "CHALLENGE CUP", "ONCE LOBOS", 51 /* EL SALVADOR */, 10, g_challengeCupPools,
      "ONCE LOBOS AGAINST EVER STRONGER CLUBS", "specials/challengeCup" },
    { "NATIONAL CHALLENGE CUP", "BERMUDA", 83 /* NORTH AMERICA */, 2, g_nationalCupPools,
      "BERMUDA AGAINST EVER STRONGER NATIONS", "specials/nationalCup" },
};
/* The game's twelve formations (TACTIC_DEFAULT .. TACTIC_DEFEND), by number; the six USER
 * slots are the player's own and are never given to an opponent. */
enum { kFormations = TACTIC_DEFEND + 1 };
static const char *const k_formationNames[kFormations] = {
    "4-4-2", "5-4-1", "4-5-1", "5-3-2", "3-5-2", "4-3-3",
    "4-2-4", "3-4-3", "SWEEP", "5-2-3", "ATTACK", "DEFEND",
};

/* How an opponent lines up: half the time as its team file says, half the time a formation
 * of the round's character -- defensive early, balanced in the middle, attacking at the end.
 * A computer side's skills do not depend on its formation (AdjustPlayerSkills holds it at
 * 100%), so this changes how it plays, not how strong it is. ATTACK and DEFEND, the two
 * extremes, are not used (nor kept from a team file that has one). A formation listed twice
 * is twice as likely: 5-4-1 early, 4-3-3 and 4-2-4 late. */
enum { kRoundFormations = 6 };
static const uint8_t k_roundFormations[3][kRoundFormations] = {
    { TACTIC_5_4_1, TACTIC_5_3_2, TACTIC_4_5_1, TACTIC_SWEEP, TACTIC_5_2_3, TACTIC_5_4_1 },
    { TACTIC_DEFAULT, TACTIC_3_5_2, TACTIC_4_5_1, TACTIC_4_3_3, TACTIC_SWEEP, TACTIC_5_3_2 },
    { TACTIC_4_3_3, TACTIC_4_2_4, TACTIC_3_4_3, TACTIC_3_5_2, TACTIC_4_3_3, TACTIC_4_2_4 },
};

static const char *const k_roundNames[CHALLENGE_CUP_ROUNDS] = {
    "FIRST ROUND", "SECOND ROUND", "LAST 16", "QUARTER-FINAL", "SEMI-FINAL", "FINAL",
};

/* ---- the run ---- */

enum { RESULT_NONE, RESULT_WON, RESULT_LOST, RESULT_LEFT };

static struct {
    int32_t    cup;                 /* k_cups */
    TeamFileEx underdog;
    TeamFileEx opponent[CHALLENGE_CUP_ROUNDS];
    int32_t    round;               /* the round to play next; CHALLENGE_CUP_ROUNDS = won */
    int32_t    out;                 /* knocked out (in `round`) */
    int8_t     result[CHALLENGE_CUP_ROUNDS];
    uint16_t   goals[CHALLENGE_CUP_ROUNDS][2];  /* the underdog first */
} s_run;

/* The match: armed by play_round around its InitAndPlayGame, latched at kick-off. */
static int32_t  s_armed;            /* the next match is a challenge cup round */
static int32_t  s_match;            /* this match is one (latched) */
static int32_t  s_winner;           /* 1, 2, 0 = drawn; -1 = no final whistle yet */
static uint16_t s_goals[2];

/* ---- teams ---- */

/* A team file's 17-byte name against a C string; trailing spaces do not count. */
static int32_t name_is(const int8_t *team, const char *name)
{
    int32_t i;
    for (i = 0; i < kNameBytes && name[i]; i++)
        if ((char)team[i] != name[i])
            return 0;
    for (; i < kNameBytes && team[i]; i++)
        if (team[i] != ' ')
            return 0;
    return 1;
}

/* The draw has a generator of its own. The match RNG is seeded once per PRESET COMPETITION
 * from the ticks since start-up, so two games started alike drew the same six; this one
 * takes the clock at each draw (the moment CHALLENGE CUP or NEW DRAW is pressed) and keeps
 * its state between draws. It leaves the match RNG alone. */
static uint32_t s_rng;

static void seed_draw(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    s_rng ^= (uint32_t)_currentTick << 16 | _currentTick;
#ifdef SWOS_SDL_BUILD
    s_rng ^= (uint32_t)time(NULL);
#endif
    if (s_rng == 0)
        s_rng = 0x2545F491u;
}

static int32_t rand_below(int32_t n)
{
    s_rng ^= s_rng << 13;           /* xorshift32 */
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (int32_t)(s_rng % (uint32_t)n);
}

/* A team's name as a C string: the file's 17 bytes, NUL-terminated, trailing spaces off.
 * (swos_io_snprintf has no "%.17s".) */
static const char *team_name(const TeamFileEx *team, char out[kNameBytes + 1])
{
    int32_t n = 0;
    while (n < kNameBytes && team->team.teamName[n])
        n++;
    memcpy(out, team->team.teamName, (uint32_t)n);
    while (n > 0 && out[n - 1] == ' ')
        n--;
    out[n] = 0;
    return out;
}

static TeamFileEx *team_file(uint16_t *count)
{
    *count = *(uint16_t *)_teamFileBuffer;  /* host order once loaded (LoadTeamFile) */
    return (TeamFileEx *)(_teamFileBuffer + 2);
}

/* Round 1 may draw this team: the underdog's division (a national file has only one), no
 * stronger than the cup allows. */
static int32_t first_round_rival(const CupDef *cup, const TeamFileEx *team,
                                 const TeamFileEx *underdog)
{
    return team != underdog && team->team.league == underdog->team.league &&
           TeamStars_HalfStars(team) <= cup->firstMaxHalfStars;
}

/* The underdog, and round 1's opponent. */
static int32_t draw_first_round(const CupDef *cup)
{
    TeamFileEx *rec;
    uint16_t count, i;
    int32_t underdog = -1, rivals = 0, pick;

    if (LoadTeamFile(cup->underdogFile) != 0)
        return 0;
    rec = team_file(&count);
    for (i = 0; i < count; i++)
        if (name_is(rec[i].team.teamName, cup->underdog))
            underdog = i;
    if (underdog < 0)
        return 0;
    for (i = 0; i < count; i++)
        if (first_round_rival(cup, &rec[i], &rec[underdog]))
            rivals++;
    if (rivals == 0)
        return 0;
    pick = rand_below(rivals);
    for (i = 0; i < count; i++)
        if (first_round_rival(cup, &rec[i], &rec[underdog]) && pick-- == 0)
            s_run.opponent[0] = rec[i];
    s_run.underdog = rec[underdog];
    return 1;
}

/* A team of the round's band: the one the pool names, if its team file has it by that
 * number and name (another database may not). */
static int32_t draw_round(const CupDef *cup, int32_t round)
{
    const ChallengeCupPool *pool = &cup->pools[round - 1];
    int32_t tries;

    for (tries = 0; tries < kPoolTries; tries++) {
        const ChallengeCupPoolTeam *t = &pool->teams[rand_below(pool->count)];
        TeamFileEx *rec;
        uint16_t count, i;
        if (LoadTeamFile(t->country) != 0)
            continue;
        rec = team_file(&count);
        for (i = 0; i < count; i++)
            if (rec[i].team.teamOrdinal == t->ordinal && name_is(rec[i].team.teamName, t->name)) {
                s_run.opponent[round] = rec[i];
                return 1;
            }
#ifdef SWOS_SDL_BUILD
        fprintf(stderr, "[challenge-cup] %s (team file %u, team %u) is not in this database\n",
                t->name, (unsigned)t->country, (unsigned)t->ordinal);
#endif
    }
    return 0;
}

static void choose_formations(void)
{
    int32_t round;

    for (round = 0; round < CHALLENGE_CUP_ROUNDS; round++) {
        TeamFile *team = &s_run.opponent[round].team;
        if (rand_below(2) || team->tactics >= TACTIC_ATTACK)
            team->tactics = k_roundFormations[round / 2][rand_below(kRoundFormations)];
    }
}

/* A new run of a cup: the underdog and six opponents. 0 = the teams could not be found
 * (shown). */
static int32_t new_run(int32_t which)
{
    const CupDef *cup = &k_cups[which];
    int32_t round, ok;

    memset(&s_run, 0, sizeof s_run);
    s_run.cup = which;
    seed_draw();
    _teamsLoaded = 0;               /* the files as they are, not a cached career's */
    ok = draw_first_round(cup);
    for (round = 1; ok && round < CHALLENGE_CUP_ROUNDS; round++)
        ok = draw_round(cup, round);
    _teamsLoaded = 0;
    if (!ok) {
        ShowErrorMenu((uint8_t *)"THE CUP'S TEAMS WERE NOT FOUND");
        return 0;
    }
    choose_formations();
#ifdef SWOS_SDL_BUILD
    fprintf(stderr, "[challenge-cup] %s, new draw:", cup->title);
    for (round = 0; round < CHALLENGE_CUP_ROUNDS; round++) {
        char name[kNameBytes + 1];
        fprintf(stderr, " %s (%d, %s)", team_name(&s_run.opponent[round], name),
                (int)TeamStars_HalfStars(&s_run.opponent[round]),
                k_formationNames[s_run.opponent[round].team.tactics]);
    }
    fprintf(stderr, "\n");
#endif
    return 1;
}

/* ---- a round ---- */

static void restore_menus(void)
{
    CommonMenuExit();
    _fadeState = 0;
    _fadeInitFlag = 0x03E7u;
    ResetMenuViewState();
    PrepareMenuForFadeIn();
}

/* The round's match, set up as the friendly's selection would set it up (ShowFriendlyMenu,
 * ChooseTeamsMenu) with the underdog as the player's team and the opponent the computer's. */
static void play_round(void)
{
    int32_t round = s_run.round;

    _pitchTypeOrSeason = 0;
    _friendlyPitchType = _pitchType != kSeasonal ? _pitchType : kRandomPitch;
    _friendlySeason = kSeasonSep;
    _minSubstitutes = 2;
    _maxSubstitutes = 5;
    _selectedTeams[0] = s_run.underdog;
    _selectedTeams[0].team.teamControls = TEAM_PLAYER_COACH;
    _selectedTeams[1] = s_run.opponent[round];
    _selectedTeams[1].team.teamControls = TEAM_COMPUTER;
    _numSelectedTeams = 2;
    InitTeams();

    s_armed = 1;
    s_match = 0;
    s_winner = -1;
    PlayFriendly();
    s_armed = 0;

    if (s_match) {                  /* kicked off: the round is played */
        s_run.goals[round][0] = s_goals[0];
        s_run.goals[round][1] = s_goals[1];
        if (s_winner == 1) {
            s_run.result[round] = RESULT_WON;
            s_run.round++;
        } else {                    /* lost, or left before the final whistle */
            s_run.result[round] = s_winner < 0 ? RESULT_LEFT : RESULT_LOST;
            s_run.out = 1;
        }
#ifdef SWOS_SDL_BUILD
        char name[kNameBytes + 1];
        fprintf(stderr, "[challenge-cup] %s v %s: %u-%u, %s\n", k_roundNames[round],
                team_name(&s_run.opponent[round], name), (unsigned)s_goals[0],
                (unsigned)s_goals[1], s_winner == 1 ? "through" : s_winner < 0 ? "left" : "out");
#endif
    }
    s_match = 0;
    _teamsLoaded = 0;
    restore_menus();
}

void ChallengeCup_TakeMatchRules(void)
{
    if (!s_armed)
        return;
    StringCopy((uint8_t *)k_cups[s_run.cup].title, _gameName);
    StringCopy((uint8_t *)k_roundNames[s_run.round], _gameRound);
    _extraTimeState = 1;
    _penaltiesState = 1;
}

void ChallengeCup_BeginMatch(void)
{
    s_match = s_armed;
    s_winner = -1;
    s_goals[0] = s_goals[1] = 0;
}

void ChallengeCup_FinalWhistle(uint16_t goals1, uint16_t goals2, int32_t winner)
{
    if (!s_match)
        return;
    s_goals[0] = goals1;
    s_goals[1] = goals2;
    s_winner = winner;
}

int32_t ChallengeCup_InMatch(void)
{
    return s_match;
}

/* ---- the screens: _specialsMenu ---- */

enum { SCREEN_SPECIALS, SCREEN_CUP };
enum { ACT_NONE, ACT_EXIT, ACT_PLAY, ACT_NEW, ACT_CUP /* + k_cups index */ };
enum {
    ROW_HEADER = 0, ROW_TEAM = 1, ROW_FIRST_ROUND = 2,          /* 2..7: the rounds */
    ROW_STATUS = 8, ROW_PLAY = 9, ROW_NEXT = 10, ROW_EXIT = 11,
    ROW_SPECIAL_FIRST = 2,          /* SPECIALS: each cup's button, its line under it */
    ROW_SPECIAL_RULES = 8,
};
enum {                              /* menu palette (docs/LOCAL_WORKFLOW.md) */
    COL_NONE = 0, COL_ORANGE = 6, COL_RED = 10, COL_BLUE = 13, COL_GREEN = 14,
    COL_EXIT = 12, COL_HEADER = 0x17,
};

static int32_t s_screen, s_action, s_actions[kRows];
static int32_t s_requested;
static char    s_text[kRows][48];

static void row(int32_t n, int32_t y, const char *label, int32_t action, int32_t color)
{
    MenuEntry *e = CalcMenuEntryAddress((uint16_t)n);
    swos_io_snprintf(s_text[n], sizeof s_text[n], "%s", label);
    e->fg = (uintptr_t)s_text[n];
    e->x = 8; e->y = (uint16_t)y;
    e->dwWidth = 304; e->dwHeight = 14;
    e->isInvisible = 0;
    e->isDisabled = action == ACT_NONE;
    e->bgColor = (uint16_t)color;
    s_actions[n] = action;
}

/* The team a row shows the stars of: the underdog, or a round's opponent. */
static const TeamFileEx *row_team(uint16_t ordinal)
{
    if (ordinal == ROW_TEAM)
        return &s_run.underdog;
    if (ordinal >= ROW_FIRST_ROUND && ordinal < ROW_FIRST_ROUND + CHALLENGE_CUP_ROUNDS)
        return &s_run.opponent[ordinal - ROW_FIRST_ROUND];
    return 0;
}

/* The squad screen's value stars at the row's right end. */
static void draw_row_stars(MenuEntry *e)
{
    const TeamFileEx *team = row_team(e->ordinal);
    SpriteGraphics *spr;
    int32_t frame;

    if (!team)
        return;
    frame = TEAM_STARS_FIRST_FRAME + TeamStars_HalfStars(team) - 1;
    spr = ((SpriteGraphics **)_spriteGraphicsPtr)[frame];
    DrawSprite16Pixels((uint16_t)frame, (int16_t)(e->x + e->dwWidth - 4 - spr->pixWidth),
                       (int16_t)(e->y + (e->dwHeight - spr->nlines) / 2));
}

static void cup_screen(void)
{
    char buf[48], name[kNameBytes + 1];
    int32_t i, done = s_run.round == CHALLENGE_CUP_ROUNDS;

    row(ROW_HEADER, 0, k_cups[s_run.cup].title, ACT_NONE, COL_HEADER);
    row(ROW_TEAM, 20, team_name(&s_run.underdog, name), ACT_NONE, COL_NONE);
    CalcMenuEntryAddress(ROW_TEAM)->afterDrawCb = draw_row_stars;
    for (i = 0; i < CHALLENGE_CUP_ROUNDS; i++) {
        int32_t color = s_run.result[i] == RESULT_WON  ? COL_GREEN
                      : s_run.result[i] != RESULT_NONE ? COL_RED
                      : i == s_run.round && !s_run.out ? COL_ORANGE : COL_BLUE;
        swos_io_snprintf(buf, sizeof buf, "%s: %s", k_roundNames[i],
                         team_name(&s_run.opponent[i], name));
        row(ROW_FIRST_ROUND + i, 42 + 17 * i, buf, ACT_NONE, color);
        CalcMenuEntryAddress((uint16_t)(ROW_FIRST_ROUND + i))->afterDrawCb = draw_row_stars;
    }

    if (done) {
        swos_io_snprintf(buf, sizeof buf, "CHAMPIONS! WON THE FINAL %u-%u",
                         (unsigned)s_run.goals[CHALLENGE_CUP_ROUNDS - 1][0],
                         (unsigned)s_run.goals[CHALLENGE_CUP_ROUNDS - 1][1]);
    } else if (s_run.out) {
        int32_t r = s_run.round;
        uint16_t g1 = s_run.goals[r][0], g2 = s_run.goals[r][1];
        if (s_run.result[r] == RESULT_LEFT)
            swos_io_snprintf(buf, sizeof buf, "MATCH LEFT: OUT IN THE %s", k_roundNames[r]);
        else if (g1 == g2)
            swos_io_snprintf(buf, sizeof buf, "%u-%u, LOST ON PENALTIES: OUT", (unsigned)g1,
                             (unsigned)g2);
        else
            swos_io_snprintf(buf, sizeof buf, "LOST %u-%u: OUT IN THE %s", (unsigned)g1,
                             (unsigned)g2, k_roundNames[r]);
    } else if (s_run.round > 0) {
        int32_t r = s_run.round - 1;
        uint16_t g1 = s_run.goals[r][0], g2 = s_run.goals[r][1];
        swos_io_snprintf(buf, sizeof buf, g1 == g2 ? "%u-%u, WON ON PENALTIES" : "WON %u-%u",
                         (unsigned)g1, (unsigned)g2);
    } else {
        swos_io_snprintf(buf, sizeof buf, "WIN SIX TIES TO LIFT THE CUP");
    }
    row(ROW_STATUS, 145, buf, ACT_NONE, done ? COL_GREEN : s_run.out ? COL_RED : COL_NONE);
    CalcMenuEntryAddress(ROW_STATUS)->dwHeight = 9;
    if (!done && !s_run.out) {      /* how the next opponent lines up */
        const TeamFileEx *next = &s_run.opponent[s_run.round];
        swos_io_snprintf(buf, sizeof buf, "NEXT: %s, PLAYING %s", team_name(next, name),
                         k_formationNames[next->team.tactics]);
        row(ROW_NEXT, 156, buf, ACT_NONE, COL_NONE);
        CalcMenuEntryAddress(ROW_NEXT)->dwHeight = 9;
    }

    if (done || s_run.out) {
        row(ROW_PLAY, 168, "NEW DRAW", ACT_NEW, COL_GREEN);
    } else {
        swos_io_snprintf(buf, sizeof buf, "PLAY %s", k_roundNames[s_run.round]);
        row(ROW_PLAY, 168, buf, ACT_PLAY, COL_GREEN);
    }
    row(ROW_EXIT, 185, "EXIT", ACT_EXIT, COL_EXIT);
}

static void specials_screen(void)
{
    int32_t i;

    row(ROW_HEADER, 0, "SPECIALS", ACT_NONE, COL_HEADER);
    for (i = 0; i < kCups; i++) {
        int32_t button = ROW_SPECIAL_FIRST + 3 * i, y = 50 + 42 * i;
        row(button, y, k_cups[i].title, ACT_CUP + i, COL_BLUE);
        row(button + 1, y + 18, k_cups[i].about, ACT_NONE, COL_NONE);
        CalcMenuEntryAddress((uint16_t)(button + 1))->dwHeight = 9;
    }
    row(ROW_SPECIAL_RULES, 145, "SIX CUP TIES, ONE DEFEAT AND YOU ARE OUT", ACT_NONE, COL_NONE);
    CalcMenuEntryAddress(ROW_SPECIAL_RULES)->dwHeight = 9;
    row(ROW_EXIT, 185, "EXIT", ACT_EXIT, COL_EXIT);
}

void Specials_MenuInit(void)
{
    int32_t i, first = -1, prev = -1;

    for (i = 0; i < kRows; i++) {
        MenuEntry *e = CalcMenuEntryAddress((uint16_t)i);
        e->isInvisible = e->isDisabled = 1;
        e->leftEntry = e->rightEntry = e->upEntry = e->downEntry = -1;
        e->afterDrawCb = 0;
        s_actions[i] = ACT_NONE;
    }
    if (s_screen == SCREEN_CUP)
        cup_screen();
    else
        specials_screen();
    for (i = 0; i < kRows; i++)         /* the buttons, top to bottom */
        if (s_actions[i]) {
            if (first < 0)
                first = i;
            if (prev >= 0) {
                CalcMenuEntryAddress((uint16_t)prev)->downEntry = (int8_t)i;
                CalcMenuEntryAddress((uint16_t)i)->upEntry = (int8_t)prev;
            }
            prev = i;
        }
    SetCurrentEntry((uint16_t)first);
#ifdef SWOS_SDL_BUILD
    {   /* one packed menu, two screens: name this one in the [menu] show line */
        extern void MenuLog_Screen(const char *name);
        MenuLog_Screen(s_screen == SCREEN_CUP ? k_cups[s_run.cup].log : "specials/specials");
    }
#endif
}

void Specials_MenuSelect(MenuEntry *entry)
{
    if (entry->ordinal >= kRows)
        return;
    s_action = s_actions[entry->ordinal];
    if (s_action)
        SetExitMenuFlag();
}

void Specials_Request(void)
{
    s_requested = 1;
}

int32_t Specials_TakeRequest(void)
{
    int32_t r = s_requested;
    s_requested = 0;
    return r;
}

int32_t Specials_Show(void)
{
    s_screen = SCREEN_SPECIALS;
    while (!_quitToDos) {
        s_action = ACT_NONE;
        ShowMenu(_specialsMenu);
        if (_quitToDos)
            break;
        if (s_screen == SCREEN_SPECIALS) {
            if (s_action < ACT_CUP || s_action >= ACT_CUP + kCups)
                return 1;               /* EXIT: back to the world list */
            if (new_run(s_action - ACT_CUP))
                s_screen = SCREEN_CUP;
        } else if (s_action == ACT_PLAY) {
            play_round();
        } else if (s_action == ACT_NEW) {
            new_run(s_run.cup);
        } else {
            s_screen = SCREEN_SPECIALS; /* EXIT: the run is given up */
        }
    }
    return 0;
}
