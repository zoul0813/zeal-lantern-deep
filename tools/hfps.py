#!/usr/bin/env python3
"""Deterministic frame rate check in zeal-native headless console mode.
Boots ZOS, execs the game from HostFS, presses Enter on the title, then reads the
game's `_fps_frames` counter across 1 emulated second (10M T-states).
Usage: hfps.py <game.bin> [extra console commands run before measuring, ';' separated]"""
import os, re, subprocess, sys
import argparse
import shlex
from emulator import add_arguments, resolve, boot_commands

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('bin')
ap.add_argument('extra', nargs='?', default='', help='extra console commands, separated by semicolons')
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)
binp = os.path.realpath(a.bin)
mapp = binp[:-4] + '.map'
addr = None
for l in open(mapp):
    m = re.match(r'\s+([0-9A-F]{8})\s+_fps_frames\b', l)
    if m: addr = int(m.group(1), 16)
assert addr, 'no _fps_frames in map (build with -DDEBUG_FPS=ON)'
cmds = boot_commands(os.path.basename(binp))
cmds += ['tap ENTER', 'run 60000000', 'tap ENTER', 'run 3000000', 'press ENTER', 'run 3000000', 'release ENTER', 'run 40000000']
extra = a.extra.split(';') if a.extra else []
cmds += extra
cmds += ['rb 0x%x 2' % addr, 'run 10000000', 'rb 0x%x 2' % addr, 'rb 0x%x 8' % (addr + 2), 'quit']
env = dict(os.environ)
p = subprocess.run([a.emu, '-r', a.rom,
                    '-H', os.path.dirname(binp), '-o', '-g'] + shlex.split(a.args), input='\n'.join(cmds) + '\n', capture_output=True, text=True, env=env, timeout=300)
vals = re.findall(r'\[DBG\] [0-9a-f]{4}: ([0-9a-f]{2}) ([0-9a-f]{2})', p.stdout + p.stderr)
if len(vals) < 2:
    print((p.stdout + p.stderr)[-3000:]); sys.exit(1)
a = int(vals[0][1] + vals[0][0], 16); b = int(vals[1][1] + vals[1][0], 16)
print('frames at start %d, after 1s %d -> %d fps' % (a, b, b - a))
print('\n'.join(l.strip() for l in re.findall(r'\[DBG\][^\n]*', p.stdout + p.stderr)[2:]))
