"""Shaft reachability validation, using procedural tile IDs from cells.tsx.

Check random rock both solid and empty. Every reachable cell must retain a path
through the bottom, using the original fall/walk/jump/ladder movement model.
"""
W, H = 18, 8


def check(rows, rock):
    g = [[(rock if c == 2 else c) for c in r] for r in rows]

    def solid(x, y):
        if x < 0 or x >= W:
            return True
        if y < 0:
            return True
        if y >= H:
            return False
        return g[y][x] == 1

    def ladder(x, y):
        return 0 <= x < W and 0 <= y < H and g[y][x] == 4

    def floor(x, y):
        # can stand in (x, y)? looks at the cell below
        if ladder(x, y):
            return True
        if y + 1 >= H:
            return False
        c = g[y + 1][x]
        return c in (1, 3) or (c == 4 and not ladder(x, y))

    EXIT = 'exit'

    def moves(x, y):
        if not floor(x, y):
            if y + 1 >= H:
                return [EXIT]
            return [(x, y + 1)]
        out = []
        for dx in (-1, 1):
            if not solid(x + dx, y):
                out.append((x + dx, y))
        if ladder(x, y) or ladder(x, y + 1):
            if not solid(x, y + 1):
                out.append((x, y + 1) if y + 1 < H else EXIT)
            if ladder(x, y) and not solid(x, y - 1) and y > 0:
                out.append((x, y - 1))
        for dy in range(1, 4):
            if solid(x, y - dy):
                break
            for sgn in (-1, 1):
                for dx in range(0, 4):
                    cx = x + sgn * dx
                    if any(solid(x + sgn * k, y - dy) for k in range(0, dx + 1)):
                        break
                    if y - dy >= 0:
                        out.append((cx, y - dy))
        return out

    # Build the movement graph once. Forward reachability finds all cells a
    # top-row entry can reach; reverse reachability finds all cells with an exit.
    # This preserves the original per-entry/per-cell check without repeating it.
    graph = {(x, y): moves(x, y) for y in range(H) for x in range(W) if not solid(x, y)}
    graph[EXIT] = []
    reverse = {cell: [] for cell in graph}
    for cell, targets in graph.items():
        for target in targets:
            reverse[target].append(cell)

    def reach(edges, starts):
        seen = set(starts)
        todo = list(starts)
        while todo:
            for target in edges[todo.pop()]:
                if target not in seen:
                    seen.add(target)
                    todo.append(target)
        return seen

    entries = [(x, 0) for x in range(W) if not solid(x, 0)]
    problems = ['row 0 blocked at %d' % x for x in range(W) if solid(x, 0)]
    traps = reach(graph, entries) - reach(reverse, [EXIT])
    problems += ['trap at %s' % (cell,) for cell in sorted(traps)]
    return problems



def validate(template):
    cells = [g - 1 if g else 0 for g in template.gids]
    if any(c > 15 for c in cells):
        raise ValueError(f'{template.path}: unknown procedural cell')
    rows = [cells[i:i + W] for i in range(0, len(cells), W)]
    for rock in (1, 0):
        problems = check(rows, rock)
        if problems:
            raise ValueError(f'{template.path}: random rock={rock}: {sorted(set(problems))[:4]}')
