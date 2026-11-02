/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* random_team.c -- see random_team.h. Moved out of penalty_shootout_menu.c
 * (2026-11-02) when SHOOTING PRACTICE's start screen got a RANDOM TEAM button
 * of its own. */

#include "random_team.h"
#include "team_stars.h"

extern uint8_t  _teamFileBuffer[];
extern volatile uint16_t _currentTick;
extern QualifyingGroup _europeTable, _africaTable, _southAmericaTable,
                       _northAmericaTable, _asiaTable, _oceaniaTable;

extern int16_t LoadTeamFile(uint16_t extNumber);

enum {
    kNationalFirst = 80, kNationalLast = 85,
    kOriginalCustomFile = 72, kEditedCustomFile = 100,
    kTries = 24,
    kBandFileTries = 32,
};

static uint32_t s_rng;

void RandomTeam_Seed(void)
{
    s_rng = s_rng * 1103515245u + 12345u;
    s_rng ^= (uint32_t)_currentTick << 16 | _currentTick;
    if (s_rng == 0)
        s_rng = 0x2545F491u;
}

static uint16_t randBelow(uint16_t n)
{
    s_rng ^= s_rng << 13;           /* xorshift32 */
    s_rng ^= s_rng >> 17;
    s_rng ^= s_rng << 5;
    return (uint16_t)(s_rng % n);
}

static int16_t addCountries(uint8_t *files, int16_t n, const QualifyingGroup *g)
{
    int16_t i;
    for (i = 0; i < 44 && g->countries[i] != 0xFF && n < RT_MAX_FILES; i++)
        files[n++] = g->countries[i];
    return n;
}

int16_t RandomTeam_GroupFiles(int16_t group, uint8_t *files)
{
    static const QualifyingGroup *const k_continents[] = {
        &_europeTable, &_africaTable, &_northAmericaTable,
        &_asiaTable, &_southAmericaTable, &_oceaniaTable,
    };
    int16_t n = 0, i;

    switch (group) {
    case RT_GROUP_EUROPE:        return addCountries(files, 0, &_europeTable);
    case RT_GROUP_AFRICA:        return addCountries(files, 0, &_africaTable);
    case RT_GROUP_NORTH_AMERICA: return addCountries(files, 0, &_northAmericaTable);
    case RT_GROUP_ASIA:          return addCountries(files, 0, &_asiaTable);
    case RT_GROUP_SOUTH_AMERICA: return addCountries(files, 0, &_southAmericaTable);
    case RT_GROUP_OCEANIA:       return addCountries(files, 0, &_oceaniaTable);
    case RT_GROUP_ORIGINAL:      files[0] = kOriginalCustomFile; return 1;
    case RT_GROUP_CUSTOM:        files[0] = kEditedCustomFile;   return 1;
    case RT_GROUP_NATIONAL:
        for (i = kNationalFirst; i <= kNationalLast; i++)
            files[n++] = (uint8_t)i;
        return n;
    default:                                /* ALL: every club and national file */
        for (i = 0; i < (int16_t)(sizeof k_continents / sizeof k_continents[0]); i++)
            n = addCountries(files, n, k_continents[i]);
        for (i = kNationalFirst; i <= kNationalLast && n < RT_MAX_FILES; i++)
            files[n++] = (uint8_t)i;
        return n;
    }
}

static int16_t sameTeam(const TeamFileEx *a, const TeamFileEx *b)
{
    int16_t i;
    if (a->team.countryNumber != b->team.countryNumber ||
        a->team.teamOrdinal != b->team.teamOrdinal)
        return 0;
    for (i = 0; i < (int16_t)sizeof a->team.teamName; i++)  /* no memcmp on the m68k */
        if (a->team.teamName[i] != b->team.teamName[i])
            return 0;
    return 1;
}

int16_t RandomTeam_Draw(const uint8_t *files, int16_t numFiles, TeamFileEx *out,
                        const TeamFileEx *other)
{
    int16_t tries;
    for (tries = 0; tries < kTries; tries++) {
        uint8_t file = files[randBelow((uint16_t)numFiles)];
        uint16_t count;
        TeamFileEx *rec;
        if (LoadTeamFile(file) != 0)
            return -1;
        count = *(uint16_t *)_teamFileBuffer;  /* host order once loaded */
        if (count == 0)
            continue;
        rec = (TeamFileEx *)(_teamFileBuffer + 2) + randBelow(count);
        if (rec->team.teamName[0] == 0 || (other && sameTeam(rec, other)))
            continue;
        *out = *rec;
        return 1;
    }
    return 0;
}

int16_t RandomTeam_DrawInBand(const uint8_t *files, int16_t numFiles, TeamFileEx *out,
                              const TeamFileEx *other,
                              int16_t minHalfStars, int16_t maxHalfStars)
{
    int16_t tries, found = 0, bestGap = 0x7FFF;

    for (tries = 0; tries < kBandFileTries; tries++) {
        uint8_t file = files[randBelow((uint16_t)numFiles)];
        uint16_t count, i, start, matches = 0;
        TeamFileEx *recs;
        if (LoadTeamFile(file) != 0)
            return -1;
        count = *(uint16_t *)_teamFileBuffer;
        if (count == 0)
            continue;
        recs = (TeamFileEx *)(_teamFileBuffer + 2);
        /* Scan the whole file from a random start, keeping a random one of the
         * teams in the band (reservoir of one) and the closest outside it. */
        start = randBelow(count);
        for (i = 0; i < count; i++) {
            TeamFileEx *rec = &recs[(uint16_t)((start + i) % count)];
            int16_t stars, gap;
            if (rec->team.teamName[0] == 0 || (other && sameTeam(rec, other)))
                continue;
            stars = (int16_t)TeamStars_HalfStars(rec);
            gap = stars < minHalfStars ? (int16_t)(minHalfStars - stars)
                : stars > maxHalfStars ? (int16_t)(stars - maxHalfStars) : 0;
            if (gap == 0) {
                matches++;
                if (randBelow(matches) == 0)
                    *out = *rec;
            } else if (gap < bestGap && matches == 0 && bestGap != 0) {
                bestGap = gap;
                *out = *rec;
                found = 1;
            }
        }
        if (matches)
            return 1;
    }
    return found;
}
