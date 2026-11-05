/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* penalty_shootout.c — PENALTY SHOOTOUT training mode. See penalty_shootout.h. */

#include "penalty_shootout.h"

int32_t g_penaltyShootoutMode;
/* OPTIONS -> GK CONTROL, kept for the current session on both targets. */
uint16_t g_keeperControl = KEEPER_CONTROL_DEFAULT;

extern uint16_t _playingPenalties;
extern uint16_t _penalty;
extern Sprite  *_penaltyShooterSprite;
extern TeamGeneralInfo *_lastTeamPlayed;

/* The friendly flow this mode borrows whole, WITHOUT the main-menu fixup on
 * the way out (menus.c): this handler runs under the 4-entry PRACTICE submenu,
 * where CommonMenuExit would write through main-menu entries that are not
 * there. PracticeMenuSelected makes that call once, after the submenu closes. */
extern void FriendlyFlow(void);

/* The engine's own shootout (match_setup.c), the one a drawn cup tie goes to.
 * It runs and ends itself: NextPenalty (goalkeeper.c) advances it and calls
 * EndOfGame once a side cannot be caught. */
extern void StartPenalties(void);

/* PenaltyShootoutMenu -- the PRACTICE submenu's PENALTY SHOOTOUT button.
 *
 * The whole mode is "a friendly, with the flag set", exactly as SHOOTING
 * PRACTICE is (src/shooting_practice.c, where the reuse is argued in full):
 * the flow seeds the RNG, saves/restores gameType and invalidates the team
 * cache, all of which this mode needs too. The screens it puts up read
 * g_penaltyShootoutMode to say PENALTY SHOOTOUT where they would say FRIENDLY.
 *
 * Clearing on the way out is what makes the flag safe: every path back to the
 * submenu, including an abort, returns through here. */
__attribute__((used, externally_visible))
void PenaltyShootoutMenu(void)
{
    g_penaltyShootoutMode = 1;
    FriendlyFlow();
    g_penaltyShootoutMode = 0;
}

/* LATCHED for the whole match by PenaltyShootout_BeginMatch, for the same
 * reason shooting practice latches (its bug log, docs/SHOOTING_PRACTICE.md §3):
 * the match must never depend on a menu global that outlives the menu. */
static int16_t s_shootoutMatch;
static int16_t s_shootoutStarted;
/* The start screen's GOALKEEPER CONTROL, latched beside the mode: 1 = the
 * computer guesses for a human side's keeper too (penalty_shootout_menu.c). */
static int16_t s_keeperComputer;

void PenaltyShootout_BeginMatch(void)
{
    s_shootoutMatch = (int16_t)(g_penaltyShootoutMode != 0);
    s_shootoutStarted = 0;
    s_keeperComputer = (int16_t)(s_shootoutMatch && g_shootoutKeeperComputer != 0);
    PenaltyShootout_ShotReset();
}

/* The REPLAY button after a friendly restarts the match straight through
 * InitGameRestoreTeams + GameLoop (game_loop.c end_of_match_flow), never
 * re-entering InitializeInGameTeamsAndStartGame, so BeginMatch does not run
 * again: the latch survives but the one-shot stayed spent, and the replay of a
 * shootout played as a plain friendly (reported 2026-10-26). Re-arm it; the
 * latch itself is left alone, so a replayed friendly stays a friendly. */
void PenaltyShootout_MatchRestarted(void)
{
    s_shootoutStarted = 0;
    PenaltyShootout_ShotReset();
}

/* The defending keeper at ANY penalty is its player's to throw
 * (penalty_shootout.h): the training shootout, a drawn cup tie's shootout, and
 * an in-match penalty kick. (Until 2026-10-26 it was the training mode's
 * alone — the s_shootoutMatch latch gated it.) Reads the defending side's
 * HELD direction, which processInputForPlayer writes every frame a human pad
 * is polled (team_control.c), so a direction held before the kick commits the
 * dive on the first frame the engine sees a shot.
 *
 * The window is the engine's own penalty-save window — _playingPenalties for
 * a shootout, _penalty from an in-match kick's strike until the next touch —
 * the same pair ShouldGoalkeeperDive's penalty branch tests (goalkeeper.c).
 * Outside the training drill a neutral stick falls back to the AI keeper
 * (see the switch below), so normal matches only ever gain control, never
 * lose the save the AI would have made.
 *
 * Directions are the engine's 0..7 compass (0=N clockwise to 7=NW; the dive
 * directions 2=E and 6=W are the ones GoalkeeperJumping and the kick-off
 * turnFlags already use): W=jump left, SW=dive left low, E/SE mirrored. N, S
 * and the two up-diagonals are not dives, so a thumb resting UP or DOWN alone
 * leaves the keeper standing. */
int16_t ManualKeeper_DiveInput(TeamGeneralInfo *team)
{
    if (g_keeperControl == KEEPER_CONTROL_AI)
        return -1;
    if (team->playerNumber == 0)
        return -1;                      /* a CPU-run side keeps the AI keeper */
    if (_playingPenalties) {
        /* Only the DEFENDING side's keeper: the shooting side's keeper, idle
         * at the far end, runs the same goalie update and must stay the AI's. */
        if (_penaltyShooterSprite != (Sprite *)0 &&
            _penaltyShooterSprite->teamNumber == team->teamNumber)
            return -1;
    } else if (_penalty) {
        /* In-match kick: _lastTeamPlayed is the kicker's side — set beside
         * _penalty at the strike (update_players.c execute_shot/pass), and
         * every later touch clears _penalty, so the pair stays consistent. */
        if (_lastTeamPlayed == team)
            return -1;
    } else {
        return -1;                      /* open play: the AI decides as ever */
    }

    switch (team->direction) {
    case 2:   /* E  — jump right       */
    case 3:   /* SE — dive right, low  */
    case 5:   /* SW — dive left, low   */
    case 6:   /* W  — jump left        */
        return team->direction;
    default:
        /* Neutral stick. In the training drill the keeper STANDS — no guess,
         * no dive, the mode's whole point. In a real match the AI decides as
         * ever, so a player who never touches the stick keeps the keeper the
         * game always had, and only a held dive direction overrides the dice
         * (asked for after the first live build, 2026-10-26). */
        return (s_shootoutMatch || g_keeperControl == KEEPER_CONTROL_MANUAL) ? 0 : -1;
    }
}

/* ===================================================================
 * THE GUESSING GAME — the drill's kicks (penalty_shootout.h, "EIGHT
 * PLACES"). Asked for after the first Amiga test of the manual keeper
 * (2026-11-01): the dive was always early or late, because a penalty's
 * flight is a handful of frames and the save was decided by physics. In
 * the drill the outcome is now the guess alone.
 *
 * A place is a compass direction (0=N clockwise to 7=NW), screen-relative
 * for both players, plus SHOT_CENTER for the neutral stick -- five places:
 *
 *        UP+LEFT 7                        UP+RIGHT 1      high
 *        LEFT 6        (nothing) 8        RIGHT 2         low
 *
 * (DOWN+side reads as that side; UP or DOWN alone as the middle.) The shot
 * is always on target; the keeper saves exactly when his place equals the
 * shot's. Eight places at first (high/middle/low a side, two in the
 * middle): 1 in 8 was too hard to save, so it became five.
 *
 * Drill state, not in the rollback registry: the drill is an offline
 * friendly and never runs under netplay (same as s_shootoutMatch).
 * =================================================================== */
#include "goalkeeper.h"
#include "random.h"
#include "sfx_crowd.h"

#define SHOT_CENTER       8
#define SHOT_FRAMES       16   /* spot to net: ~4 px a frame, 0.32 s at 50 Hz */
#define SHOT_KEEPER_DUE   20   /* the keeper commits when the ball is this close
                                * (ShouldGoalkeeperDive's far penalty distance) */
#define SHOT_X_LEFT       313  /* inside the posts (CheckIfBallOutOfPlay: 302..366) */
#define SHOT_X_CENTER     336
#define SHOT_X_RIGHT      359

extern TeamGeneralInfo _topTeamData;
extern TeamGeneralInfo _bottomTeamData;
extern Sprite _ballSprite;
extern int16_t _gameState;
extern Sprite *_lastPlayerPlayed;
extern void GoalkeeperCaughtTheBall(Sprite *keeper, TeamGeneralInfo *team);
extern void ResetBothTeamSpinTimers(void);

static struct {
    int16_t active;      /* the ball is flying the scripted shot */
    int16_t frame;       /* 1..SHOT_FRAMES */
    int16_t place;       /* the shot's place */
    int16_t keeperPlace; /* the keeper's committed place, -1 = not yet */
    int16_t x0, y0;      /* where it was struck */
    int16_t xEnd, yEnd, zEnd;
    int16_t topGoal;     /* shooting at the upper goal (the shootout's end) */
} s_shot;

void PenaltyShootout_ShotReset(void)
{
    s_shot.active = 0;
    s_shot.keeperPlace = -1;
}

/* FIVE places since the first play test (2026-11-01: "with 8 you will never
 * save one" -- 1 in 5 now): up-left, left, the middle, right, up-right. A
 * side's DOWN diagonal is that side; UP or DOWN alone is the middle. */
static int16_t placeFromStick(int16_t dir)
{
    switch (dir) {
    case 7:         return 7;       /* high left */
    case 6: case 5: return 6;       /* left */
    case 1:         return 1;       /* high right */
    case 2: case 3: return 2;       /* right */
    default:        return SHOT_CENTER;
    }
}

/* A human TAKER since 2026-11-05 (the user's play test: LEFT alone kicked with
 * no lean, the shootout lets him face only NW / N / NE): the stick picks the
 * SIDE -- anything with LEFT in it the left, with RIGHT the right, else the
 * middle -- and the press the HEIGHT: a held (strong) press high, a tap low.
 * So UP+LEFT does both heights and the taker leans with every side kick. The
 * keeper still guesses with the stick alone (placeFromStick). */
static int16_t takerPlace(int16_t dir, int16_t strong)
{
    switch (dir) {
    case 5: case 6: case 7: return strong ? 7 : 6;
    case 1: case 2: case 3: return strong ? 1 : 2;
    default:                return SHOT_CENTER;
    }
}

static int16_t randomPlace(void)
{
    static const int16_t k_places[5] = { 7, 6, SHOT_CENTER, 1, 2 };
    return k_places[Rand() % 5];
}

static int16_t placeIsLeft(int16_t p)  { return p == 7 || p == 6; }
static int16_t placeIsRight(int16_t p) { return p == 1 || p == 2; }

static TeamGeneralInfo *shotDefendingTeam(void)
{
    /* The upper goal is the bottom team's target, so the top team defends it. */
    return s_shot.topGoal ? &_topTeamData : &_bottomTeamData;
}

/* How far the ball still has to come to the keeper's line (negative once
 * it is past him). */
static int16_t ballDistanceToKeeper(const Sprite *keeper, const Sprite *ball)
{
    return s_shot.topGoal ? (int16_t)(ball->y.s.i - keeper->y.s.i)
                          : (int16_t)(keeper->y.s.i - ball->y.s.i);
}

void PenaltyShootout_ShotStruck(Sprite *shooter, TeamGeneralInfo *team, int16_t strong)
{
    Sprite *ball = &_ballSprite;
    int16_t place;
    (void)shooter;

    if (!s_shootoutMatch || !_playingPenalties || _gameState != ST_PENALTIES)
        return;

    place = team->playerNumber ? takerPlace(team->direction, strong) : randomPlace();

    s_shot.active = 1;
    s_shot.frame = 0;
    s_shot.place = place;
    s_shot.keeperPlace = -1;
    s_shot.x0 = ball->x.s.i;
    s_shot.y0 = ball->y.s.i;
    s_shot.topGoal = (int16_t)(ball->y.s.i <= 449);
    s_shot.xEnd = placeIsLeft(place) ? SHOT_X_LEFT
                : placeIsRight(place) ? SHOT_X_RIGHT : SHOT_X_CENTER;
    /* Deep enough to be over the line (y < 129 / > 769), short of the back
     * net and of the sloping roof (UpdateBall: z > 10 is the bar there). */
    s_shot.yEnd = s_shot.topGoal ? 124 : 774;
    switch (place) {
    case 7: case 1: s_shot.zEnd = 13; break;   /* high, under the bar (15) */
    case 6: case 2: s_shot.zEnd = 3;  break;   /* low to the side */
    default:        s_shot.zEnd = 4;  break;   /* the middle */
    }
    ball->deltaZ.raw = 0;
    ResetBothTeamSpinTimers();     /* no aftertouch on a drill kick */
}

int16_t PenaltyShootout_BallScripted(void)
{
    return s_shot.active;
}

/* The save itself, the engine's own claim sequences (update_players.c):
 * a diving keeper claims as at "Close enough — claim ball" (DoGoalkeeperSprites
 * then carries the ball in his dive), a standing one as at
 * claim_ball_and_freeze_speeds. */
static int16_t s_kickSaved;     /* a save since the HUD last asked */

static void shotSaved(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    s_shot.active = 0;
    _lastTeamPlayed = team;
    _lastPlayerPlayed = keeper;
    ball->x.s.i = keeper->x.s.i;
    ball->y.s.i = keeper->y.s.i;
    ball->destX = ball->x.s.i;
    ball->destY = ball->y.s.i;
    ball->deltaX.raw = 0;
    ball->deltaY.raw = 0;
    ball->deltaZ.raw = 0;
    ball->speed = 0;
    keeper->speed = 0;
    if (keeper->playerState == PL_GOALIE_DIVING_HIGH ||
        keeper->playerState == PL_GOALIE_DIVING_LOW)
        team->goalkeeperDivingRight = 1;
    else {
        if (keeper->playerState != PL_GOALIE_CATCHING_BALL) {
            GoalkeeperCaughtTheBall(keeper, team);
            keeper->speed = 0;
        }
        keeper->destX = keeper->x.s.i;
        keeper->destY = keeper->y.s.i;
        team->goalkeeperDivingLeft = 1;
    }
    GoalkeeperClaimedTheBall(keeper, ball, team);
    PlayMissGoalSample();
    s_kickSaved = 1;            /* the HUD's cross (penalty_shootout_hud.c) */
}

int16_t PenaltyShootout_TakeKickSaved(void)
{
    int16_t saved = s_kickSaved;
    s_kickSaved = 0;
    return saved;
}

int16_t PenaltyShootout_ScriptBall(Sprite *ball)
{
    int32_t t, n = SHOT_FRAMES;
    int32_t dx, dy;
    TeamGeneralInfo *team;
    Sprite *keeper;

    if (!s_shot.active)
        return 0;

    t = ++s_shot.frame;
    dx = (int32_t)s_shot.xEnd - s_shot.x0;
    dy = (int32_t)s_shot.yEnd - s_shot.y0;
    ball->x.raw = ((int32_t)s_shot.x0 << 16) + (int32_t)((dx << 16) / n) * t;
    ball->y.raw = ((int32_t)s_shot.y0 << 16) + (int32_t)((dy << 16) / n) * t;
    ball->z.raw = (int32_t)(((int32_t)s_shot.zEnd << 16) / n) * t;
    ball->deltaX.raw = (int32_t)((dx << 16) / n);
    ball->deltaY.raw = (int32_t)((dy << 16) / n);
    ball->deltaZ.raw = 0;
    ball->destX = s_shot.xEnd;
    ball->destY = s_shot.yEnd;

    team = shotDefendingTeam();
    keeper = team->spritesTable[0];
    if (ballDistanceToKeeper(keeper, ball) <= 0 && ball->z.s.i <= 15) {
        int16_t kp = s_shot.keeperPlace;
        if (kp < 0)                 /* never committed: whatever is held now */
            kp = team->playerNumber ? placeFromStick(team->direction) : SHOT_CENTER;
        if (kp == s_shot.place) {
            shotSaved(keeper, ball, team);
            return 1;
        }
    }

    if (t >= n) {
        /* In the net: physics again, settling against it. */
        s_shot.active = 0;
        ball->speed = 256;
        ball->destY = (int16_t)(s_shot.yEnd + (s_shot.topGoal ? -8 : 8));
        ball->deltaZ.raw = -0x8000;
    }
    return 1;
}

/* The defending keeper while a drill kick flies: he stands until the ball
 * is SHOT_KEEPER_DUE away, then commits to the place his player is holding
 * (a computer keeper guesses) and moves there. Returns 1 when it has
 * handled the keeper this frame. */
int16_t PenaltyShootout_Keeper(Sprite *keeper, Sprite *ball, TeamGeneralInfo *team)
{
    int16_t kp;

    if (!s_shot.active || team != shotDefendingTeam())
        return 0;

    keeper->destX = keeper->x.s.i;
    keeper->destY = keeper->y.s.i;
    if (s_shot.keeperPlace >= 0 || ballDistanceToKeeper(keeper, ball) > SHOT_KEEPER_DUE)
        return 1;

    kp = (team->playerNumber && !s_keeperComputer) ? placeFromStick(team->direction)
                                                   : randomPlace();
    s_shot.keeperPlace = kp;

    if (placeIsLeft(kp) || placeIsRight(kp)) {
        int16_t d0 = placeIsLeft(kp) ? 6 : 2;
        keeper->direction = d0;
        GoalkeeperJumping(d0, 0, d0, keeper, ball, team);
        GoalkeeperForceDiveAnim(keeper, team, (int16_t)(kp == 6 || kp == 2));
        team->passKickTimer = 0;
        team->passingKickingPlayer = (Sprite *)0;
    }
    /* SHOT_CENTER: he stays where he is. */
    return 1;
}

/* ===================================================================
 * THE KEEPER'S ARROW. While a human keeper waits for a computer taker's
 * kick, an arrow on the goal shows the place he is holding (UP+LEFT an
 * arrow up-left on the goal's top left, and so on; nothing held, no
 * arrow). It goes the moment the ball is struck (_gameState leaves
 * ST_PENALTIES). Asked for 2026-11-01.
 *
 * Drawn after the sprites (game_loop.c) with DrawSpriteClipped, which on
 * the Amiga queues its own undraw like any bob, so the pitch comes back
 * the next frame. The seven arrows are built once per run from two
 * drawings: in the Amiga's linked layout (per line, one plane row per
 * plane, the mask repeated per plane; CHIP RAM, the blitter reads them)
 * or as chunky pixels on SDL. Red with an orange edge, as SWOS 2020's,
 * beside the keeper on the side, height and middle he is holding.
 * =================================================================== */
#include "display.h"
#include "sprites.h"
#include "swos_sections.h"

#define ARROW_BASE   11              /* the drawings */
#define ARROW_N      13              /* with a one-pixel outline */
#define ARROW_DIGIT_SPRITE 1074      /* the shirt number "1" (game_hud.c) */
/* SWOS 2020's arrow is red with a lighter edge. The match palette's first 16
 * (_gamePalette, OCS and AGA alike): 10 = red 0xF00, 6 = orange. */
#define ARROW_COLOR_FILL   10
#define ARROW_COLOR_EDGE   6

static const char *const k_arrowLeft[ARROW_BASE] = {
    ".....#.....",
    "....##.....",
    "...###.....",
    "..#########",
    ".##########",
    "###########",
    ".##########",
    "..#########",
    "...###.....",
    "....##.....",
    ".....#.....",
};
static const char *const k_arrowUpLeft[ARROW_BASE] = {
    "#######....",
    "######.....",
    "#####......",
    "######.....",
    "###.###....",
    "##...###...",
    "#.....###..",
    ".......###.",
    "........###",
    ".........##",
    "..........#",
};

extern uint8_t *_spriteGraphicsPtr;
extern int16_t _foulYCoordinate;
extern FixedPoint _cameraX, _cameraY;   /* Render_CameraX/Y on the Amiga */

#ifdef SWOS_SDL_BUILD
static uint8_t s_arrowPix[8][ARROW_N * 16];
#else
/* + 2 words: a shifted blit reads one word past the last row (alwm masks it) */
#define ARROW_WORDS (ARROW_N * PITCH_NUM_PLANES + 2)
SWOS_CHIP_BSS static uint16_t s_arrowData[8][ARROW_WORDS];
SWOS_CHIP_BSS static uint16_t s_arrowMask[8][ARROW_WORDS];
#endif
static SpriteGraphics s_arrowGfx[8];
static int16_t s_arrowsBuilt;

static int16_t arrowBasePixel(int16_t dir, int16_t x, int16_t y)
{
    const int16_t m = ARROW_BASE - 1;
    if (x < 0 || y < 0 || x > m || y > m)
        return 0;
    switch (dir) {
    case 6: return k_arrowLeft[y][x] == '#';
    case 2: return k_arrowLeft[y][m - x] == '#';
    case 0: return k_arrowLeft[x][y] == '#';
    case 7: return k_arrowUpLeft[y][x] == '#';
    case 1: return k_arrowUpLeft[y][m - x] == '#';
    case 5: return k_arrowUpLeft[m - y][x] == '#';
    case 3: return k_arrowUpLeft[m - y][m - x] == '#';
    default: return 0;
    }
}

/* 0 transparent, 1 fill, 2 outline */
static int16_t arrowPixel(int16_t dir, int16_t x, int16_t y)
{
    int16_t dx, dy;
    if (arrowBasePixel(dir, x - 1, y - 1))
        return 1;
    for (dy = -1; dy <= 1; dy++)
        for (dx = -1; dx <= 1; dx++)
            if (arrowBasePixel(dir, x - 1 + dx, y - 1 + dy))
                return 2;
    return 0;
}

static void buildArrows(void)
{
    /* Only for its plane depth: the arrows are drawn at the HUD number's. */
    SpriteGraphics *digit = ((SpriteGraphics **)_spriteGraphicsPtr)[ARROW_DIGIT_SPRITE];
    const uint16_t fill = ARROW_COLOR_FILL, outline = ARROW_COLOR_EDGE;
    int16_t x, y, dir;

    for (dir = 0; dir < 8; dir++) {
        SpriteGraphics *g = &s_arrowGfx[dir];
        g->type = 1;                    /* linked planes, as the bobs */
        g->pixWidth = ARROW_N;
        g->nlines = ARROW_N;
        g->wquads = 1;
        g->centerX = ARROW_N / 2;
        g->centerY = ARROW_N / 2;
        g->ordinal = digit->ordinal;    /* its plane depth (spriteNumPlanes) */
#ifdef SWOS_SDL_BUILD
        for (y = 0; y < ARROW_N; y++)
            for (x = 0; x < ARROW_N; x++) {
                int16_t v = arrowPixel(dir, x, y);
                s_arrowPix[dir][y * 16 + x] = (uint8_t)(v == 1 ? fill : v == 2 ? outline : 0);
            }
        g->sprData = s_arrowPix[dir];
        g->maskPtr = (uint8_t *)0;
        g->bltsize = 0;
#else
        {
            uint16_t sp = spriteNumPlanes(g->ordinal), p;
            for (y = 0; y < ARROW_N; y++) {
                uint16_t mask = 0, planes[PITCH_NUM_PLANES] = { 0 };
                for (x = 0; x < ARROW_N; x++) {
                    int16_t v = arrowPixel(dir, x, y);
                    uint16_t col = (uint16_t)(v == 1 ? fill : v == 2 ? outline : 0);
                    uint16_t bit = (uint16_t)(0x8000u >> x);
                    if (!v)
                        continue;
                    mask |= bit;
                    for (p = 0; p < sp; p++)
                        if (col & (1u << p))
                            planes[p] |= bit;
                }
                for (p = 0; p < sp; p++) {
                    s_arrowData[dir][y * sp + p] = planes[p];
                    s_arrowMask[dir][y * sp + p] = mask;
                }
            }
            g->sprData = (uint8_t *)s_arrowData[dir];
            g->maskPtr = (uint8_t *)s_arrowMask[dir];
            g->bltsize = (uint16_t)(((ARROW_N * sp) << 6) | 1);
        }
#endif
    }
    s_arrowsBuilt = 1;
}

/* The shootout screen (penalty_shootout_hud.c) is the drill's alone. */
int16_t PenaltyShootout_HudActive(void)
{
    return (int16_t)(s_shootoutMatch && _playingPenalties);
}

void PenaltyShootout_DrawKeeperArrow(void)
{
    TeamGeneralInfo *def;
    int16_t place, wx, wy, sx, sy;

    if (!s_shootoutMatch || s_keeperComputer || !_playingPenalties ||
        _gameState != ST_PENALTIES)
        return;                         /* COMPUTER keeper control: no guess to show */
    if (_foulYCoordinate > 449)
        return;                         /* the shootout's spot is the upper one */
    def = &_topTeamData;
    if (!def->playerNumber || !def->opponentsTeam || def->opponentsTeam->playerNumber)
        return;                         /* only a human keeper against the computer */
    place = placeFromStick(def->direction);
    if (place == SHOT_CENTER)
        return;
    if (!s_arrowsBuilt)
        buildArrows();

    /* The place on the goal (mouth 302..366, crossbar ~16 above the line). */
    wx = placeIsLeft(place) ? 316 : placeIsRight(place) ? 356 : 336;
    wy = (place == 7 || place == 1) ? 116 : 124;
    sx = (int16_t)(wx - s_arrowGfx[place].centerX - Render_CameraX());
    sy = (int16_t)(wy - s_arrowGfx[place].centerY - Render_CameraY());
    if (sx >= _screen_width || sy >= SCREEN_HEIGHT || sx <= -ARROW_N || sy <= -ARROW_N)
        return;
    DrawSpriteClipped(&s_arrowGfx[place], sx, sy);
}

void PenaltyShootout_MatchTick(void)
{
    if (!s_shootoutMatch || s_shootoutStarted)
        return;
    /* First frame of the loop: straight to the shootout. By now StartingMatch
     * has run (InitGameRestoreTeams calls it before GameLoop), so both teams'
     * sprites and data exist, and StartPenalties -> NextPenalty rebuilds the
     * scene wholesale anyway (InitTeamsData, StopAllPlayers, its own game
     * state and camera) -- the walk-on and the kickoff it replaces were pure
     * ceremony. Until 2026-10-26 this waited for the kickoff to be taken
     * (ST_GAME_IN_PROGRESS, the arcade's entry point), which made every
     * shootout open with a pointless kick-off whistle. */
    s_shootoutStarted = 1;
    StartPenalties();

    /* The camera starts where the kick-off left it, mid-pitch, and MoveCamera
     * eases it to the penalty goal a quarter of the way a frame: a dozen-odd
     * frames of scrolling over all 22 players, which an A1200 could not hold
     * at full speed (2026-11-01). Run its easing to the end now instead, so
     * the first frame already looks at the goal -- the same target, reached
     * at once. A large step is what ScrollToCurrent does for any jump. */
    {
        extern FixedPoint _cameraX, _cameraY;
        extern void MoveCamera(void);
        int16_t i;
        for (i = 0; i < 64; i++) {
            int32_t x = _cameraX.raw, y = _cameraY.raw;
            MoveCamera();
            if (_cameraX.raw == x && _cameraY.raw == y)
                break;
        }
    }
}
