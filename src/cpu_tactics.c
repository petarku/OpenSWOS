/* cpu_tactics.c -- ai-sidegoal-improvements branch (an experiment, not for main).
 *
 * A COMPUTER team plays the MOZG tactics set in place of the built-in formations. Each
 * built-in formation has a group of MOZG versions with the same shape by name
 * (defenders - defensive mid - attacking mid - forwards; include/cpu_tactics_data.h has
 * the groups). Which one it plays:
 *
 *   - one is picked for the match from the two teams' names (not from the game's random
 *     numbers, so nothing else in the match changes);
 *   - winning or drawing: it stays;
 *   - losing: it moves on one in the group per goal behind, and one more in the second
 *     half while still behind. Pulling level brings back the one it started with.
 *
 * It is worked out from the score every time, so there is no state to save, replay or
 * roll back. Only the positioning of players without the ball reads it
 * (SetPlayerWithNoBallDestination); the redirect for a keeper's ball / goal kick is the
 * MOZG file's own (an index into the built-in table, as the files were made for). */

#include "cpu_tactics_data.h"
#include "game_types.h"
#ifdef SWOS_SDL_BUILD
#include <stdio.h>
#endif

extern TeamGame  _topTeamInGame;
extern uint16_t  _team1TotalGoals;
extern uint16_t  _team2TotalGoals;
extern uint16_t  _halfNumber;

static uint16_t name_hash(const TeamGame *t)
{
    uint16_t h = 0;
    int16_t i;
    for (i = 0; i < 17 && t->teamName[i]; i++)
        h = (uint16_t)(h * 31 + (uint8_t)t->teamName[i]);
    return h;
}

TeamTactics *CpuTactics_For(TeamGeneralInfo *team)
{
    const int8_t *group;
    int16_t count, ours, theirs, behind, pick;
    uint16_t seed;
    TeamGame *opp;

    if (team->playerNumber != 0 || team->isPlCoach != 0)
        return (TeamTactics *)0;
    if (team->tactics < 0 || team->tactics >= 12)
        return (TeamTactics *)0;
    group = _cpuTacticsGroups[team->tactics];
    for (count = 0; count < CPU_TACTICS_GROUP_MAX && group[count] >= 0; count++)
        ;
    if (count == 0)
        return (TeamTactics *)0;

    if (team->inGameTeamPtr == &_topTeamInGame) {
        ours = (int16_t)_team1TotalGoals;
        theirs = (int16_t)_team2TotalGoals;
    } else {
        ours = (int16_t)_team2TotalGoals;
        theirs = (int16_t)_team1TotalGoals;
    }
    behind = theirs > ours ? (int16_t)(theirs - ours) : 0;
    if (behind && _halfNumber >= 2)
        behind++;

    opp = team->opponentsTeam ? team->opponentsTeam->inGameTeamPtr : (TeamGame *)0;
    seed = (uint16_t)(name_hash(team->inGameTeamPtr) * 7 + (opp ? name_hash(opp) : 0));

    /* Each "losing event" (a goal that leaves us further behind; still behind in the
     * second half) switches only about one time in three. The dice are the match's own
     * seed and the event's number, so the same events always give the same answer. */
    {
        int16_t k, switches = 0;
        for (k = 1; k <= behind; k++) {
            uint16_t h = (uint16_t)(seed ^ (uint16_t)(k * 0x9E37u));
            h = (uint16_t)(h * 0x6255u + 0x3619u);
            if ((h >> 8) % 3 == 0)
                switches++;
        }
        pick = (int16_t)((seed + (uint16_t)switches) % (uint16_t)count);
    }

#ifdef SWOS_SDL_BUILD
    {
        /* The log only: which version each side is on, said when it changes. */
        static int16_t s_last[2] = { -1, -1 };
        int16_t side = team->inGameTeamPtr == &_topTeamInGame ? 0 : 1;
        int16_t which = (int16_t)(team->tactics * 8 + pick);
        if (s_last[side] != which) {
            s_last[side] = which;
            fprintf(stderr, "[cpu-tactics] %.17s (%d-%d): formation %d -> MOZG %.9s\n",
                    (const char *)team->inGameTeamPtr->teamName, ours, theirs, team->tactics,
                    (const char *)_cpuTacticsMozg[group[pick]]);
        }
    }
#endif
    return (TeamTactics *)(void *)_cpuTacticsMozg[group[pick]];
}
