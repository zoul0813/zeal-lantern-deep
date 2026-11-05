#!/usr/bin/env python3
"""
Writes the Lantern Deep music as ZMT (Zeal Music Tracker) files in assets/.

All tracks are original. Each pattern is 32 sixteenth steps on up to 3 voices; voice 3 is
never written, so the game's sound effects own it. The files load with ZGDK's zmt_rom_load()
and can also be opened in ZMT itself.

Step tokens (one per sixteenth):
  .      nothing (the previous note keeps sounding)
  -      note off
  C5 F#4 a note (octave 4 holds middle C, like ZMT)
  k s h  drum hits for a noise voice (kick, snare, hat); a hit is cut off on the next empty step
"""
import os
import struct

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "assets")

SEMI = {"C": 0, "D": 2, "E": 4, "F": 5, "G": 7, "A": 9, "B": 11}
NOTE_OFF = 108
WAV = {"square": 0, "triangle": 1, "saw": 2, "noise": 3}
VOL = {0: 0xF0, 25: 0xF2, 50: 0xF4, 75: 0xF6, 100: 0xF8}
# noise "pitch" only changes the colour of the hiss
DRUMS = {"k": 24, "s": 64, "h": 96}   # C2, E5, C8


def note(tok):
    name = tok[0]
    sharp = 1 if tok[1] == "#" else (-1 if tok[1] == "b" else 0)
    octave = int(tok[-1])
    return octave * 12 + SEMI[name] + sharp


def steps(line, drums=False):
    toks = line.split()
    assert len(toks) == 32, (len(toks), line)
    out = []
    for i, t in enumerate(toks):
        if t == ".":
            # cut drum hits after one step, they are noise and would ring forever
            if drums and i > 0 and toks[i - 1] in DRUMS:
                out.append(NOTE_OFF)
            else:
                out.append(None)
        elif t == "-":
            out.append(NOTE_OFF)
        elif drums:
            out.append(DRUMS[t])
        else:
            out.append(note(t))
    return out


def voice(wave, vol, line):
    """One voice of a pattern: waveform, volume (percent) and 32 step tokens."""
    return (WAV[wave], VOL[vol], steps(line, drums=(wave == "noise")))


def pattern_bytes(voices):
    out = bytearray()
    bitmap = 0
    for i, v in enumerate(voices):
        if v is not None:
            bitmap |= 1 << i
    out.append(bitmap)
    for v in voices:
        if v is None:
            continue
        wave, vol, notes = v
        cells = []
        step_bits = 0
        for i, n in enumerate(notes):
            hdr = 0
            data = []
            if n is not None:
                hdr |= 0x01
                data.append(n)
            if i == 0:
                # every pattern sets its own waveform and volume, so patterns and tracks can follow
                # each other in any order
                hdr |= 0x02 | 0x04
                data += [wave, vol]
            if hdr:
                step_bits |= 1 << i
                cells.append(bytes([hdr] + data))
        out += struct.pack("<I", step_bits)
        for c in cells:
            out += c
    return bytes(out)


def zmt(title, tempo, patterns, arrangement):
    """tempo is frames per quarter note (a step is tempo/4 frames). arrangement: pattern indexes."""
    assert tempo % 4 == 0
    assert len(patterns) <= 16 and len(arrangement) <= 64
    out = bytearray(b"ZMT\x00")
    out += title.ljust(12)[:12].encode("ascii")
    out.append(tempo)
    out.append(len(patterns))
    bits = (1 << len(arrangement)) - 1
    out += struct.pack("<II", bits & 0xFFFFFFFF, bits >> 32)
    for p in arrangement:
        out += bytes([p, 0xFF])
    for p in patterns:
        out += pattern_bytes(p)
    return bytes(out)


# ---------------------------------------------------------------------------
# "Deep", the in-game loop. A minor, 28 frames per quarter (~128 bpm), A B A C.
# Voice 0 is a quiet square arpeggio (the lantern flicker) that turns into the melody in B and C,
# voice 1 a triangle bass, voice 2 sparse noise "drips". main.c drops the tempo to 20 on low oil.
# ---------------------------------------------------------------------------
DRIP = "k . . . h . . . . . h . . . . . k . . . h . . . . . h . h . . ."
DRIP_FILL = "k . . . h . . . . . h . . . k . k . . . h . . . s . h . s . s s"

DEEP = [
    [   # A: Am | F, arpeggio
        voice("square", 25, "A4 C5 E5 A5 E5 C5 A4 C5 E5 A5 E5 C5 A4 C5 E5 C5 F4 A4 C5 F5 C5 A4 F4 A4 C5 F5 C5 A4 F4 A4 C5 A4"),
        voice("triangle", 100, "A2 . . . . . . . A2 . . . E2 . . . F2 . . . . . . . F2 . . . C3 . . ."),
        voice("noise", 25, DRIP),
        None,
    ],
    [   # B: Dm | E, melody
        voice("square", 50, "D5 . . . F5 . A5 . G5 . F5 . E5 . . . G#5 . . . . . B5 . A5 . G#5 . E5 . - ."),
        voice("triangle", 100, "D2 . . . . . . . D3 . . . A2 . . . E2 . . . . . . . E3 . . . B2 . . ."),
        voice("noise", 25, DRIP),
        None,
    ],
    [   # C: F | E, melody resolving back to A
        voice("square", 50, "C6 . . . B5 . A5 . F5 . . . A5 . C6 . B5 . . . . . G#5 . . . . . . . - ."),
        voice("triangle", 100, "F2 . . . . . . . F3 . . . C3 . . . E2 . . . . . . . E3 . . . G#2 . . ."),
        voice("noise", 25, DRIP_FILL),
        None,
    ],
]

# ---------------------------------------------------------------------------
# Title: slow and low. Am | G, F | E.
# ---------------------------------------------------------------------------
TITLE = [
    [
        voice("triangle", 100, "E5 . . . . . D5 . C5 . . . B4 . . . A4 . . . . . . . G4 . . . . . - ."),
        voice("square", 25, "A3 E4 A4 E4 A3 E4 A4 E4 A3 E4 A4 E4 A3 E4 A4 E4 G3 D4 G4 D4 G3 D4 G4 D4 G3 D4 G4 D4 G3 D4 G4 D4"),
        voice("triangle", 75, "A2 . . . . . . . . . . . . . . . G2 . . . . . . . . . . . . . . ."),
        None,
    ],
    [
        voice("triangle", 100, "C5 . . . D5 . E5 . F5 . . . E5 . D5 . E5 . . . . . . . . . . . - . . ."),
        voice("square", 25, "F3 C4 F4 C4 F3 C4 F4 C4 F3 C4 F4 C4 F3 C4 F4 C4 E3 B3 E4 B3 E3 B3 E4 B3 E3 B3 E4 B3 E3 G#3 E4 G#3"),
        voice("triangle", 75, "F2 . . . . . . . . . . . . . . . E2 . . . . . . . . . . . . . . ."),
        None,
    ],
]

# ---------------------------------------------------------------------------
# Game over: the lantern goes out. One pattern, 6 frames a step (192 frames).
# ---------------------------------------------------------------------------
GAMEOVER = [[
    voice("square", 50, "E5 . . . D5 . . . C5 . . . B4 . . . A4 . . . . . . . . . . . - . . ."),
    voice("triangle", 100, "A3 . . . G3 . . . F3 . . . E3 . . . A2 . . . . . . . . . . . - . . ."),
    None,
    None,
]]

TRACKS = {
    "deep.zmt": zmt("DEEP", 28, DEEP, [0, 1, 0, 2]),
    "title.zmt": zmt("LANTERN DEEP", 36, TITLE, [0, 1]),
    "gameover.zmt": zmt("LIGHTS OUT", 24, GAMEOVER, [0]),
}

if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, data in TRACKS.items():
        with open(os.path.join(OUT, name), "wb") as f:
            f.write(data)
        print(f"{name}: {len(data)} bytes")
