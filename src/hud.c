/*
 * HUD on row 0 of the fog layer (it never scrolls) and centered text. The font tiles sit at their
 * ASCII codes; text and icons are mapped through ASCII_MAP.
 *
 *   [lamp][oil bar x5] DDDDM [gem]GGG [flare]F
 *
 * The bar is the share of the tank (bigger with BIG TANKs), the gem count is the purse (gems not
 * spent at lift stations). hud_banner() puts a line of text there instead for a while.
 */
#include <core.h>
#include "game.h"
#include <zgdk/ascii.h>

void hud_init(void) {
    // This game's font occupies tiles 32-95; unsupported characters are blank.
    mem_set(ASCII_MAP, 32, sizeof(ASCII_MAP));
    ascii_map(' ', 64, 32);
    ascii_map(HUD_LAMP_ICON, 1, HUD_LAMP);
    ascii_map(HUD_OIL_EMPTY, 5, HUD_BAR0); // consecutive glyphs: empty through full
    ascii_map(HUD_GEM_ICON, 1, HUD_GEM);
    ascii_map(HUD_FLARE_ICON, 1, HUD_FLARE);
}

void text_print(const char *str, uint8_t x, uint8_t y) {
    uint8_t len = 0;
    while (str[len]) len++;
    hud_tilemap_nput_s_layer(&vctx, str, len, FOG_LAYER, x, y);
}

void text_center(const char *str, uint8_t y) {
    uint8_t len = 0;
    while (str[len]) len++;
    text_print(str, (WIDTH - len) >> 1, y);
}

/* zero padded; zgdk_itoa takes an int and has no padding */
void format_num(uint16_t value, char *out, uint8_t digits) {
    int8_t i;
    for (i = digits - 1; i >= 0; i--) {
        out[i] = '0' + (value % 10);
        value /= 10;
    }
}

uint16_t game_score(void) {
    uint32_t s = (uint32_t)game.depth + (uint32_t)game.gems * 10;
    return s > 0xFFFF ? 0xFFFF : (uint16_t)s;
}

static const char *banner_text;

void hud_banner(const char *text, uint8_t frames) {
    banner_text = text;
    game.banner = frames;
    game.hud_dirty = 1;
}

void hud_draw(void) {
    uint8_t line[WIDTH];
    uint8_t i;
    uint8_t q = (uint8_t)(game.oil / (game.oil_max / 20));   // quarters of the 5-segment bar (oil_max is a multiple of 20)
    game.hud_dirty = 0;
    if (game.banner) {
        tilemap_fill(&vctx, FOG_LAYER, ascii_to(' '), 0, HUD_ROW, WIDTH, 1);
        text_center(banner_text, HUD_ROW);
        return;
    }
    mem_set(line, ' ', WIDTH);
    line[0] = HUD_LAMP_ICON;
    for (i = 0; i < 5; i++) {
        uint8_t fill = q > 4 ? 4 : q;
        line[1 + i] = HUD_OIL_EMPTY + fill;
        q -= fill;
    }
    format_num(game.depth > 9999 ? 9999 : game.depth, (char *)&line[7], 4);
    line[11] = 'M';
    line[13] = HUD_GEM_ICON;
    format_num(game.purse, (char *)&line[14], 3);
    line[17] = HUD_FLARE_ICON;
    line[18] = '0' + game.flares;
    hud_tilemap_nput_s_layer(&vctx, (const char *)line, WIDTH, FOG_LAYER, 0, HUD_ROW);
}
