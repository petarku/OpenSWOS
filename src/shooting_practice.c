/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* shooting_practice.c — SHOOTING PRACTICE training mode. See shooting_practice.h. */

#include "shooting_practice.h"
#include "ball_physics.h"    /* GetPlayerPointerFromShirtNumber -- which line a player is on */

int32_t g_shootingPracticeMode;

/* The start screen's choices (src/shooting_practice_menu.c). The lines: 1 = that
 * side keeps its players of that line, 0 = they are taken off. The player's own
 * side has all three by default, the computer's none -- the drill as it always
 * was: shoot past a lone keeper. */
uint16_t g_practicePlayerDefense  = 1;
uint16_t g_practicePlayerMidfield = 1;
uint16_t g_practicePlayerAttack   = 1;
uint16_t g_practiceCpuDefense;
uint16_t g_practiceCpuMidfield;
uint16_t g_practiceCpuAttack;
/* 0 = attack up (default), 1 = attack down. */
uint16_t g_shootingPracticeAttackDown;

extern uint16_t _team1Computer;
extern uint16_t _team2Computer;
extern uint16_t _numSelectedTeams;
extern TeamFileEx _selectedTeams[];
extern TeamGeneralInfo _topTeamData;
extern TeamGeneralInfo _bottomTeamData;

/* The friendly flow this mode borrows whole (src/menus.c) — FriendlyFlow, not
 * FriendlyMenu: this handler runs under the 4-entry PRACTICE submenu, where
 * FriendlyMenu's closing CommonMenuExit would write through main-menu entries
 * that are not there (docs/MENUS.md §12). The submenu's opener,
 * PracticeMenuSelected, makes that call once, after it closes. */
extern void FriendlyFlow(void);

/* teams.c's names for TeamFile.teamControls, which has no shared header. */
#define TEAM_NOT_SELECTED 0
#define TEAM_COMPUTER     1

/* The PRACTICE submenu's SHOOTING PRACTICE button.
 *
 * The whole mode is "a friendly, with the flag set". FriendlyFlow seeds the
 * RNG, saves and restores gameType around the flow and invalidates the team
 * cache so the country lists reload pristine (ShowFriendlyMenu, menus.c, where
 * that last one is a deliberate fix over an original SWOS bug) -- all of which
 * this mode needs exactly as a friendly does, so it reuses them rather than
 * repeating them. ShowFriendlyMenu puts up this mode's own start screen
 * (_shootingPracticeSetupMenu, SWOS 2020's TEAM TRAINING layout, since
 * 2026-11-02) in place of the friendly's, and the screens after it read
 * g_shootingPracticeMode to say PRACTICE where they would say FRIENDLY.
 *
 * Clearing on the way out is what makes the flag safe: every path back to the
 * submenu, including an abort, returns through here. */
__attribute__((used, externally_visible))
void ShootingPracticeMenu(void)
{
    g_shootingPracticeMode = 1;
    FriendlyFlow();
    g_shootingPracticeMode = 0;
    ShootingPractice_BeginMatch(); /* Clear the latch before menus/attract replays. */
}

int16_t ShootingPractice_SelectionOk(void)
{
    int16_t cpu = 0;
    int16_t human = 0;
    int16_t i;

    for (i = 0; i < (int16_t)_numSelectedTeams; i++) {
        uint8_t ctrl = _selectedTeams[i].team.teamControls;
        if (ctrl == TEAM_COMPUTER)
            cpu++;
        else if (ctrl != TEAM_NOT_SELECTED)
            human++;      /* PLAYER-COACH or COACH: a side somebody is running */
    }
    return (cpu == 1 && human == 1);
}

/* 0 = inactive, else the computer side's team number (1 or 2), LATCHED for the
 * whole match by ShootingPractice_BeginMatch.
 *
 * It is latched rather than recomputed because the match must not depend on a
 * menu global at all: reading the mode live meant the restart after a goal
 * could find it "off" and hand the kickoff back to a side that has no outfield
 * players to take it. That was a real bug when the mode was a menu toggle that
 * cleared itself; the latch is kept because it is the right shape regardless --
 * what the match plays by is decided once, at kick-off. */
static int16_t s_cpuTeamNum;

/* Latched beside it: which lines each side keeps in THIS match (the start
 * screen's switches at the moment of kick-off), and the direction. */
static int16_t s_humanLines;     /* LINE_* bits */
static int16_t s_cpuLines;
static int16_t s_attackDown;

#define LINE_DEFENSE  1
#define LINE_MIDFIELD 2
#define LINE_ATTACK   4

static int16_t cpuTeamNum(void)
{
    return s_cpuTeamNum;
}

void ShootingPractice_BeginMatch(void)
{
    s_cpuTeamNum = 0;
    s_humanLines = 0;
    s_cpuLines = 0;
    s_attackDown = 0;
    if (!g_shootingPracticeMode)
        return;
    /* Exactly one side must be the computer. The team selection will not start a
     * drill otherwise (ShootingPractice_SelectionOk), but this must never apply
     * to CPU-v-CPU or human-v-human whatever got us here. */
    if (_team1Computer && !_team2Computer)
        s_cpuTeamNum = 1;
    else if (_team2Computer && !_team1Computer)
        s_cpuTeamNum = 2;
    s_humanLines = (int16_t)((g_practicePlayerDefense  ? LINE_DEFENSE  : 0) |
                             (g_practicePlayerMidfield ? LINE_MIDFIELD : 0) |
                             (g_practicePlayerAttack   ? LINE_ATTACK   : 0));
    s_cpuLines   = (int16_t)((g_practiceCpuDefense  ? LINE_DEFENSE  : 0) |
                             (g_practiceCpuMidfield ? LINE_MIDFIELD : 0) |
                             (g_practiceCpuAttack   ? LINE_ATTACK   : 0));
    /* The start screen never lets both sides down to their keepers; should it
     * ever happen, the player keeps their whole side rather than nobody being
     * able to kick off. */
    if (!s_humanLines && !s_cpuLines)
        s_humanLines = LINE_DEFENSE | LINE_MIDFIELD | LINE_ATTACK;
    s_attackDown = (g_shootingPracticeAttackDown != 0);
}

int16_t ShootingPractice_IsActive(void)
{
    return s_cpuTeamNum != 0;
}

int16_t ShootingPractice_ForcedTopTeamNum(void)
{
    /* Despite its name, _teamPlayingUp identifies the team occupying the
     * TOP half and attacking DOWN (InitTeamsData / PrepareForInitialKick). */
    return s_cpuTeamNum ? (s_attackDown ? 3 - s_cpuTeamNum : s_cpuTeamNum) : 0;
}

int16_t ShootingPractice_ForcedKickoffTeamNum(void)
{
    /* The side with outfield players takes every kickoff -- a keeper alone
     * cannot. That is the player's, unless the player has taken all their lines
     * off: then only their keeper defends the goal and the computer kicks off every
     * time, after its own goals too. */
    int16_t cpu = cpuTeamNum();
    if (!cpu)
        return 0;
    return s_humanLines ? (int16_t)(3 - cpu) : cpu;
}

static int16_t lineOf(int8_t pos)
{
    switch (pos) {
    case POS_RIGHT_BACK: case POS_LEFT_BACK: case POS_DEFENDER:
        return LINE_DEFENSE;
    case POS_MIDFIELDER: case POS_LEFT_WING: case POS_RIGHT_WING:
        return LINE_MIDFIELD;
    case POS_ATTACKER:
        return LINE_ATTACK;
    default:
        return 0;
    }
}

static void stripTeam(TeamGeneralInfo *team, int16_t keepLines)
{
    Sprite * const *sprites = team->spritesTable;
    int16_t i;

    /* index 0 is the goalkeeper (goalkeeper.c: "first sprite = goalkeeper") —
     * always stays. 1..10 are the ten outfield players: one whose position
     * (PlayerInfo.position, via the sprite's playerOrdinal) is on a line the
     * side keeps stays, so what stays follows the team's actual line-up. */
    for (i = 1; i <= 10; i++) {
        Sprite *pl = sprites[i];
        if (keepLines) {
            PlayerInfo *pi = GetPlayerPointerFromShirtNumber(pl, team);
            if (lineOf(pi->position) & keepLines)
                continue;
        }
        pl->sentAway = 1;
        pl->destX    = -20;
        pl->destY    = 449;
        pl->x.s.i    = -20;
        pl->y.s.i    = 449;
    }
}

void ShootingPractice_StripOutfieldPlayers(void)
{
    int16_t cpu = cpuTeamNum();
    TeamGeneralInfo *cpuTeam, *humanTeam;
    if (!cpu)
        return;

    cpuTeam   = (_topTeamData.teamNumber == cpu) ? &_topTeamData : &_bottomTeamData;
    humanTeam = (cpuTeam == &_topTeamData) ? &_bottomTeamData : &_topTeamData;
    if (s_cpuLines != (LINE_DEFENSE | LINE_MIDFIELD | LINE_ATTACK))
        stripTeam(cpuTeam, s_cpuLines);
    if (s_humanLines != (LINE_DEFENSE | LINE_MIDFIELD | LINE_ATTACK))
        stripTeam(humanTeam, s_humanLines);
}
