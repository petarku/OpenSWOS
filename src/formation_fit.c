/* formation_fit.c -- how well a formation suits a team's players. See formation_fit.h. */

#include <string.h>

#include "formation_fit.h"

extern uint8_t  _playerPositionsArray[];
extern uint16_t _teamSkillCurrent;
extern uint16_t ComputeOldPlayerSkillForViewResult(TeamFileEx *team, uint16_t goalsCount);

uint16_t FormationFit_Value(const TeamFileEx *team, int32_t formation, uint8_t *values)
{
    static TeamFileEx copy;             /* 684 bytes: not on the Amiga's stack */
    uint8_t  keptValues[32];
    uint16_t keptSkill = _teamSkillCurrent, value;

    memcpy(keptValues, _playerPositionsArray, sizeof keptValues);
    copy = *team;
    copy.team.tactics = (uint8_t)formation;
    copy.team.playerValuesDirty = TEAM_VALUES_DIRTY;    /* score it, not the cached values */
    value = ComputeOldPlayerSkillForViewResult(&copy, 0);
    if (values)
        memcpy(values, _playerPositionsArray, sizeof keptValues);
    memcpy(_playerPositionsArray, keptValues, sizeof keptValues);
    _teamSkillCurrent = keptSkill;
    return value;
}

int32_t FormationFit_Sum(const TeamFileEx *team, int32_t formation)
{
    uint8_t values[32];
    int32_t i, sum = 0;

    FormationFit_Value(team, formation, values);
    for (i = 0; i < 11; i++) {
        uint8_t n = team->team.playerNumbers[i];
        if (n < 32)
            sum += values[n];
    }
    return sum;
}
