/* team_stars.h -- how strong a team is, in the stars the squad screen draws.
 *
 * A player's value stars are half a star apart, one to five stars (InitPlayerValueDisplayStruct,
 * teams.c: frames 113..122). A team's rating is the average of its starting eleven's, in the
 * same half stars, so a team rated "3.5" is one whose eleven the player sees averaging three
 * and a half stars on VIEW OPPO. Both targets.
 *
 * scripts/team_stars.py rates every team of the database the same way, sorted, for choosing
 * opponents by strength (docs/CHALLENGE_CUP.md). */
#ifndef TEAM_STARS_H
#define TEAM_STARS_H

#include "game_types.h"

/* The squad screen's first value-star frame: half a star. Frame + n is n + 1 half stars. */
#define TEAM_STARS_FIRST_FRAME 113

/* One player's value stars as the squad screen shows a team's file: 1..10 half stars,
 * from the price alone (no form: a team file is not the career squad). */
int32_t TeamStars_PlayerHalfStars(const PlayerFile *player);

/* The starting eleven's average, rounded to the nearest half star: 1..10. */
int32_t TeamStars_HalfStars(const TeamFileEx *team);

#endif /* TEAM_STARS_H */
