/*
 * mod_canvas.h -- the Amiga's CANVAS for mods: an 8-bit picture a mod paints (the game's font read
 * pixel by pixel, pixmaps), converted to sprite planes in CHIP RAM and drawn as one sprite with
 * DrawSpriteClipped at spritesEnd -- the Amiga's match frame keeps what is drawn into it and undraws
 * only the sprites, so a mod's text over the match is a sprite there (matchDrawn is the PC's).
 * The owner's way (complete-work src/penalty_shootout_hud.c); first used by the practice mod's
 * shoot-out board, shared since the autoplay mod's "AUTO-PLAY" (2026-11-25). Amiga only.
 */
#ifndef MOD_CANVAS_H
#define MOD_CANVAS_H

#ifndef SWOS_SDL_BUILD
#include <stdint.h>
#include "display.h"            /* SPRITE_NUM_PLANES */
#include "sprites.h"
#include "swos_sections.h"

/* A canvas's sprite planes: wq words a line, h lines (+2: a shifted blit reads on) -- give it
 * SWOS_CHIP_BSS arrays of this many words for its data and its mask. */
#define CANVAS_PLANES_WORDS(wq, h) ((wq) * (h) * SPRITE_NUM_PLANES + 2)

enum { CANVAS_WHITE = 2, CANVAS_DARK = 8 };     /* (the match palette's first 16: sprites are 16 colours) */

typedef struct {
    int16_t w, h, wq, maxWq, maxH;
    uint8_t *pix;                       /* w * h, colour 0 see-through */
    uint16_t *data, *mask;              /* its sprite planes, CHIP RAM */
    SpriteGraphics g;                   /* what DrawSpriteClipped draws, after canvas_publish */
} Canvas;

void    canvas_init(Canvas *c, int16_t wq, int16_t h, uint8_t *pix, uint16_t *data, uint16_t *mask);
void    canvas_size(Canvas *c, int16_t w, int16_t h);  /* as small as what it shows (up to its maximum) */
void    canvas_put(Canvas *c, int32_t x, int32_t y, uint8_t col);
void    canvas_fill(Canvas *c, uint8_t col);
void    canvas_pixmap(Canvas *c, const char *const *rows, int32_t w, int32_t h, int32_t x, int32_t y,
                      const char *keys, const uint8_t *cols);
/* text in the game's font (big: _charTables[1], else the small one), its body white, its shadow
 * dark; returns its width */
int32_t canvas_text(Canvas *c, int32_t x, int32_t y, const char *text, int32_t big);
int32_t canvas_text_width(const char *text, int32_t big);
void    canvas_publish(Canvas *c);      /* the picture as sprite planes: c->g ready to draw */
#endif

#endif
