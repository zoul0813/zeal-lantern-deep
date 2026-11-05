#include <stdint.h>
#include <zos_errors.h>
#include <zvb_gfx.h>
#include <zvb_hardware.h>
/* Not <zgdk.h>: zgdk/rect.h, ascii.h and sound/music.h define static functions in the header, so every
 * file that includes them gets its own copy (rect_collide alone is 347 bytes). See README, ZGDK issues. */
#include <zgdk/types.h>
#include <zgdk/input.h>
#include <zgdk/sprites.h>
#include <zgdk/tilemap.h>
#include <zgdk/hud/tilemap.h>
#include <zgdk/hiscore.h>
#include <zgdk/utils/rand.h>
#include <zgdk/sound/sounds.h>
#include <zgdk/tilemap/scroll.h>
#include <core.h>
#include <zgdk/physics/body.h>
#include <zgdk/tilemap/flags.h>
#include <zgdk/sound/sfx.h>
#include <zgdk/input.h>
#include <zgdk/camera.h>
#include "stream.h"
#include "gfx.h"
#include "entities.h"

#ifndef GAME_H
#define GAME_H

#define SCREEN_W       320
#define SCREEN_H       240

#define WIDTH          20
#define HEIGHT         15

#define SHAFT_LAYER    0   // rock, ladders, pickups (collision), scrolls with the camera
#define FOG_LAYER      1   // darkness around the lantern + HUD on row 0, never scrolls
#define HUD_ROW        0

/* World buffer: 20 x WORLD_ROWS tiles, rebased (stream_shift) as the camera goes down */
#define WORLD_ROWS     24
#define CHUNK_W        18
#define CHUNK_H        8
#define CHUNK_BYTES    (CHUNK_W * CHUNK_H)       // native ZTM byte cells, assets/cells.tsx

#define POOL_SIZE      16
#define ARENA_SIZE     POOL_SIZE

/* entity types */
#define E_HERO    0
#define E_BEETLE  1
#define E_WISP    2
#define E_FLARE   3
#define E_FX      4   // sparkle / dust, frees itself when its animation ends

/* tile flags beyond the core's */
#define TF_GEM    TF_USER1
#define TF_OIL    TF_USER2
#define TF_FLARE  TF_USER3

/* input */
#define IN_LEFT   BUTTON_LEFT
#define IN_RIGHT  BUTTON_RIGHT
#define IN_UP     BUTTON_UP
#define IN_DOWN   BUTTON_DOWN
#define IN_JUMP   BUTTON_B                // Z / Space, SNES B
#define IN_FLARE  BUTTON_A                // X, SNES A
#define IN_DIM    (BUTTON_Y | BUTTON_X)   // A or S (hold), SNES Y or X
#define IN_START  BUTTON_START            // Enter, SNES Start
#define IN_QUIT   BUTTON_SELECT           // Right Shift, SNES Select

#ifndef HISCORE_PATH
#ifdef EMULATOR
#define HISCORE_PATH "H:/ldeep.zhs"   // HostFS: the game folder
#else
#define HISCORE_PATH "B:/ldeep.zhs"
#endif
#endif

/* lantern oil, in frames of burn at full light */
#define OIL_MAX        (60U * 60U)
#define OIL_CAN        (22U * 60U)
#define OIL_HIT        (8U * 60U)
#define OIL_STOLEN     (10U * 60U)
#define FLARES_START   3
#define FLARES_MAX     9
#define OIL_TANK       (20U * 60U)   // each BIG TANK bought at a lift station
#define TANKS_MAX      2

/* lift station shop (station.c): the items, in menu order */
#define ITEM_FILL      0
#define ITEM_FLARES    1
#define ITEM_TANK      2
#define ITEM_WICK      3
#define ITEM_LEAVE     4
#define ITEMS          5

typedef struct {
    uint16_t oil;
    uint16_t oil_max;    // OIL_MAX + OIL_TANK per tank bought
    uint16_t depth;      // deepest row reached, 1 row = 1 m
    uint16_t gems;       // collected this run (score)
    uint16_t purse;      // gems not spent at lift stations (the HUD count)
    uint8_t tanks;       // BIG TANKs bought
    uint8_t wick;        // SLOW WICK bought
    uint8_t station;     // lift stations reached
    uint8_t shop;        // 1: landed on a station floor, main.c opens the shop; 2: the shop is open
    uint8_t banner;      // frames left of a HUD banner (hud_banner())
    uint8_t flares;
    uint8_t dim;         // lantern dimmed this frame
    uint8_t dead;
    uint8_t hud_dirty;
    uint16_t base;       // world rows dropped by rebasing (world row of buffer row 0)
} game_t;

extern gfx_context vctx;
extern game_t game;
extern uint16_t input_now;
extern uint16_t input_pressed;
extern camera_t cam;

/* map_data.c (generated from TMX templates in assets/chunks/) */
extern const uint8_t CHUNK_TIERS[];
extern const uint8_t CHUNK_COUNT;

/* shaft.c */
zos_err_t shaft_init(void);
void shaft_start(void);
uint8_t shaft_update(int16_t hero_px_y);
void shaft_vblank(void);
uint8_t shaft_tier(void);
uint8_t shaft_flags(int16_t px, int16_t py);   // tile flags at a buffer pixel, fast path; outside is rock
uint8_t shaft_station(uint16_t feet_row);      // 1 once, when the miner stands on a new station floor
int16_t shaft_lamp_y(void);                    // buffer px of the station lamp, -1 when none is near
void shaft_lamp_off(void);
#define STATION_LAMP_X 9                       // its tile column (chunk column 8 in assets/chunks/station.tmx)

/*
 * Raster line (ZVB control register): 0-479 drawing, 480-524 vblank. Game logic starts at line 0;
 * work that can wait (the fog) only starts while the line is below FRAME_DEADLINE, so it never
 * pushes a frame past its vblank.
 */
#define FRAME_DEADLINE 380   // leaves ~100 lines: one fog step, the HUD and the sprite sync
INLINE uint16_t raster_line(void) {
    uint8_t lo = zvb_ctrl_vpos_low;   // reading the low byte latches the high byte
    return lo | ((uint16_t)zvb_ctrl_vpos_high << 8);
}
void shaft_take(uint8_t tx, uint8_t ty);     // pickup collected: put the back wall there

/* fog.c */
zos_err_t fog_init(void);
void fog_reset(void);
void fog_update(void);
void fog_fill(uint8_t tile);

/* actors.c */
void actors_init(void);
void actors_start(void);
void actors_spawn(char marker, uint8_t tx, uint8_t ty);
void actors_update(void);
game_entity_t *actors_hero(void);
uint8_t actors_lights(int8_t *xs, int8_t *ys, uint8_t *radius);

/* hud.c */
void hud_draw(void);
/* Character glyphs mapped to icon tiles by hud_init(). */
#define HUD_LAMP_ICON '@'
#define HUD_OIL_EMPTY '#'
#define HUD_OIL_QUARTER '$'
#define HUD_OIL_HALF '%'
#define HUD_OIL_THREE_QUARTERS '&'
#define HUD_OIL_FULL '\''
#define HUD_GEM_ICON '*'
#define HUD_FLARE_ICON '+'

void hud_init(void);
void text_print(const char *str, uint8_t x, uint8_t y);
void text_center(const char *str, uint8_t y);
void format_num(uint16_t value, char *out, uint8_t digits);
void hud_banner(const char *text, uint8_t frames);   // replaces the HUD row for a while
uint16_t game_score(void);

/* sfx.c (on ZGDK sound/sfx: sfx_tick() comes from there) */
void game_sfx_init(void);
void game_sfx_jump(void);
void game_sfx_throw(void);
void game_sfx_gem(void);
void game_sfx_oil(void);
void game_sfx_flare_pick(void);
void game_sfx_hurt(void);
void game_sfx_steal(void);
void game_sfx_stomp(void);
void game_sfx_low(void);
void game_sfx_die(void);
void game_sfx_station(void);
void game_sfx_buy(void);
void game_sfx_nope(void);
void game_sfx_best(void);

/* station.c: the lift station shop (the loop is in main.c) */
extern uint8_t shop_sel;
uint8_t station_can(uint8_t item);    // 1 when it is for sale and the purse covers it
uint8_t station_buy(uint8_t item);    // 1 when bought
void station_draw(void);

/* music.c */
#define BGM_TITLE    0
#define BGM_DEEP     1
#define BGM_GAMEOVER 2
#define BGM_NONE     0xFF
zos_err_t bgm_init(void);
void bgm_play(uint8_t song);
void bgm_stop(void);
void bgm_pause(void);
void bgm_resume(void);
void bgm_tempo(uint8_t tempo);
void bgm_tick(void);

/* autopilot.c (AUTOPILOT builds, for recording videos) */
uint16_t autopilot_input(void);

#endif
