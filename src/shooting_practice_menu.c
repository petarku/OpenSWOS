/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* shooting_practice_menu.c — SHOOTING PRACTICE's start screen
 * (_shootingPracticeSetupMenu, menu_data.c), SWOS 2020's TEAM TRAINING layout
 * (asked for 2026-11-02, without its TACKLE ALLOWED, CPU GHOSTED, INSTRUCTION
 * rows and its pitch picture):
 *
 *                      SHOOTING PRACTICE
 *        ATTACK GOAL      UP / DOWN
 *        CPU SKILL        RANDOM / BEGINNER / AMATEUR / PROFESSIONAL / WORLD CLASS
 *                         PLAYER    CPU
 *        DEFENSE            ON      OFF
 *        MIDFIELD           ON      OFF
 *        ATTACK             ON      OFF
 *        PITCH TYPE       RANDOM ...
 *        CHOOSE TEAM                RANDOM TEAM
 *                            EXIT
 *
 * ShowFriendlyMenu shows it IN PLACE of the friendly's own screen while the mode
 * is on (as PENALTY SHOOTOUT's start screen is), so the friendly flow around it
 * is unchanged. CHOOSE TEAM is the friendly's own ChooseTeamsMenu, wrapped so it
 * comes back here (TrainingChooseTeamsMenu, menus.c). RANDOM TEAM
 * draws the player's side from every club and national team and the computer's
 * from the CPU SKILL band, and plays them as ChooseTeamsMenu plays a chosen pair
 * (InitTeams + PlayFriendly), so the PLAY MATCH screen still comes first.
 *
 * The line switches say which outfield players each side keeps
 * (ShootingPractice_StripOutfieldPlayers). Both sides down to their keepers is
 * never allowed: the switch that would do it does not turn off. */

#include "shooting_practice.h"
#include "menu_system.h"
#include "random_team.h"

uint16_t g_shootingPracticeCpuSkill;

extern MenuEntry _menuEntries[];
extern TeamFileEx _selectedTeams[];
extern uint16_t _numSelectedTeams;
extern uint16_t _teamsLoaded;
extern uint16_t _pitchType, _pitchTypeOrSeason, _friendlyPitchType;

extern void InitTeams(void);
extern void PlayFriendly(void);
extern void ShowErrorMenu(uint8_t *text);

#define TEAM_COMPUTER     1      /* TeamFile.teamControls (play_match_menu.h) */
#define TEAM_PLAYER_COACH 2
#define SEASONAL          0xFFFE /* _pitchType */
#define PINK_TO_BROWN_9   9      /* the disabled-entry colour (menus.c) */

/* Runtime ordinals (menu_data.c _shootingPracticeSetupMenu): 0 title,
 * 1 ATTACK GOAL, 2 its button, 3 CPU SKILL, 4 its button, 5 PLAYER, 6 CPU,
 * 7 DEFENSE, 8 player, 9 cpu, 10 MIDFIELD, 11 player, 12 cpu, 13 ATTACK,
 * 14 player, 15 cpu, 16 PITCH TYPE, 17 its button, 18 CHOOSE TEAM,
 * 19 RANDOM TEAM, 20 EXIT. */
#define ORD_PITCH_TYPE 17

#define NUM_SKILLS 5          /* RANDOM + the four bands */

/* The CPU SKILL bands, in TeamStars_HalfStars' half stars (1..10): the
 * database's 1,700 teams are ~590 / ~890 / ~200 / ~30 across them
 * (docs/TEAM_STARS.txt). Skill 0, RANDOM (the default), is any strength. */
static const int8_t k_skillBand[NUM_SKILLS][2] = {
    { 1, 10 },      /* RANDOM:       any team (drawn without a band) */
    { 1, 3 },       /* BEGINNER:     up to 1.5 stars */
    { 4, 5 },       /* AMATEUR:      2 - 2.5 stars */
    { 6, 7 },       /* PROFESSIONAL: 3 - 3.5 stars */
    { 8, 10 },      /* WORLD CLASS:  4 stars and up */
};

/* The switch behind each line button, by ordinal. */
static uint16_t *lineFlag(int16_t ordinal)
{
    switch (ordinal) {
    case 8:  return &g_practicePlayerDefense;
    case 9:  return &g_practiceCpuDefense;
    case 11: return &g_practicePlayerMidfield;
    case 12: return &g_practiceCpuMidfield;
    case 14: return &g_practicePlayerAttack;
    case 15: return &g_practiceCpuAttack;
    default: return (uint16_t *)0;
    }
}

/* onInit AND onReturn. The friendly's screen sets the match's pitch here
 * (FriendlyMenuReinit): the OPTIONS pitch type, which then cannot be changed
 * here either, or the player's pick when OPTIONS says SEASONAL. */
__attribute__((used, externally_visible))
void ShootingPracticeSetupInit(void)
{
    _pitchTypeOrSeason = 0;
    if (_pitchType != SEASONAL) {
        MenuEntry *e = &_menuEntries[ORD_PITCH_TYPE];
        _friendlyPitchType = _pitchType;
        e->bgColor = (uint16_t)((e->bgColor & 0xFFF0) + PINK_TO_BROWN_9);
        e->isDisabled = 1;
    }
}

/* ATTACK GOAL: UP / DOWN. */
__attribute__((used, externally_visible))
void ChangePracticeDirection(MenuEntry *entry)
{
    g_shootingPracticeAttackDown ^= 1;
    CallDrawMenuItem(entry);
}

/* CPU SKILL: RANDOM, BEGINNER, AMATEUR, PROFESSIONAL, WORLD CLASS, round again. */
__attribute__((used, externally_visible))
void ChangePracticeCpuSkill(MenuEntry *entry)
{
    g_shootingPracticeCpuSkill = (uint16_t)((g_shootingPracticeCpuSkill + 1) % NUM_SKILLS);
    CallDrawMenuItem(entry);
}

/* The six DEFENSE / MIDFIELD / ATTACK buttons: flip the switch, unless that
 * leaves both sides with only their keepers -- then nobody could kick off. */
__attribute__((used, externally_visible))
void ChangePracticeLine(MenuEntry *entry)
{
    uint16_t *flag = lineFlag((int16_t)entry->ordinal);
    if (!flag)
        return;
    *flag ^= 1;
    if (!g_practicePlayerDefense && !g_practicePlayerMidfield && !g_practicePlayerAttack &&
        !g_practiceCpuDefense && !g_practiceCpuMidfield && !g_practiceCpuAttack) {
        *flag = 1;
        return;
    }
    CallDrawMenuItem(entry);
}

/* RANDOM TEAM: the player's side from every club and national team, the
 * computer's from the CPU SKILL band (any team for RANDOM), then the PLAY
 * MATCH screen. */
__attribute__((used, externally_visible))
void ShootingPracticeRandomTeams(void)
{
    uint8_t files[RT_MAX_FILES];
    int16_t numFiles, got;
    uint16_t skill = g_shootingPracticeCpuSkill < NUM_SKILLS ? g_shootingPracticeCpuSkill : 0;

    numFiles = RandomTeam_GroupFiles(RT_GROUP_ALL, files);
    if (numFiles == 0)
        return;

    RandomTeam_Seed();
    _teamsLoaded = 0;           /* the files as they are, never a cached career's */
    got = RandomTeam_Draw(files, numFiles, &_selectedTeams[0], (TeamFileEx *)0);
    if (got > 0 && skill == 0)
        got = RandomTeam_Draw(files, numFiles, &_selectedTeams[1], &_selectedTeams[0]);
    else if (got > 0)
        got = RandomTeam_DrawInBand(files, numFiles, &_selectedTeams[1], &_selectedTeams[0],
                                    k_skillBand[skill][0], k_skillBand[skill][1]);
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
    PlayFriendly();             /* the PLAY MATCH screen, then the drill */
    /* and back to this start screen, as CHOOSE TEAM(S) comes back
     * (TrainingChooseTeamsMenu, menus.c) -- not out to the PRACTICE submenu. */
}
