# Board

The perfboard as data, a checker for it, and the drawings made from it: [`docs/hardware/wiring.pdf`](../../docs/hardware/wiring.pdf) (placement, the wiring seen from underneath with the jumpers dotted, the connection list, the wire runs, assembly order, schematics). The board is a 2.54 mm isolated-pad perfboard, 21 columns (A to U) by 32 rows (53 x 81 mm); `COLS` and `ROWS` in `model.py`. The drawings are made, not measured: the KY-040's header order in particular is assumed (GND, +, SW, DT, CLK) and is checked when it arrives.

Needs Python 3 and matplotlib (`pip3 install matplotlib`).

## Build and check

```bash
python3 tools/board/build.py
```

This writes `docs/hardware/wiring.pdf`. It checks the board first and refuses to write anything if a check fails. The same input gives the same file byte for byte, so a changed PDF in a diff means a changed board or drawing.

```bash
python3 tools/board/build.py --check
```

runs the checks alone.

## What is checked

`check.py` proves what the drawings show, under the rules of an isolated-pad board:

- A bare wire underneath touches every pad it passes over, so it claims every hole on its way; two nets may not claim the same hole.
- A jumper on top touches only its two ends. It may not pass over a lead, another jumper or a part's body.
- Every net is one connected piece (its pins, its wires and its bar), and no two nets touch.

## Files

- `model.py`: holes, parts, pins, wires, and the board.
- `layout.py`: this board: where each part sits, what each pin carries, and every wire. `FIXED` are laid by hand; `ROUTED` are the microphones' wires, as `explore.py` proposed them.
- `check.py`: the rules above.
- `draw.py`, `build.py`: the drawings and the PDF.
- `route.py`, `explore.py`: a maze router that proposes wires, for when the layout changes. Its jumpers run straight, as they are seen from the top.

## Changing the layout

Edit `layout.py`. To have wires proposed, route everything except `FIXED` in many orders and keep the cheapest:

```bash
python3 tools/board/explore.py out.png 200
```

It prints the wires as Python, to paste into `ROUTED`, and draws them to `out.png`. A third argument tries other options, e.g. `"ml_turned=True, sd=4"` (see `DEFAULTS` in `layout.py`). Then build and look at the pages: the checker proves the connections, not that the result reads well.

If a GPIO moves, change `app/config.h` to match.
