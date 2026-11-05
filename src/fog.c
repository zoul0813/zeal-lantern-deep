/*
 * Darkness: layer 1 is a screen-fixed sheet of fog tiles (dark, 75%, 50%, 25%, clear) over the
 * scrolling shaft on layer 0. Each light (the lantern, landed flares) stamps a soft circle into a
 * 20x14 level grid; only cells whose level changed are written to VRAM. Enemy sprites are drawn
 * with SPRITE_BEHIND_FG, so the fog hides them in the dark for free.
 * Recomputed only when a light moves to another tile or changes radius, in the time each frame has left.
 */
#include <core.h>
#include "game.h"
#include "assets.h"

#define FOG_ROWS   (HEIGHT - 1)   // row 0 is the HUD
#define MAX_LIGHTS 6
#define LEVELS     5

static const uint8_t FOG_TILES[LEVELS] = { TILE_EMPTY, FOG_25, FOG_50, FOG_75, FOG_DARK };

typedef uint8_t fog_grid[FOG_ROWS][WIDTH];
static fog_grid *cur_grid, *want_grid, *base_grid;   // in the dead tileset blob, see fog_init()
#define cur  (*cur_grid)    // levels on screen
#define want (*want_grid)   // levels being built
#define base (*base_grid)   // flares only: they move only when the camera does, so they are cached
static int8_t last_x[MAX_LIGHTS], last_y[MAX_LIGHTS];
static uint8_t last_r[MAX_LIGHTS];
static uint8_t last_n = 0xFF;
enum { IDLE, FLARES, COPY, LANTERN, WRITE };
static uint8_t stage;   // fog_update(), see step()
static uint8_t base_i;  // next flare to stamp
static int8_t diff_y;

zos_err_t fog_init(void) {
    cur_grid = blob_take(sizeof(fog_grid));
    want_grid = blob_take(sizeof(fog_grid));
    base_grid = blob_take(sizeof(fog_grid));
    return (cur_grid && want_grid && base_grid) ? ERR_SUCCESS : ERR_NO_MORE_MEMORY;
}

void fog_fill(uint8_t tile) {
    uint8_t y;
    tilemap_fill(&vctx, FOG_LAYER, tile, 0, 1, WIDTH, FOG_ROWS);
    for (y = 0; y < FOG_ROWS; y++) mem_set(cur[y], tile == TILE_EMPTY ? 0 : LEVELS - 1, WIDTH);
    last_n = 0xFF;
    stage = IDLE;
}

void fog_reset(void) {
    fog_fill(FOG_DARK);
}

/* level (0 clear .. 4 dark) at distance |dx|, |dy| from a light of radius r: (r + ring + 0.5)^2 thresholds */
static const uint8_t LUT[4][7][7] = {
    {
        { 0, 1, 2, 3, 4, 4, 4 },
        { 1, 1, 2, 3, 4, 4, 4 },
        { 2, 2, 3, 4, 4, 4, 4 },
        { 3, 3, 4, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
    },
    {
        { 0, 0, 1, 2, 3, 4, 4 },
        { 0, 0, 1, 2, 3, 4, 4 },
        { 1, 1, 2, 3, 3, 4, 4 },
        { 2, 2, 3, 3, 4, 4, 4 },
        { 3, 3, 3, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
    },
    {
        { 0, 0, 0, 1, 2, 3, 4 },
        { 0, 0, 0, 1, 2, 3, 4 },
        { 0, 0, 1, 2, 2, 3, 4 },
        { 1, 1, 2, 2, 3, 4, 4 },
        { 2, 2, 2, 3, 4, 4, 4 },
        { 3, 3, 3, 4, 4, 4, 4 },
        { 4, 4, 4, 4, 4, 4, 4 },
    },
    {
        { 0, 0, 0, 0, 1, 2, 3 },
        { 0, 0, 0, 0, 1, 2, 3 },
        { 0, 0, 0, 1, 1, 2, 3 },
        { 0, 0, 1, 1, 2, 3, 4 },
        { 1, 1, 1, 2, 3, 3, 4 },
        { 2, 2, 2, 3, 3, 4, 4 },
        { 3, 3, 3, 4, 4, 4, 4 },
    },
};

/*
 * A stamp is resumable: stamp_begin() clips the circle, stamp_rows() does a few rows per call, so
 * a full-radius light (13x13 cells) is spread over several fog steps.
 */
#define STAMP_ROWS 4
static fog_grid *st_grid;
static int8_t st_cx, st_cy, st_y, st_yb, st_xa, st_xb;
static uint8_t st_r;

static void stamp_begin(fog_grid *grid, int8_t cx, int8_t cy, uint8_t r) {
    int8_t span = r + 3;
    st_grid = grid;
    st_cx = cx;
    st_cy = cy;
    st_r = r > 3 ? 3 : r;
    st_y = cy - 1 - span;
    st_yb = cy - 1 + span;
    st_xa = cx - span;
    st_xb = cx + span;
    if (st_y < 0) st_y = 0;
    if (st_yb > FOG_ROWS - 1) st_yb = FOG_ROWS - 1;
    if (st_xa < 0) st_xa = 0;
    if (st_xb > WIDTH - 1) st_xb = WIDTH - 1;
}

/* returns 1 when the stamp is finished */
static uint8_t stamp_rows(void) {
    uint8_t n = STAMP_ROWS;
    for (; st_y <= st_yb && n; st_y++, n--) {
        int8_t dy = st_y + 1 - st_cy;
        const uint8_t *lv = LUT[st_r][dy < 0 ? -dy : dy];
        uint8_t *row = &(*st_grid)[st_y][st_xa];
        int8_t dx = st_xa - st_cx;
        uint8_t x = st_xb - st_xa + 1;
        for (; x; x--, dx++, row++) {
            uint8_t level = lv[dx < 0 ? -dx : dx];
            if (level < *row) *row = level;
        }
    }
    return st_y > st_yb;
}

static int8_t x0, x1, y0, y1;   // box to recompute, in grid cells

static void grow(int8_t cx, int8_t cy, uint8_t r) {
    int8_t span = r + 3;
    int8_t a = cx - span, b = cx + span, c = cy - 1 - span, d = cy - 1 + span;
    if (a < 0) a = 0;
    if (b > WIDTH - 1) b = WIDTH - 1;
    if (c < 0) c = 0;
    if (d > FOG_ROWS - 1) d = FOG_ROWS - 1;
    if (a < x0) x0 = a;
    if (b > x1) x1 = b;
    if (c < y0) y0 = c;
    if (d > y1) y1 = d;
}

static void remember(uint8_t n, const int8_t *xs, const int8_t *ys, const uint8_t *rs) {
    uint8_t i;
    last_n = n;
    for (i = 0; i < n; i++) {
        last_x[i] = xs[i];
        last_y[i] = ys[i];
        last_r[i] = rs[i];
    }
}

/*
 * The work is split into small steps (a few rows of a flare stamp, a few rows of the box copy, a
 * few rows of the lantern stamp, one row of changed cells), each well under 40 raster lines, and
 * fog_update() runs steps only while the frame has time left (raster_line() in game.h). A busy
 * frame does less fog and a quiet one catches up; the fog may trail the light by a frame or two,
 * which reads as a soft lag.
 */
#define STARVED_MAX 3   // after this many frames with no time left, run one step anyway
#define COPY_ROWS   7

static uint8_t starved;
static uint8_t stamping;   // a stamp_begin() is in progress
static int8_t copy_y;

static void write_row(void) {
    uint8_t *w = &want[diff_y][x0];
    uint8_t *c = &cur[diff_y][x0];
    uint8_t x;
    for (x = x0; x <= (uint8_t)x1; x++, w++, c++) {
        if (*w != *c) {
            *c = *w;
            gfx_tilemap_place(&vctx, FOG_TILES[*w], FOG_LAYER, x, diff_y + 1);
        }
    }
    if (++diff_y > y1) stage = IDLE;
}

/* Look at the lights; returns 0 when nothing changed */
static uint8_t check_lights(void) {
    int8_t xs[MAX_LIGHTS], ys[MAX_LIGHTS];
    uint8_t rs[MAX_LIGHTS];
    uint8_t n, i;
    uint8_t flares_moved = 0;

    n = actors_lights(xs, ys, rs);   // [0] is the lantern (radius 0xFF when out), then flares
    if (n > MAX_LIGHTS) n = MAX_LIGHTS;
    if (last_n == 0xFF || n != last_n) {
        flares_moved = 1;
    } else {
        for (i = 1; i < n; i++) {
            if (xs[i] != last_x[i] || ys[i] != last_y[i] || rs[i] != last_r[i]) {
                flares_moved = 1;
                break;
            }
        }
        if (!flares_moved && xs[0] == last_x[0] && ys[0] == last_y[0] && rs[0] == last_r[0]) return 0;
    }

    if (flares_moved) {
        mem_set(base, LEVELS - 1, sizeof(fog_grid));
        x0 = 0; x1 = WIDTH - 1; y0 = 0; y1 = FOG_ROWS - 1;
        remember(n, xs, ys, rs);
        base_i = 1;
        stamping = 0;
        stage = FLARES;
        return 1;
    }
    // only the lantern moved: the box around its old and new spot
    x0 = WIDTH; x1 = 0; y0 = FOG_ROWS; y1 = 0;
    if (rs[0] != 0xFF) grow(xs[0], ys[0], rs[0]);
    if (last_r[0] != 0xFF) grow(last_x[0], last_y[0], last_r[0]);
    remember(n, xs, ys, rs);
    if (x0 > x1 || y0 > y1) return 0;
    copy_y = y0;
    stage = COPY;
    return 1;
}

/* The lights are looked at once per frame at most (a sputtering lantern changes on every look) */
static uint8_t checked;

/* One step of fog work; returns 0 when there is nothing to do */
static uint8_t step(void) {
    switch (stage) {
        case FLARES:
            // flares into the flare grid, a few rows at a time
            if (stamping) {
                if (stamp_rows()) stamping = 0;
            } else if (base_i < last_n) {
                stamp_begin(base_grid, last_x[base_i], last_y[base_i], last_r[base_i]);
                base_i++;
                stamping = 1;
            } else {
                copy_y = y0;
                stage = COPY;
            }
            return 1;
        case COPY: {
            // the box from the flare grid into the wanted levels
            uint8_t n = COPY_ROWS;
            for (; copy_y <= y1 && n; copy_y++, n--) mem_cpy(&want[copy_y][x0], &base[copy_y][x0], x1 - x0 + 1);
            if (copy_y > y1) {
                stage = LANTERN;
                if (last_r[0] != 0xFF) stamp_begin(want_grid, last_x[0], last_y[0], last_r[0]);
                else st_y = 1, st_yb = 0;   // lantern out: nothing to stamp
            }
            return 1;
        }
        case LANTERN:
            if (stamp_rows()) {
                diff_y = y0;
                stage = WRITE;
            }
            return 1;
        case WRITE:
            write_row();
            return 1;
        default:
            if (checked) return 0;
            checked = 1;
            return check_lights();
    }
}

void fog_update(void) {
    checked = 0;
    if (raster_line() >= FRAME_DEADLINE) {
        if (stage == IDLE || ++starved < STARVED_MAX) return;
        step();   // starved: one small step anyway
    }
    starved = 0;
    while (raster_line() < FRAME_DEADLINE) {
        if (!step()) return;
    }
}
