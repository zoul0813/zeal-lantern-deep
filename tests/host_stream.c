#include <zgdk/tilemap/flags.h>
/* Host test: stream a world through the 80x40 ring and check every visible tile matches */
#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "stream.h"
#include <zgdk/entities/physics.h>

TILEFLAGS_DEFINE_CACHE(40);

void *mem_cpy(void *dst, const void *src, size_t n) { return memmove(dst, src, n); }
/* Compatible initialized shape; the shift itself is stubbed below. */
static uint8_t record_storage[sizeof(physics_entity_t)];
static entity_pool_t test_entities = {record_storage, record_storage, 1, sizeof(physics_entity_t), offsetof(sprite_entity_t, type)};
static unsigned entity_shifts;
static int16_t entity_dx, entity_dy;
void physics_entities_shift(entity_pool_t *pool, int16_t dx, int16_t dy) {
    assert(pool == &test_entities); entity_shifts++; entity_dx = dx; entity_dy = dy;
}

Tilemap *_TILEMAP;
int16_t tilemap_get_xy(uint8_t x, uint8_t y) { (void)x; (void)y; return 0; }
static uint8_t vram[40][80];

zos_err_t tilemap_register(Tilemap *tilemap, uint8_t *tiles) { _TILEMAP = tilemap; tilemap->tiles = tiles; return ERR_SUCCESS; }
void tilemap_scroll(uint8_t layer, uint16_t x, uint16_t y) { (void)layer; (void)x; (void)y; }
gfx_error gfx_tilemap_load(gfx_context *ctx, void *tiles, uint8_t size, uint8_t layer, uint8_t x, uint8_t y) {
    uint8_t i;
    (void)ctx; (void)layer;
    if (x + size > 80 || y >= 40) { printf("FAIL: out of ring write x %d size %d y %d\n", x, size, y); return 1; }
    for (i = 0; i < size; i++) vram[y][x + i] = ((uint8_t *)tiles)[i];
    return GFX_SUCCESS;
}
gfx_error gfx_tilemap_place(gfx_context *ctx, uint8_t tile, uint8_t layer, uint8_t x, uint8_t y) {
    (void)ctx; (void)layer;
    if (x >= 80 || y >= 40) { printf("FAIL: out of ring place %d %d\n", x, y); return 1; }
    vram[y][x] = tile;
    return GFX_SUCCESS;
}

#define SCREEN_W 320
#define SCREEN_H 240

#define W 120
#define H 60
static uint8_t world[W * H];

static int check(const camera_t *cam) {
    int x, y, bad = 0;
    for (y = cam->y >> 4; y <= (cam->y + 239) >> 4 && y < H; y++)
        for (x = cam->x >> 4; x <= (cam->x + 319) >> 4 && x < W; x++)
            if (vram[y % 40][x % 80] != world[y * W + x]) bad++;
    return bad;
}

int main(void) {
    tilemap_stream_t s;
    Tilemap tm;
    camera_t cam;
    int i, fails = 0, step;
    for (i = 0; i < W * H; i++) world[i] = (uint8_t)(i * 7 + i / W);
    camera_init_view(&cam, W * 16, H * 16, 160, 120);
    assert(stream_init(NULL, &s, &tm, world, W, H, 1, &cam) == ERR_SUCCESS);
    assert(s.view_w == 160 && s.view_h == 120);
    camera_init_view(&cam, W * 16, H * 16, SCREEN_W, SCREEN_H);
    stream_init(NULL, &s, &tm, world, W, H, 1, &cam);
    if (check(&cam)) { fails++; printf("FAIL: initial window\n"); }
    // pan right, down, diagonally back, at varying speeds
    for (step = 0; step < 900; step++) {
        int16_t dx = step < 300 ? 3 : step < 600 ? -2 : 4;
        int16_t dy = step < 150 ? 0 : step < 450 ? 2 : -3;
        camera_set(&cam, cam.x + dx, cam.y + dy);
        tilemap_stream_update(NULL, &s, cam.x, cam.y);
        if (check(&cam)) { fails++; printf("FAIL: step %d cam %d,%d\n", step, cam.x, cam.y); break; }
    }
    // teleport
    camera_set(&cam, 900, 500);
    tilemap_stream_update(NULL, &s, cam.x, cam.y);
    if (check(&cam)) { fails++; printf("FAIL: teleport\n"); }
    // edit a visible tile
    tilemap_stream_set_tile(NULL, &s, (cam.x >> 4) + 3, (cam.y >> 4) + 2, 0xEE);
    if (check(&cam)) { fails++; printf("FAIL: set tile\n"); }
    // endless vertical shaft: 20 x 24 buffer, rebase one row whenever the camera passes row 4
    {
        static uint8_t shaft[20 * 24];
        uint16_t base = 0;   // absolute row of buffer row 0
        int r, c;
        tilemap_stream_t v;
        camera_t vc;
        #define SHAFT(row, col) ((uint8_t)((row) * 3 + (col) * 5 + 1))
        for (r = 0; r < 24; r++) for (c = 0; c < 20; c++) shaft[r * 20 + c] = SHAFT(r, c);
        for (r = 0; r < 40; r++) for (c = 0; c < 80; c++) vram[r][c] = 0;
        camera_init_view(&vc, 20 * 16, 24 * 16, SCREEN_W, SCREEN_H);
        stream_init(NULL, &v, &tm, shaft, 20, 24, 1, &vc);
        for (step = 0; step < 2000; step++) {
            camera_set(&vc, 0, vc.y + 2);
            while (vc.y >= 4 * 16) {
                stream_shift(&v, &vc, &test_entities, 0, 1);
                base++;
                for (c = 0; c < 20; c++) shaft[23 * 20 + c] = SHAFT(base + 23, c);
            }
            tilemap_stream_update(NULL, &v, vc.x, vc.y);
            {
                int x, y, bad = 0;
                for (y = vc.y >> 4; y <= (vc.y + 239) >> 4; y++)
                    for (x = 0; x < 20; x++) {
                        uint8_t got = vram[(y + v.ring_y) % 40][(x + v.ring_x) % 80];
                        if (got != SHAFT(base + y, x)) bad++;
                    }
                if (bad) { fails++; printf("FAIL: shaft step %d base %u cam %d (%d bad)\n", step, base, vc.y, bad); break; }
            }
            if ((uint16_t)((vc.y + vc.origin_y) % 640) != (uint16_t)(((base * 16) + vc.y) % 640)) {
                fails++; printf("FAIL: hardware scroll drifted at step %d\n", step); break;
            }
        }
        if (base < 200) { fails++; printf("FAIL: shaft only rebased %u rows\n", base); }
    }

    // Direct SDK follow preserves fixed-point conversion; stream adapters preserve
    // first-layer entity ownership and atomic rejection of invalid coordinated shifts.
    {
        body_t b = {0};
        tilemap_stream_t other;
        static uint8_t other_tiles[W * H];
        unsigned shifted;
        int16_t old_x;
        camera_init_view(&cam, W * 16, H * 16, SCREEN_W, SCREEN_H);
        b.x = PHYSICS_FP(600) + 8; b.y = PHYSICS_FP(400) + 8;
        b.w = 16; b.h = 30; b.vx = 1;
        cam.lead = 32;
        camera_follow_target(&cam, PHYSICS_FP_INT(b.x), PHYSICS_FP_INT(b.y), b.w, b.h,
                             b.vx < 0 ? -1 : b.vx != 0);
        if (cam.x != 434 || cam.y != 280) { fails++; printf("FAIL: body bounds follow\n"); }
        stream_init(NULL, &s, &tm, world, W, H, 1, &cam);
        other = s;
        memcpy(other_tiles, world, sizeof world);
        other.tiles = other_tiles;
        shifted = entity_shifts;
        old_x = cam.x;
        stream_shift(&s, &cam, &test_entities, 1, 1);
        stream_shift(&other, NULL, &test_entities, 1, 1);
        if (entity_shifts != shifted + 1 || entity_dx != -16 || entity_dy != -16 ||
            cam.x != old_x - 16 || s.ring_x != other.ring_x || s.tx != other.tx) {
            fails++; printf("FAIL: coordinated two-layer shift\n");
        }
        shifted = entity_shifts;
        old_x = cam.x;
        {
            entity_pool_t incompatible = test_entities;
            uint8_t saved_world[sizeof world];
            camera_t saved_camera = cam;
            tilemap_stream_t saved_stream = s;
            incompatible.tag_offset++;
            memcpy(saved_world, world, sizeof world);
            stream_shift(&s, &cam, &incompatible, 1, 1);
            if (memcmp(saved_world, world, sizeof world) || memcmp(&saved_camera, &cam, sizeof cam)
                || memcmp(&saved_stream, &s, sizeof s) || entity_shifts != shifted) {
                fails++; printf("FAIL: incompatible entity pool changed owners\n");
            }
        }
        stream_shift(&s, &cam, &test_entities, W, 0);
        if (entity_shifts != shifted || cam.x != old_x) {
            fails++; printf("FAIL: invalid shift changed owners\n");
        }
    }

    printf(fails ? "stream: %d FAILED\n" : "stream: all passed\n", fails);
    return fails != 0;
}
