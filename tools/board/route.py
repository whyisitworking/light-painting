"""A maze router for the perfboard, to propose routes: A* over both faces,
under the same rules check.py proves. Each net grows as a tree from its rail
(or first pin), joining the nearest pin next. Straight runs are cheap, bends
and jumpers cost more, so what it finds reads simply.

It proposes; the routes a layout keeps are written into it as data, and
check.py proves them.
"""

import heapq
import itertools

from model import COLS, ROWS, Wire, bar_margin, body_holes, steps

STEP = 1.0
BEND = 3.0
# A jumper's two bends through the board, and a longer jumper's wire
VIA = 6.0
TOP_STEP = 1.2
# A wire lying along one on the other face: harmless, but in a drawing of
# both faces one hides the other
OVERLAP = 4.0

DIRS = [(1, 0), (-1, 0), (0, 1), (0, -1)]


class Router:
    def __init__(self, board):
        self.board = board
        self.below = {}
        self.leads = set()
        self.keepout = set()
        self.top_used = set()
        for part in board.parts:
            self.keepout |= body_holes(part.body)
            for pin in part.pins:
                self.below[pin.hole] = pin.net or f"nc:{part.ref}.{pin.name}"
                self.leads.add(pin.hole)
        for net, (row, c0, c1) in board.rails.items():
            for c in range(c0, c1 + 1):
                self.below[(c, row)] = net
            for h in bar_margin(row, c0, c1):
                self.below.setdefault(h, net)
        # Underside holes a wire runs over, not only pads and rails
        self.below_wired = set()
        self.wires = []
        for wire in board.wires:
            self.commit_wire(wire)
        self.cost = 0.0

    def commit_wire(self, wire):
        self.wires.append(wire)
        holes = steps(wire.points)
        if wire.face == "B":
            for h in holes:
                self.below[h] = wire.net
            self.below_wired |= set(holes)
        else:
            for h in (holes[0], holes[-1]):
                self.below[h] = wire.net
            self.top_used |= set(holes)

    def free_below(self, h, net):
        return self.below.get(h, net) == net

    def free_top(self, h):
        return (h not in self.keepout and h not in self.leads
                and h not in self.top_used)


    def connect(self, net, target, sources):
        """Cheapest path from any of sources to target, as a list of
        (hole, face); None if there is none"""
        def guess(h):
            return abs(h[0] - target[0]) + abs(h[1] - target[1])

        counter = itertools.count()
        frontier = []
        best = {}
        came = {}
        for h in sources:
            state = (h, "B", None)
            best[state] = 0.0
            heapq.heappush(frontier, (guess(h), next(counter), 0.0, state))
        while frontier:
            _, _, cost, state = heapq.heappop(frontier)
            if cost > best.get(state, float("inf")):
                continue
            h, face, direction = state
            if h == target and face == "B":
                path = [state]
                while path[-1] in came:
                    path.append(came[path[-1]])
                return [(s[0], s[1]) for s in reversed(path)], cost
            moves = []
            for d in DIRS:
                n = (h[0] + d[0], h[1] + d[1])
                if not (1 <= n[0] <= COLS and 1 <= n[1] <= ROWS):
                    continue
                if face == "B" and not self.free_below(n, net):
                    continue
                if face == "T" and not self.free_top(n):
                    continue
                # A jumper runs straight: seen from the top, bent ones look
                # untidy
                if face == "T" and direction is not None and d != direction:
                    continue
                step = STEP if face == "B" else TOP_STEP
                if face == "T" and n in self.below_wired or \
                        face == "B" and n in self.top_used:
                    step += OVERLAP
                bend = BEND if direction is not None and d != direction \
                    else 0.0
                moves.append(((n, face, d), step + bend))
            # A jumper end: through the board, free on both faces
            if self.free_top(h) and self.free_below(h, net):
                other = "T" if face == "B" else "B"
                # A jumper must leave its end: no end straight after one
                if came.get(state, (None, face))[1] == face:
                    moves.append(((h, other, None), VIA))
            for new_state, extra in moves:
                new_cost = cost + extra
                if new_cost < best.get(new_state, float("inf")):
                    best[new_state] = new_cost
                    came[new_state] = state
                    heapq.heappush(frontier, (new_cost + guess(new_state[0]),
                                              next(counter), new_cost,
                                              new_state))
        return None

    def add_path(self, net, path):
        """Turns a path into wires, one per stretch on a face"""
        wires = []
        run = [path[0]]
        for item in path[1:]:
            if item[1] != run[-1][1]:
                wires.append((run[-1][1], [h for h, _ in run]))
                run = [item]
            else:
                run.append(item)
        wires.append((run[-1][1], [h for h, _ in run]))
        added = []
        for face, holes in wires:
            if face == "B" and len(holes) == 1:
                continue
            wire = Wire(net=net, face=face, points=corners(holes))
            self.commit_wire(wire)
            added.append(wire)
        return added

    def pieces(self, net, terminals):
        """The net's joined pieces so far, each a set of underside holes"""
        parent = {}

        def find(x):
            while parent.setdefault(x, x) != x:
                x = parent[x]
            return x

        def union(a, b):
            parent[find(a)] = find(b)

        for t in terminals:
            find(t)
        rail = self.board.rails.get(net)
        if rail:
            row, c0, c1 = rail
            for c in range(c0, c1 + 1):
                union((c, row), (c0, row))
        for wire in self.wires:
            if wire.net != net:
                continue
            holes = steps(wire.points)
            ends = holes if wire.face == "B" else [holes[0], holes[-1]]
            for h in ends:
                union(h, ends[0])
        groups = {}
        for h in list(parent):
            groups.setdefault(find(h), set()).add(h)
        return list(groups.values())

    def route_net(self, net, terminals):
        """The net as a tree: the piece holding its rail, or first pin, then
        the nearest other piece joined to it, and so on"""
        added = []
        while True:
            pieces = self.pieces(net, terminals)
            if len(pieces) == 1:
                return added
            rail = self.board.rails.get(net)
            start = (rail[1], rail[0]) if rail else terminals[0]
            tree = next(p for p in pieces if start in p)
            others = [p for p in pieces if p is not tree]
            # The nearest piece next
            target = min((h for p in others for h in p),
                         key=lambda t: min(abs(t[0] - s[0]) + abs(t[1] - s[1])
                                           for s in tree))
            found = self.connect(net, target, tree)
            if found is None:
                return None
            path, cost = found
            self.cost += cost
            added += self.add_path(net, path)


def corners(holes):
    """The corners of a hole by hole path"""
    if len(holes) < 3:
        return list(holes)
    kept = [holes[0]]
    for a, b, c in zip(holes, holes[1:], holes[2:]):
        if (b[0] - a[0], b[1] - a[1]) != (c[0] - b[0], c[1] - b[1]):
            kept.append(b)
    kept.append(holes[-1])
    return kept


def terminals_by_net(board):
    nets = {}
    for _, pin in board.pins():
        if pin.net:
            nets.setdefault(pin.net, []).append(pin.hole)
    return nets


def route(board, order):
    """Routes the nets in order on a copy's worth of state; returns (wires,
    cost) or (None, net that failed)"""
    router = Router(board)
    nets = terminals_by_net(board)
    for net in order:
        if router.route_net(net, nets[net]) is None:
            return None, net
    return router.wires, router.cost
