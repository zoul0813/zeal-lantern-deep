/*
 * A simple bot for recording gameplay videos (AUTOPILOT builds only). It presses the buttons a
 * player would: walks to pickups on its ledge, otherwise to the nearest hole in the floor, jumps
 * over rock in the way, throws a flare when a wisp gets close and dims the lantern when out of flares.
 */
#include "game.h"

static int8_t dir = DIRECTION_RIGHT;
static int16_t last_x;
static uint8_t stuck;
static uint8_t cooldown;
static uint8_t jump_hold;
static uint8_t wander;
static uint16_t best_depth;
static uint8_t no_progress;     // frames to keep walking one way after getting stuck

#define WALKABLE(f)  (!((f) & TF_SOLID))
#define FLOOR(f)     ((f) & (TF_SOLID | TF_ONEWAY))

/* Is there something on the hero's row within `range` tiles? Returns signed tile distance or 0 */
static int8_t find_on_row(int16_t cx, int16_t cy, uint8_t want, uint8_t range) {
    int8_t d;
    for (d = 1; d <= (int8_t)range; d++) {
        if (shaft_flags(cx + (d << 4), cy) & want) return d;
        if (shaft_flags(cx - (d << 4), cy) & want) return -d;
    }
    return 0;
}

static int8_t find_hole(int16_t cx, int16_t cy) {
    int8_t d, best = 0;
    int16_t below = cy + 16;
    // walk outward while the hero's row is free; the first cell with no floor under it is the hole
    for (d = 1; d < WIDTH - 1; d++) {
        int16_t x = cx + (d << 4);
        if (x >= WIDTH * 16) break;
        if (shaft_flags(x, cy) & TF_SOLID) break;
        if (!FLOOR(shaft_flags(x, below))) { best = d; break; }
    }
    for (d = 1; d < WIDTH - 1; d++) {
        int16_t x = cx - (d << 4);
        if (x < 0) break;
        if (shaft_flags(x, cy) & TF_SOLID) break;
        if (!FLOOR(shaft_flags(x, below))) {
            if (!best || d < best) best = -d;
            break;
        }
    }
    return best;
}

static uint16_t think(void);

/* Lift station shop: oil when the tank is under half, then the upgrades, then flares, then leave. One key
 * every 12 frames, with nothing held in between, so each one is a fresh press. */
static uint16_t shop_keys(void) {
    static uint8_t wait;
    uint8_t want = ITEM_LEAVE;
    if (wait) {
        wait--;
        return 0;
    }
    wait = 12;
    if (game.oil < game.oil_max / 2 && station_can(ITEM_FILL)) want = ITEM_FILL;
    else if (station_can(ITEM_TANK)) want = ITEM_TANK;
    else if (station_can(ITEM_WICK)) want = ITEM_WICK;
    else if (game.flares < 3 && station_can(ITEM_FLARES)) want = ITEM_FLARES;
    if (shop_sel < want) return IN_DOWN;
    if (shop_sel > want) return IN_UP;
    return IN_JUMP;
}

/* decide every 4th frame (the tile scans are not free), hold the keys in between */
uint16_t autopilot_input(void) {
    static uint8_t tick;
    static uint16_t keys;
    if (game.shop == 2) return shop_keys();
    if ((tick++ & 3) == 0) keys = think();
    else keys &= ~IN_FLARE;   // a flare is a press, not a hold
    if (jump_hold) {
        jump_hold--;
        keys |= IN_JUMP;
    } else {
        keys &= ~IN_JUMP;
    }
    return keys;
}

static uint16_t think(void) {
    game_entity_t *h = actors_hero();
    game_entity_t *e;
    body_t *b = &h->entity.physics.body;
    uint16_t keys = 0;
    int16_t cx = PHYSICS_FP_INT(b->x) + 5, cy = PHYSICS_FP_INT(b->y) + 7;
    uint8_t wisp_near = 0;

    if (cooldown) cooldown--;
    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        int16_t dx, dy;
        if (e->entity.physics.visual.type != E_WISP) continue;
        dx = PHYSICS_FP_INT(e->entity.physics.body.x) - cx;
        dy = PHYSICS_FP_INT(e->entity.physics.body.y) - cy;
        if (dx > -90 && dx < 90 && dy > -90 && dy < 90) wisp_near = 1;
    }
    if (wisp_near) {
        if (game.flares && !cooldown) {
            keys |= IN_FLARE;
            cooldown = 22;
        } else if (!game.flares) {
            keys |= IN_DIM;
        }
    }

    // no new depth for a while: panic, run and jump the other way for a bit
    if (game.depth != best_depth) {
        best_depth = game.depth;
        no_progress = 0;
    } else if (++no_progress > 45) {
        no_progress = 0;
        dir = -dir;
        jump_hold = 18;
        wander = 12 + (rand8() & 15);
    }
    if (wander) {
        if ((wander & 7) == 0) jump_hold = 18;
        wander--;
        return keys | (dir > 0 ? IN_RIGHT : IN_LEFT);
    }

    // ladders only lead down here: climb them
    if ((b->state & BODY_PASS_ONEWAY) ? !(b->state & BODY_ON_GROUND)
                                   : ((b->state & BODY_ON_GROUND) && (shaft_flags(cx, cy + 9) & TF_LADDER))) {
        return keys | IN_DOWN;
    }
    if (b->state & BODY_ON_GROUND) {
        int8_t target = find_on_row(cx, cy, TF_GEM | TF_OIL | TF_FLARE, 3);
        if (!target) target = find_hole(cx, cy);
        if (target > 0) dir = DIRECTION_RIGHT;
        else if (target < 0) dir = DIRECTION_LEFT;
        // rock in the way: jump it
        if (shaft_flags(cx + dir * 12, cy) & TF_SOLID) jump_hold = 16;
    }
    if (PHYSICS_FP_INT(b->x) == last_x && (b->state & BODY_ON_GROUND)) {
        if (++stuck > 15) {
            stuck = 0;
            dir = -dir;
            jump_hold = 18;
            wander = 22;
        }
    } else {
        stuck = 0;
    }
    last_x = PHYSICS_FP_INT(b->x);

    keys |= dir > 0 ? IN_RIGHT : IN_LEFT;
    return keys;
}
