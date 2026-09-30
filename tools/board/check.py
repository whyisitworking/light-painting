"""Proves a board is buildable and wired as its nets say.

The rules, from how isolated-pad perfboard is built:
  - A bare underside (B) wire touches every pad it lies over: every hole
    along it belongs to its net, and no two nets may share an underside hole
    (a crossing is a shared hole).
  - A top (T) jumper touches nothing but its two ends, which bend through
    the board and are soldered to their pads: the ends belong to its net.
    Along the way it may not pass over a component lead, another jumper's
    end, another jumper, or anything under a part's body.
  - A part sits on the board, clear of the other parts: no other part's
    body or lead under its body.
  - A heavy bar reaches the pads beside it and past its ends: those are
    its own net's or nobody's.
  - Every pin of a net, and its rail if it has one, end up joined, and
    every wire is part of that: no net without pins or a rail, no wire
    joined to none of its net's.
"""

from model import COLS, ROWS, bar_margin, body_holes, hole_name, steps


def _on_board(h):
    return 1 <= h[0] <= COLS and 1 <= h[1] <= ROWS


def _body_on_board(body):
    if body is None:
        return True
    if body[0] == "rect":
        _, c0, r0, c1, r1 = body
    else:
        _, c, r, radius = body
        c0, r0, c1, r1 = c - radius, r - radius, c + radius, r + radius
    return 0.5 <= c0 and c1 <= COLS + 0.5 and 0.5 <= r0 and r1 <= ROWS + 0.5


def check(board):
    """A list of problems, empty if the board is right"""
    problems = []
    below = {}  # hole -> net owning the underside pad
    leads = {}  # hole -> "ref.pin", a lead sticking up on top
    top_ends = {}  # hole -> wire ref
    top_runs = {}  # hole -> wire ref, a jumper passing over
    keepout = {}
    parent = {}

    def find(x):
        while parent.setdefault(x, x) != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    def union(a, b):
        parent[find(a)] = find(b)

    def claim(h, net, what):
        if not _on_board(h):
            problems.append(f"{what}: {hole_name(h)} is off the board")
            return
        owner = below.get(h)
        if owner is not None and owner != net:
            problems.append(f"{what}: {hole_name(h)} is already {owner}")
        below.setdefault(h, net)

    for part in board.parts:
        if not _body_on_board(part.body):
            problems.append(f"{part.ref}: its body is off the board")
        for h in body_holes(part.body):
            if h in keepout:
                problems.append(f"{part.ref}: its body is over "
                                f"{keepout[h]}'s at {hole_name(h)}")
            keepout.setdefault(h, part.ref)
        for pin in part.pins:
            net = pin.net or f"(unused {part.ref}.{pin.name})"
            claim(pin.hole, net, f"{part.ref}.{pin.name}")
            if pin.hole in leads:
                problems.append(f"{part.ref}.{pin.name}: {hole_name(pin.hole)}"
                                f" already holds {leads[pin.hole]}")
            leads[pin.hole] = f"{part.ref}.{pin.name}"

    for part in board.parts:
        for h in body_holes(part.body):
            owner = leads.get(h, part.ref + ".")
            if not owner.startswith(part.ref + "."):
                problems.append(f"{part.ref}: its body is over {owner} at "
                                f"{hole_name(h)}")

    for net, (row, c0, c1) in board.rails.items():
        holes = [(c, row) for c in range(c0, c1 + 1)]
        for h in holes:
            claim(h, net, f"rail {net}")
            union(h, holes[0])
        # The pads beside it and past its ends: its own net's or nobody's
        for h in bar_margin(row, c0, c1):
            claim(h, net, f"rail {net}'s edge")

    for wire in board.wires:
        what = f"{wire.ref or 'wire'} ({wire.net}, {wire.face})"
        if not wire.points:
            problems.append(f"{what}: no holes")
            continue
        try:
            holes = steps(wire.points)
        except ValueError as error:
            problems.append(f"{what}: {error}")
            continue
        if len(set(holes)) != len(holes):
            problems.append(f"{what}: runs over itself")
        if wire.face == "B":
            for h in holes:
                claim(h, wire.net, what)
                union(h, holes[0])
        elif wire.face == "T":
            if len(holes) < 2:
                problems.append(f"{what}: a jumper needs two ends")
                continue
            for h in holes:
                if h in keepout:
                    problems.append(f"{what}: {hole_name(h)} is under "
                                    f"{keepout[h]}")
            for h in (holes[0], holes[-1]):
                claim(h, wire.net, what)
                if h in leads:
                    problems.append(f"{what}: end {hole_name(h)} is "
                                    f"{leads[h]}'s hole")
                if h in top_ends or h in top_runs:
                    problems.append(f"{what}: end {hole_name(h)} is taken "
                                    f"on top")
                top_ends[h] = what
            for h in holes[1:-1]:
                if h in leads:
                    problems.append(f"{what}: passes over {leads[h]} at "
                                    f"{hole_name(h)}")
                if h in top_ends or h in top_runs:
                    problems.append(f"{what}: crosses another jumper at "
                                    f"{hole_name(h)}")
                top_runs[h] = what
            union(holes[0], holes[-1])
        else:
            problems.append(f"{what}: unknown face")

    # Every pin and rail of a net joined
    terminals = {}
    for part, pin in board.pins():
        if pin.net:
            terminals.setdefault(pin.net, []).append(
                (pin.hole, f"{part.ref}.{pin.name}"))
    for net, (row, c0, _) in board.rails.items():
        terminals.setdefault(net, []).append(((c0, row), f"rail {net}"))
    for net, ends in terminals.items():
        groups = {}
        for h, name in ends:
            groups.setdefault(find(h), []).append(name)
        if len(groups) > 1:
            parts = [", ".join(names) for names in groups.values()]
            problems.append(f"net {net} is in {len(groups)} pieces: "
                            + " | ".join(parts))

    # Every wire joined to its net's pins or rail: none loose, none on a net
    # nothing else carries
    for wire in board.wires:
        what = f"{wire.ref or 'wire'} ({wire.net}, {wire.face})"
        if wire.net not in terminals:
            problems.append(f"{what}: no pin or rail carries {wire.net}")
        elif wire.points and all(_on_board(tuple(h)) for h in wire.points):
            roots = {find(h) for h, _ in terminals[wire.net]}
            if find(tuple(wire.points[0])) not in roots:
                problems.append(f"{what}: joined to none of {wire.net}'s "
                                f"pins")

    return problems
