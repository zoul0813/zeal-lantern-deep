#include <stdint.h>
#include <stddef.h>
#include <zvb_gfx.h>
#include <zgdk/sound/tracker.h>
#include "game.h"
#include "assets.h"
#include <zgdk/tileset.h>

zar_file_t pack;

/* ZAR provides page-safe reads; ZGDK handles chunk decoding and VRAM uploads. */
static zos_err_t read_tileset(void *entry, void *buffer, uint16_t *size) {
    return zar_entry_read(&pack, (zar_entry_t *)entry, buffer, size);
}

/*
 * The tileset, palette, music and shaft chunks live in lanterndeep.zar (cmake/assets.cmake)
 * and are read when needed. This work area is first the buffer the tileset streams through (one LZ
 * chunk at a time, at most TILE_CHUNK_MAX bytes), then blob_take() hands it out for good: the music
 * patterns (3 x 772), the shaft (480) and the three fog grids (3 x 280). It replaces the embedded
 * tileset blob, which had to be 4.8 KB.
 */
#define WORK_SIZE (3 * sizeof(pattern_t) + WIDTH * WORLD_ROWS + 3 * (HEIGHT - 1) * WIDTH)
typedef char work_holds_a_tile_chunk[(WORK_SIZE >= TILE_CHUNK_MAX) ? 1 : -1];

static uint8_t work[WORK_SIZE];
static uint16_t blob_used;

zos_err_t load_palette(gfx_context* ctx) {
    uint16_t size = sizeof(work);
    zos_err_t err = zar_entry_load(&pack, "tiles.ztp", work, &size);
    if (err != ERR_SUCCESS) return err;
    return gfx_palette_load(ctx, work, size, 0) == GFX_SUCCESS ? ERR_SUCCESS : ERR_FAILURE;
}

zos_err_t load_tileset(gfx_context* ctx, gfx_tileset_options* options) {
    zar_entry_t entry;
    zos_err_t err;
    err = zar_entry_from_name(&pack, "tiles.tch", &entry);
    if (err != ERR_SUCCESS) return err;
    return tileset_load_chunks(ctx, read_tileset, &entry, gfx_tileset_load_lz,
                               work, sizeof(work), options);
}

void *blob_take(uint16_t size) {
    uint8_t *p = work + blob_used;
    if (sizeof(work) - blob_used < size) return NULL;
    blob_used += size;
    return p;
}
