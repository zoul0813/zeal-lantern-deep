#include <zgdk/physics/platformer.h>
/*
 * The miner, beetles, wisps, flares and effects, all on the core's entity pool.
 * Coordinates are buffer pixels (the shaft rebases them as it scrolls, see shaft.c).
 *
 * Oil is both the timer and the health: the lantern burns it every frame (a quarter as fast when
 * dimmed), and every hit costs some. Wisps hunt light: they home in on the nearest flare, else on the
 * lantern unless it is dimmed. Beetles walk ledges and can be stomped.
 */
#include <core.h>
#include "game.h"

/* ---------------------------------------------------------------- tuning (12.4 fixed point) */
static const platformer_config_t HERO_PHYS = {
    .gravity = 3, .max_fall = 64, .jump_vel = -74, .jump_cut = -24,
    .accel = 4, .decel = 6, .max_run = 22, .coyote = 5, .buffer = 6, .air_jumps = 0,
};
static const platformer_config_t ITEM_PHYS = {
    .gravity = 3, .max_fall = 56, .jump_vel = 0, .jump_cut = 0,
    .accel = 0, .decel = 0, .max_run = 32, .coyote = 0, .buffer = 0, .air_jumps = 0,
};

#define HERO_W        10
#define HERO_H        14
#define CLIMB_SPEED   20
#define BEETLE_SPEED  6
#define WISP_SPEED    12      // max speed, + 4 per depth tier
#define WISP_SENSE    112     // px a wisp can see the lantern from
#define FLARE_SENSE   128
#define INVULN        90
#define STOMP_BOUNCE  -52
#define HURT_BOUNCE   -44
#define MAX_FLARES    4       // lit flares at once (the oldest burns out)

/* ---------------------------------------------------------------- animations */
static const uint8_t F_STAND[] = { MINER_STAND };
static const uint8_t F_WALK[] = { MINER_WALK1, MINER_STAND, MINER_WALK2, MINER_STAND };
static const uint8_t F_JUMP[] = { MINER_JUMP };
static const uint8_t F_CLIMB[] = { MINER_CLIMB1, MINER_CLIMB2 };
static const uint8_t F_HURT[] = { MINER_HURT };
static const uint8_t F_DEAD[] = { MINER_DEAD };
static const uint8_t F_BEETLE[] = { BEETLE1, BEETLE2 };
static const uint8_t F_FLAT[] = { BEETLE_FLAT, BEETLE_FLAT, BEETLE_FLAT, DUST };
static const uint8_t F_WISP[] = { WISP1, WISP2 };
static const uint8_t F_FLARE_LIT[] = { FLARE1, FLARE2 };
static const uint8_t F_FLARE_FLY[] = { FLARE_FLY };
static const uint8_t F_SPARK[] = { SPARK1, SPARK2, SPARK1, SPARK2 };
static const uint8_t F_POOF[] = { SPARK2, DUST, DUST };

static const anim_def A_STAND = { F_STAND, 1, 8, ANIM_LOOP };
static const anim_def A_WALK = { F_WALK, 4, 6, ANIM_LOOP };
static const anim_def A_JUMP = { F_JUMP, 1, 8, ANIM_LOOP };
static const anim_def A_CLIMB = { F_CLIMB, 2, 8, ANIM_LOOP };
static const anim_def A_HURT = { F_HURT, 1, 8, ANIM_LOOP };
static const anim_def A_DEAD = { F_DEAD, 1, 8, ANIM_LOOP };
static const anim_def A_BEETLE = { F_BEETLE, 2, 10, ANIM_LOOP };
static const anim_def A_FLAT = { F_FLAT, 4, 12, 0 };
static const anim_def A_WISP = { F_WISP, 2, 6, ANIM_LOOP };
static const anim_def A_FLARE_LIT = { F_FLARE_LIT, 2, 5, ANIM_LOOP };
static const anim_def A_FLARE_FLY = { F_FLARE_FLY, 1, 8, ANIM_LOOP };
static const anim_def A_SPARK = { F_SPARK, 4, 5, 0 };
static const anim_def A_POOF = { F_POOF, 3, 6, 0 };

/* ---------------------------------------------------------------- state */
static gfx_sprite arena[ARENA_SIZE];
static game_entity_t pool[POOL_SIZE];
static game_entity_t *hero;
static int8_t facing = DIRECTION_RIGHT;
static uint8_t invuln;
static uint8_t hurt_timer;
static uint8_t frame;
static uint8_t warned;

game_entity_t *actors_hero(void) {
    return hero;
}

void actors_init(void) {
    sprites_register_arena(arena, ARENA_SIZE);
    sprite_entities_init(&game_entities, pool, POOL_SIZE, sizeof(*pool));
}

static game_entity_t *spawn_fx(const anim_def *anim, int16_t px, int16_t py) {
    game_entity_t *e = physics_entities_spawn(&game_entities, E_FX, px, py, 1, 1);
    if (e) animated_physics_entity_restart(&e->entity, anim);
    return e;
}

void actors_start(void) {
    entity_pool_clear(&game_entities);
    hero = physics_entities_spawn(&game_entities, E_HERO, 9 * 16 + 3, 3 * 16 + 2, HERO_W, HERO_H);
    hero->entity.physics.visual.ox = -3;
    hero->entity.physics.visual.oy = -2;
    animated_physics_entity_play(&hero->entity, &A_STAND);
    facing = DIRECTION_RIGHT;
    invuln = 0;
    hurt_timer = 0;
    warned = 0;
}

/* Called by the shaft generator for markers in a new row (buffer tile coordinates) */
void actors_spawn(char marker, uint8_t tx, uint8_t ty) {
    game_entity_t *e;
    int16_t px = tx << 4, py = ty << 4;
    if (marker == 'b') {
        e = physics_entities_spawn(&game_entities, E_BEETLE, px + 2, py + 8, 12, 8);
        if (!e) return;
        e->entity.physics.visual.ox = -2;
        e->entity.physics.visual.oy = -8;
        e->timer = rand8() & 1;   // walking left
        animated_physics_entity_play(&e->entity, &A_BEETLE);
    } else {
        e = physics_entities_spawn(&game_entities, E_WISP, px + 3, py + 3, 10, 10);
        if (!e) return;
        e->entity.physics.visual.ox = -3;
        e->entity.physics.visual.oy = -3;
        e->timer = rand8();
        animated_physics_entity_play(&e->entity, &A_WISP);
    }
    // enemies sit behind the fog layer: the dark hides them
    e->entity.physics.visual.sprite->flags |= SPRITE_BEHIND_FG;
}

/* ---------------------------------------------------------------- oil */
static void oil_take(uint16_t amount) {
    game.oil = (game.oil > amount) ? game.oil - amount : 0;
}

static void hurt(int16_t from_x, uint16_t cost) {
    body_t *b = &hero->entity.physics.body;
    if (invuln || game.dead) return;
    oil_take(cost);
    invuln = INVULN;
    hurt_timer = 20;
    game.hud_dirty = 1;
    b->vy = HURT_BOUNCE;
    b->vx = (PHYSICS_FP_INT(b->x) + (HERO_W >> 1) < from_x) ? -28 : 28;
    b->state &= ~BODY_PASS_ONEWAY;
    game_sfx_hurt();
}

static void burn(void) {
    uint8_t tier = shaft_tier();
    // dimmed: a quarter of the burn; deeper tiers burn a little faster; the slow wick skips one frame in four
    if (game.dim && (frame & 3)) return;
    if (game.wick && (frame & 3) == 2) return;
    oil_take(1 + (tier >= 2 && (frame & 1)));
    if (game.oil < OIL_MAX / 5 && !(warned & 1)) { warned |= 1; game_sfx_low(); }
    if (game.oil < OIL_MAX / 12 && !(warned & 2)) { warned |= 2; game_sfx_low(); }
    if (game.oil > OIL_MAX / 4) warned = 0;
}

/* ---------------------------------------------------------------- hero */
static void add_gem(void) {
    if (game.gems < 999) game.gems++;
    if (game.purse < 999) game.purse++;
}

static void collect(void) {
    body_t *b = &hero->entity.physics.body;
    int16_t cx = PHYSICS_FP_INT(b->x) + (HERO_W >> 1);
    int16_t cy = PHYSICS_FP_INT(b->y) + (HERO_H >> 1);
    uint8_t flags = shaft_flags(cx, cy);
    if (!(flags & (TF_GEM | TF_OIL | TF_FLARE))) return;
    if (flags & TF_GEM) {
        add_gem();
        game_sfx_gem();
    } else if (flags & TF_OIL) {
        game.oil = (game.oil + OIL_CAN > game.oil_max) ? game.oil_max : game.oil + OIL_CAN;
        game_sfx_oil();
    } else {
        if (game.flares < FLARES_MAX) game.flares++;
        game_sfx_flare_pick();
    }
    game.hud_dirty = 1;
    shaft_take((uint8_t)(cx >> 4), (uint8_t)(cy >> 4));
    spawn_fx(&A_SPARK, (cx & ~15) + 3, (cy & ~15) + 2);
}

static void throw_flare(void) {
    game_entity_t *e, *oldest = NULL;
    uint8_t lit = 0;
    body_t *b = &hero->entity.physics.body;
    if (!game.flares) return;
    ENTITY_POOL_FOR_EACH(&game_entities, e, entity.physics.visual.type) {
        if (e->entity.physics.visual.type != E_FLARE) continue;
        if (!oldest) oldest = e;
        lit++;
    }
    if (lit >= MAX_FLARES) entity_pool_free(&game_entities, oldest);
    e = physics_entities_spawn(&game_entities, E_FLARE, PHYSICS_FP_INT(b->x) + 3, PHYSICS_FP_INT(b->y) + 2, 4, 8);
    if (!e) return;
    e->entity.physics.visual.ox = -6;
    e->entity.physics.visual.oy = -8;
    e->entity.physics.body.vx = facing > 0 ? 30 : -30;
    e->entity.physics.body.vy = -36;
    e->entity.physics.visual.flip = facing > 0 ? 0 : SPRITE_FLIP_X;
    animated_physics_entity_play(&e->entity, &A_FLARE_FLY);
    game.flares--;
    game.hud_dirty = 1;
    game_sfx_throw();
}

static void hero_update(void) {
    body_t *b = &hero->entity.physics.body;
    int8_t dir = DIRECTION_NONE;
    uint16_t keys = input_now;
    uint16_t depth;

    if (game.dead) {
        b->vx = 0;
        platformer_move(b, &HERO_PHYS);
        hero->entity.physics.visual.hidden = 0;
        animated_physics_entity_play(&hero->entity, &A_DEAD);
        return;
    }
    if (hurt_timer) {
        hurt_timer--;
        keys = 0;   // knocked back: no control for a moment
    }
    if (keys & IN_LEFT) dir = DIRECTION_LEFT;
    if (keys & IN_RIGHT) dir = DIRECTION_RIGHT;
    if (dir) facing = dir;
    game.dim = (keys & IN_DIM) != 0;

    if (platformer_ladder(b, keys & IN_UP, keys & IN_DOWN, dir, CLIMB_SPEED) && dir == DIRECTION_NONE) {
        // center on the ladder so the miner clears the rock beside it
        int16_t cx = PHYSICS_FP_INT(b->x) + (HERO_W >> 1);
        b->x = PHYSICS_FP(((cx >> 4) << 4) + 8 - (HERO_W >> 1));
    }
    if (platformer_update(b, &HERO_PHYS, &hero->platformer, dir, keys & IN_JUMP, input_pressed & IN_JUMP & keys)) game_sfx_jump();
    platformer_move(b, &HERO_PHYS);

    if (b->state & BODY_TOUCHING_TRIGGER) hurt(PHYSICS_FP_INT(b->x) + (HERO_W >> 1) - (facing << 3), OIL_HIT);
    if ((input_pressed & IN_FLARE) && !hurt_timer) throw_flare();
    collect();

    depth = game.base + ((PHYSICS_FP_INT(b->y) + HERO_H) >> 4);
    if (depth > game.depth) {
        game.depth = depth;
        game.hud_dirty = 1;
    }
    // landed on a lift station floor: main.c opens the shop after this frame
    if ((b->state & BODY_ON_GROUND) && shaft_station(depth)) game.shop = 1;

    hero->entity.physics.visual.flip = (facing < 0) ? SPRITE_FLIP_X : 0;
    if (hurt_timer) animated_physics_entity_play(&hero->entity, &A_HURT);
    else if (b->state & BODY_PASS_ONEWAY) {
        animated_physics_entity_play(&hero->entity, &A_CLIMB);
        if (b->vy == 0 && b->vx == 0) hero->entity.anim.timer = 0;   // hold the frame when not moving
    } else if (!(b->state & BODY_ON_GROUND)) animated_physics_entity_play(&hero->entity, &A_JUMP);
    else if (b->vx) animated_physics_entity_play(&hero->entity, &A_WALK);
    else animated_physics_entity_play(&hero->entity, &A_STAND);

    if (invuln) {
        invuln--;
        hero->entity.physics.visual.hidden = (invuln & 4) != 0;
    } else {
        hero->entity.physics.visual.hidden = 0;
    }
}

/* ---------------------------------------------------------------- enemies and items */
/*
 * Beetles only use the full body physics while falling. On the ground they walk with two tile
 * lookups a frame (wall ahead, floor ahead), which is much cheaper than platformer_move().
 */
static void beetle_update(game_entity_t *e) {
    body_t *b = &e->entity.physics.body;
    int16_t x = PHYSICS_FP_INT(b->x), y = PHYSICS_FP_INT(b->y);
    int16_t speed = e->timer ? -BEETLE_SPEED : BEETLE_SPEED;
    int16_t front = speed > 0 ? x + b->w : x - 1;
    if (!(shaft_flags(x + (b->w >> 1), y + b->h) & (TF_SOLID | TF_ONEWAY))) {
        b->vx = 0;
        platformer_move(b, &ITEM_PHYS);
        return;
    }
    if ((shaft_flags(front, y + 4) & TF_SOLID) ||
        !(shaft_flags(front, y + b->h) & (TF_SOLID | TF_ONEWAY))) {
        e->timer ^= 1;   // turn at walls and ledges
    } else {
        b->x += speed;
    }
    e->entity.physics.visual.flip = e->timer ? SPRITE_FLIP_X : 0;
}

/* flares seen in the last actors_update() pass, so wisps and the fog don't walk the whole pool */
static game_entity_t *flares[MAX_FLARES + 1];
static uint8_t flare_count;

static game_entity_t *nearest_flare(int16_t x, int16_t y) {
    uint8_t i;
    for (i = 0; i < flare_count; i++) {
        game_entity_t *f = flares[i];
        int16_t dx, dy;
        if (f->entity.physics.visual.type != E_FLARE) continue;
        dx = PHYSICS_FP_INT(f->entity.physics.body.x) - x;
        dy = PHYSICS_FP_INT(f->entity.physics.body.y) - y;
        if (dx > -FLARE_SENSE && dx < FLARE_SENSE && dy > -FLARE_SENSE && dy < FLARE_SENSE) return f;
    }
    return NULL;
}

static int16_t approach(int16_t v, int16_t target, int16_t step) {
    if (v < target) return (v + step > target) ? target : v + step;
    if (v > target) return (v - step < target) ? target : v - step;
    return v;
}

static void wisp_update(game_entity_t *e) {
    body_t *b = &e->entity.physics.body;
    int16_t x = PHYSICS_FP_INT(b->x), y = PHYSICS_FP_INT(b->y);
    int16_t max = WISP_SPEED + (shaft_tier() << 2);
    int16_t tx = x, ty = y;
    uint8_t chase = 0;
    game_entity_t *f = nearest_flare(x, y);

    e->timer++;
    if (f) {
        // circle the flare: aim a little around it so it hovers instead of sitting on it
        tx = PHYSICS_FP_INT(f->entity.physics.body.x) + ((e->timer & 64) ? 10 : -14);
        ty = PHYSICS_FP_INT(f->entity.physics.body.y) - 14 + ((e->timer & 32) ? 4 : -4);
        chase = 1;
    } else if (!game.dim && !game.dead) {
        int16_t hx = PHYSICS_FP_INT(hero->entity.physics.body.x), hy = PHYSICS_FP_INT(hero->entity.physics.body.y);
        int16_t dx = hx - x, dy = hy - y;
        if (dx > -WISP_SENSE && dx < WISP_SENSE && dy > -WISP_SENSE && dy < WISP_SENSE) {
            tx = hx;
            ty = hy;
            chase = 1;
        }
    }
    if (chase) {
        b->vx = approach(b->vx, tx > x ? max : -max, 1);
        b->vy = approach(b->vy, ty > y ? max : -max, 1);
    } else {
        // lost the light: drift and slow down
        b->vx = approach(b->vx, (e->timer & 128) ? 4 : -4, 1);
        b->vy = approach(b->vy, (e->timer & 64) ? 2 : -2, 1);
    }
    // wisps pass through rock
    b->x += b->vx;
    b->y += b->vy;
    if (b->x < PHYSICS_FP(16)) { b->x = PHYSICS_FP(16); b->vx = 0; }
    if (b->x > PHYSICS_FP(WIDTH * 16 - 26)) { b->x = PHYSICS_FP(WIDTH * 16 - 26); b->vx = 0; }
    e->entity.physics.visual.flip = b->vx < 0 ? SPRITE_FLIP_X : 0;
}

static void flare_update(game_entity_t *e) {
    body_t *b = &e->entity.physics.body;
    if (e->timer) return;   // landed: stays lit
    platformer_move(b, &ITEM_PHYS);
    if (b->state & BODY_HIT_WALL) b->vx = -b->vx / 2;
    if (b->state & BODY_ON_GROUND) {
        b->vx = 0;
        e->timer = 1;
        e->entity.physics.visual.flip = 0;
        animated_physics_entity_play(&e->entity, &A_FLARE_LIT);
    }
}

static int16_t hero_px, hero_py;   // hero position in pixels, set once per actors_update()

static void touch_hero(game_entity_t *e) {
    body_t *h = &hero->entity.physics.body;
    int16_t ex;
    // enemies are at most 12 px wide: skip body_overlap() unless one is close
    uint16_t dx = (uint16_t)(PHYSICS_FP_INT(e->entity.physics.body.x) - hero_px + 16);
    uint16_t dy = (uint16_t)(PHYSICS_FP_INT(e->entity.physics.body.y) - hero_py + 16);
    if (dx >= 32 || dy >= 32) return;
    ex = PHYSICS_FP_INT(e->entity.physics.body.x) + (e->entity.physics.body.w >> 1);
    if (game.dead || !body_overlap(h, &e->entity.physics.body)) return;
    if (e->entity.physics.visual.type == E_BEETLE) {
        // falling onto its back squashes it
        if (h->vy > 0 && PHYSICS_FP_INT(h->y) + HERO_H <= PHYSICS_FP_INT(e->entity.physics.body.y) + 6) {
            h->vy = STOMP_BOUNCE;
            e->entity.physics.visual.type = E_FX;
            e->entity.physics.body.vx = 0;
            animated_physics_entity_restart(&e->entity, &A_FLAT);
            add_gem();
            game.hud_dirty = 1;
            game_sfx_stomp();
            return;
        }
        hurt(ex, OIL_HIT);
    } else if (e->entity.physics.visual.type == E_WISP && !invuln) {
        // it drinks the lantern and vanishes
        hurt(ex, OIL_STOLEN);
        game_sfx_steal();
        e->entity.physics.visual.type = E_FX;
        e->entity.physics.visual.sprite->flags &= ~SPRITE_BEHIND_FG;
        animated_physics_entity_restart(&e->entity, &A_POOF);
    }
}

void actors_update(void) {
    game_entity_t *e;
    int16_t top = cam.y - 48;
    int16_t bottom = WORLD_ROWS * 16;
    int16_t view_bottom = cam.y + SCREEN_H + 16;
    uint8_t count;

    frame++;
    // the oil bar moves in 3-second steps; redraw it twice a second
    if ((frame & 31) == 0) game.hud_dirty = 1;
    if (game.banner && !--game.banner) game.hud_dirty = 1;
    if (!game.dead) {
        burn();
        if (game.oil == 0) {
            game.dead = 1;
            game.hud_dirty = 1;
            game_sfx_die();
        }
    }
    hero_update();
    hero_px = PHYSICS_FP_INT(hero->entity.physics.body.x);
    hero_py = PHYSICS_FP_INT(hero->entity.physics.body.y);

    count = 0;
    // the pool is ours: walking it directly avoids dynamic-stride pool iteration
    for (e = pool; e < pool + POOL_SIZE; e++) {
        int16_t y;
        if (e->entity.physics.visual.type == ENTITY_FREE) continue;
        y = PHYSICS_FP_INT(e->entity.physics.body.y);
        if (e == hero) {
            e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
            continue;
        }
        // scrolled away above, or fell out of the buffer
        if (y < top || y > bottom) {
            entity_pool_free(&game_entities, e);
            continue;
        }
        // not on screen yet: wait below the view (spawned rows are generated ahead of the camera)
        if (y > view_bottom) continue;
        switch (e->entity.physics.visual.type) {
            case E_BEETLE: beetle_update(e); touch_hero(e); break;
            case E_WISP: wisp_update(e); touch_hero(e); break;
            case E_FLARE:
                flare_update(e);
                if (count <= MAX_FLARES) flares[count++] = e;
                break;
            case E_FX:
                if (e->entity.anim.done) {
                    e->entity.physics.visual.sprite->flags &= ~SPRITE_BEHIND_FG;
                    entity_pool_free(&game_entities, e);
                    continue;
                }
                break;
        }
        e->entity.physics.visual.sprite->tile = anim_tick(&e->entity.anim);
    }
    flare_count = count;
}

/* Lights for the fog, in screen tiles: the lantern first, then flares */
uint8_t actors_lights(int8_t *xs, int8_t *ys, uint8_t *radius) {
    game_entity_t *e;
    uint8_t i, n = 1;
    // the lantern is always entry 0; radius 0xFF when it is out
    radius[0] = 0xFF;
    xs[0] = ys[0] = 0;
    if (!game.dead) {
        int16_t x = PHYSICS_FP_INT(hero->entity.physics.body.x) + (HERO_W >> 1);
        int16_t y = PHYSICS_FP_INT(hero->entity.physics.body.y) + (HERO_H >> 1) - cam.y;
        uint8_t r;
        if (game.dim) r = 0;
        else if (game.oil >= OIL_MAX / 2) r = 3;
        else if (game.oil >= OIL_MAX / 5) r = 2;
        else if (game.oil >= OIL_MAX / 12) r = 1;
        else r = (rand8() < 40) ? 0 : 1;   // sputtering
        xs[0] = (int8_t)(x >> 4);
        ys[0] = (int8_t)(y >> 4);
        radius[0] = r;
    }
    {
        // a lift station's lamp, cached with the flares by the fog (it only moves when the camera does)
        int16_t y = shaft_lamp_y();
        if (y >= 0 && (y -= cam.y) >= 0 && y < SCREEN_H) {
            xs[n] = STATION_LAMP_X;
            ys[n] = (int8_t)(y >> 4);
            radius[n] = 3;
            n++;
        }
    }
    for (i = 0; i < flare_count && n < 6; i++) {
        int16_t x, y;
        e = flares[i];
        if (e->entity.physics.visual.type != E_FLARE) continue;
        x = PHYSICS_FP_INT(e->entity.physics.body.x) + 2;
        y = PHYSICS_FP_INT(e->entity.physics.body.y) + 4 - cam.y;
        if (y < -64 || y > 300) continue;
        xs[n] = (int8_t)(x >> 4);
        ys[n] = (int8_t)(y >> 4);
        radius[n] = e->timer ? 2 : 0;
        n++;
    }
    return n;
}
