#include "stream.h"
#include <zgdk/entities/physics.h>

void stream_shift(tilemap_stream_t *s, camera_t *camera, entity_pool_t *entities, uint8_t dtx, uint8_t dty) {
    uint16_t dx = (uint16_t)dtx * 16, dy = (uint16_t)dty * 16;
    if (!s || s->read) return;
    if (camera && entities && !physics_entities_compatible(entities)) return;
    /* Check every owner before changing any. camera_rebase needs only that the
     * shift doesn't take the camera below 0; stream rebase validates the offsets
     * that RAM compaction needs too. */
    if (camera && ((uint16_t)camera->x < dx || (uint16_t)camera->y < dy)) return;
    if (tilemap_stream_rebase(s, dtx, dty) != ERR_SUCCESS) return;
    (void)tilemap_stream_shift_buffer(s, dtx, dty);
    if (camera) {
        (void)camera_rebase(camera, dx, dy);
        if (entities) physics_entities_shift(entities, -(int16_t)dx, -(int16_t)dy);
    }
}
