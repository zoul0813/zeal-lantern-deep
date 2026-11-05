#include <zgdk/tilemap/flags.h>
/*
 * The endless shaft: a 20 x WORLD_ROWS tile buffer on layer 0, streamed through the core
 * (stream.h). The camera only ever goes down. Every time it passes REBASE_Y, stream_shift()
 * drops the top row (buffer, camera, &game_entities, entities and the hardware scroll origin all move together, so
 * nothing jumps) and a new row is generated at the bottom from the current chunk.
 */
#include <core.h>
#include "game.h"
#include <zgdk/utils/grid_rows.h>
#include "assets.h"

TILEFLAGS_DEFINE_CACHE(24);

#define REBASE_Y      48     // camera y (px) that triggers dropping the top row
#define HERO_SCREEN_Y 88     // the camera keeps the miner this far from the top of the screen
#define OIL_DROUGHT   24     // rows without oil before a 'g' slot becomes an oil can
#define STATION_EVERY 100    // m between lift stations
#define STATION_FLOOR 4      // chunk row of the station floor, the lamp is 2 rows above (assets/chunks/station.tmx)

camera_t cam;
static tilemap_stream_t stream;
static Tilemap tilemap;
static uint8_t *world;   // WIDTH * WORLD_ROWS, in the dead tileset blob (shaft_init)

/* Tile IDs in assets/cells.tsx are procedural cell classes. */
enum { CELL_BACKGROUND, CELL_ROCK, CELL_RANDOM_ROCK, CELL_PLANK, CELL_LADDER,
       CELL_SPIKES, CELL_GEM, CELL_LOOT, CELL_OIL, CELL_FLARE, CELL_BEETLE,
       CELL_WISP, CELL_BEAM, CELL_CRYSTAL, CELL_STATION_WALL, CELL_STATION_LAMP };
static const uint8_t *chunk;
static uint8_t chunk_buf[CHUNK_BYTES];   // the picked chunk, read from chunks.ztm in lanterndeep.zar
static zar_entry_t chunk_file;
static uint8_t chunk_row;
static uint8_t last_chunk = 0xFF;
static uint8_t since_oil;
static uint16_t gen_row;         // world row gen_next() makes next
static uint16_t next_station;    // depth of the next lift station
static uint16_t station_floor;   // world row of a station floor not stood on yet, 0 for none
static uint16_t lamp_row;        // world row of the last station's lamp, 0 for none

/* tile flags, filled in shaft_init(): the same layout repeats for every depth tier */
static uint8_t FLAGS[256];
static const uint8_t TIER_FLAGS[TIER_SIZE] = {
    [OFS_ROCK0] = TF_SOLID, [OFS_ROCK1] = TF_SOLID, [OFS_ROCK2] = TF_SOLID, [OFS_ROCK3] = TF_SOLID,
    [OFS_ROCK_TOP] = TF_SOLID,
    [OFS_PLANK] = TF_ONEWAY,
    [OFS_LADDER] = TF_LADDER,
    [OFS_LADDER_TOP] = TF_LADDER | TF_ONEWAY,
    [OFS_SPIKES] = TF_TRIGGER,
    [OFS_GEM] = TF_GEM,
    [OFS_OIL] = TF_OIL,
    [OFS_FLARE_ITEM] = TF_FLARE,
};

#define is_rock(t)  (FLAGS[t] & TF_SOLID)

/* depth tier from world depth: 0-99 m brown, 100-199 slate, 200+ crimson, then cycles with more enemies */
uint8_t shaft_tier(void) {
    uint16_t row = game.base + WORLD_ROWS;
    if (row < 100) return 0;
    if (row < 200) return 1;
    return 2;
}

static uint8_t tier_tiles(void) {
    uint16_t row = game.base + WORLD_ROWS;
    return TIER_BASE + (uint8_t)((row / 100) % TIER_COUNT) * TIER_SIZE;
}

static void pick_chunk(void) {
    uint8_t tier = shaft_tier();
    uint8_t i, pick;
    uint16_t size;
    if (gen_row + CHUNK_H > next_station) {
        // the lift station: the first chunk whose floor is past STATION_EVERY - 4 m (chunks start every 8 rows)
        next_station += STATION_EVERY;
        station_floor = gen_row + STATION_FLOOR;
        lamp_row = station_floor - 2;
        pick = CHUNK_COUNT;   // stored after the others in chunks.ztm
    } else {
        // a few tries for a chunk of this tier or lower, not the same as last time
        for (i = 0; i < 8; i++) {
            uint8_t n = rand8() % CHUNK_COUNT;
            if (CHUNK_TIERS[n] > tier || n == last_chunk) continue;
            // deeper tiers mostly use their own chunks
            if (CHUNK_TIERS[n] < tier && (rand8() & 1)) continue;
            last_chunk = n;
            break;
        }
        if (last_chunk == 0xFF) last_chunk = 0;
        pick = last_chunk;
    }
    size = CHUNK_BYTES;
    // 144 bytes from the archive, about once every 8 rows; a failed read produces empty cells
    if (zar_entry_read_at(&pack, &chunk_file, (uint16_t)pick * CHUNK_BYTES, chunk_buf, &size) == ERR_SUCCESS && size == CHUNK_BYTES)
        chunk = chunk_buf;
    else {
        mem_set(chunk_buf, CELL_BACKGROUND, CHUNK_BYTES);
        chunk = chunk_buf;
    }
    chunk_row = 0;
}

/* Generate buffer row `r` from the next chunk row */
/*
 * Rows are generated one frame ahead into next_row, so the frame that scrolls (rebase memmove,
 * new VRAM row) doesn't also pay for generating. Spawns wait for the commit.
 */
static uint8_t next_row[WIDTH];
static uint8_t next_ready;
static uint8_t spawn_x[4];
static char spawn_m[4];
static uint8_t spawn_n;

static void gen_next(const uint8_t *above) {
    uint8_t *dst = next_row;
    uint8_t base = tier_tiles();
    const uint8_t *src;
    uint8_t x;

    if (chunk == NULL || chunk_row >= CHUNK_H) pick_chunk();
    src = chunk + chunk_row * CHUNK_W;
    chunk_row++;
    gen_row++;
    since_oil++;

    dst[0] = base + (rand8() & 3);
    dst[WIDTH - 1] = base + (rand8() & 3);
    for (x = 1; x < WIDTH - 1; x++) {
        uint8_t c = src[x - 1];
        uint8_t t;
        switch (c) {
            case CELL_ROCK: t = base + (rand8() & 3); break;
            case CELL_RANDOM_ROCK: t = (rand8() & 1) ? base + (rand8() & 3) : base + OFS_BACK2; break;
            case CELL_PLANK: t = base + OFS_PLANK; break;
            case CELL_LADDER: t = base + ((above && (FLAGS[above[x]] & TF_LADDER)) ? OFS_LADDER : OFS_LADDER_TOP); break;
            case CELL_SPIKES: t = base + OFS_SPIKES; break;
            case CELL_GEM: t = base + OFS_GEM; break;
            case CELL_OIL: t = base + OFS_OIL; since_oil = 0; break;
            case CELL_LOOT:
                if (since_oil > OIL_DROUGHT) { t = base + OFS_OIL; since_oil = 0; }
                else t = base + ((rand8() < 100) ? OFS_GEM : OFS_BACK2);
                break;
            case CELL_FLARE: t = base + OFS_FLARE_ITEM; break;
            case CELL_BEAM: t = base + OFS_BEAM; break;
            case CELL_CRYSTAL: t = base + OFS_CRYSTAL; break;
            case CELL_STATION_WALL: t = STA_WALL; break;
            case CELL_STATION_LAMP: t = STA_LAMP; break;
            case CELL_BEETLE:
            case CELL_WISP:
                if (spawn_n < sizeof(spawn_x)) {
                    spawn_x[spawn_n] = x;
                    spawn_m[spawn_n++] = c == CELL_BEETLE ? 'b' : 'w';
                }
                t = base + OFS_BACK2;
                break;
            default: {
                uint8_t n = rand8();
                t = base + (n < 40 ? OFS_BACK0 : n < 70 ? OFS_BACK1 : OFS_BACK2);
            }
        }
        dst[x] = t;
    }
    // rock with open space above gets the lit top edge
    next_ready = 1;
    for (x = 0; x < WIDTH; x++) {
        if (is_rock(dst[x]) && (above == NULL || !is_rock(above[x]))) {
            dst[x] = base + OFS_ROCK_TOP;
        }
    }
}

static void commit(uint8_t r) {
    uint8_t i;
    mem_cpy(world + (uint16_t)r * WIDTH, next_row, WIDTH);
    for (i = 0; i < spawn_n; i++) actors_spawn(spawn_m[i], spawn_x[i], r);
    spawn_n = 0;
    next_ready = 0;
}

#define LAST_ROW (world + (WORLD_ROWS - 1) * WIDTH)

/*
 * Tile flags at a buffer pixel. Same answer as the core's tileflags_at(), without its per-call
 * cache validation: the shaft buffer never moves (stream_shift keeps it), so the hot callers
 * (beetles, pickups, the bot) use this. (The core's tileflags_fast() has no bounds check.)
 */
static const uint8_t *shaft_rows[WORLD_ROWS];

uint8_t shaft_flags(int16_t px, int16_t py) {
    if ((uint16_t)px >= WIDTH * 16 || (uint16_t)py >= WORLD_ROWS * 16) return TF_SOLID;
    return FLAGS[shaft_rows[(uint8_t)((uint16_t)py >> 4)][(uint8_t)((uint16_t)px >> 4)]];
}

zos_err_t shaft_init(void) {
    uint8_t t;
    for (t = 0; t < TIER_COUNT; t++) mem_cpy(&FLAGS[TIER_BASE + t * TIER_SIZE], (void *)TIER_FLAGS, TIER_SIZE);
    world = blob_take(WIDTH * WORLD_ROWS);
    if (!world) return ERR_NO_MORE_MEMORY;
    if (zar_entry_from_name(&pack, "chunks.ztm", &chunk_file) != ERR_SUCCESS) return ERR_NO_SUCH_ENTRY;
    grid_rows_init(shaft_rows, world, WIDTH, WORLD_ROWS);
    return ERR_SUCCESS;
}

void shaft_start(void) {
    uint8_t r;
    game.base = 0;
    {
        uint16_t size = CHUNK_BYTES;
        if (zar_entry_read_at(&pack, &chunk_file, (uint16_t)(CHUNK_COUNT + 1) * CHUNK_BYTES, chunk_buf, &size) != ERR_SUCCESS || size != CHUNK_BYTES)
            mem_set(chunk_buf, CELL_BACKGROUND, CHUNK_BYTES);
        chunk = chunk_buf;
    }
    chunk_row = 0;
    last_chunk = 0xFF;
    since_oil = 0;
    gen_row = 0;
    next_station = STATION_EVERY;
    station_floor = 0;
    lamp_row = 0;
    spawn_n = 0;
    for (r = 0; r < WORLD_ROWS; r++) {
        gen_next(r ? world + (r - 1) * WIDTH : NULL);
        commit(r);
    }

    camera_init_view(&cam, WIDTH * 16, WORLD_ROWS * 16, SCREEN_W, SCREEN_H);
    tileflags_register(FLAGS);
    tileflags_set_outside(TF_SOLID);
    stream_init(&vctx, &stream, &tilemap, world, WIDTH, WORLD_ROWS, SHAFT_LAYER, &cam);
    entity_view_set(0, 0);
}

/* Returns the number of rows scrolled in (0 most frames) */
uint8_t shaft_update(int16_t hero_y) {
    int16_t target = hero_y - HERO_SCREEN_Y;
    uint8_t rows = 0;
    if (target > cam.y) camera_set(&cam, 0, target);
    while (cam.y >= REBASE_Y) {
        if (!next_ready) gen_next(LAST_ROW);
        stream_shift(&stream, &cam, &game_entities, 0, 1);
        game.base++;
        commit(WORLD_ROWS - 1);
        rows++;
    }
    tilemap_stream_update(&vctx, &stream, cam.x, cam.y);
    entity_view_set(0, cam.y);
    // a quiet frame: get the next row ready
    if (!rows && !next_ready) gen_next(LAST_ROW);
    return rows;
}

/* in vblank: the shaft scrolls, the fog layer stays put */
void shaft_vblank(void) {
    camera_scroll_layer(&cam, SHAFT_LAYER, 0, 0);
}

void shaft_take(uint8_t tx, uint8_t ty) {
    tilemap_stream_set_tile(&vctx, &stream, tx, ty, tier_tiles() + OFS_BACK2);
}

uint8_t shaft_station(uint16_t feet_row) {
    if (!station_floor || feet_row != station_floor) return 0;
    station_floor = 0;
    return 1;
}

/* the miner left the station: its lamp goes out (as a light it makes the fog redo the whole screen on every scroll step) */
void shaft_lamp_off(void) {
    lamp_row = 0;
}

int16_t shaft_lamp_y(void) {
    uint16_t r;
    if (!lamp_row) return -1;
    if (lamp_row < game.base) {   // scrolled away above
        lamp_row = 0;
        return -1;
    }
    r = lamp_row - game.base;
    return r < WORLD_ROWS ? (int16_t)(r << 4) + 8 : -1;
}
