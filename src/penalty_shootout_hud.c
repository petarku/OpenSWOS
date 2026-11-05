/*
 * Amiga SWOS RE Project
 * SWOS Amiga C/68000 Reversed by Ben Baker (https://github.com/benbaker76)
 * SWOS DOS Reversed by Zlatko Karakaš (https://github.com/zlatkok)
 */
/* penalty_shootout_hud.c — PENALTY SHOOTOUT's screen while the kicks are
 * taken, SWOS 2020's (asked for 2026-11-01; penalty_shootout.h):
 *
 *   on the left    the shooting side's ten takers in shooting order (face,
 *                  shirt number, name), the one taking this kick highlighted;
 *   along the foot both teams, their shootout score, and the last five kicks
 *                  each: a tick scored, a cross missed, * the kick being
 *                  taken, - not taken yet (past five, the columns scroll);
 *   on the right   who controls the defending side (by the goal) and the
 *                  shooting side (by the spot).
 *
 * HOW IT IS DRAWN. Each of the four pieces is a CANVAS -- an 8-bit picture
 * (colour 0 = see-through) that this file paints itself, reading the game's
 * own font and face sprites pixel by pixel -- shown as ONE sprite a frame
 * through DrawSpriteClipped, which on the Amiga also queues its undraw like
 * any bob. A canvas is repainted only when what it shows changes: a new
 * kick, a result, the sides swapping. On the Amiga it is then converted once
 * into sprite planes in CHIP RAM.
 *
 * The first version drew everything every frame -- grid tiles, bars, every
 * glyph -- straight onto the pitch: a thousand-odd small blits a frame, which
 * an A1200 could not keep up with (the first Amiga test, 2026-11-01: "really
 * slow"). Four sprites a frame cost next to nothing.
 *
 * The kick record is display state, rebuilt from the engine's own counters
 * (_teamNPenaltyAttempts / _teamNPenaltyGoals) and the drill's save signal
 * (PenaltyShootout_TakeKickSaved); the drill is an offline friendly, so none
 * of it is in the rollback registry. */

#include "penalty_shootout.h"
#include "game_types.h"
#include "display.h"
#include "sprites.h"
#include "text_render.h"
#include "ball_physics.h"       /* GetPlayerPointerFromShirtNumber */
#include "game_hud.h"           /* PlaceCurrentPlayerName: the taker in the corner */
#include "swos_sections.h"

extern uint16_t _playingPenalties;
extern int16_t  _team1PenaltyAttempts, _team2PenaltyAttempts;
extern uint16_t _team1PenaltyGoals, _team2PenaltyGoals;
extern int16_t  _team1PenaltyShooterIndex, _team2PenaltyShooterIndex;
extern TeamGeneralInfo _topTeamData, _bottomTeamData;
extern uint8_t  _smallCharsTable[];
extern uint16_t _playerFacesTable[];
extern uint16_t _useIndividualPlayerSkinColor;
extern uint8_t *_spriteGraphicsPtr;     /* (_screen_width: display.h, a constant on the m68k) */

#define DEPTH_SPRITE 1074       /* the HUD shirt number: the canvases' plane depth */

/* The takers list is SDL's only. On the Amiga even trimmed to its bars it is
 * more than half the HUD's area, and every pixel of a bob is blitted plane by
 * plane twice a frame over the 8-plane pitch: with it the A1200 could not
 * hold the frame rate (tested 2026-11-01, three builds). */
#ifdef SWOS_SDL_BUILD
#define HUD_TAKERS_LIST 1
#define HUD_DURING_KICK 1       /* on through the strike, the flight and the dive */
#define HUD_LABELS      1       /* CPU / PLAYER n beside the goal and the spot */
#else
#define HUD_TAKERS_LIST 0
#define HUD_DURING_KICK 0
#define HUD_LABELS      0       /* the user's call on the first Amiga tests */
#endif

extern int16_t _gameState;

/* Colours: the match palette's first 16 (OCS and AGA alike); sprites are 16
 * colours on both targets. 8 is the font's own shadow colour, a dark green. */
enum { C_WHITE = 2, C_BLACK = 3, C_BROWN = 4, C_DARK = 8, C_RED = 10, C_MAROON = 12,
       C_LIGHT_BLUE = 13, C_GREEN = 14 };

/* ---- the kick record -------------------------------------------------- */

enum { KICK_NONE, KICK_TAKING, KICK_SCORED, KICK_MISSED };
enum { kMaxKicks = 40, kShownKicks = 5 };

static int8_t  s_kick[2][kMaxKicks];
static int16_t s_attempts[2];
static int16_t s_goalsAtKick[2];
static int16_t s_taking = -1;           /* the side whose kick is being taken */
static int16_t s_boardDirty = 1;
static int16_t s_panelStale = 1;        /* panel and labels: a new shootout */

static int16_t attemptsOf(int16_t side)
{
    return side == 0 ? _team1PenaltyAttempts : _team2PenaltyAttempts;
}

static int16_t goalsOf(int16_t side)
{
    return (int16_t)(side == 0 ? _team1PenaltyGoals : _team2PenaltyGoals);
}

static void resolveTaking(int8_t result)
{
    if (s_taking >= 0 && s_attempts[s_taking] > 0 && s_attempts[s_taking] <= kMaxKicks)
        s_kick[s_taking][s_attempts[s_taking] - 1] = result;
    s_taking = -1;
    s_boardDirty = 1;
}

/* Follows the engine: NextPenalty counts a kick when it sets it up, a goal
 * raises the side's penalty goals, and the drill's keeper reports a save. A
 * kick still open when the next one is set up was missed. */
static void trackKicks(void)
{
    int16_t side, i;

    if (attemptsOf(0) + attemptsOf(1) < s_attempts[0] + s_attempts[1]) {
        for (side = 0; side < 2; side++) {          /* a new shootout */
            s_attempts[side] = 0;
            for (i = 0; i < kMaxKicks; i++)
                s_kick[side][i] = KICK_NONE;
        }
        s_taking = -1;
        s_boardDirty = 1;
        s_panelStale = 1;                           /* new teams: repaint all */
    }
    if (s_taking >= 0 && goalsOf(s_taking) > s_goalsAtKick[s_taking])
        resolveTaking(KICK_SCORED);
    if (PenaltyShootout_TakeKickSaved() && s_taking >= 0)
        resolveTaking(KICK_MISSED);
    for (side = 0; side < 2; side++) {
        int16_t a = attemptsOf(side);
        if (a <= s_attempts[side])
            continue;
        if (s_taking >= 0)
            resolveTaking(KICK_MISSED);
        s_attempts[side] = a;
        s_goalsAtKick[side] = goalsOf(side);
        if (a <= kMaxKicks)
            s_kick[side][a - 1] = KICK_TAKING;
        s_taking = side;
        s_boardDirty = 1;
    }
}

/* ---- canvases ----------------------------------------------------------- */

typedef struct Canvas {
    int16_t w, h, wq;                   /* w = wq * 16: as painted (canvasSize) */
    int16_t maxWq, maxH;                /* the buffers' size */
    uint8_t *pix;                       /* w * h, colour 0 see-through */
#ifndef SWOS_SDL_BUILD
    uint16_t *data, *mask;              /* the sprite planes, CHIP RAM */
#endif
    SpriteGraphics g;
} Canvas;

enum {
    PANEL_WQ = 9, PANEL_H = 91,         /* ten 9-line rows */
    BOARD_WQ = 19, BOARD_H = 34,
    LABEL_WQ = 4, LABEL_H = 11,
};
/* Sprites are 16 colours, 4 planes deep, on both targets (display.h): the
 * planes are sized for that, and canvasPublish refuses a deeper template. */
#define PLANES_WORDS(wq, h) ((wq) * (h) * SPRITE_NUM_PLANES + 2)  /* +2: a shifted blit reads on */

static uint8_t s_panelPix[PANEL_WQ * 16 * PANEL_H];
static uint8_t s_boardPix[BOARD_WQ * 16 * BOARD_H];
static uint8_t s_labelPix[2][LABEL_WQ * 16 * LABEL_H];
#ifndef SWOS_SDL_BUILD
SWOS_CHIP_BSS static uint16_t s_boardData[PLANES_WORDS(BOARD_WQ, BOARD_H)];
SWOS_CHIP_BSS static uint16_t s_boardMask[PLANES_WORDS(BOARD_WQ, BOARD_H)];
SWOS_CHIP_BSS static uint16_t s_labelData[2][PLANES_WORDS(LABEL_WQ, LABEL_H)];
SWOS_CHIP_BSS static uint16_t s_labelMask[2][PLANES_WORDS(LABEL_WQ, LABEL_H)];
#endif

static Canvas s_panel, s_board, s_label[2];

static void canvasInit(Canvas *c, int16_t wq, int16_t h, uint8_t *pix,
                       uint16_t *data, uint16_t *mask)
{
    c->maxWq = c->wq = wq;
    c->w = (int16_t)(wq * 16);
    c->maxH = c->h = h;
    c->pix = pix;
#ifndef SWOS_SDL_BUILD
    c->data = data;
    c->mask = mask;
#else
    (void)data;
    (void)mask;
#endif
}

/* The rectangle the sprite will be, as small as what it shows: every pixel
 * of it is blitted twice a frame on the Amiga (drawn, then undrawn), the
 * see-through ones too, so its AREA is the HUD's cost. */
static void canvasSize(Canvas *c, int16_t w, int16_t h)
{
    int16_t wq = (int16_t)((w + 15) / 16);
    c->wq = wq < 1 ? 1 : wq > c->maxWq ? c->maxWq : wq;
    c->w = (int16_t)(c->wq * 16);
    c->h = h < 1 ? 1 : h > c->maxH ? c->maxH : h;
}

static void canvasClear(Canvas *c)
{
    int32_t i, n = (int32_t)c->w * c->h;
    for (i = 0; i < n; i++)
        c->pix[i] = 0;
}

static void canvasFill(Canvas *c, int16_t x, int16_t y, int16_t w, int16_t h, uint8_t col)
{
    int16_t xx, yy;
    for (yy = y; yy < y + h; yy++) {
        if (yy < 0 || yy >= c->h)
            continue;
        for (xx = x; xx < x + w; xx++)
            if (xx >= 0 && xx < c->w)
                c->pix[yy * c->w + xx] = col;
    }
}

/* A sprite's colour at (x, y), 0 = nothing: chunky bytes on SDL; on the
 * Amiga one row per plane each line, wquads words wide (the font's glyphs and
 * the linked bobs alike). */
static uint8_t spritePixel(const SpriteGraphics *s, int16_t x, int16_t y)
{
#ifdef SWOS_SDL_BUILD
    return ((const uint8_t *)s->sprData)[y * s->wquads * 16 + x];
#else
    uint16_t sp = s->type == 0 ? SPRITE_NUM_PLANES : spriteNumPlanes(s->ordinal), p;
    const uint16_t *line = (const uint16_t *)s->sprData + (uint32_t)y * sp * s->wquads;
    uint16_t bit = (uint16_t)(0x8000u >> (x & 15));
    uint8_t col = 0;
    for (p = 0; p < sp; p++)
        if (line[p * s->wquads + (x >> 4)] & bit)
            col |= (uint8_t)(1u << p);
    return col;
#endif
}

static SpriteGraphics *gameSprite(uint16_t index)
{
    return ((SpriteGraphics **)_spriteGraphicsPtr)[index];
}

static void canvasSprite(Canvas *c, uint16_t index, int16_t x, int16_t y)
{
    const SpriteGraphics *s = gameSprite(index);
    int16_t sx, sy;
    for (sy = 0; sy < s->nlines; sy++)
        for (sx = 0; sx < s->pixWidth; sx++) {
            uint8_t col = spritePixel(s, sx, sy);
            int16_t xx = (int16_t)(x + sx), yy = (int16_t)(y + sy);
            if (col && xx >= 0 && xx < c->w && yy >= 0 && yy < c->h)
                c->pix[yy * c->w + xx] = col;
        }
}

/* The font the way DrawMenuText draws it: the glyph body (colour bit 1) in
 * `color`, its drop shadow (bit 3) in the shadow colour. Returns the width. */
static int16_t canvasText(Canvas *c, int16_t x, int16_t y, const uint8_t *text, uint8_t color)
{
    CharsTableHeader *ct = (CharsTableHeader *)_smallCharsTable;
    const uint16_t *conv = (const uint16_t *)(ct + 1);
    int16_t x0 = x;
    uint8_t ch;

    while ((ch = *text++) != 0) {
        const SpriteGraphics *g;
        int16_t gx, gy;
        if (ch == ' ') {
            x = (int16_t)(x + ct->spaceWidth);
            continue;
        }
        g = gameSprite((uint16_t)(conv[ch - ' '] + ct->spriteIndexOffset));
        for (gy = 0; gy < g->nlines; gy++)
            for (gx = 0; gx < g->pixWidth; gx++) {
                uint8_t v = spritePixel(g, gx, gy);
                int16_t xx = (int16_t)(x + gx), yy = (int16_t)(y + gy);
                if (!(v & 0x0A) || xx < 0 || xx >= c->w || yy < 0 || yy >= c->h)
                    continue;
                c->pix[yy * c->w + xx] = (v & 0x02) ? color : C_DARK;
            }
        x = (int16_t)(x + g->pixWidth + ct->charSpacing);
    }
    return (int16_t)(x - x0);
}

static int16_t textWidth(const uint8_t *text)
{
    return (int16_t)GetTextWidth((uint8_t *)text, _smallCharsTable);
}

/* The tick and the cross, 7x7 with a black edge. */
static const char *const k_tick[7] = {
    "......#", ".....##", "#...##.", "##.##..", ".###...", "..#....", ".......",
};
static const char *const k_cross[7] = {
    "#.....#", "##...##", ".##.##.", "..###..", ".##.##.", "##...##", "#.....#",
};

static void canvasMark(Canvas *c, int16_t x, int16_t y, int16_t missed)
{
    const char *const *art = missed ? k_cross : k_tick;
    int16_t ax, ay, dx, dy;
    for (ay = 0; ay < 7; ay++)                     /* the edge first ... */
        for (ax = 0; ax < 7; ax++)
            if (art[ay][ax] == '#')
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++)
                        canvasFill(c, (int16_t)(x + ax + dx), (int16_t)(y + ay + dy), 1, 1, C_BLACK);
    for (ay = 0; ay < 7; ay++)                     /* ... then the mark on it */
        for (ax = 0; ax < 7; ax++)
            if (art[ay][ax] == '#')
                canvasFill(c, (int16_t)(x + ax), (int16_t)(y + ay), 1, 1,
                           missed ? C_RED : C_GREEN);
}

/* The canvas as a sprite: on SDL the picture itself; on the Amiga converted
 * to the linked planes the blitter draws (per line one row per plane, the
 * mask repeated per plane), as the keeper's arrow is. */
static void canvasPublish(Canvas *c)
{
    SpriteGraphics *depth = gameSprite(DEPTH_SPRITE);
    SpriteGraphics *g = &c->g;

    g->type = 1;
    g->pixWidth = c->w;
    g->nlines = c->h;
    g->wquads = c->wq;
    g->centerX = 0;
    g->centerY = 0;
    g->ordinal = depth->ordinal;
#ifdef SWOS_SDL_BUILD
    g->sprData = c->pix;
    g->maskPtr = (uint8_t *)0;
    g->bltsize = 0;
#else
    {
        uint16_t sp = spriteNumPlanes(g->ordinal), p;
        int16_t y, wd, b;
        if (sp > SPRITE_NUM_PLANES) {       /* the planes would not fit: show nothing */
            g->nlines = 0;
            return;
        }
        for (y = 0; y < c->h; y++)
            for (wd = 0; wd < c->wq; wd++) {
                uint16_t mask = 0, planes[PITCH_NUM_PLANES] = { 0 };
                const uint8_t *px = c->pix + y * c->w + wd * 16;
                for (b = 0; b < 16; b++) {
                    uint8_t col = px[b];
                    uint16_t bit = (uint16_t)(0x8000u >> b);
                    if (!col)
                        continue;
                    mask |= bit;
                    for (p = 0; p < sp; p++)
                        if (col & (1u << p))
                            planes[p] |= bit;
                }
                for (p = 0; p < sp; p++) {
                    uint32_t at = ((uint32_t)y * sp + p) * c->wq + wd;
                    c->data[at] = planes[p];
                    c->mask[at] = mask;
                }
            }
        g->sprData = (uint8_t *)c->data;
        g->maskPtr = (uint8_t *)c->mask;
        g->bltsize = (uint16_t)(((c->h * sp) << 6) | c->wq);
    }
#endif
}

static void canvasDraw(Canvas *c, int16_t x, int16_t y)
{
    if (c->g.nlines > 0)
        DrawSpriteClipped(&c->g, x, y);
}

/* ---- the pieces ------------------------------------------------------- */

static void number(uint8_t *out, int16_t n)
{
    if (n >= 10) {
        *out++ = (uint8_t)('0' + n / 10);
        n %= 10;
    }
    *out++ = (uint8_t)('0' + n);
    *out = 0;
}

static TeamGeneralInfo *sideTeam(int16_t side)
{
    return _topTeamData.teamNumber == side + 1 ? &_topTeamData : &_bottomTeamData;
}

static const uint8_t *controlLabel(const TeamGeneralInfo *t)
{
    if (t->playerNumber == 1)
        return (const uint8_t *)"PLAYER 1";
    if (t->playerNumber == 2)
        return (const uint8_t *)"PLAYER 2";
    return (const uint8_t *)"CPU";
}

/* The shooting side's ten takers, in the order they shoot (spritesTable 10
 * first, down to 1: NextPenalty), the one at the spot highlighted. The bars
 * are as long as the side's longest name needs. */
static int16_t s_panelTeam = -1, s_panelSlot = -1;

/* As SWOS 2020 draws it: your side's takers on dark red, the other side's on red,
 * the one at the spot light blue on both. "Yours" is PLAYER 1's side; with no
 * PLAYER 1, a human side; with none, team 1. */
static int16_t isYourSide(const TeamGeneralInfo *t)
{
    const TeamGeneralInfo *o = t == &_topTeamData ? &_bottomTeamData : &_topTeamData;
    if (t->playerNumber == 1 || o->playerNumber == 1)
        return t->playerNumber == 1;
    if ((t->playerNumber != 0) != (o->playerNumber != 0))
        return t->playerNumber != 0;
    return t->teamNumber == 1;
}

/* Dark red is the maroon kit's entry 12, which a match with purple in it repaints
 * purple (team_colors.c); the dark brown-red 4 stands in then. */
static uint8_t takersBarColor(const TeamGeneralInfo *t)
{
    extern int32_t TeamColors_PurpleInMatch(void);
    if (!isYourSide(t))
        return C_RED;
    return TeamColors_PurpleInMatch() ? C_BROWN : C_MAROON;
}

static void paintPanel(void)
{
    TeamGeneralInfo *t = &_bottomTeamData;
    int16_t current = t->teamNumber == 1 ? _team1PenaltyShooterIndex
                                         : _team2PenaltyShooterIndex;
    int16_t face = _useIndividualPlayerSkinColor ? 9 : 0;
    int16_t row, longest = 0, barW;

    for (row = 0; row < 10; row++) {
        PlayerInfo *pi = GetPlayerPointerFromShirtNumber(t->spritesTable[10 - row], t);
        int16_t w = textWidth((const uint8_t *)pi->shortName);
        if (w > longest)
            longest = w;
    }
    barW = (int16_t)(face + 16 + longest + 3);
    canvasSize(&s_panel, (int16_t)(barW + 2), PANEL_H);
    if (barW > s_panel.w - 2)
        barW = (int16_t)(s_panel.w - 2);
    canvasClear(&s_panel);

    for (row = 0; row < 10; row++) {
        int16_t slot = (int16_t)(10 - row);
        int16_t y = (int16_t)(row * 9 + 1);
        PlayerInfo *pi = GetPlayerPointerFromShirtNumber(t->spritesTable[slot], t);
        uint8_t num[4];
        int16_t x = 1;

        canvasFill(&s_panel, 0, (int16_t)(y - 1), (int16_t)(barW + 2), 10, C_BLACK);  /* rows share an edge */
        canvasFill(&s_panel, 1, y, barW, 8, slot == current ? C_LIGHT_BLUE : takersBarColor(t));
        if (face) {
            canvasSprite(&s_panel, _playerFacesTable[(uint8_t)pi->face], x, y);
            x = (int16_t)(x + face);
        }
        number(num, (int16_t)(uint8_t)pi->shirtNumber);
        canvasText(&s_panel, (int16_t)(x + 12 - textWidth(num)), (int16_t)(y + 1), num, C_WHITE);
        canvasText(&s_panel, (int16_t)(x + 15), (int16_t)(y + 1),
                   (const uint8_t *)pi->shortName, C_WHITE);
    }
    canvasPublish(&s_panel);
}

/* Both sides' last five kicks on a dark band, as wide as the longer team
 * name needs. */
enum { KICK_STEP = 18 };

static const uint8_t *sideName(int16_t side)
{
    TeamGeneralInfo *t = sideTeam(side);
    return t->inGameTeamPtr ? (const uint8_t *)((TeamGame *)t->inGameTeamPtr)->teamName
                            : (const uint8_t *)"";
}

static void paintBoard(void)
{
    int16_t first = (int16_t)(s_attempts[0] > s_attempts[1] ? s_attempts[0] : s_attempts[1]);
    int16_t side, c, nameW, nameRight, scoreX, kickX0;

    nameW = textWidth(sideName(0));
    if (textWidth(sideName(1)) > nameW)
        nameW = textWidth(sideName(1));
    nameRight = (int16_t)(nameW + 6);
    scoreX = (int16_t)(nameRight + 12);
    kickX0 = (int16_t)(scoreX + 18);
    canvasSize(&s_board, (int16_t)(kickX0 + (kShownKicks - 1) * KICK_STEP + 10), BOARD_H);
    first = (int16_t)(first > kShownKicks ? first - kShownKicks : 0);
    canvasClear(&s_board);
    canvasFill(&s_board, 0, 0, s_board.w, s_board.h, C_DARK);
    for (c = 0; c < kShownKicks; c++) {
        uint8_t num[4];
        number(num, (int16_t)(first + c + 1));
        canvasText(&s_board, (int16_t)(kickX0 + c * KICK_STEP - textWidth(num) / 2), 2,
                   num, C_WHITE);
    }
    for (side = 0; side < 2; side++) {
        int16_t y = side == 0 ? 13 : 24;
        const uint8_t *name = sideName(side);
        uint8_t num[4];

        canvasText(&s_board, (int16_t)(nameRight - textWidth(name)), y, name, C_WHITE);
        number(num, goalsOf(side));
        canvasText(&s_board, (int16_t)(scoreX - textWidth(num) / 2), y, num, C_WHITE);
        for (c = 0; c < kShownKicks; c++) {
            int16_t k = (int16_t)(first + c), cx = (int16_t)(kickX0 + c * KICK_STEP);
            int8_t state = k < kMaxKicks ? s_kick[side][k] : KICK_NONE;
            if (state == KICK_SCORED || state == KICK_MISSED) {
                canvasMark(&s_board, (int16_t)(cx - 3), (int16_t)(y - 1), state == KICK_MISSED);
            } else {
                const uint8_t *s = (const uint8_t *)(state == KICK_TAKING ? "*" : "-");
                canvasText(&s_board, (int16_t)(cx - textWidth(s) / 2), y, s, C_WHITE);
            }
        }
    }
    canvasPublish(&s_board);
}

static int16_t s_labelPlayer[2] = { -1, -1 };

static void paintLabel(int16_t which, const TeamGeneralInfo *t)
{
    Canvas *c = &s_label[which];
    const uint8_t *text = controlLabel(t);
    canvasSize(c, (int16_t)(textWidth(text) + 6), LABEL_H);
    canvasClear(c);
    canvasFill(c, 0, 0, c->w, c->h, C_DARK);
    canvasText(c, (int16_t)(c->w - 3 - textWidth(text)), 2, text, C_WHITE);
    canvasPublish(c);
}

/* ---- the frame ---------------------------------------------------------- */

/* The match's own HUD stands down while this screen is up (asked for
 * 2026-11-01: "only the new overlay"): the name of the player on the ball,
 * the number over each side's controlled player and the marked-player
 * marker. Each is hidden the game's own way, imageIndex -1, after the frame's
 * HUD updates have placed it and before the sprites are drawn (game_loop.c).
 * The clock and the result board go in game_state.c. */
extern Sprite _currentPlayerNameSprite;
extern Sprite *_penaltyShooterSprite;
extern uint16_t _currentPlayerIndicatorPosition;
extern Sprite _team1CurPlayerNumSprite, _team2CurPlayerNumSprite;
extern Sprite _playerMarkSprite;

void PenaltyShootout_HideMatchHud(void)
{
    if (!PenaltyShootout_HudActive())
        return;
    /* Without the takers list (the Amiga) the corner names the taker instead,
     * as it names the man on the ball in a match -- unless OPTIONS has the
     * name display off. */
    if (!HUD_TAKERS_LIST && _penaltyShooterSprite && _currentPlayerIndicatorPosition != 0)
        PlaceCurrentPlayerName(&_currentPlayerNameSprite, _penaltyShooterSprite, &_bottomTeamData);
    else
        _currentPlayerNameSprite.imageIndex = -1;
    _team1CurPlayerNumSprite.imageIndex = -1;
    _team2CurPlayerNumSprite.imageIndex = -1;
    _playerMarkSprite.imageIndex = -1;
}

static int16_t s_canvasesReady;

void PenaltyShootout_DrawHud(void)
{
    TeamGeneralInfo *shooter = &_bottomTeamData;
    int16_t slot, bx, right;

    if (!PenaltyShootout_HudActive() || !shooter->spritesTable || !shooter->inGameTeamPtr)
        return;
    if (!s_canvasesReady) {
#ifdef SWOS_SDL_BUILD
        canvasInit(&s_panel, PANEL_WQ, PANEL_H, s_panelPix, 0, 0);
        canvasInit(&s_board, BOARD_WQ, BOARD_H, s_boardPix, 0, 0);
        canvasInit(&s_label[0], LABEL_WQ, LABEL_H, s_labelPix[0], 0, 0);
        canvasInit(&s_label[1], LABEL_WQ, LABEL_H, s_labelPix[1], 0, 0);
#else
        canvasInit(&s_panel, PANEL_WQ, PANEL_H, s_panelPix, 0, 0);   /* never drawn here */
        canvasInit(&s_board, BOARD_WQ, BOARD_H, s_boardPix, s_boardData, s_boardMask);
        canvasInit(&s_label[0], LABEL_WQ, LABEL_H, s_labelPix[0], s_labelData[0], s_labelMask[0]);
        canvasInit(&s_label[1], LABEL_WQ, LABEL_H, s_labelPix[1], s_labelData[1], s_labelMask[1]);
#endif
        s_canvasesReady = 1;
        s_panelTeam = s_panelSlot = -1;
        s_labelPlayer[0] = s_labelPlayer[1] = -1;
        s_boardDirty = 1;
    }

    trackKicks();

    /* Amiga: shown only while a kick is set up (ST_PENALTIES), gone from the
     * strike to the next kick -- the keeper's dive frames are the dearest bobs
     * the match draws. And REPAINTED only then: a repaint is CPU work (every
     * glyph read pixel by pixel, the picture converted to planes) that costs
     * an A1200 frames, and a result arrives mid-dive (the save) or in the
     * celebration, which is where it showed as a slow keeper (2026-11-01). The
     * record is kept regardless; the next kick's set-up paints it. */
    if (!HUD_DURING_KICK && _gameState != ST_PENALTIES)
        return;

    if (s_panelStale) {
        s_panelStale = 0;
        s_panelTeam = s_panelSlot = -1;
        s_labelPlayer[0] = s_labelPlayer[1] = -1;
    }
    slot = shooter->teamNumber == 1 ? _team1PenaltyShooterIndex : _team2PenaltyShooterIndex;
    if (HUD_TAKERS_LIST && (shooter->teamNumber != s_panelTeam || slot != s_panelSlot)) {
        s_panelTeam = shooter->teamNumber;
        s_panelSlot = slot;
        paintPanel();
    }
    if (s_boardDirty) {
        s_boardDirty = 0;
        paintBoard();
    }
    if (HUD_LABELS && _topTeamData.playerNumber != s_labelPlayer[0]) {
        s_labelPlayer[0] = _topTeamData.playerNumber;
        paintLabel(0, &_topTeamData);
    }
    if (HUD_LABELS && _bottomTeamData.playerNumber != s_labelPlayer[1]) {
        s_labelPlayer[1] = _bottomTeamData.playerNumber;
        paintLabel(1, &_bottomTeamData);
    }

    /* Placed in the 336-wide frame the match HUD is laid out for; the board
     * centred on a wider SDL screen, the labels on its right edge. */
    bx = (int16_t)((_screen_width - SCREEN_WIDTH) / 2);
    right = (int16_t)(_screen_width - 8);
    if (HUD_TAKERS_LIST)
        canvasDraw(&s_panel, 8, 72);
    canvasDraw(&s_board, (int16_t)(bx + (SCREEN_WIDTH - s_board.w) / 2), 204);
    if (HUD_LABELS) {
        canvasDraw(&s_label[0], (int16_t)(right - s_label[0].w), 96);
        canvasDraw(&s_label[1], (int16_t)(right - s_label[1].w), 152);
    }
}
