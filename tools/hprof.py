#!/usr/bin/env python3
"""Sampling profiler: run the game headless, sample PC every N T-states during play,
and attribute samples to functions from the linker map (PC < 0x4000 = ZOS kernel).
Usage: hprof.py <game.bin> [samples] [extra console commands before sampling, ; separated]"""
import os, re, subprocess, sys, bisect, collections
import argparse
import shlex
from emulator import add_arguments, resolve, boot_commands

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('bin')
ap.add_argument('samples', nargs='?', type=int, default=2000)
ap.add_argument('extra', nargs='?', default='', help='extra console commands, separated by semicolons')
add_arguments(ap)
a = ap.parse_args()
resolve(a, ap)
binp = os.path.realpath(a.bin); n = a.samples
syms = []
for l in open(binp[:-4] + '.map'):
    m = re.match(r'\s+([0-9A-F]{8})\s+(\S+)\s*(\S*)', l)
    if m:
        address = int(m.group(1), 16)
        if 0x4000 <= address < 0x10000: syms.append((address, m.group(2) + ' [' + (m.group(3) or 'lib') + ']'))
# add static functions from the per-object .sym files (offsets relative to the module's code area)
glob = {name.split(' ')[0]: a for a, name in syms}
build = os.environ.get('BUILD_DIR')
if build:
    for root, _, files in os.walk(build):
        for f in files:
            if not f.endswith('.c.sym'): continue
            entries = []
            for l in open(os.path.join(root, f)):
                m = re.match(r'\s+7\s+(\S+)\s+([0-9A-F]{6})\s+(G?R)', l)
                if m: entries.append((m.group(1), int(m.group(2), 16), m.group(3)))
            base = None
            for name, off, kind in entries:
                if kind == 'GR' and name in glob: base = glob[name] - off; break
            if base is None: continue
            for name, off, kind in entries:
                if kind == 'R': syms.append((base + off, name + ' [' + f[:-6] + ']'))
syms.sort(); addrs = [a for a, _ in syms]
cmds = boot_commands(os.path.basename(binp))
cmds += ['tap ENTER', 'run 60000000', 'tap ENTER', 'run 3000000', 'press ENTER', 'run 3000000', 'release ENTER', 'run 40000000']
if a.extra: cmds += a.extra.split(';')
for i in range(n): cmds += ['run 4999', 'regs']
cmds.append('quit')
env = dict(os.environ)
p = subprocess.run([a.emu, '-r', a.rom,
                    '-H', os.path.dirname(binp), '-o', '-g'] + shlex.split(a.args), input='\n'.join(cmds) + '\n', capture_output=True, text=True, env=env, timeout=600)
pcs = [int(x, 16) for x in re.findall(r'PC=([0-9a-f]{4})', p.stdout + p.stderr)]
c = collections.Counter()
for pc in pcs:
    if pc < 0x4000: c['<kernel %02xxx>' % (pc >> 8)] += 1
    else:
        i = bisect.bisect_right(addrs, pc) - 1
        c[syms[i][1] if i >= 0 else '?'] += 1
tot = sum(c.values())
if os.environ.get('SEQ'):
    # per loop iteration: samples between two vblank waits, and what ran in the long ones
    names = []
    for pc in pcs:
        if pc < 0x4000: names.append('<kernel>')
        else:
            i = bisect.bisect_right(addrs, pc) - 1
            names.append(syms[i][1] if i >= 0 else '?')
    waiting = [n.startswith('_gfx_wait_vblank') for n in names]
    runs, cur = [], []
    for w, n in zip(waiting, names):
        if w:
            if cur: runs.append(cur)
            cur = []
        else: cur.append(n)
    lens = collections.Counter(len(r) for r in runs)
    print('iteration lengths (samples of 5000 T, a frame is ~33):', sorted(lens.items()))
    long = collections.Counter()
    for r in runs:
        if len(r) > 30:
            for n in r: long[n] += 1
    for k, v in long.most_common(15): print('   long: %4d %s' % (v, k))
for k, v in c.most_common(30): print('%5.1f%%  %s' % (100.0 * v / tot, k))
