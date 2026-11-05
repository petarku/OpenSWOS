/* challenge_cup.h -- CHALLENGE CUP and NATIONAL CHALLENGE CUP: the weakest club (ONCE LOBOS)
 * or national team (BERMUDA) in the game against stronger and stronger opponents, one match
 * a round (src/challenge_cup.c, docs/CHALLENGE_CUP.md).
 *
 * PRESET COMPETITION -> SPECIALS (the row under CLASSICS in the world list) -> either cup.
 * Six rounds, a cup tie each: extra time, then penalties, on a draw; a defeat ends the run.
 * Both targets. */
#ifndef CHALLENGE_CUP_H
#define CHALLENGE_CUP_H

#include "swos_types.h"

#define CHALLENGE_CUP_ROUNDS 6

/* One team of an opponent band (src/challenge_cup_pool.c, from scripts/team_stars.py). */
typedef struct ChallengeCupPoolTeam {
    uint8_t     country;        /* its team file, TEAM.nnn */
    uint8_t     ordinal;        /* its number in that file */
    const char *name;           /* checked when it is loaded */
} ChallengeCupPoolTeam;

typedef struct ChallengeCupPool {
    const ChallengeCupPoolTeam *teams;
    int32_t                     count;
    int32_t                     halfStars;  /* the band's rating, for the docs; the screen
                                             * shows each team's own */
} ChallengeCupPool;

/* Rounds 2..6 of each cup: round 1's opponent comes from the underdog's own division. */
extern const ChallengeCupPool g_challengeCupPools[CHALLENGE_CUP_ROUNDS - 1];  /* clubs */
extern const ChallengeCupPool g_nationalCupPools[CHALLENGE_CUP_ROUNDS - 1];   /* nations */

/* SPECIALS in PRESET COMPETITION's world list (teams.c): the chooser closes as if backed
 * out of, and PresetCompetitionMenu then opens the SPECIALS screen. */
void    Specials_Request(void);
int32_t Specials_TakeRequest(void);
/* The SPECIALS screen and whatever is picked on it, until its EXIT (1: back to the world
 * list) or the game quitting (0). */
int32_t Specials_Show(void);

/* The match hooks. TakeMatchRules: InitAndPlayGame, after it has cleared the tie rules --
 * names the match and makes it a cup tie. BeginMatch: at kick-off, beside the training
 * modes' latches. FinalWhistle: EndOfGame. InMatch: the friendly's REPLAY / EXIT menu is
 * not offered (a round is played once). */
void    ChallengeCup_TakeMatchRules(void);
void    ChallengeCup_BeginMatch(void);
void    ChallengeCup_FinalWhistle(uint16_t goals1, uint16_t goals2, int32_t winner);
int32_t ChallengeCup_InMatch(void);

#endif /* CHALLENGE_CUP_H */
