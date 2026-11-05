#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
#include "zgdk/tilemap.h"
#include "zgdk/tilemap/stream.h"
#include <zgdk/camera.h>
#include <zgdk/entities.h>

#ifndef GAME_STREAM_H
#define GAME_STREAM_H

/**
 * Streams a world tile map bigger than the 80x40 hardware tilemap into one layer.
 * The layer is used as a ring buffer: world tile (tx, ty) lives at (tx % 80, ty % 40),
 * and only the visible 21x16 window (plus one tile) is kept up to date. Hardware scroll
 * wraps at 1280x640, so the camera position maps straight onto the scroll registers.
 *
 * The world buffer is also registered as the ZGDK Tilemap, so tile flags and body
 * collision work in world coordinates. For worlds taller than 40 rows, build the core
 * with TILEFLAGS_DEFINE_CACHE(world_height) in one game C file (2 bytes per row).
 */

/**
 * @brief Register the world as the ZGDK Tilemap and draw the window around the camera
 */
zos_err_t stream_init(gfx_context *ctx, tilemap_stream_t *stream, Tilemap *tilemap, uint8_t *tiles,
                      uint8_t width, uint8_t height, uint8_t layer, const camera_t *camera);

/* Update, edit and redraw directly with tilemap_stream_update(ctx, stream,
 * camera->x, camera->y), tilemap_stream_set_tile and tilemap_stream_redraw. */

/**
 * @brief Rebase for worlds longer than the buffer or than 2047 px (endless shafts, long runs)
 *
 * Moves the world buffer content by `dtx` columns / `dty` rows toward the origin
 * (positive = drop the first columns/rows), and shifts the camera, the loaded window and
 * every entity by the same amount, so nothing visibly moves and VRAM isn't redrawn.
 * The vacated last columns/rows keep their old bytes: fill them before the camera
 * reaches them, tilemap_stream_update() draws them when they come into view.
 * Two streamed layers: pass camera and entity pool to the first call only.
 * A NULL camera shifts just that stream's buffer and ring. A NULL pool rebases
 * the camera/stream without bodies. Incompatible pools reject the coordinated
 * shift before any owner is mutated.
 * Use only positive shifts; keep the buffer at least view + shift + 1 tiles on that axis.
 */
void stream_shift(tilemap_stream_t *stream, camera_t *camera, entity_pool_t *entities, uint8_t dtx, uint8_t dty);

#endif
