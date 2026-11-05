#!/usr/bin/env python3
"""Pack SDK-exported tiles and metadata from git-tracked assets/chunks/*.tmx."""
from tmx_helpers import *
from check_chunks import validate
a=options(); normal=[Map(p,(18,8)) for p in sorted((ROOT/'assets/chunks').glob('chunk[0-9][0-9].tmx'))]
if not 1 <= len(normal) <= 254: raise ValueError('expected 1..254 procedural templates')
if any(m.props.get('tier') not in (0,1,2) for m in normal): raise ValueError('chunk tier must be 0, 1 or 2')
if not any(m.props['tier'] == 0 for m in normal): raise ValueError('at least one tier-0 chunk is required')
station=Map(ROOT/'assets/chunks/station.tmx',(18,8)); start=Map(ROOT/'assets/chunks/start.tmx',(18,8))
for template in normal+[station,start]: validate(template)
parts=[m.tiles(a) for m in normal+[station,start]]
if any(v>15 for data in parts for v in data): raise ValueError('unknown procedural cell')
write(a.out/'chunks.ztm',b''.join(parts))
write(a.out/'map_data.c','#include "game.h"\nconst uint8_t CHUNK_TIERS[] = { '+', '.join(str(m.props['tier']) for m in normal)+' };\nconst uint8_t CHUNK_COUNT = '+str(len(normal))+';\n')
