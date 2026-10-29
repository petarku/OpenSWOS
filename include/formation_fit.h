/* formation_fit.h -- how well a formation suits a team's players, by the game's own measure.
 *
 * The verdict the tactics ticks and crosses show (docs/TACTICS_POSITION_VALUE.md): each of the
 * starting eleven scored by how his skills suit the areas the formation sends him to
 * (GetPlayerPrice), and the team's average of that (ComputeOldPlayerSkillForViewResult).
 * Higher is better. The team is scored as it is -- a COMPUTER side the way the game scores a
 * computer side -- on a copy, and the globals those routines leave their results in are put
 * back, so asking changes nothing on screen. Both targets. */
#ifndef FORMATION_FIT_H
#define FORMATION_FIT_H

#include "game_types.h"

/* The ten normal formations, 4-4-2 .. 5-2-3 (TACTIC_DEFAULT .. TACTIC_5_2_3): what an
 * opponent may be given. ATTACK, DEFEND and the USER slots are not among them. */
#define FORMATION_FIT_NORMAL 10

/* The team's value in `formation`; `values` (32 bytes, indexed like _playerPositionsArray:
 * by player file number) gets each player's, if not NULL. */
uint16_t FormationFit_Value(const TeamFileEx *team, int32_t formation, uint8_t *values);

/* The same, finer: the sum of the starting eleven's own values (the team value is that
 * divided by 15 and rounded down, so it hides small differences between formations). */
int32_t FormationFit_Sum(const TeamFileEx *team, int32_t formation);

#endif /* FORMATION_FIT_H */
