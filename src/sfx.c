/*
 * Sound effects: tables for ZGDK's sequencer (zgdk/sound/sfx.h) on voice 3.
 * Voices 0-2 belong to the ZMT music (music.c), which never writes voice 3.
 * Priorities keep a jump from cutting a pickup or a hit (sfx_play_prio).
 */
#include "game.h"

static const sfx_seg S_JUMP[]  = { SFX_SEG(WAV_SQUARE, 280, 560, 7), SFX_END };
static const sfx_seg S_THROW[] = { SFX_SEG(WAV_NOISE, 2400, 5200, 6), SFX_SEG(WAV_NOISE, 7000, 7000, 10), SFX_END };
static const sfx_seg S_GEM[]   = { SFX_SEG(WAV_TRIANGLE, 1568, 1568, 3), SFX_SEG(WAV_TRIANGLE, 2093, 2093, 3),
                                   SFX_SEG(WAV_TRIANGLE, 2637, 2637, 6), SFX_END };
static const sfx_seg S_OIL[]   = { SFX_SEG(WAV_SQUARE, 330, 330, 4), SFX_SEG(WAV_SQUARE, 392, 392, 4),
                                   SFX_SEG(WAV_SQUARE, 494, 494, 4), SFX_SEG(WAV_SQUARE, 659, 700, 10), SFX_END };
static const sfx_seg S_FLARE[] = { SFX_SEG(WAV_TRIANGLE, 880, 1760, 8), SFX_END };
static const sfx_seg S_HURT[]  = { SFX_SEG(WAV_SAWTOOTH, 400, 120, 14), SFX_END };
static const sfx_seg S_STEAL[] = { SFX_SEG(WAV_TRIANGLE, 1800, 200, 24), SFX_SEG(WAV_NOISE, 600, 200, 8), SFX_END };
static const sfx_seg S_STOMP[] = { SFX_SEG(WAV_NOISE, 1200, 300, 5), SFX_SEG(WAV_SQUARE, 660, 990, 5), SFX_END };
static const sfx_seg S_LOW[]   = { SFX_SEG(WAV_SQUARE, 988, 988, 6), SFX_SEG(WAV_SQUARE, 0, 0, 4),
                                   SFX_SEG(WAV_SQUARE, 988, 988, 6), SFX_SEG(WAV_SQUARE, 0, 0, 4),
                                   SFX_SEG(WAV_SQUARE, 988, 988, 6), SFX_END };
static const sfx_seg S_DIE[]   = { SFX_SEG(WAV_TRIANGLE, 660, 520, 20), SFX_SEG(WAV_TRIANGLE, 520, 330, 30),
                                   SFX_SEG(WAV_TRIANGLE, 330, 110, 50), SFX_END };
static const sfx_seg S_STATION[] = { SFX_SEG(WAV_TRIANGLE, 523, 523, 6), SFX_SEG(WAV_TRIANGLE, 659, 659, 6),
                                     SFX_SEG(WAV_TRIANGLE, 784, 784, 6), SFX_SEG(WAV_TRIANGLE, 1047, 1047, 16), SFX_END };
static const sfx_seg S_BUY[]   = { SFX_SEG(WAV_SQUARE, 1319, 1319, 3), SFX_SEG(WAV_SQUARE, 1976, 1976, 8), SFX_END };
static const sfx_seg S_NOPE[]  = { SFX_SEG(WAV_SQUARE, 150, 150, 6), SFX_SEG(WAV_SQUARE, 0, 0, 3), SFX_SEG(WAV_SQUARE, 150, 150, 8), SFX_END };
static const sfx_seg S_BEST[]  = { SFX_SEG(WAV_SQUARE, 784, 784, 5), SFX_SEG(WAV_SQUARE, 988, 988, 5), SFX_SEG(WAV_SQUARE, 1175, 1175, 5),
                                   SFX_SEG(WAV_SQUARE, 1568, 1568, 5), SFX_SEG(WAV_SQUARE, 1175, 1175, 4), SFX_SEG(WAV_SQUARE, 1568, 1568, 18), SFX_END };

void game_sfx_init(void) {
    sfx_init(VOICE3, VOL_100);
}

void game_sfx_jump(void)       { sfx_play_prio(S_JUMP, 0); }
void game_sfx_throw(void)      { sfx_play_prio(S_THROW, 1); }
void game_sfx_flare_pick(void) { sfx_play_prio(S_FLARE, 2); }
void game_sfx_gem(void)        { sfx_play_prio(S_GEM, 2); }
void game_sfx_oil(void)        { sfx_play_prio(S_OIL, 2); }
void game_sfx_stomp(void)      { sfx_play_prio(S_STOMP, 2); }
void game_sfx_hurt(void)       { sfx_play_prio(S_HURT, 3); }
void game_sfx_steal(void)      { sfx_play_prio(S_STEAL, 3); }
void game_sfx_low(void)        { sfx_play_prio(S_LOW, 3); }
void game_sfx_die(void)        { sfx_play_prio(S_DIE, 4); }
void game_sfx_station(void)    { sfx_play_prio(S_STATION, 4); }
void game_sfx_buy(void)        { sfx_play_prio(S_BUY, 2); }
void game_sfx_nope(void)       { sfx_play_prio(S_NOPE, 2); }
void game_sfx_best(void)       { sfx_play_prio(S_BEST, 3); }
