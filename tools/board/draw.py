"""Draws a board with matplotlib: the grid, the parts, and the wires of one
face or both. Seen from the component side, or from underneath (columns
mirrored), as the solderer holds it."""

from matplotlib.patches import Circle, FancyBboxPatch, Rectangle

from model import COLS, ROWS, col_name, steps

NET_COLORS = {
    "5V": "#c62828", "GND": "#263238", "3V3": "#e08a00", "VSYS": "#ad1457",
    "SCK": "#00838f", "WS": "#1565c0", "SD": "#6a1b9a",
    "ENC_A": "#2e7d32", "ENC_B": "#827717", "ENC_SW": "#c2185b",
    "LED_GPIO": "#8d6e63", "LED_IN": "#5d4037", "LED_OUT": "#d84315",
    "DATA": "#bf360c",
}
BOARD = "#f6f1e7"
BODY = {"part": "#dfe8f0", "axial": "#eadbb8", "radial": "#e8dcc4",
        "pad": "#ffffff"}


def color(net):
    return NET_COLORS.get(net, "#9e9e9e")


class View:
    """Maps holes to drawing coordinates: x right, y down, one hole = 1"""

    def __init__(self, mirrored=False):
        self.mirrored = mirrored

    def x(self, c):
        return COLS + 1 - c if self.mirrored else c

    def xy(self, h):
        return self.x(h[0]), h[1]


def grid(ax, view):
    ax.add_patch(Rectangle((0.5, 0.5), COLS, ROWS, facecolor=BOARD,
                           edgecolor="#8a9aa4", linewidth=1.2, zorder=0))
    xs, ys = zip(*[(c, r) for c in range(1, COLS + 1)
                   for r in range(1, ROWS + 1)])
    ax.scatter(xs, ys, s=2.5, color="#c8c0b0", zorder=1, linewidths=0)
    for c in range(1, COLS + 1):
        for y, va in ((0.1, "bottom"), (ROWS + 0.9, "top")):
            ax.text(view.x(c), y, col_name(c), ha="center", va=va,
                    fontsize=5.5, color="#5b6b75")
    for r in range(1, ROWS + 1):
        for x, ha in ((0.1, "right"), (COLS + 0.9, "left")):
            ax.text(x, r, str(r), ha=ha, va="center", fontsize=5.5,
                    color="#5b6b75")
    ax.set_xlim(-0.8, COLS + 1.8)
    ax.set_ylim(ROWS + 1.8, -0.8)
    ax.set_aspect("equal")
    ax.axis("off")


def parts(ax, board, view, labels=True):
    for part in board.parts:
        body = part.body
        fill = BODY.get(part.kind, BODY["part"])
        if body and body[0] == "rect":
            _, c0, r0, c1, r1 = body
            x0, x1 = sorted((view.x(c0), view.x(c1)))
            ax.add_patch(FancyBboxPatch(
                (x0, r0), x1 - x0, r1 - r0, boxstyle="round,pad=0,rounding_size=0.25",
                facecolor=fill, edgecolor="#7d8b93", linewidth=0.8, zorder=2,
                alpha=0.9))
            if labels:
                ax.text((x0 + x1) / 2, (r0 + r1) / 2, part.label or part.ref,
                        ha="center", va="center", fontsize=6 if part.kind ==
                        "part" else 4.5, color="#1b2b35", zorder=6)
        elif body and body[0] == "circle":
            _, c, r, radius = body
            ax.add_patch(Circle((view.x(c), r), radius, facecolor=fill,
                                edgecolor="#7d8b93", linewidth=0.8, zorder=2,
                                alpha=0.9))
            if labels:
                ax.text(view.x(c), r, part.label or part.ref, ha="center",
                        va="center", fontsize=6, color="#1b2b35", zorder=6)
        for pin in part.pins:
            x, y = view.xy(pin.hole)
            if pin.net:
                ax.add_patch(Circle((x, y), 0.22, facecolor="white",
                                    edgecolor=color(pin.net), linewidth=1.1,
                                    zorder=7))
            else:
                ax.scatter([x], [y], marker="x", s=10, color="#9e9e9e",
                           linewidths=0.8, zorder=7)


def rails(ax, board, view):
    for net, (row, c0, c1) in board.rails.items():
        x0, x1 = sorted((view.x(c0), view.x(c1)))
        ax.add_patch(Rectangle((x0 - 0.3, row - 0.3), x1 - x0 + 0.6, 0.6,
                               facecolor=color(net), edgecolor="none",
                               zorder=3, alpha=0.85))


def wires(ax, board, view, faces=("B", "T")):
    for wire in board.wires:
        if wire.face not in faces:
            continue
        xs, ys = zip(*[view.xy(h) for h in wire.points])
        if wire.face == "B":
            ax.plot(xs, ys, color=color(wire.net), linewidth=1.6,
                    solid_capstyle="round", zorder=4)
        else:
            # Both faces in one drawing: the jumper, on the other face,
            # dotted
            ax.plot(xs, ys, color=color(wire.net), linewidth=2.2,
                    linestyle=(0, (0.1, 1.6)) if len(faces) > 1 else "-",
                    dash_capstyle="round", solid_capstyle="round",
                    zorder=5)
            for h in (wire.points[0], wire.points[-1]):
                x, y = view.xy(h)
                ax.scatter([x], [y], marker="D", s=18, facecolor="white",
                           edgecolor=color(wire.net), linewidths=1.0,
                           zorder=8)


def stats(board):
    b = [w for w in board.wires if w.face == "B"]
    t = [w for w in board.wires if w.face == "T"]
    length = sum(len(steps(w.points)) - 1 for w in board.wires)
    bends = sum(len(w.points) - 2 for w in board.wires)
    return (f"{len(b)} underside wires, {len(t)} jumpers, {length} holes of "
            f"wire, {bends} bends")
