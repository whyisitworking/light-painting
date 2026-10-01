"""Proposes routes for layout.py: routes the nets in many orders, keeps the
cheapest that routes everything, checks it and draws it to a PNG. Prints the
wires as Python, for layout.py to keep.

    python3 tools/board/explore.py out.png [tries] ["option=value, ..."]

The options are layout.DEFAULTS's, e.g. "ml_turned=True, sd=4".
"""

import ast
import random
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402

import draw  # noqa: E402
import layout  # noqa: E402
from check import check  # noqa: E402
from model import hole_name  # noqa: E402
from route import route, terminals_by_net  # noqa: E402


def parse(text):
    """ "a=1, b='GND'" as a dict, each value a Python literal"""
    options = {}
    for item in filter(None, (i.strip() for i in text.split(","))):
        key, value = item.split("=", 1)
        options[key.strip()] = ast.literal_eval(value.strip())
    return options


def main():
    out = sys.argv[1]
    tries = int(sys.argv[2]) if len(sys.argv) > 2 else 200
    options = parse(sys.argv[3] if len(sys.argv) > 3 else "")
    board = layout.board(routed=False, **options)
    nets = list(terminals_by_net(board))
    best = None
    rng = random.Random(1)
    failures = {}
    for i in range(tries):
        order = nets[:] if i == 0 else rng.sample(nets, len(nets))
        wires, result = route(board, order)
        if wires is None:
            failures[result] = failures.get(result, 0) + 1
            continue
        if best is None or result < best[1] - 1e-9:
            best = (wires, result, order)
    if best is None:
        print("nothing routed; failures by net:", failures)
        return 1
    board.wires = best[0]
    problems = check(board)
    print(f"cost {best[1]:.1f}, order {best[2]}")
    print(draw.stats(board))
    print("problems:", problems or "none")
    for w in board.wires[len(layout.FIXED):]:
        names = ", ".join(f'"{hole_name(h)}"' for h in w.points)
        print(f'    w("{w.net}", "{w.face}", {names}),')
    fig, ax = plt.subplots(figsize=(8, 10))
    draw.grid(ax, draw.View())
    draw.parts(ax, board, draw.View())
    draw.rails(ax, board, draw.View())
    draw.wires(ax, board, draw.View())
    fig.savefig(out, dpi=130, bbox_inches="tight")
    return 0 if not problems else 1


if __name__ == "__main__":
    sys.exit(main())
