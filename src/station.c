/*
 * Lift stations: about every 100 m the shaft puts in a lit timber hut (shaft.c, 'station' in
 * assets/chunks/station.tmx). When the miner lands on its floor the game stops and this shop opens on the
 * fog layer, below the floor so the station stays in view. Gems in the purse buy oil, flares and
 * upgrades for the rest of the run; spending them doesn't lower the score (that counts gems found).
 * The loop is station() in main.c.
 */
#include <core.h>
#include "game.h"
#include "controls.h"

#define PANEL_Y 7   // fog layer rows 7-14: the miner stands on screen row 5, the floor is row 6

static const char *const NAMES[ITEMS] = { "FILL OIL", "3 FLARES", "BIG TANK", "SLOW WICK", "GO DEEPER" };
static const uint8_t COST[ITEMS] = { 4, 3, 10, 12, 0 };
/* what the next 100 m bring, one line per station (the last one repeats) */
static const char *const HINTS[] = { "WISPS HUNT THE LIGHT", "THE DEEP BURNS FAST", "DEEPER STILL" };

uint8_t shop_sel;

static uint8_t for_sale(uint8_t item) {
    switch (item) {
        case ITEM_FILL: return game.oil < game.oil_max;
        case ITEM_FLARES: return game.flares < FLARES_MAX;
        case ITEM_TANK: return game.tanks < TANKS_MAX;
        case ITEM_WICK: return !game.wick;
    }
    return 1;
}

uint8_t station_can(uint8_t item) {
    return for_sale(item) && game.purse >= COST[item];
}

uint8_t station_buy(uint8_t item) {
    if (!station_can(item)) return 0;
    game.purse -= COST[item];
    switch (item) {
        case ITEM_FILL: game.oil = game.oil_max; break;
        case ITEM_FLARES: game.flares = (game.flares + 3 > FLARES_MAX) ? FLARES_MAX : game.flares + 3; break;
        case ITEM_TANK:
            // a bigger tank comes full of the extra oil
            game.tanks++;
            game.oil_max += OIL_TANK;
            game.oil += OIL_TANK;
            break;
        case ITEM_WICK: game.wick = 1; break;
    }
    game.hud_dirty = 1;
    return 1;
}

/* "LIFT STATION 100M": the depth without leading zeros */
static void draw_title(void) {
    char title[WIDTH] = "LIFT STATION ";
    char digits[4];
    uint8_t i = 0, n = 13;
    format_num(game.station * 100U, digits, 4);
    while (i < 3 && digits[i] == '0') i++;
    while (i < 4) title[n++] = digits[i++];
    title[n++] = 'M';
    title[n] = 0;
    text_center(title, PANEL_Y);
}

void station_draw(void) {
    char line[WIDTH - 1];
    uint8_t i, h = game.station - 1;
    tilemap_fill(&vctx, FOG_LAYER, FOG_DARK, 0, PANEL_Y, WIDTH, HEIGHT - PANEL_Y);
    draw_title();
    text_center(HINTS[h < 2 ? h : 2], PANEL_Y + 1);
    for (i = 0; i < ITEMS; i++) {
        const char *name = NAMES[i];
        uint8_t x = 3;
        mem_set(line, ' ', sizeof(line) - 1);
        line[sizeof(line) - 1] = 0;
        if (i == shop_sel) line[1] = '>';
        while (*name) line[x++] = *name++;
        if (i != ITEM_LEAVE) {
            if (!for_sale(i)) {
                line[15] = line[16] = '-';
            } else {
                format_num(COST[i], &line[15], 2);
                if (line[15] == '0') line[15] = ' ';
                line[17] = HUD_GEM_ICON;
            }
        }
        text_print(line, 0, PANEL_Y + 2 + i);
    }
    // purse, and how to pick
    mem_cpy(line, CONTROL_PROMPT("         Z TO PICK", "         " PAD_B " TO PICK"), sizeof(line));   // 18 cells and the NUL
    line[1] = HUD_GEM_ICON;
    format_num(game.purse, &line[2], 3);
    text_print(line, 0, PANEL_Y + 7);
}
