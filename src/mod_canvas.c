/*
 * mod_canvas.c -- the Amiga's canvases for mods (include/mod_canvas.h): an 8-bit picture shown as
 * one sprite. Moved out of mod_practice.c (its shoot-out board) for the autoplay mod, unchanged.
 * Amiga only.
 */
#ifndef SWOS_SDL_BUILD
#include "mod_canvas.h"
#include "display.h"
#include "sprites.h"
#include "text_render.h"        /* GetTextWidth */

#include <string.h>

extern uint8_t *_spriteGraphicsPtr;
extern uint8_t _smallCharsTable[];
extern void *_charTables[];

#define DEPTH_SPRITE 1074               /* the HUD's shirt number: the canvases' plane depth */

void canvas_init(Canvas *c, int16_t wq, int16_t h, uint8_t *pix, uint16_t *data, uint16_t *mask)
{
    c->maxWq = c->wq = wq;
    c->w = (int16_t)(wq * 16);
    c->maxH = c->h = h;
    c->pix = pix;
    c->data = data;
    c->mask = mask;
}

/* as small as what it shows: every pixel of it is blitted twice a frame, see-through ones too */
void canvas_size(Canvas *c, int16_t w, int16_t h)
{
    const int16_t wq = (int16_t)((w + 15) / 16);
    c->wq = wq < 1 ? 1 : wq > c->maxWq ? c->maxWq : wq;
    c->w = (int16_t)(c->wq * 16);
    c->h = h < 1 ? 1 : h > c->maxH ? c->maxH : h;
}

void canvas_put(Canvas *c, int32_t x, int32_t y, uint8_t col)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h)
        c->pix[y * c->w + x] = col;
}

void canvas_fill(Canvas *c, uint8_t col)
{
    for (int32_t i = 0; i < (int32_t)c->w * c->h; i++)
        c->pix[i] = col;
}

void canvas_pixmap(Canvas *c, const char *const *rows, int32_t w, int32_t h, int32_t x, int32_t y,
                   const char *keys, const uint8_t *cols)
{
    for (int32_t j = 0; j < h; j++)
        for (int32_t i = 0; i < w; i++) {
            const char *k = strchr(keys, rows[j][i]);
            if (rows[j][i] != '.' && k)
                canvas_put(c, x + i, y + j, cols[k - keys]);
        }
}

/* a sprite's colour at (x, y): one row per plane each line, wquads words wide */
static uint8_t sprite_pixel(const SpriteGraphics *s, int32_t x, int32_t y)
{
    const uint16_t sp = s->type == 0 ? SPRITE_NUM_PLANES : spriteNumPlanes(s->ordinal);
    const uint16_t *line = (const uint16_t *)s->sprData + (uint32_t)y * sp * s->wquads;
    const uint16_t bit = (uint16_t)(0x8000u >> (x & 15));
    uint8_t col = 0;
    for (uint16_t p = 0; p < sp; p++)
        if (line[p * s->wquads + (x >> 4)] & bit)
            col |= (uint8_t)(1u << p);
    return col;
}

static SpriteGraphics *game_sprite(uint16_t index) { return ((SpriteGraphics **)_spriteGraphicsPtr)[index]; }

/* the game's font (big: the PC board's, _charTables[1]; else the small one) as DrawMenuText draws
 * it: its body (bit 1) white, its shadow (bit 3) dark */
static uint8_t *font_of(int32_t big) { return big ? (uint8_t *)_charTables[1] : _smallCharsTable; }

int32_t canvas_text(Canvas *c, int32_t x, int32_t y, const char *text, int32_t big)
{
    const CharsTableHeader *ct = (const CharsTableHeader *)font_of(big);
    const uint16_t *conv = (const uint16_t *)(ct + 1);
    const int32_t x0 = x;
    for (uint8_t ch; (ch = (uint8_t)*text++) != 0;) {
        if (ch == ' ') {
            x += ct->spaceWidth;
            continue;
        }
        const SpriteGraphics *g = game_sprite((uint16_t)(conv[ch - ' '] + ct->spriteIndexOffset));
        for (int32_t gy = 0; gy < g->nlines; gy++)
            for (int32_t gx = 0; gx < g->pixWidth; gx++) {
                const uint8_t v = sprite_pixel(g, gx, gy);
                if (v & 0x0A)
                    canvas_put(c, x + gx, y + gy, (v & 0x02) ? CANVAS_WHITE : CANVAS_DARK);
            }
        x += g->pixWidth + ct->charSpacing;
    }
    return x - x0;
}

int32_t canvas_text_width(const char *t, int32_t big) { return (int32_t)GetTextWidth((uint8_t *)t, font_of(big)); }

/* the canvas as linked sprite planes (per line one row per plane, the mask repeated per plane) */
void canvas_publish(Canvas *c)
{
    SpriteGraphics *g = &c->g;
    g->type = 1;
    g->pixWidth = c->w;
    g->nlines = c->h;
    g->wquads = c->wq;
    g->centerX = g->centerY = 0;
    g->ordinal = game_sprite(DEPTH_SPRITE)->ordinal;
    const uint16_t sp = spriteNumPlanes(g->ordinal);
    if (sp > SPRITE_NUM_PLANES) {           /* (the planes would not fit: nothing shown) */
        g->nlines = 0;
        return;
    }
    for (int32_t y = 0; y < c->h; y++)
        for (int32_t wd = 0; wd < c->wq; wd++) {
            uint16_t mask = 0, planes[8] = { 0 };
            const uint8_t *px = c->pix + y * c->w + wd * 16;
            for (int32_t b = 0; b < 16; b++) {
                if (!px[b])
                    continue;
                const uint16_t bit = (uint16_t)(0x8000u >> b);
                mask |= bit;
                for (uint16_t p = 0; p < sp; p++)
                    if (px[b] & (1u << p))
                        planes[p] |= bit;
            }
            for (uint16_t p = 0; p < sp; p++) {
                const uint32_t at = ((uint32_t)y * sp + p) * c->wq + wd;
                c->data[at] = planes[p];
                c->mask[at] = mask;
            }
        }
    g->sprData = (uint8_t *)c->data;
    g->maskPtr = (uint8_t *)c->mask;
    g->bltsize = (uint16_t)(((c->h * sp) << 6) | c->wq);
}
#endif
