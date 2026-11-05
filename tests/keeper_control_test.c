/* Exercise the production penalty decision; unused UI services fail if called. */
#include "penalty_shootout.h"
#include "menu_system.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

uint16_t _playingPenalties, _penalty;
Sprite *_penaltyShooterSprite;
TeamGeneralInfo *_lastTeamPlayed;
void FriendlyFlow(void) { assert(0); }
void StartPenalties(void) { assert(0); }
void MoveCamera(void) { assert(0); }

/* ---- the drill's FIVE PLACES: engine services, recorded not run ---- */
TeamGeneralInfo _topTeamData, _bottomTeamData;
Sprite _ballSprite;
int16_t _gameState;
Sprite *_lastPlayerPlayed;
static int s_claims, s_dives, s_lowDives, s_catches;
static int16_t s_strong = 1;   /* the taker's press: 1 held (a shot), 0 a tap (a pass) */
static uint16_t s_rand;
uint16_t Rand(void) { return s_rand++; }
void PlayMissGoalSample(void) {}
void ResetBothTeamSpinTimers(void) {}
void GoalkeeperJumping(int16_t d0, int16_t v, int16_t d3, Sprite *k, Sprite *b,
                       TeamGeneralInfo *t)
{ k->playerState = PL_GOALIE_DIVING_HIGH; s_dives++; }
void GoalkeeperForceDiveAnim(Sprite *k, TeamGeneralInfo *t, int16_t low)
{ if (low) { k->playerState = PL_GOALIE_DIVING_LOW; s_lowDives++; } }
void GoalkeeperCaughtTheBall(Sprite *k, TeamGeneralInfo *t)
{ k->playerState = PL_GOALIE_CATCHING_BALL; s_catches++; }
void GoalkeeperClaimedTheBall(Sprite *k, Sprite *b, TeamGeneralInfo *t)
{ s_claims++; }
uint16_t g_shootoutKeeperComputer;   /* the start screen's GOALKEEPER CONTROL */
/* the keeper's arrow (drawing only; not exercised here) */
uint8_t *_spriteGraphicsPtr;
int16_t _foulYCoordinate;
FixedPoint _cameraX, _cameraY;
int _screen_width = 336;
int16_t Render_CameraX(void) { return 0; }
int16_t Render_CameraY(void) { return 0; }
void DrawSpriteClipped(SpriteGraphics *g, int16_t x, int16_t y) { assert(0); }

/* One drill kick, frame by frame as the game runs it: UpdateBall's script,
 * then the goalie update. Returns 1 saved, 0 in the net (crossed the line
 * inside the posts under the bar, CheckIfBallOutOfPlay's window). */
static int playKick(int shooterDir, int keeperDir, int humanShooter, int humanKeeper)
{
    static Sprite keeper;
    static Sprite *keeperTable[11];
    int frame, crossed = 0;

    memset(&keeper, 0, sizeof keeper);
    keeper.playerState = PL_NORMAL;
    keeper.x.s.i = 336; keeper.y.s.i = 133;
    keeperTable[0] = &keeper;
    _topTeamData.spritesTable = keeperTable;
    _topTeamData.playerNumber = humanKeeper;
    _topTeamData.direction = keeperDir;
    _bottomTeamData.playerNumber = humanShooter;
    _bottomTeamData.direction = shooterDir;
    memset(&_ballSprite, 0, sizeof _ballSprite);
    _ballSprite.x.s.i = 336; _ballSprite.y.s.i = 187;   /* NextPenalty's spot */
    s_claims = 0;

    _gameState = ST_PENALTIES;
    PenaltyShootout_ShotStruck(0, &_bottomTeamData, s_strong);
    assert(PenaltyShootout_BallScripted());
    _gameState = ST_GAME_IN_PROGRESS;
    for (frame = 0; frame < 40 && PenaltyShootout_BallScripted(); frame++) {
        PenaltyShootout_ScriptBall(&_ballSprite);
        if (s_claims)
            return 1;
        if (_ballSprite.y.s.i < 129) {
            int x = _ballSprite.x.s.i - 1;
            assert(x >= 302 && x <= 366 && _ballSprite.z.s.i <= 15);
            crossed = 1;
        }
        if (keeper.playerState == PL_NORMAL)
            PenaltyShootout_Keeper(&keeper, &_ballSprite, &_topTeamData);
    }
    assert(!PenaltyShootout_BallScripted());
    assert(crossed);
    return 0;
}

/* The taker's place: the side from the stick (anything with LEFT / RIGHT in
 * it), the height from the press -- held high, a tap low. */
static int takerPlaceOf(int dir, int strong)
{
    switch (dir) {
    case 5: case 6: case 7: return strong ? 7 : 6;
    case 1: case 2: case 3: return strong ? 1 : 2;
    default: return 8;
    }
}

/* The keeper's five places: DOWN+side is that side, UP / DOWN / nothing the middle. */
static int placeOf(int dir)
{
    switch (dir) {
    case 7: return 7;
    case 6: case 5: return 6;
    case 1: return 1;
    case 2: case 3: return 2;
    default: return 8;
    }
}

static void testFivePlaces(void)
{
    int s, k, saves = 0, n;
    _playingPenalties = 1;
    g_penaltyShootoutMode = 1;
    PenaltyShootout_BeginMatch();

    /* Human v human: every stick and press against every stick. */
    for (s_strong = 0; s_strong < 2; s_strong++)
        for (s = -1; s < 8; s++)
            for (k = -1; k < 8; k++)
                assert(playKick(s, k, 1, 1) == (takerPlaceOf(s, s_strong) == placeOf(k)));
    s_strong = 1;

    /* The keeper's dive follows his place: UP+side a high dive, the side
     * (with or without DOWN) a low one, the middle stands. */
    s_dives = s_lowDives = s_catches = 0;
    playKick(2, 7, 1, 1);  assert(s_dives == 1 && s_lowDives == 0);
    playKick(2, 6, 1, 1);  assert(s_dives == 2 && s_lowDives == 1);
    playKick(2, 5, 1, 1);  assert(s_dives == 3 && s_lowDives == 2);
    playKick(2, 0, 1, 1);  assert(s_dives == 3 && s_catches == 0);
    playKick(2, -1, 1, 1); assert(s_dives == 3 && s_catches == 0);

    /* Computer taker: always on target (playKick asserts it), and a keeper
     * who guesses saves about one in five. */
    for (k = -1; k < 8; k++)
        for (n = 0; n < 64; n++)
            saves += playKick(0, k, 0, 1);
    assert(saves > 9 * 64 / 10 && saves < 9 * 64 * 2 / 5);

    /* GOALKEEPER CONTROL = COMPUTER: the human keeper's stick no longer
     * decides. He holds the shot's own place every time, and still saves only
     * about one in five (the computer guesses for him). Latched at kick-off. */
    g_shootoutKeeperComputer = 1;
    PenaltyShootout_BeginMatch();
    saves = 0;
    for (n = 0; n < 9 * 64; n++)
        saves += playKick(7, 7, 1, 1);
    assert(saves > 9 * 64 / 10 && saves < 9 * 64 * 2 / 5);
    g_shootoutKeeperComputer = 0;
    PenaltyShootout_BeginMatch();
    assert(playKick(7, 7, 1, 1) == 1);

    /* Outside the drill nothing is scripted. */
    g_penaltyShootoutMode = 0;
    PenaltyShootout_BeginMatch();
    _gameState = ST_PENALTIES;
    PenaltyShootout_ShotStruck(0, &_bottomTeamData, s_strong);
    assert(!PenaltyShootout_BallScripted());
    _playingPenalties = 0;
    puts("five places: every shot against every guess, dives, CPU taker passed");
}

int main(void)
{
    testFivePlaces();
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
