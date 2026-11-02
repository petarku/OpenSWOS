/* random_team.h -- drawing teams at random for the training modes' start screens
 * (PENALTY SHOOTOUT's RANDOM SELECTION, SHOOTING PRACTICE's RANDOM TEAM).
 *
 * The groups are the team-selection screen's own: the five club continents'
 * country lists (_europeTable ... _oceaniaTable), NATIONAL (team files 80..85),
 * ORIGINAL (TEAM.072, ORIGINAL CUSTOM TEAMS), CUSTOM (CUSTOMS.EDT, file 100);
 * ALL is every club and national file. A draw uses its own generator, seeded
 * from the clock, and leaves the match RNG alone. Both targets. */
#ifndef RANDOM_TEAM_H
#define RANDOM_TEAM_H

#include "game_types.h"

enum {
    RT_GROUP_ALL, RT_GROUP_EUROPE, RT_GROUP_AFRICA, RT_GROUP_NATIONAL,
    RT_GROUP_NORTH_AMERICA, RT_GROUP_ASIA, RT_GROUP_ORIGINAL,
    RT_GROUP_SOUTH_AMERICA, RT_GROUP_OCEANIA, RT_GROUP_CUSTOM, RT_GROUP_COUNT
};

/* Largest file list a group returns. */
#define RT_MAX_FILES 96

/* Reseed the draw from the clock; call once per button press. */
void RandomTeam_Seed(void);

/* The team files a group draws from, into files[RT_MAX_FILES]; returns how many. */
int16_t RandomTeam_GroupFiles(int16_t group, uint8_t *files);

/* One team of the files, not `other` (may be NULL): a random file, then a random
 * team of it. 1 = drawn, 0 = none found, -1 = a file would not load
 * (LoadTeamFile has shown why). */
int16_t RandomTeam_Draw(const uint8_t *files, int16_t numFiles, TeamFileEx *out,
                        const TeamFileEx *other);

/* As RandomTeam_Draw, but a team whose strength (TeamStars_HalfStars, 1..10) is
 * in [minHalfStars, maxHalfStars]: random files are scanned whole for a team in
 * the band. Never comes back empty-handed while any team loads -- after its
 * tries it takes the closest strength it saw. */
int16_t RandomTeam_DrawInBand(const uint8_t *files, int16_t numFiles, TeamFileEx *out,
                              const TeamFileEx *other,
                              int16_t minHalfStars, int16_t maxHalfStars);

#endif /* RANDOM_TEAM_H */
