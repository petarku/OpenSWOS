/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
#ifndef GAME_HUD_H
#define GAME_HUD_H

#include "game_types.h"

void UpdateControlledPlayerNumbers(void);
void MarkPlayer(void);
void SetCurrentPlayerName(void);
/* Its placement of player's name sprite in the configured corner, on its own
 * (PENALTY SHOOTOUT names its taker there on the Amiga). */
void PlaceCurrentPlayerName(Sprite *spr, Sprite *player, TeamGeneralInfo *team);

#endif
