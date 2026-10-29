/* ghost_teams.c -- building a ghost team from the player's own database. See ghost_teams.h.
 *
 * Each player is taken from his team's file (LoadTeamFile, as the challenge cups load their
 * opponents), found by team number and player number and checked by both names, so another
 * database cannot field the wrong man. He comes fresh: the runtime flags, cards, injuries,
 * goals and form a team file may carry are cleared, and the shirts are numbered 1..16 in
 * squad order. */

#include <string.h>
#ifdef SWOS_SDL_BUILD
#include <stdio.h>
#endif

#include "ghost_teams.h"
#include "play_match_menu.h"    /* TEAM_COMPUTER */

extern uint8_t  _teamFileBuffer[];
extern uint16_t _teamsLoaded;
extern int16_t  LoadTeamFile(uint16_t extNumber);

/* A file's name field against a C string; trailing spaces and the rest of the field do not
 * count. */
static int32_t name_is(const int8_t *field, int32_t size, const char *name)
{
    int32_t i;
    for (i = 0; i < size && name[i]; i++)
        if ((char)field[i] != name[i])
            return 0;
    for (; i < size && field[i]; i++)
        if (field[i] != ' ')
            return 0;
    return 1;
}

static void copy_name(int8_t *field, int32_t size, const char *name)
{
    int32_t i;
    for (i = 0; i < size - 1 && name[i]; i++)
        field[i] = (int8_t)name[i];
    for (; i < size; i++)
        field[i] = 0;
}

/* The player `ref` names, or NULL. */
static const PlayerFile *find_player(const GhostPlayerRef *ref)
{
    const TeamFileEx *rec = (const TeamFileEx *)(_teamFileBuffer + 2);
    uint16_t count, i;

    if (LoadTeamFile(ref->country) != 0)
        return 0;
    count = *(uint16_t *)_teamFileBuffer;   /* host order once loaded */
    for (i = 0; i < count; i++)
        if (rec[i].team.teamOrdinal == ref->ordinal &&
            name_is(rec[i].team.teamName, sizeof rec[i].team.teamName, ref->team)) {
            const PlayerFile *p = &rec[i].players[ref->index];
            if (ref->index < 16 && name_is(p->playerName, sizeof p->playerName, ref->name))
                return p;
            return 0;
        }
    return 0;
}

int32_t GhostTeams_Build(const GhostTeam *ghost, uint8_t ordinal, TeamFileEx *out)
{
    TeamFile *team = &out->team;
    int32_t i;

    memset(out, 0, sizeof *out);
    for (i = 0; i < 16; i++) {
        const PlayerFile *p = find_player(&ghost->players[i]);
        PlayerFile *dst = &out->players[i];
        if (!p) {
#ifdef SWOS_SDL_BUILD
            fprintf(stderr, "[ghost] %s: %s of %s (team file %u, team %u, player %u) is not "
                    "in this database\n", ghost->name, ghost->players[i].name,
                    ghost->players[i].team, (unsigned)ghost->players[i].country,
                    (unsigned)ghost->players[i].ordinal, (unsigned)ghost->players[i].index);
#endif
            _teamsLoaded = 0;
            return 0;
        }
        *dst = *p;
        dst->shirtNumber = (int8_t)(i + 1);
        dst->cachedMatchValue = 0;
        dst->positionAndFace &= (uint8_t)~(PF_INFORM | PF_LOAN | PF_TRIALIST);
        dst->cardsInjuries = 0;
        memset(dst->goalStats, 0, sizeof dst->goalStats);
        dst->form = 0;
        team->playerNumbers[i] = (uint8_t)i;
    }
    _teamsLoaded = 0;                   /* the buffer holds whichever file came last */

    team->countryNumber = GHOST_TEAM_FILE;
    team->teamOrdinal = (uint8_t)(GHOST_TEAM_FIRST_ORDINAL + ordinal);
    team->globalTeamNumber = (int16_t)(0x7F00 | ordinal);
    team->teamControls = TEAM_COMPUTER;
    copy_name(team->teamName, sizeof team->teamName, ghost->name);
    copy_name(team->coachName, sizeof team->coachName, ghost->coach);
    team->tactics = TACTIC_DEFAULT;     /* the 4-4-2 it is built for */
    team->prShirtType = ghost->homeKit[0];
    team->prStripesColor = ghost->homeKit[1];
    team->prBasicColor = ghost->homeKit[2];
    team->prShortsColor = ghost->homeKit[3];
    team->prSocksColor = ghost->homeKit[4];
    team->secShirtType = ghost->awayKit[0];
    team->secStripesColor = ghost->awayKit[1];
    team->secBasicColor = ghost->awayKit[2];
    team->secShortsColor = ghost->awayKit[3];
    team->secSocksColor = ghost->awayKit[4];
    team->playerValuesDirty = TEAM_VALUES_DIRTY;
    return 1;
}

int32_t GhostTeams_IsGhost(const TeamFileEx *team)
{
    return team->team.countryNumber == GHOST_TEAM_FILE &&
           team->team.teamOrdinal >= GHOST_TEAM_FIRST_ORDINAL;
}
