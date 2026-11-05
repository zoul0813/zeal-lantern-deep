#!/usr/bin/env python3
"""Generate the Lantern Deep tileset (assets/tileset.gif) and its tile index header (src/gfx.h).

All art is text pixel art or seeded noise in this script, so it stays diffable and reproducible.
One 256-tile, 8-bit tileset is shared by both layers and the sprites. Palette index 0 is the cave
black on layer 0 and transparent on layer 1 (the fog) and on sprites.

Layout:
    0         empty (transparent on the fog layer)
    1..4      fog: dark, 75%, 50%, 25%
    5..12     HUD icons and oil bar
    32..95    font at its ASCII codes, so ZGDK's default ASCII_MAP prints text with no setup
    96..146   shaft tiles, 17 per depth tier (3 tiers): rock, rock top, back wall, decor, and the
              props (planks, ladders, spikes, pickups) drawn on that tier's back wall
    147..165  sprites
    166..167  lift station wall and lamp (layer 0, the same at every depth)
Run: python3 tools/make_assets.py   (from the game folder; needs Pillow)
"""
import os
import random
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# ---------------------------------------------------------------- palette
PAL = [
    ('.', (6, 5, 10)),       # 0 cave black / transparent
    ('k', (0, 0, 0)),        # fog and outlines
    ('w', (255, 255, 255)),
    ('l', (200, 200, 212)),
    ('m', (110, 110, 130)),
    ('y', (255, 214, 80)),   # lantern glass
    ('Y', (255, 250, 200)),  # flame core
    ('o', (240, 136, 40)),
    ('r', (220, 50, 40)),
    ('R', (120, 20, 20)),
    ('h', (150, 100, 60)),   # tier 0 rock (brown)
    ('H', (100, 64, 36)),
    ('d', (60, 38, 24)),
    ('b', (46, 32, 26)),     # tier 0 back wall
    ('B', (70, 50, 38)),
    ('s', (120, 130, 150)),  # tier 1 rock (slate)
    ('S', (72, 80, 100)),
    ('D', (40, 44, 60)),
    ('z', (32, 36, 50)),     # tier 1 back wall
    ('Z', (50, 56, 74)),
    ('c', (160, 70, 56)),    # tier 2 rock (crimson)
    ('C', (104, 38, 36)),
    ('x', (60, 20, 24)),
    ('q', (50, 22, 24)),     # tier 2 back wall
    ('Q', (74, 34, 34)),
    ('g', (90, 245, 170)),   # gem
    ('G', (20, 150, 100)),
    ('p', (170, 210, 255)),  # wisp
    ('P', (80, 120, 255)),
    ('n', (60, 96, 210)),    # miner overalls
    ('N', (30, 48, 120)),
    ('f', (240, 190, 140)),  # skin
    ('e', (190, 90, 220)),   # beetle
    ('E', (100, 40, 130)),
    ('u', (190, 130, 70)),   # wood
    ('U', (116, 74, 40)),
    ('t', (255, 90, 120)),   # flare stick
    ('T', (150, 30, 60)),
]
CI = {ch: i for i, (ch, _) in enumerate(PAL)}

tiles = [None] * 256
names = {}


def grid(rows):
    rows = [r.replace(' ', '') for r in rows]
    assert len(rows) == 16, len(rows)
    for r in rows:
        assert len(r) == 16, (r, len(r))
    return [[CI[ch] for ch in r] for r in rows]


def put(index, name, px):
    assert tiles[index] is None, (index, name)
    tiles[index] = px
    if name:
        names[name] = index


def blank(c=0):
    return [[c] * 16 for _ in range(16)]


def flipx(px):
    return [list(reversed(r)) for r in px]


def recolor(px, mapping):
    m = {CI[a]: CI[b] for a, b in mapping.items()}
    return [[m.get(v, v) for v in r] for r in px]


# ---------------------------------------------------------------- fog
K = CI['k']
put(0, 'TILE_EMPTY', blank())
put(1, 'FOG_DARK', blank(K))
put(2, 'FOG_75', [[0 if (x % 2 == 0 and y % 2 == 0) else K for x in range(16)] for y in range(16)])
put(3, 'FOG_50', [[0 if (x + y) % 2 == 0 else K for x in range(16)] for y in range(16)])
put(4, 'FOG_25', [[K if (x % 2 == 0 and y % 2 == 0) else 0 for x in range(16)] for y in range(16)])

# ---------------------------------------------------------------- HUD (drawn on black, the fog layer)
put(5, 'HUD_LAMP', grid([
    "kkkkkkkkkkkkkkkk",
    "kkkkkkmmmmkkkkkk",
    "kkkkkmkkkkmkkkkk",
    "kkkkkkmmmmkkkkkk",
    "kkkkmmmmmmmmkkkk",
    "kkkkmyyyyyymkkkk",
    "kkkkmyyYYyymkkkk",
    "kkkkmyYYYYymkkkk",
    "kkkkmyYYYYymkkkk",
    "kkkkmyyYYyymkkkk",
    "kkkkmyyyyyymkkkk",
    "kkkkmmmmmmmmkkkk",
    "kkkkkmmmmmmkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
]))
for q in range(5):   # oil bar segment, q quarters full
    px = blank(K)
    for x in range(16):
        px[4][x] = CI['m']
        px[11][x] = CI['m']
    for y in range(5, 11):
        for x in range(16):
            px[y][x] = CI['o'] if x < q * 4 else CI['d']
            if x < q * 4 and y == 6:
                px[y][x] = CI['y']
    put(6 + q, 'HUD_BAR%d' % q, px)
put(11, 'HUD_GEM', grid([
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkGGGGGGkkkkk",
    "kkkkGggwgggGkkkk",
    "kkkGggwggggGGkkk",
    "kkGGGGGGGGGGGGkk",
    "kkkGggggggggGkkk",
    "kkkkGggggggGkkkk",
    "kkkkkGggggGkkkkk",
    "kkkkkkGggGkkkkkk",
    "kkkkkkkGGkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
]))
put(12, 'HUD_FLARE', grid([
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkykkkkkk",
    "kkkkkkkkyYykkkkk",
    "kkkkkkkkkokkkkkk",
    "kkkkkkkktTkkkkkk",
    "kkkkkkkttTkkkkkk",
    "kkkkkkkttkkkkkkk",
    "kkkkkkttTkkkkkkk",
    "kkkkkkttkkkkkkkk",
    "kkkkkttTkkkkkkkk",
    "kkkkkttkkkkkkkkk",
    "kkkkkTkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
    "kkkkkkkkkkkkkkkk",
]))

# ---------------------------------------------------------------- rock and back wall, 3 depth tiers
TIERS = [('h', 'H', 'd', 'b', 'B'), ('s', 'S', 'D', 'z', 'Z'), ('c', 'C', 'x', 'q', 'Q')]
TIER_BASE = 96
TIER_SIZE = 17   # rock x4, rock top, back x3, beam, crystal, then the 7 props on this tier's back wall


def rock(rng, light, mid, dark, top):
    px = [[CI[mid]] * 16 for _ in range(16)]
    # blotches of dark and light
    for _ in range(7):
        cx, cy, r = rng.randrange(16), rng.randrange(16), rng.choice((1, 2, 2, 3))
        col = CI[rng.choice((dark, dark, light))]
        for y in range(16):
            for x in range(16):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r:
                    px[y][x] = col
    # a crack
    x, y = rng.randrange(3, 13), rng.randrange(3, 13)
    for _ in range(6):
        px[y][x] = CI[dark]
        x = max(0, min(15, x + rng.choice((-1, 0, 1))))
        y = max(0, min(15, y + 1))
    if top:
        for x in range(16):
            px[0][x] = CI[light]
            px[1][x] = CI[light] if rng.random() < 0.7 else CI[mid]
            px[2][x] = CI[mid] if px[2][x] == CI[light] else px[2][x]
    return px


def back(rng, base, light, density):
    px = blank(CI[base])
    for _ in range(density):
        x, y = rng.randrange(16), rng.randrange(16)
        px[y][x] = CI[rng.choice(('.', light, light))]
        if rng.random() < 0.5 and x < 15:
            px[y][x + 1] = CI[light]
    return px


for t, (light, mid, dark, bk, bkl) in enumerate(TIERS):
    rng = random.Random(1000 + t)
    base = TIER_BASE + t * TIER_SIZE
    for v in range(4):
        put(base + v, 'T%d_ROCK%d' % (t, v) if t == 0 else None, rock(rng, light, mid, dark, False))
    put(base + 4, 'T0_ROCK_TOP' if t == 0 else None, rock(rng, light, mid, dark, True))
    put(base + 5, 'T0_BACK0' if t == 0 else None, back(rng, bk, bkl, 6))
    put(base + 6, 'T0_BACK1' if t == 0 else None, back(rng, bk, bkl, 14))
    put(base + 7, 'T0_BACK2' if t == 0 else None, blank(CI[bk]))
    # support beam (decor): vertical wooden post
    px = blank()
    for y in range(16):
        for x in (6, 7, 8, 9):
            px[y][x] = CI['U'] if x in (6, 9) else CI['u']
        if y % 8 == 3:
            px[y][7] = CI['d']
    put(base + 8, 'T0_BEAM' if t == 0 else None, recolor(px, {'.': bk, 'u': 'U', 'U': 'd'}))
    # crystal vein in the back wall
    px = back(rng, bk, bkl, 6)
    for (x, y) in ((4, 9), (5, 8), (5, 9), (6, 7), (6, 8), (10, 11), (11, 10), (11, 11)):
        px[y][x] = CI['G'] if t != 2 else CI['R']
    put(base + 9, 'T0_CRYSTAL' if t == 0 else None, px)

# ---------------------------------------------------------------- props and pickups (on cave black)
put(13, 'TILE_PLANK', grid([
    "uuuuuuuuuuuuuuuu",
    "uhuuuuuuuhuuuuuu",
    "UUUUUUUUUUUUUUUU",
    "kUkkkkkkkkkkkkUk",
    ".U............U.",
    ".U............U.",
    "..U..........U..",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
]))
LADDER = [
    "...U........U...",
    "...uUUUUUUUUu...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...U........U...",
    "...uUUUUUUUUu...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
    "...u........u...",
]
put(14, 'TILE_LADDER', grid(LADDER))
put(15, 'TILE_LADDER_TOP', grid(["..uU........Uu..", "..uuUUUUUUUUuu.."] + LADDER[2:]))
put(16, 'TILE_SPIKES', grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "..l.....l.....l.",
    "..l....lll....l.",
    ".lml...lml...lml",
    ".lml..lmmml..lml",
    "lmmml.lmmml.lmmm",
    "lmmmllmmmmmllmmm",
    "mmmmmmmmmmmmmmmm",
    "dddddddddddddddd",
    "dddddddddddddddd",
]))
put(17, 'TILE_GEM', grid([
    "................",
    "................",
    "................",
    "................",
    ".....GGGGGG.....",
    "....GggwgggG....",
    "...GggwggggGG...",
    "..GGGGGGGGGGGG..",
    "...GggggggggG...",
    "....GggggggG....",
    ".....GggggG.....",
    "......GggG......",
    ".......GG.......",
    "................",
    "................",
    "................",
]))
put(18, 'TILE_OIL', grid([
    "................",
    "................",
    "......mmmm......",
    "......mkkm......",
    "....rrrrrrrr....",
    "...rRRRRRRRRr...",
    "...rRyyyyyyRr...",
    "...rRyRRRRyRr...",
    "...rRyRRRRyRr...",
    "...rRyyyyyyRr...",
    "...rRRRRRRRRr...",
    "...rRRRRRRRRr...",
    "...rRRRRRRRRr...",
    "....rrrrrrrr....",
    "................",
    "................",
]))
put(19, 'TILE_FLARE_ITEM', grid([
    "................",
    "................",
    "................",
    "...........tt...",
    "..........tTt...",
    ".........tTt....",
    "........tTt.....",
    ".......tTt......",
    "......tTt.......",
    ".....tTt........",
    "....tTt.........",
    "...lTt..........",
    "...ll...........",
    "................",
    "................",
    "................",
]))

# props on each tier's back wall (layer 0 has no transparency), at tier base + 10..16
PROPS = ['PLANK', 'LADDER', 'LADDER_TOP', 'SPIKES', 'GEM', 'OIL', 'FLARE_ITEM']
for t, (light, mid, dark, bk, bkl) in enumerate(TIERS):
    for i, name in enumerate(PROPS):
        src = tiles[13 + i]
        put(TIER_BASE + t * TIER_SIZE + 10 + i, None, [[CI[bk] if v == 0 else v for v in r] for r in src])
for i, name in enumerate(PROPS):
    tiles[13 + i] = None
    del names['TILE_' + name]

# ---------------------------------------------------------------- font (5x7 scaled x2, on black for the fog layer)
FONT = {
    ' ': ["", "", "", "", "", "", ""],
    '!': ["..#..", "..#..", "..#..", "..#..", "..#..", ".....", "..#.."],
    '-': [".....", ".....", ".....", "#####", ".....", ".....", "....."],
    '.': [".....", ".....", ".....", ".....", ".....", ".##..", ".##.."],
    ':': [".....", ".##..", ".##..", ".....", ".##..", ".##..", "....."],
    '?': [".###.", "#...#", "....#", "...#.", "..#..", ".....", "..#.."],
    "'": ["..#..", "..#..", ".#...", ".....", ".....", ".....", "....."],
    '=': [".....", ".....", "#####", ".....", "#####", ".....", "....."],
    '>': [".#...", "..#..", "...#.", "....#", "...#.", "..#..", ".#..."],
    '0': [".###.", "#...#", "#..##", "#.#.#", "##..#", "#...#", ".###."],
    '1': ["..#..", ".##..", "..#..", "..#..", "..#..", "..#..", ".###."],
    '2': [".###.", "#...#", "....#", "...#.", "..#..", ".#...", "#####"],
    '3': ["#####", "...#.", "..#..", "...#.", "....#", "#...#", ".###."],
    '4': ["...#.", "..##.", ".#.#.", "#..#.", "#####", "...#.", "...#."],
    '5': ["#####", "#....", "####.", "....#", "....#", "#...#", ".###."],
    '6': ["..##.", ".#...", "#....", "####.", "#...#", "#...#", ".###."],
    '7': ["#####", "....#", "...#.", "..#..", ".#...", ".#...", ".#..."],
    '8': [".###.", "#...#", "#...#", ".###.", "#...#", "#...#", ".###."],
    '9': [".###.", "#...#", "#...#", ".####", "....#", "...#.", ".##.."],
    'A': [".###.", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'B': ["####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."],
    'C': [".###.", "#...#", "#....", "#....", "#....", "#...#", ".###."],
    'D': ["###..", "#..#.", "#...#", "#...#", "#...#", "#..#.", "###.."],
    'E': ["#####", "#....", "#....", "####.", "#....", "#....", "#####"],
    'F': ["#####", "#....", "#....", "####.", "#....", "#....", "#...."],
    'G': [".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".####"],
    'H': ["#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"],
    'I': [".###.", "..#..", "..#..", "..#..", "..#..", "..#..", ".###."],
    'J': ["..###", "...#.", "...#.", "...#.", "...#.", "#..#.", ".##.."],
    'K': ["#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"],
    'L': ["#....", "#....", "#....", "#....", "#....", "#....", "#####"],
    'M': ["#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"],
    'N': ["#...#", "#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#"],
    'O': [".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'P': ["####.", "#...#", "#...#", "####.", "#....", "#....", "#...."],
    'Q': [".###.", "#...#", "#...#", "#...#", "#.#.#", "#..#.", ".##.#"],
    'R': ["####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"],
    'S': [".####", "#....", "#....", ".###.", "....#", "....#", "####."],
    'T': ["#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."],
    'U': ["#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."],
    'V': ["#...#", "#...#", "#...#", "#...#", "#...#", ".#.#.", "..#.."],
    'W': ["#...#", "#...#", "#...#", "#.#.#", "#.#.#", "#.#.#", ".#.#."],
    'X': ["#...#", "#...#", ".#.#.", "..#..", ".#.#.", "#...#", "#...#"],
    'Y': ["#...#", "#...#", ".#.#.", "..#..", "..#..", "..#..", "..#.."],
    'Z': ["#####", "....#", "...#.", "..#..", ".#...", "#....", "#####"],
}
for ch, rows in FONT.items():
    px = blank(K)
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c == '#':
                for dy in range(2):
                    for dx in range(2):
                        px[1 + y * 2 + dy][3 + x * 2 + dx] = CI['y']
                        if y < 2:
                            px[1 + y * 2 + dy][3 + x * 2 + dx] = CI['Y']
    put(ord(ch), None, px)
for i in range(32, 96):
    if tiles[i] is None:
        tiles[i] = blank(K)

# ---------------------------------------------------------------- sprites
S = TIER_BASE + TIER_SIZE * len(TIERS)


def spr(name, px):
    global S
    put(S, name, px)
    S += 1


# miner, facing right, lantern in the right hand
MINER_TOP = [
    "................",
    "......kkkk......",
    ".....kyyyyk.....",
    "....kyyyyyyk....",
    "...kooooooooo...",
    "....kfffkf......",
    "....kffffff.....",
    ".....kffff......",
]
LEGS = {
    'stand': [
        "....knnnnnk.k...",
        "...knnnnnnnfk...",
        "...kfnnnnnk.yk..",
        "....kNNNNNkyYy..",
        "....knnknnkyyy..",
        "....knnknnk.....",
        "...kHHHkHHHk....",
        "................",
    ],
    'walk1': [
        "....knnnnnk.k...",
        "...knnnnnnnfk...",
        "...kfnnnnnk.yk..",
        "....kNNNNNkyYy..",
        "...knnkknnkyyy..",
        "..knnk..knnk....",
        "..kHHk...kHHk...",
        "................",
    ],
    'walk2': [
        "....knnnnnk.k...",
        "...knnnnnnnfk...",
        "...kfnnnnnk.yk..",
        "....kNNNNNkyYy..",
        ".....knnnk.yyy..",
        ".....knnnk......",
        ".....kHHHk......",
        "................",
    ],
    'jump': [
        "..fknnnnnk.k....",
        "...knnnnnnnfk...",
        "....knnnnnk.yk..",
        "....kNNNNNkyYy..",
        "...knnkknnkyyy..",
        "..kHHk..knnk....",
        "........kHHk....",
        "................",
    ],
}
for name, legs in (('MINER_STAND', 'stand'), ('MINER_WALK1', 'walk1'), ('MINER_WALK2', 'walk2'), ('MINER_JUMP', 'jump')):
    spr(name, grid(MINER_TOP + LEGS[legs]))
CLIMB = [
    "................",
    "......kkkk......",
    ".....kyyyyk.....",
    "....kyyyyyyk....",
    "...kooooooook...",
    "..fk.kHHHHk.kf..",
    "..nk.kHHHHk.kn..",
    "..knnnnnnnnnnk..",
    "...knnnnnnnnk...",
    "....knnnnnnk....",
    "....kNNNNNNk....",
    "....knnkknnk....",
    "....knnkknnk....",
    "...kHHHkkHHHk...",
    "................",
    "................",
]
spr('MINER_CLIMB1', grid(CLIMB))
spr('MINER_CLIMB2', flipx(grid(CLIMB)))
spr('MINER_HURT', recolor(grid(MINER_TOP + LEGS['jump']), {'n': 'r', 'N': 'R', 'f': 'w'}))
spr('MINER_DEAD', grid([
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "....kkkk........",
    "...kyyyyk.......",
    "..kyyyyyykkkkkk.",
    "..kooookffnnnnHk",
    "...kkkkkkNNnnnHk",
    "........kkkkkkk.",
]))

BEETLE = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "................",
    "......kkkkk.....",
    "....kkeeeeekk...",
    "...keEeeeeeEek..",
    "..keEEeeeeeEEek.",
    "..keeeEEEEEeeekk",
    "..kEEEEEEEEEEkrk",
    "...kkkkkkkkkkkk.",
    "...k.k.k..k.k...",
    "................",
]
spr('BEETLE1', grid(BEETLE))
b2 = grid(BEETLE)
b2[14] = [CI[c] for c in "....k.k.k..k.k.."]
spr('BEETLE2', b2)
spr('BEETLE_FLAT', grid([
    "................", "................", "................", "................",
    "................", "................", "................", "................",
    "................", "................", "................", "................",
    "...kkkkkkkkkkk..",
    "..keEEeeeeEEEek.",
    "..kkkkkkkkkkkkk.",
    "................",
]))
WISP = [
    "................",
    "................",
    "..pp........pp..",
    ".pPPp......pPPp.",
    ".pPPPp....pPPPp.",
    "..pPPPpwwpPPPp..",
    "...pPPwwwwPPp...",
    "....pwwYYwwp....",
    "....pwwYYwwp....",
    "...pPPwwwwPPp...",
    "..pPPPpwwpPPPp..",
    ".pPPPp....pPPPp.",
    ".pPPp......pPPp.",
    "..pp........pp..",
    "................",
    "................",
]
spr('WISP1', grid(WISP))
w2 = grid([
    "................",
    "................",
    "................",
    "................",
    "...pp......pp...",
    "..pPPp....pPPp..",
    "..pPPPpwwpPPPp..",
    "....pwwYYwwp....",
    "....pwwYYwwp....",
    "..pPPPpwwpPPPp..",
    "..pPPp....pPPp..",
    "...pp......pp...",
    "................",
    "................",
    "................",
    "................",
])
spr('WISP2', w2)
FLARE_LIT = [
    "................",
    "................",
    "................",
    "................",
    "................",
    "......y.........",
    ".....yYy..y.....",
    "....yYYYy.......",
    ".....yYyo.......",
    "......ot........",
    "......tT........",
    ".....tT.........",
    ".....tT.........",
    "....tT..........",
    "...ltT..........",
    "..lllll.........",
]
spr('FLARE1', grid(FLARE_LIT))
f2 = [r for r in FLARE_LIT]
f2[5] = ".......y........"
f2[6] = "..y...yYy.......".ljust(16, '.')
f2[7] = ".....yYYYy......"
spr('FLARE2', grid(f2))
spr('FLARE_FLY', grid([
    "................",
    "................",
    "................",
    "................",
    "..........yy....",
    ".........yYy....",
    "........oty.....",
    ".......tTt......",
    "......tTt.......",
    ".....tTt........",
    "....lTt.........",
    "....ll..........",
    "................",
    "................",
    "................",
    "................",
]))
spr('SPARK1', grid([
    "................", "................", "................", ".......w........",
    ".......w........", "................", "...w..yYy..w....", "......YwY.......",
    "......yYy.......", "................", ".......w........", ".......w........",
    "................", "................", "................", "................",
]))
spr('SPARK2', grid([
    "................", "................", "..w.........w...", "................",
    "................", ".......y........", "................", ".....y.Y.y......",
    "................", ".......y........", "................", "................",
    "..w.........w...", "................", "................", "................",
]))
spr('DUST', grid([
    "................", "................", "................", "................",
    "................", "................", "................", "................",
    "................", "................", "..m.......m.....", ".mlm.....mlm....",
    "mllm..m..mllm...", ".mm..mlm..mm....", "......m.........", "................",
]))

# ---------------------------------------------------------------- lift station (layer 0, every 100 m)
# not tier tiles: the station is the same timber hut at every depth. Tile flags 0 (back wall).
STA_WALL = [
    "UUUUUUUUUUUUUUUU",
    "uuuuuuuhuuuuuuuu",
    "uuuuuuuuuuuuuuuu",
    "dddddddddddddddd",
    "uuuuhuuuuuuuuuuu",
    "uuuuuuuuuuuuuhuu",
    "UUUUUUUUUUUUUUUU",
    "dddddddddddddddd",
    "uuuuuuuuuuhuuuuu",
    "uuhuuuuuuuuuuuuu",
    "uuuuuuuuuuuuuuuu",
    "dddddddddddddddd",
    "uuuuuuuuuuuuuuuu",
    "uuuuuuhuuuuuuuuu",
    "UUUUUUUUUUUUUUUU",
    "dddddddddddddddd",
]
spr('STA_WALL', grid(STA_WALL))
lamp = grid(STA_WALL)
for y, row in enumerate([
    ".......kk.......",
    ".......mm.......",
    "......mkkm......",
    ".....mkkkkm.....",
    "....mmmmmmmm....",
    "....myyYYyym....",
    "....myYwwYym....",
    "....myYwwYym....",
    "....myyYYyym....",
    "....mmmmmmmm....",
    ".....mkkkkm.....",
    "................",
    "................",
    "................",
    "................",
    "................",
]):
    for x, ch in enumerate(row):
        if ch != '.':
            lamp[y][x] = CI[ch]
spr('STA_LAMP', lamp)

SPR_END = S
for i in range(256):
    if tiles[i] is None:
        tiles[i] = blank()

# ---------------------------------------------------------------- write gif + header
COLS = 16
img = Image.new('P', (16 * COLS, 16 * ((SPR_END + COLS - 1) // COLS)))
flat = []
for (_, rgb) in PAL:
    flat += list(rgb)
flat += [0] * (768 - len(flat))
img.putpalette(flat)
for i in range(SPR_END):
    tx, ty = (i % COLS) * 16, (i // COLS) * 16
    for y in range(16):
        for x in range(16):
            img.putpixel((tx + x, ty + y), tiles[i][y][x])
os.makedirs(os.path.join(ROOT, 'assets'), exist_ok=True)
img.save(os.path.join(ROOT, 'assets', 'tileset.gif'))
os.makedirs(os.path.join(ROOT, 'screenshots', 'assets'), exist_ok=True)
img.convert('RGB').resize((img.width * 3, img.height * 3), Image.NEAREST).save(os.path.join(ROOT, 'screenshots', 'assets', 'tileset_preview.png'))

with open(os.path.join(ROOT, 'src', 'gfx.h'), 'w') as f:
    f.write('/* Generated by tools/make_assets.py, do not edit */\n#ifndef GFX_H\n#define GFX_H\n\n')
    for name, idx in sorted(names.items(), key=lambda kv: kv[1]):
        f.write('#define %-24s %3d\n' % (name, idx))
    f.write('\n/* tile offsets inside a depth tier */\n')
    for i, name in enumerate(['ROCK0', 'ROCK1', 'ROCK2', 'ROCK3', 'ROCK_TOP', 'BACK0', 'BACK1', 'BACK2', 'BEAM', 'CRYSTAL'] + PROPS):
        f.write('#define OFS_%-20s %3d\n' % (name, i))
    f.write('\n#define TIER_BASE   %d\n#define TIER_SIZE   %d\n#define TIER_COUNT  %d\n' % (TIER_BASE, TIER_SIZE, len(TIERS)))
    f.write('#define TILE_COUNT  %d\n\n#endif\n' % SPR_END)
print('tiles used:', SPR_END)

# Shared SNES art, owned locally so the game remains standalone.
from snes_glyphs import install as install_snes_glyphs
install_snes_glyphs(ROOT, SPR_END)
