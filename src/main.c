/*
 * Lantern Deep: a vertical-scrolling platformer for Zeal 8-bit OS.
 * A miner descends an endless shaft. The lantern burns oil every second and is the only light;
 * when the oil runs out, the run is over. Flares light an area for good and lure the wisps away.
 */
#include <zos_vfs.h>
#include <stdint.h>
#include <zos_errors.h>
#include <zos_sys.h>
#include <zos_video.h>
#include "game.h"
#include "controls.h"
#include "entities.h"
#include "assets.h"
#include <zgdk/sprites.h>

#define DEAD_FADE_FRAMES   90
#define GAME_OVER_FRAMES   300
#define TEMPO_DEEP         28   // frames per quarter note, must match tools/make_music.py
#define TEMPO_LOW          20   // when the oil is low

gfx_context vctx;
game_t game;
entity_pool_t game_entities;
uint16_t input_now;
uint16_t input_pressed;
static uint16_t input_last;
#ifdef AUTOPILOT
static uint8_t autopilot_on;
#endif
#ifdef DEBUG_FPS
uint16_t fps_frames;   // game loop counter, read from memory by tools/hfps.py
#endif

static const highscore_t DEFAULT_SCORES[HISCORES_COUNT] = {
    { { 'Z', 'E', 'L' }, 1500 }, { { 'D', 'E', 'P' }, 1200 }, { { 'O', 'I', 'L' }, 1000 },
    { { 'G', 'E', 'M' }, 800 },  { { 'B', 'U', 'G' }, 600 },  { { 'Z', 'G', 'D' }, 500 },
    { { 'Z', 'V', 'B' }, 400 },  { { 'Z', 'O', 'S' }, 300 },  { { 'Z', 'D', 'E' }, 200 },
    { { 'M', 'O', 'L' }, 100 },
};

static const hiscore_config_t HISCORE_CONFIG = {
    .context = &vctx,
    .width = WIDTH,
    .height = HEIGHT,
    .layer = FOG_LAYER,
    .empty_tile = FOG_DARK,
    .default_scores = DEFAULT_SCORES,
    .path = HISCORE_PATH,
};

static void fatal(zos_err_t err, const char *what) {
    // printf would pull in ~2.6 KB of formatting code
    uint16_t len = 0;
    char code[4] = { ' ', '0' + (err / 10) % 10, '0' + err % 10, '\n' };
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
    while (what[len]) len++;
    write(DEV_STDOUT, what, &len);
    len = sizeof(code);
    write(DEV_STDOUT, code, &len);
    exit(err);
}

static void read_input(void) {
    input_now = input_read();
#ifdef AUTOPILOT
    if (autopilot_on) input_now = (input_now & IN_START) | autopilot_input();
#endif
    input_pressed = input_now & ~input_last;
    input_last = input_now;
}

static uint8_t scrolling;

static void frame_end(void) {
    // the HUD is on the fog layer, which never scrolls: it can be written outside vblank
    if (game.hud_dirty) hud_draw();
    physics_entities_sync(&game_entities);
    gfx_wait_vblank(&vctx);
    sprites_render(&vctx);
    if (scrolling) shaft_vblank();
    gfx_wait_end_vblank(&vctx);
    bgm_tick();
    sfx_tick();
}

static void wait_frames(uint16_t frames) {
    while (frames--) {
        read_input();
        frame_end();
    }
}

static void init(void) {
    zos_err_t err;

    err = input_init(1);
    if (err) fatal(err, "input");

    gfx_enable_screen(0);
    err = gfx_initialize(ZVB_CTRL_VID_MODE_GFX_320_8BIT, &vctx);
    if (err) fatal(err, "gfx");
    err = sprites_clear(&vctx);   // a previous program may have left sprites in the table
    if (err) fatal(err, "sprites");
    err = zar_file_open(PACK_FILE, &pack);   // lanterndeep.zar, next to the .bin
    if (err) fatal(err, PACK_FILE);
    err = load_palette(&vctx);
    if (err) fatal(err, "palette");
    gfx_tileset_options options = { .compression = TILESET_COMP_LZ };
    err = load_tileset(&vctx, &options);
    if (err) fatal(err, "tileset");
    hud_init();
    tilemap_fill(&vctx, SHAFT_LAYER, TILE_EMPTY, 0, 0, 80, 40);
    tilemap_fill(&vctx, FOG_LAYER, FOG_DARK, 0, 0, 80, 40);
    tilemap_scroll(SHAFT_LAYER, 0, 0);
    tilemap_scroll(FOG_LAYER, 0, 0);

    actors_init();
    sound_init_hardware();       // resets every voice, so it goes before the tracker and the sfx voice
    err = bgm_init();   // after load_tileset(), it reuses the tileset's load buffer
    if (err) fatal(err, "music");
    err = shaft_init();
    if (err) fatal(err, "shaft");
    if (fog_init()) fatal(ERR_NO_MORE_MEMORY, "ram");
    game_sfx_init();
    hiscore_init(&HISCORE_CONFIG);
    rand8_seed(0x1A57);

    gfx_enable_screen(1);
}

static void deinit(void) {
    bgm_stop();
    sound_deinit();
    zar_file_close(&pack);
    (void)sprites_clear(&vctx);   // best-effort cleanup before returning to ZOS
    ioctl(DEV_STDOUT, CMD_RESET_SCREEN, NULL);
}

/* Title backdrop: a shaft mouth on layer 0 seen through a lit circle in the fog */
static void title_backdrop(void) {
    uint8_t y;
    scrolling = 0;
    tilemap_scroll(SHAFT_LAYER, 0, 0);
    tilemap_fill(&vctx, SHAFT_LAYER, T0_BACK2, 0, 0, WIDTH, 16);
    for (y = 0; y < 16; y++) {
        gfx_tilemap_place(&vctx, T0_ROCK0 + (y & 3), SHAFT_LAYER, 0, y);
        gfx_tilemap_place(&vctx, T0_ROCK0 + ((y + 1) & 3), SHAFT_LAYER, 1, y);
        gfx_tilemap_place(&vctx, T0_ROCK0 + ((y + 2) & 3), SHAFT_LAYER, WIDTH - 2, y);
        gfx_tilemap_place(&vctx, T0_ROCK0 + ((y + 3) & 3), SHAFT_LAYER, WIDTH - 1, y);
        if (y & 1) gfx_tilemap_place(&vctx, T0_BEAM, SHAFT_LAYER, 4, y);
        if (y & 1) gfx_tilemap_place(&vctx, T0_BEAM, SHAFT_LAYER, WIDTH - 5, y);
    }
    tilemap_fill(&vctx, SHAFT_LAYER, T0_ROCK0 + OFS_PLANK, 5, 11, 10, 1);
    gfx_tilemap_place(&vctx, T0_ROCK0 + OFS_GEM, SHAFT_LAYER, 6, 9);
    gfx_tilemap_place(&vctx, T0_ROCK0 + OFS_OIL, SHAFT_LAYER, 13, 10);
    tilemap_fill(&vctx, FOG_LAYER, FOG_DARK, 0, 0, WIDTH, HEIGHT);
}

static void title_page(void) {
    char hi[] = "BEST 00000";
    uint16_t top = hiscore_get(0)->score;
    format_num(top ? top : DEFAULT_SCORES[0].score, &hi[5], 5);
    tilemap_fill(&vctx, FOG_LAYER, FOG_DARK, 0, 0, WIDTH, HEIGHT);
    text_center("LANTERN DEEP", 1);
    text_center("THE OIL IS YOUR TIME", 3);
    text_center(hi, 5);
    tilemap_fill(&vctx, FOG_LAYER, FOG_50, 5, 7, 10, 1);
    tilemap_fill(&vctx, FOG_LAYER, FOG_25, 5, 8, 10, 3);
    tilemap_fill(&vctx, FOG_LAYER, TILE_EMPTY, 6, 8, 8, 3);
    tilemap_fill(&vctx, FOG_LAYER, FOG_50, 5, 11, 10, 1);
    text_center(CONTROL_PROMPT("Z JUMP X FLARE A DIM", PAD_B " JUMP " PAD_A " FLARE " PAD_Y " DIM"), 13);
    text_center(PROMPT_START, 14);
}

static const uint8_t F_IDLE[] = { MINER_STAND, MINER_WALK2 };
static const anim_def A_IDLE = { F_IDLE, 2, 30, ANIM_LOOP };

/* Title screen, alternating with the hiscore table. Returns 0 to quit. */
static uint8_t title(void) {
    uint16_t t = 0;
    uint8_t page = 0xFF;
    uint16_t seed = 0;
    game_entity_t *miner;

    entity_pool_clear(&game_entities);
    entity_view_set(0, 0);
    title_backdrop();
    game.hud_dirty = 0;
    input_flush();
    controls_menu_begin();
    bgm_play(BGM_TITLE);
    miner = physics_entities_spawn(&game_entities, E_HERO, 9 * 16 + 3, 10 * 16 + 2, 10, 14);
    miner->entity.physics.visual.ox = -3;
    miner->entity.physics.visual.oy = -2;
    animated_physics_entity_play(&miner->entity, &A_IDLE);

    while (1) {
        uint8_t want = (t / 360) & 1;
        if (want != page) {
            page = want;
            if (page == 0) title_page();
            else {
                hiscore_show();
                text_center("DEEPEST MINERS", 1);
                text_center(PROMPT_START, 14);
            }
            miner->entity.physics.visual.hidden = page;
        }
        miner->entity.physics.visual.sprite->tile = anim_tick(&miner->entity.anim);
        read_input();
        if (controls_menu_update()) page = 0xFF;
        seed++;
        if (input_pressed & IN_START) break;
        if (input_pressed & IN_QUIT) return 0;
        frame_end();
        t++;
    }
    rand8_seed(seed | 1);
    entity_pool_clear(&game_entities);
    return 1;
}

static void pause(void) {
    text_center(" PAUSED ", 7);
    bgm_pause();
    input_flush();
    input_last = 0;
    do {
        read_input();
        frame_end();
    } while (!(input_pressed & IN_START));
    fog_reset();
    bgm_resume();
}

/*
 * Lift station (station.c): the miner landed on its floor. The shaft stops, the station lamp lights
 * up for a moment, then the shop opens under the floor. Up/Down pick, Z (or X, Enter) buys or leaves.
 */
static void station(void) {
    uint8_t i;
    game.shop = 2;   // open (the bot looks at this)
    game.station++;
    game_sfx_station();
    for (i = 0; i < 30; i++) {   // let the fog catch up with the lamp
        read_input();
        fog_update();
        frame_end();
    }
    shop_sel = 0;
    station_draw();
    input_flush();
    input_last = 0;
    while (1) {
        read_input();
        if (input_pressed & (IN_UP | IN_DOWN)) {
            if (input_pressed & IN_UP) shop_sel = shop_sel ? shop_sel - 1 : ITEMS - 1;
            else shop_sel = (shop_sel + 1) % ITEMS;
            station_draw();
        } else if (input_pressed & (IN_JUMP | IN_FLARE | IN_START)) {
            if (shop_sel == ITEM_LEAVE) break;
            if (station_buy(shop_sel)) {
                game_sfx_buy();
                station_draw();
            } else {
                game_sfx_nope();
            }
        }
        frame_end();
    }
    // input_last keeps the key that closed the shop, so it doesn't also jump or throw
    game.shop = 0;
    shaft_lamp_off();
    fog_reset();
}

static uint16_t best;    // score to beat this run: the top of the hiscore table
static uint8_t beaten;

static void game_over(void) {
    char line[] = "DEPTH 0000M";
    char gems[] = "GEMS 000";
    char score[] = "SCORE 00000";
    char top[] = "BEST 00000";
    format_num(game.depth, &line[6], 4);
    format_num(game.gems, &gems[5], 3);
    format_num(game_score(), &score[6], 5);
    format_num(best, &top[5], 5);
    entity_pool_clear(&game_entities);   // nothing shows in the dark
    fog_fill(FOG_DARK);
    text_center("THE LANTERN", 4);
    text_center("IS OUT", 5);
    text_center(line, 8);
    text_center(gems, 9);
    text_center(score, 11);
    text_center(beaten ? "NEW BEST!" : top, 13);
    bgm_play(BGM_GAMEOVER);
}

static void play(void) {
    uint16_t dead_timer = 0;
    uint8_t tempo = TEMPO_DEEP;

    mem_set(&game, 0, sizeof(game));
    game.oil = game.oil_max = OIL_MAX;
    game.flares = FLARES_START;
    best = hiscore_get(0)->score;
    if (!best) best = DEFAULT_SCORES[0].score;
    beaten = 0;
#ifdef AUTOPILOT
    autopilot_on = 1;
#endif
    tilemap_fill(&vctx, SHAFT_LAYER, TILE_EMPTY, 0, 0, 80, 40);
    fog_reset();
    actors_start();
    shaft_start();
    scrolling = 1;
    game.hud_dirty = 1;
    bgm_play(BGM_DEEP);
    bgm_tempo(TEMPO_DEEP);

    while (1) {
        game_entity_t *hero = actors_hero();
        read_input();
        if ((input_pressed & IN_START) && !game.dead) pause();

        actors_update();
        shaft_update(PHYSICS_FP_INT(hero->entity.physics.body.y));
        // the score only moves when the HUD does
        if (game.hud_dirty && !beaten && game_score() > best) {
            beaten = 1;
            hud_banner("NEW BEST!", 120);   // 2 s: it hides the oil bar
            game_sfx_best();
        }

        if (!game.dead) {
            uint8_t want = game.oil < OIL_MAX / 5 ? TEMPO_LOW : TEMPO_DEEP;
            if (want != tempo) {
                tempo = want;
                bgm_tempo(tempo);
            }
            // the fog uses whatever time the frame has left (fog.c), so it goes last
            fog_update();
        } else {
            dead_timer++;
            if (dead_timer < DEAD_FADE_FRAMES) fog_update();
            else if (dead_timer == DEAD_FADE_FRAMES) {
                bgm_stop();
                game_over();
            } else if (dead_timer > GAME_OVER_FRAMES || (dead_timer > DEAD_FADE_FRAMES + 60 && (input_pressed & IN_START))) {
                break;
            }
        }
#ifdef DEBUG_FPS
        fps_frames++;
#endif
        frame_end();
        if (game.shop) station();
    }

    scrolling = 0;
    entity_pool_clear(&game_entities);
    physics_entities_sync(&game_entities);
    bgm_stop();
#ifdef AUTOPILOT
    autopilot_on = 0;
#endif
    tilemap_scroll(SHAFT_LAYER, 0, 0);
    tilemap_fill(&vctx, SHAFT_LAYER, TILE_EMPTY, 0, 0, WIDTH, 16);
    frame_end();
    input_flush();
    hiscore_add(game_score());
}

int main(void) {
    init();
    while (title()) play();
    deinit();
    return 0;
}
