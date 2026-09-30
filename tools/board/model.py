"""The perfboard's model: the grid, parts with pins and bodies, and wires.

Holes are (column, row), both from 1: column 1 is A, 27 is AA, row 1 the top
edge, as seen from the component side with the USB connector on the left.
A wire runs on one face, B (underside, bare wire over the pads) or T (top,
the component side, a jumper that bends through the board at both ends and
is soldered underneath there).
"""

from dataclasses import dataclass, field

COLS = 21
ROWS = 32


def col_name(c):
    """1 -> A ... 26 -> Z, 27 -> AA"""
    return chr(ord("A") + c - 1) if c <= 26 else "A" + chr(ord("A") + c - 27)


def col_number(name):
    if len(name) == 2:
        return 26 + ord(name[1]) - ord("A") + 1
    return ord(name) - ord("A") + 1


def hole(name):
    """'AA23' -> (27, 23)"""
    i = next(i for i, ch in enumerate(name) if ch.isdigit())
    return col_number(name[:i]), int(name[i:])


def hole_name(h):
    return f"{col_name(h[0])}{h[1]}"


@dataclass
class Pin:
    name: str
    hole: tuple
    # None: soldered but connected to nothing, e.g. an unused socket pin
    net: str | None


@dataclass
class Part:
    ref: str
    value: str
    pins: list
    # ("rect", c0, r0, c1, r1) or ("circle", c, r, radius), in holes. The
    # top face under it is kept clear of jumpers
    body: tuple | None = None
    label: str = ""
    # How it is drawn: "part", "axial", "radial"
    kind: str = "part"


@dataclass
class Wire:
    net: str
    face: str  # "B" or "T"
    points: list  # corner holes, (c, r), in order
    ref: str = ""


@dataclass
class Board:
    parts: list
    wires: list = field(default_factory=list)
    # Heavy bars on the underside: net -> (row, first column, last column)
    rails: dict = field(default_factory=dict)
    title: str = ""

    def pins(self):
        for part in self.parts:
            for pin in part.pins:
                yield part, pin


def steps(points):
    """Every hole along a polyline of corners, in order. Each leg is straight
    along a row or a column"""
    holes = [tuple(points[0])]
    for (c0, r0), (c1, r1) in zip(points, points[1:]):
        if c0 != c1 and r0 != r1:
            raise ValueError(f"diagonal leg {hole_name((c0, r0))}-"
                             f"{hole_name((c1, r1))}")
        dc = (c1 > c0) - (c1 < c0)
        dr = (r1 > r0) - (r1 < r0)
        c, r = c0, r0
        while (c, r) != (c1, r1):
            c, r = c + dc, r + dr
            holes.append((c, r))
    return holes


def bar_margin(row, c0, c1):
    """The holes a heavy bar comes close to without being soldered there:
    8 AWG is 3.3 mm thick on a 2.54 mm grid, so it reaches the pads beside
    it, and its cut ends overhang"""
    margin = {(c, r) for c in range(c0, c1 + 1) for r in (row - 1, row + 1)}
    margin |= {(c0 - 1, row), (c1 + 1, row)}
    return {(c, r) for c, r in margin if 1 <= c <= COLS and 1 <= r <= ROWS}


def body_holes(body):
    """The holes a body covers on the top face"""
    if body is None:
        return set()
    covered = set()
    if body[0] == "rect":
        _, c0, r0, c1, r1 = body
        for c in range(1, COLS + 1):
            for r in range(1, ROWS + 1):
                if c0 <= c <= c1 and r0 <= r <= r1:
                    covered.add((c, r))
    elif body[0] == "circle":
        _, cc, rc, radius = body
        for c in range(1, COLS + 1):
            for r in range(1, ROWS + 1):
                if (c - cc) ** 2 + (r - rc) ** 2 <= radius ** 2:
                    covered.add((c, r))
    return covered
