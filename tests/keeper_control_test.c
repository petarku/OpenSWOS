/* Exercise the production penalty decision; unused UI services fail if called. */
#include "penalty_shootout.h"
#include "menu_system.h"
#include <assert.h>
#include <stdio.h>

uint16_t _playingPenalties, _penalty;
Sprite *_penaltyShooterSprite;
TeamGeneralInfo *_lastTeamPlayed;
MenuEntry _menuEntries[32];
uint16_t _player1ClearFlag, _player2ClearFlag, _playMatchControlFlags;
uint16_t _isFirstPlayerChoosingTeam, _playerNumThatStarted, _exitGameFlag;
uint8_t _pl1Fire, _pl2Fire;
TeamFileEx *_playMatchTeam1Ptr;
void FriendlyFlow(void) { assert(0); }
void StartPenalties(void) { assert(0); }
void DrawMenu(void) { assert(0); }
void SetExitMenuFlag(void) { assert(0); }
void DrawMultipleItems(uint16_t count, MenuEntry *entry) { assert(0); }
uint8_t *GetPlayerAtIndex(uint16_t team, uint16_t ordinal)
{ assert(0); return 0; }
void BuildTransferPlayerMenuEntry(void *item, uint8_t *player, uint16_t y,
                                 int16_t idx, uint16_t style, uint8_t *extra)
{ assert(0); }

int main(void)
{
    TeamGeneralInfo defender = {0}, attacker = {0};
    Sprite shooter = {0};
    int mode, training, shootout, direction;
    assert(g_keeperControl == KEEPER_CONTROL_DEFAULT);
    defender.teamNumber = 1;
    attacker.teamNumber = shooter.teamNumber = 2;
    _penaltyShooterSprite = &shooter;
    _lastTeamPlayed = &attacker;

    for (mode = 0; mode < 3; ++mode) {
        g_keeperControl = mode;
        for (training = 0; training < 2; ++training) {
            g_penaltyShootoutMode = training;
            PenaltyShootout_BeginMatch();
            for (shootout = 0; shootout < 2; ++shootout) {
                _playingPenalties = shootout;
                _penalty = !shootout;
                for (direction = -1; direction < 8; ++direction) {
                    int dive = direction == 2 || direction == 3 ||
                               direction == 5 || direction == 6;
                    int expected = mode == KEEPER_CONTROL_AI ? -1 :
                        dive ? direction :
                        (training || mode == KEEPER_CONTROL_MANUAL) ? 0 : -1;
                    defender.playerNumber = 1;
                    defender.direction = direction;
                    assert(ManualKeeper_DiveInput(&defender) == expected);
                    defender.playerNumber = 0;
                    assert(ManualKeeper_DiveInput(&defender) == -1);
                    attacker.playerNumber = 2;
                    attacker.direction = direction;
                    assert(ManualKeeper_DiveInput(&attacker) == -1);
                }
            }
            _playingPenalties = _penalty = 0;
            defender.playerNumber = 1;
            defender.direction = 2;
            assert(ManualKeeper_DiveInput(&defender) == -1);
        }
    }
    puts("keeper control: defaults, all modes, contexts, directions and ownership passed");
    return 0;
}
