/* team_stars.c -- a team's strength in value stars. See team_stars.h. */

#include "team_stars.h"

extern int8_t _avgTeamValues[];

/* GetDisplayedPlayerValue's path for a player outside the career squads (teams.c):
 * the value table entry for the price, 1..80. Then InitPlayerValueDisplayStruct's
 * (value - 1) / 8, one frame per half star. */
int32_t TeamStars_PlayerHalfStars(const PlayerFile *player)
{
    int32_t price = player->price;
    int32_t value = price < 0 ? 1 : _avgTeamValues[price];

    if (value < 1)
        value = 1;
    if (value > 80)
        value = 80;
    return (value - 1) / 8 + 1;
}

int32_t TeamStars_HalfStars(const TeamFileEx *team)
{
    int32_t i, sum = 0;

    for (i = 0; i < 11; i++) {
        uint8_t n = team->team.playerNumbers[i];
        sum += TeamStars_PlayerHalfStars(&team->players[n < 16 ? n : i]);
    }
    return (sum * 2 + 11) / 22;     /* sum / 11, rounded half up */
}
