#pragma once

/* ai-sidegoal-improvements branch: the computer side plays the MOZG tactics set in place
 * of the twelve built-in formations (src/cpu_tactics.c). The data is generated into
 * src/cpu_tactics_data.c from the 31 MOZG .TAC files. */

#include "game_types.h"

#define CPU_TACTICS_MOZG_COUNT  31
#define CPU_TACTICS_GROUP_MAX   5

/* The 31 MOZG tactics, each a raw 370-byte TeamTactics record. */
extern const uint8_t _cpuTacticsMozg[CPU_TACTICS_MOZG_COUNT][370];
/* Per built-in formation (_teamTacticsTable order 0..11): the MOZG tactics that rotate
 * in its place, -1 padded. An empty group (SWEEP) keeps the built-in one. */
extern const int8_t  _cpuTacticsGroups[12][CPU_TACTICS_GROUP_MAX];

/* The tactics a COMPUTER team plays this frame in place of its built-in formation, or
 * NULL to play the formation as it is (a human or coach-mode team, a user tactic, SWEEP). */
TeamTactics *CpuTactics_For(TeamGeneralInfo *team);
