"""Checks the board and draws its wiring document.

    python3 tools/board/build.py [out.pdf]    # docs/hardware/wiring.pdf
    python3 tools/board/build.py --check      # the checks alone

Every page comes from layout.py; check.py must find nothing wrong first.
"""

import sys
import textwrap
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.backends.backend_pdf import PdfPages  # noqa: E402
from matplotlib.patches import Circle, Polygon, Rectangle  # noqa: E402

import draw  # noqa: E402
import layout  # noqa: E402
from check import check  # noqa: E402
from model import COLS, ROWS, col_name, hole_name, steps  # noqa: E402

A4 = (8.27, 11.69)
INK = "#1b2b35"
MUTED = "#5b6b75"
FOOTER = "RP2350 music visualiser | 2.54 mm isolated-pad perfboard | Rev 3"

NET_ORDER = ["5V", "GND", "3V3", "VSYS", "SCK", "WS", "SD", "ENC_A", "ENC_B",
             "ENC_SW", "LED_GPIO", "LED_IN", "LED_OUT", "DATA"]
NET_NAMES = {"ENC_A": "ENC_A (CLK)", "ENC_B": "ENC_B (DT)",
             "ENC_SW": "ENC_SW (SW)"}


class Document:
    def __init__(self, pdf):
        self.pdf = pdf
        self.number = 0

    def page(self, title, subtitle=""):
        fig = plt.figure(figsize=A4)
        self.number += 1
        fig.text(0.07, 0.955, title, fontsize=17, weight="bold", color=INK,
                 va="top")
        for i, line in enumerate(textwrap.wrap(subtitle, 110)):
            fig.text(0.07, 0.918 - i * 0.016, line, fontsize=8.5,
                     color=MUTED, va="top")
        fig.add_artist(plt.Line2D([0.07, 0.93], [0.045, 0.045],
                                  color="#d0d8de", linewidth=1.5))
        fig.text(0.07, 0.03, FOOTER, fontsize=7, color=MUTED)
        fig.text(0.93, 0.03, str(self.number), fontsize=7, color=MUTED,
                 ha="right")
        return fig

    def save(self, fig):
        self.pdf.savefig(fig)
        plt.close(fig)


def paragraphs(fig, items, top, left=0.07, width=100, size=8.5, gap=0.012):
    """Wrapped paragraphs, (bold lead, text) or text, from top down; returns
    where they end"""
    y = top
    for item in items:
        lead, text = item if isinstance(item, tuple) else ("", item)
        body = (lead + " " if lead else "") + text
        lines = textwrap.wrap(body, width)
        for i, line in enumerate(lines):
            if i == 0 and lead:
                bold = fig.text(left, y, lead, fontsize=size, weight="bold",
                                color=INK, va="top")
                # The rest of the first line right after the bold lead
                box = bold.get_window_extent(
                    fig.canvas.get_renderer()).transformed(
                        fig.transFigure.inverted())
                fig.text(box.x1, y, line[len(lead):], fontsize=size,
                         color=INK, va="top")
            else:
                fig.text(left, y, line, fontsize=size, color=INK, va="top")
            y -= size * 0.0019
        y -= gap
    return y


def table(fig, header, rows, top, columns, size=8, width_chars=None,
          bottom=0.08):
    """A table from top down, its columns at the given x; returns (end, rows
    left over for the next page)"""
    y = top
    fig.add_artist(Rectangle((0.07, y - 0.022), 0.86, 0.026,
                             facecolor="#e7eef2", edgecolor="none",
                             transform=fig.transFigure))
    for x, text in zip(columns, header):
        fig.text(x, y - 0.009, text, fontsize=size, color=INK, va="center",
                 weight="bold")
    y -= 0.03
    for index, row in enumerate(rows):
        cells = [textwrap.wrap(str(c), w) or [""] for c, w in
                 zip(row, width_chars or [200] * len(row))]
        height = max(len(c) for c in cells) * size * 0.0017 + 0.008
        if y - height < bottom:
            return y, rows[index:]
        for x, lines in zip(columns, cells):
            for i, line in enumerate(lines):
                fig.text(x, y - 0.004 - i * size * 0.0017, line, fontsize=size,
                         color=INK, va="top")
        y -= height
        fig.add_artist(plt.Line2D([0.07, 0.93], [y + 0.003, y + 0.003],
                                  color="#e3e8eb", linewidth=0.6))
    return y, []


def board_axes(fig):
    return fig.add_axes([0.06, 0.16, 0.88, 0.74])


PIN_LABEL = {"MOD": 0.55, "ML": 0.55, "MR": 0.55}


def pin_labels(ax, board, view):
    """Each pin's name beside it"""
    for part in board.parts:
        for pin in part.pins:
            x, y = view.xy(pin.hole)
            if part.ref == "MOD":
                dy = -0.55 if pin.hole[1] == 9 else 0.62
                text = pin.name.replace("GP", "")
                ax.text(x, y + dy, text, fontsize=4.6, ha="center",
                        va="center", color=INK, zorder=9)
            elif part.ref in ("ML", "MR"):
                top = pin.hole[1] == min(p.hole[1] for p in part.pins)
                ax.text(x, y + (-0.52 if top else 0.52), pin.name,
                        fontsize=4.2, ha="center", va="center", color=INK,
                        zorder=9)
            elif part.ref == "ENC":
                ax.text(x, y + 0.6, pin.name, fontsize=4.6, ha="center",
                        va="center", color=INK, zorder=9)
            elif part.ref == "U1":
                # Inside the body: the resistors crowd its left side
                left = pin.hole[0] == min(p.hole[0] for p in part.pins)
                dx = 0.45 if left != view.mirrored else -0.45
                ax.text(x + dx, y, pin.name, fontsize=4.4,
                        ha="left" if dx > 0 else "right", va="center",
                        color=INK, zorder=9)
            elif part.ref in ("D1", "C1"):
                ax.text(x + 0.45, y, pin.name, fontsize=4.6, ha="left",
                        va="center", color=INK, zorder=9)


def legend(fig, nets, y=0.125):
    for i, net in enumerate(nets):
        col, row = i % 5, i // 5
        x = 0.08 + col * 0.175
        yy = y - row * 0.018
        fig.add_artist(plt.Line2D([x, x + 0.025], [yy, yy],
                                  color=draw.color(net), linewidth=2.5))
        fig.text(x + 0.032, yy, NET_NAMES.get(net, net), fontsize=7,
                 va="center", color=INK)


def wire_ids(board):
    """T1..., B1...: jumpers first, as they go in first"""
    ids = {}
    for face, prefix in (("T", "T"), ("B", "B")):
        n = 0
        for wire in board.wires:
            if wire.face == face:
                n += 1
                ids[id(wire)] = f"{prefix}{n}"
    return ids


def nets_of(board):
    nets = {}
    for part, pin in board.pins():
        if pin.net:
            nets.setdefault(pin.net, []).append(
                f"{part.ref}.{pin.name} {hole_name(pin.hole)}")
    for net, (row, c0, c1) in board.rails.items():
        nets.setdefault(net, []).insert(
            0, f"8 AWG bar {hole_name((c0, row))}-{hole_name((c1, row))}")
    return nets


def at(board, ref, pin):
    """The hole of a part's pin, by name"""
    return next(hole_name(p.hole) for part, p in board.pins()
                if part.ref == ref and p.name == pin)


def gpio(board, net):
    """The module pin that carries a net"""
    return next(p.name for part, p in board.pins()
                if part.ref == "MOD" and p.net == net)


def pins_of(board, net):
    return [f"{part.ref}.{pin.name} {hole_name(pin.hole)}"
            for part, pin in board.pins() if pin.net == net]


def bars(board):
    """Where the two bars run, e.g. "GND at row 28 and 5 V at row 31, B to
    T" """
    (g, c0, c1), (f, _, _) = board.rails["GND"], board.rails["5V"]
    return (f"GND at row {g} and 5 V at row {f}, {col_name(c0)} to "
            f"{col_name(c1)}")


def bar_leads(board):
    """", and solder D1.A to the bar in its hole" for any lead in a bar's
    hole, else "" """
    holes = {(c, row) for row, c0, c1 in board.rails.values()
             for c in range(c0, c1 + 1)}
    leads = [f"{part.ref}.{pin.name}" for part, pin in board.pins()
             if pin.hole in holes]
    if not leads:
        return ""
    return f", and solder {', '.join(leads)} to the bar in its hole"


def size(board):
    return (f"{COLS} x {ROWS} holes (A-{col_name(COLS)}), "
            f"{COLS * 2.54:.0f} x {ROWS * 2.54:.0f} mm")


def mic_note(board, ref):
    """e.g. "left channel (L/R to GND), SCK, WS, L/R along the top" """
    part = next(p for p in board.parts if p.ref == ref)
    pins = {p.name: p for p in part.pins}
    channel = "left" if pins["L/R"].net == "GND" else "right"
    side = "top" if pins["SCK"].hole[1] < pins["SD"].hole[1] else "bottom"
    turned = ", turned half round" if side == "bottom" else ""
    return (f"{channel} channel (L/R to {pins['L/R'].net}){turned}: SCK, WS, "
            f"L/R along the {side}")


def summary_page(doc, board):
    fig = doc.page("Connections and solder points",
                   f"{size(board)}, seen from the component side: LCD "
                   "facing you, USB to the left.")
    gp = {n: [p.name for part, p in board.pins()
              if part.ref == "MOD" and p.net == n] for n in NET_ORDER}
    rows = [
        ("Supply", "The 5 V supply and the strip's power to the two 8 AWG "
         f"bars underneath, under the encoder: {bars(board)}. D1 feeds "
         "the module's VSYS from the raw 5 V; U1 and the strip run on the "
         "raw 5 V; the microphones and the encoder on the module's 3.3 V."),
        ("Microphones", f"SCK on {gp['SCK'][0]}, WS on {gp['WS'][0]} (bit "
         f"clock first, word select next, as Raspberry Pi's I2S driver has "
         f"them), SD on {gp['SD'][0]}. Both share the three lines. Left mic: "
         f"{mic_note(board, 'ML')}. Right mic: {mic_note(board, 'MR')}. "
         "The firmware sums them, so which is which does not matter. No SD "
         "pull-down: the pin's bus keeper holds the line."),
        ("Encoder", f"KY-040 header in a row under the screen: GND, +, SW, "
         f"DT, CLK from the left, each signal straight under its GPIO: SW "
         f"{gp['ENC_SW'][0]}, DT {gp['ENC_B'][0]}, CLK {gp['ENC_A'][0]}. No "
         "board pull-ups: the module's and the pins' own hold the lines."),
        ("LED output", f"{gp['LED_GPIO'][0]} through R1 (1k) to U1 channel "
         "1 (74HCT125, 3.3 V to 5 V), R2 (10k) holding its input low "
         "through reset, R3 (330R) to the strip's DIN from the DATA end."),
        ("Checked", "Every net joined, no two nets on one hole or crossing "
         "on one face, no bare underside wire over another net's pad, no "
         "jumper over a lead or under a part (tools/board/check.py). A "
         "layout check, not a hardware test."),
        ("Before soldering", "Dry-fit the module's sockets and both "
         "microphones. Check the KY-040's header order against page 2 when "
         "it arrives: mounted otherwise, its wires change. The module's "
         "pinout is from Waveshare's schematic (RP2350-LCD-1.47-A, U4)."),
    ]
    y = paragraphs(fig, rows, 0.88)
    table_rows = [(NET_NAMES.get(net, net), ", ".join(pins))
                  for net, pins in sorted(nets_of(board).items(),
                                          key=lambda kv: NET_ORDER.index(
                                              kv[0]))
                  if net in ("SCK", "WS", "SD", "ENC_A", "ENC_B", "ENC_SW",
                             "LED_GPIO", "VSYS")]
    table(fig, ["Signal", "Solder points"], table_rows, y - 0.01,
          [0.08, 0.26], width_chars=[20, 95])
    doc.save(fig)


def placement_page(doc, board):
    fig = doc.page("Component placement and pin orientation",
                   "Component side, holes not millimetres. The screen, the "
                   "microphones and the knob centred on one column. Dashed: "
                   "the GND and 5 V bars, underneath the encoder.")
    ax = board_axes(fig)
    view = draw.View()
    draw.grid(ax, view)
    draw.parts(ax, board, view)
    pin_labels(ax, board, view)
    for net, (row, c0, c1) in board.rails.items():
        ax.plot([c0, c1], [row, row], color=draw.color(net), linewidth=1,
                linestyle=(0, (4, 3)), zorder=2)
    u = {n: at(board, "U1", str(n)) for n in range(1, 15)}
    encoder = ", ".join(f"{p.name} {hole_name(p.hole)}" for p in
                        next(x for x in board.parts if x.ref == "ENC").pins)
    paragraphs(fig, [
        ("U1:", f"notch up. Pin 1 = {u[1]}, pin 14 = {u[14]}. Channel 1: "
         f"enable {u[1]} (GND), input {u[2]}, output {u[3]}. C2 (100 nF) "
         f"straight across {u[14]} (pin 14, 5 V) and {u[13]} (pin 13, GND) "
         "underneath."),
        ("C1:", f"+ at {at(board, 'C1', '+')}, the striped - at "
         f"{at(board, 'C1', '-')}. D1: band (cathode) at "
         f"{at(board, 'D1', 'K')}, anode at {at(board, 'D1', 'A')}. KY-040: "
         f"header {encoder}, its board towards the bottom edge."),
    ], 0.13, size=8)
    doc.save(fig)


def wiring_page(doc, board):
    fig = doc.page("Wiring, seen from underneath",
                   "Columns mirrored, top edge still at the top. Solid: bare "
                   "wire over the pads underneath. Dotted: a jumper on the "
                   "component side, bent through and soldered at both ends "
                   "(diamonds).")
    view = draw.View(mirrored=True)
    ax = board_axes(fig)
    draw.grid(ax, view)
    draw.parts(ax, board, view)
    draw.rails(ax, board, view)
    draw.wires(ax, board, view)
    ids = wire_ids(board)
    for wire in board.wires:
        a, b = wire.points[0], wire.points[1]
        x, y = view.xy(((a[0] + b[0]) / 2, (a[1] + b[1]) / 2))
        ax.text(x, y, ids[id(wire)], fontsize=4, ha="center", va="center",
                color="white", zorder=10,
                bbox=dict(boxstyle="round,pad=0.12", facecolor=draw.color(
                    wire.net), edgecolor="none"))
    used = [n for n in NET_ORDER if any(w.net == n for w in board.wires)]
    legend(fig, used)
    doc.save(fig)


def connections_page(doc, board):
    fig = doc.page("Complete connection list",
                   "Each net: all its solder points joined, nothing else.")
    rows = [(NET_NAMES.get(net, net), "; ".join(pins))
            for net, pins in sorted(nets_of(board).items(),
                                    key=lambda kv: NET_ORDER.index(kv[0]))]
    y, _ = table(fig, ["Net", "Solder points"], rows, 0.89, [0.08, 0.24],
                 width_chars=[18, 86])
    u = {n: at(board, "U1", str(n)) for n in range(1, 15)}
    paragraphs(fig, [
        ("Unused U1 outputs:", f"pins 6 ({u[6]}), 8 ({u[8]}) and 11 "
         f"({u[11]}) stay open. Each unused channel's enable and input sit "
         "side by side and go to GND together."),
        ("C2:", f"100 nF across {u[14]} (5 V) and {u[13]} (GND) underneath, "
         "on U1's own pads: not listed above."),
        ("Off the board:", f"the strip's DIN lead to {at(board, 'R3', '2')} "
         "(DATA), run alongside the strip's GND cable; the supply's and the "
         "strip's 5 V and GND cables onto the bars."),
        ("Microphones:", "check each module has its own 100 nF from VDD to "
         "GND; if not, solder one straight across its VDD and GND pads. "
         "Chip side down: the INMP441 hears through the hole in its own "
         "board, which faces up."),
    ], y - 0.02, size=8)
    doc.save(fig)


PARTS_NOTE = {
    "MOD": "two 9-way sockets, rows 9 and 16",
    "ENC": "header in row 18, board towards the bottom",
    "U1": "notch up, channel 1",
    "D1": "Schottky, band at the VSYS end",
    "C1": "electrolytic, stripe (-) towards the bars",
}


def parts_page(doc, board):
    fig = doc.page("Parts and assembly order",
                   "Use the wire runs on the following pages with the wiring "
                   "drawing.")
    rows = []
    for part in board.parts:
        holes = ", ".join(f"{p.name} {hole_name(p.hole)}" for p in part.pins)
        if part.ref in ("MOD", "U1"):
            first, last = part.pins[0], part.pins[-1]
            holes = (f"{first.name} {hole_name(first.hole)} ... "
                     f"{last.name} {hole_name(last.hole)}")
        note = mic_note(board, part.ref) if part.ref in ("ML", "MR") else \
            PARTS_NOTE.get(part.ref, "")
        rows.append((part.ref, part.value, holes, note))
    rows.append(("C2", "100 nF ceramic",
                 f"{at(board, 'U1', '14')}, {at(board, 'U1', '13')} "
                 "(underneath)", "across U1's supply pins"))
    y, _ = table(fig, ["Part", "Value", "Holes", "Note"], rows, 0.89,
                 [0.08, 0.15, 0.34, 0.66], size=7.5,
                 width_chars=[6, 22, 38, 34])
    paragraphs(fig, [
        ("1.", "Mark A1 on both faces. Dry-fit the two 9-way sockets and both "
         "microphones, and check each microphone's pads against page 2 with "
         "a meter (SCK opposite SD, L/R opposite GND); check the KY-040's "
         "header order when it arrives."),
        ("2.", "Fit the jumpers (T wires) first, each one continuous piece: "
         "bend its ends down through their holes and solder them "
         "underneath."),
        ("3.", "Fit the sockets, the microphone headers, D1, C1, R1-R3, U1 "
         "and the KY-040 header; keep the encoder's board on insulating "
         "spacers."),
        ("4.", f"Fit the two 8 AWG bars underneath, {bars(board)}"
         f"{bar_leads(board)}. Anchor them; the supply and strip cables go "
         "straight onto them."),
        ("5.", "Lay each underside wire (B) as one piece of 24 AWG bare wire "
         "over its holes and solder it at every pad it meets. Where two "
         "wires share a hole, join them there."),
        ("6.", "C2 straight across U1 pins 14 and 13 underneath. Bridge "
         "nothing else: pins 6, 8 and 11 stay open."),
        ("7.", "Check every net for continuity, and that 5 V, VSYS, 3.3 V "
         "and GND are not shorted, before the module goes in. With USB "
         f"alone, keep the strip's 5 V off or {gpio(board, 'LED_GPIO')} "
         "low: R1 limits the current "
         "into the unpowered U1."),
    ], y - 0.02, size=8)
    doc.save(fig)


def runs_pages(doc, board):
    ids = wire_ids(board)
    rows = []
    for face in ("T", "B"):
        for wire in board.wires:
            if wire.face == face:
                chain = " - ".join(hole_name(h) for h in wire.points)
                length = len(steps(wire.points)) - 1
                rows.append((ids[id(wire)], "top" if face == "T" else
                             "under", NET_NAMES.get(wire.net, wire.net),
                             chain, str(length)))
    title = "Wire runs"
    subtitle = ("One continuous wire per row, corners in order: T on top, "
                "bent through at both ends; B bare underneath.")
    while rows:
        fig = doc.page(title, subtitle)
        _, rows = table(fig, ["Wire", "Face", "Net", "Holes, corners "
                              "included", "Pitches"], rows, 0.89,
                        [0.08, 0.14, 0.21, 0.38, 0.86], size=7.5,
                        width_chars=[6, 6, 16, 60, 6])
        doc.save(fig)
        title, subtitle = "Wire runs (continued)", ""


# A schematic box's pin pitch
ROW = 0.6


def box(ax, x, y, w, h, title, rows, side="right"):
    ax.add_patch(Rectangle((x, y - h), w, h, facecolor="#f0f4f7",
                           edgecolor=INK, linewidth=1))
    ax.text(x + 0.25, y - 0.45, title, fontsize=8.5, weight="bold",
            color=INK, va="center")
    for i, (pin, net) in enumerate(rows):
        yy = y - 1.2 - i * ROW
        ax.text(x + 0.25, yy, pin, fontsize=7, color=INK, va="center")
        if side == "right":
            ax.plot([x + w, x + w + 0.6], [yy, yy], color=INK, linewidth=1)
            ax.text(x + w + 0.75, yy, net, fontsize=7, color=INK,
                    va="center")


def resistor(ax, x, y, horizontal, label):
    if horizontal:
        ax.add_patch(Rectangle((x - 0.45, y - 0.13), 0.9, 0.26,
                               facecolor="white", edgecolor=INK, lw=1))
        ax.text(x, y + 0.35, label, fontsize=7, ha="center", color=INK)
    else:
        ax.add_patch(Rectangle((x - 0.13, y - 0.45), 0.26, 0.9,
                               facecolor="white", edgecolor=INK, lw=1))
        ax.text(x + 0.3, y, label, fontsize=7, va="center", color=INK)


def ground(ax, x, y):
    for i, half in enumerate((0.25, 0.16, 0.07)):
        ax.plot([x - half, x + half], [y - i * 0.1, y - i * 0.1], color=INK,
                linewidth=1)


def schematic_axes(fig):
    ax = fig.add_axes([0.07, 0.08, 0.86, 0.82])
    ax.set_xlim(0, 14)
    ax.set_ylim(0, 19)
    ax.axis("off")
    return ax


def schematic_pages(doc, board):
    line = dict(color=INK, linewidth=1)
    gp = {n: [p.name for part, p in board.pins()
              if part.ref == "MOD" and p.net == n] for n in NET_ORDER}

    fig = doc.page("Schematic 1 - power and microphones",
                   "Named nets join across both sheets. An electrical "
                   "drawing, not a placement.")
    ax = schematic_axes(fig)
    ax.text(0.5, 18.2, "+5V (supply and strip +)", fontsize=9, weight="bold",
            color=INK)
    ax.plot([0.5, 6.2], [17.6, 17.6], **line)
    ax.plot([1.2, 1.2], [17.6, 16.3], **line)
    ax.plot([0.8, 1.6], [16.3, 16.3], **line)
    ax.plot([0.8, 1.6], [16.0, 16.0], **line)
    ax.plot([1.2, 1.2], [16.0, 15.0], **line)
    ground(ax, 1.2, 15.0)
    ax.text(0.55, 16.35, "+", fontsize=8, color=INK)
    ax.text(1.8, 16.1, "C1  2200 uF / 10 V", fontsize=7.5, color=INK)
    ax.add_patch(Polygon([(6.2, 17.9), (6.2, 17.3), (6.8, 17.6)],
                         closed=True, facecolor="white", edgecolor=INK))
    ax.plot([6.8, 6.8], [17.3, 17.9], **line)
    ax.plot([6.8, 8.5], [17.6, 17.6], **line)
    ax.text(6.5, 18.1, "D1  1N5819", fontsize=7.5, ha="center", color=INK)
    ax.text(8.6, 17.6, "VSYS", fontsize=8, va="center", color=INK)
    rows = [("VSYS", "VSYS"), ("GND", "GND"), ("3V3 out", "3V3")]
    rows += [(g, n) for n in ("SCK", "WS", "SD", "ENC_A", "ENC_B", "ENC_SW",
                              "LED_GPIO") for g in gp[n]]
    box(ax, 0.5, 14.5, 4.2, 1.2 + len(rows) * ROW, "Waveshare RP2350-LCD-1.47",
        [(p, NET_NAMES.get(n, n)) for p, n in rows])
    for i, ref in enumerate(("ML", "MR")):
        part = next(p for p in board.parts if p.ref == ref)
        mic_rows = [(p.name, p.net) for p in part.pins]
        box(ax, 0.5 + i * 7, 6.6, 3.3, 1.2 + 6 * ROW,
            f"{'Left' if ref == 'ML' else 'Right'} mic - INMP441", mic_rows)
    paragraphs(fig, [
        "Both SD outputs share one wire: opposite L/R settings put them in "
        "alternate time slots, and each lets go of the line outside its "
        "own. The pin's bus keeper holds it then (no pull-down). SCK and WS "
        "on consecutive GPIOs, bit clock first."], 0.09, size=7.5)
    doc.save(fig)

    fig = doc.page("Schematic 2 - encoder and LED output",
                   "U1 channel 1 turns the 3.3 V LED data into 5 V logic.")
    ax = schematic_axes(fig)
    ax.text(0.3, 16.6, gp["LED_GPIO"][0], fontsize=8, color=INK)
    ax.plot([0.3, 5.6], [16.2, 16.2], **line)
    resistor(ax, 2.5, 16.2, True, "R1  1k")
    ax.plot([4.5, 4.5], [16.2, 14.6], **line)
    resistor(ax, 4.5, 14.9, False, "R2  10k")
    ax.plot([4.5, 4.5], [14.45, 13.9], **line)
    ground(ax, 4.5, 13.9)
    ax.scatter([4.5], [16.2], s=12, color=INK)
    ax.text(4.2, 16.55, "LED_IN", fontsize=7, color=INK)
    ax.add_patch(Polygon([(5.6, 16.9), (5.6, 15.5), (7.0, 16.2)],
                         closed=True, facecolor="#f0f4f7", edgecolor=INK))
    ax.text(6.1, 17.25, "U1  74HCT125", fontsize=7.5, weight="bold",
            color=INK)
    ax.text(5.35, 16.35, "2", fontsize=6.5, color=INK)
    ax.text(7.05, 16.35, "3", fontsize=6.5, color=INK)
    ax.add_patch(Circle((6.3, 15.75), 0.09, facecolor="white",
                        edgecolor=INK))
    ax.plot([6.3, 6.3], [15.66, 15.1], **line)
    ground(ax, 6.3, 15.1)
    ax.text(6.45, 15.4, "1 (OE)", fontsize=6.5, color=INK)
    ax.plot([7.0, 11.5], [16.2, 16.2], **line)
    resistor(ax, 9.0, 16.2, True, "R3  330R")
    ax.text(11.6, 16.2, "strip DIN", fontsize=8, va="center", color=INK)
    ax.text(0.3, 12.3, "+5V", fontsize=8, color=INK)
    ax.plot([0.7, 0.7], [12.1, 11.2], **line)
    ax.plot([0.3, 1.1], [11.2, 11.2], **line)
    ax.plot([0.3, 1.1], [10.95, 10.95], **line)
    ax.plot([0.7, 0.7], [10.95, 10.2], **line)
    ground(ax, 0.7, 10.2)
    ax.text(1.3, 11.05, "C2  100 nF, at U1 pins 14 and 13", fontsize=7.5,
            color=INK)
    notes = ["U1 supply: pin 14 = +5V, pin 7 = GND.",
             "Channel 1: enable 1 to GND, input 2, output 3.",
             "Unused inputs and enables 4, 5, 9, 10, 12, 13 to GND.",
             "Unused outputs 6, 8, 11 open."]
    for i, note in enumerate(notes):
        ax.text(5.2, 12.2 - i * 0.6, note, fontsize=7.5, color=INK)
    part = next(p for p in board.parts if p.ref == "ENC")
    box(ax, 0.5, 8.5, 3.3, 1.2 + 5 * ROW, "KY-040 encoder",
        [(p.name, NET_NAMES.get(p.net, p.net)) for p in part.pins])
    paragraphs(fig, [
        f"CLK to {gp['ENC_A'][0]}, DT to {gp['ENC_B'][0]}, SW to "
        f"{gp['ENC_SW'][0]}: a closed contact or pressed button pulls its "
        "line to GND. The KY-040's own pull-ups (CLK, DT) and the pins' "
        "internal ones (all three) hold them high; the encoder runs on "
        "3.3 V.",
        f"With USB alone and the strip's 5 V off, keep {gp['LED_GPIO'][0]} "
        "low or an input: R1 limits the current into the unpowered U1, it "
        "does not isolate it."], 0.2, size=7.5)
    doc.save(fig)


def reasons_page(doc, board):
    fig = doc.page("Why each part is there",
                   "And what Rev 2 had that this one does not.")
    rows = [
        ("R1  1k", "In series from the GPIO to U1's input: limits the clamp "
         "current if the module runs on USB while U1 has no 5 V."),
        ("R2  10k", "Pulls U1's input low while the GPIO is high impedance "
         "(reset, boot), so the strip sees no data. With R1, a 3.3 V high "
         "reaches U1 at about 3.0 V, above its 2.0 V threshold."),
        ("R3  330R", "Between U1's output and the strip's DIN: damps "
         "ringing on the lead and limits the current into the first LED."),
        ("D1  1N5819", "Feeds the module's VSYS from the raw 5 V, and stops "
         "USB feeding the strip's supply backwards."),
        ("C1  2200 uF", "Bulk capacitance on the 5 V bars against the "
         "strip's current steps. It does not replace C2."),
        ("C2  100 nF", "Local bypass for U1, straight across its supply "
         "pins."),
        ("Gone: R4 100k", "The SD pull-down: the pin's own bus keeper holds "
         "the shared line between the microphones' slots, drawing next to "
         "no current, and a missing microphone reads as silence."),
        ("Gone: R5-R7 10k", "The encoder pull-ups: the KY-040 carries its "
         "own on CLK and DT, and the firmware turns on the pins' internal "
         "ones on all three."),
        ("Moved pins", "SCK, WS, SD from GP0-GP2 to "
         f"{gpio(board, 'SCK')}-{gpio(board, 'SD')}, the encoder from "
         f"GP6-GP8 to {gpio(board, 'ENC_A')}, {gpio(board, 'ENC_B')}, "
         f"{gpio(board, 'ENC_SW')} and the LED data from GP9 to "
         f"{gpio(board, 'LED_GPIO')}: each chosen so its wire runs straight "
         "(app/config.h)."),
    ]
    y, _ = table(fig, ["Part", "Why"], rows, 0.89, [0.08, 0.26],
                 width_chars=[22, 80])
    doc.save(fig)


def main():
    board = layout.board()
    problems = check(board)
    if problems:
        print("The board does not check:")
        for p in problems:
            print("  " + p)
        return 1
    print("board checks:", draw.stats(board))
    if sys.argv[1:] == ["--check"]:
        return 0
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else \
        Path(__file__).resolve().parents[2] / "docs/hardware/wiring.pdf"
    out.parent.mkdir(parents=True, exist_ok=True)
    plt.rcParams["font.family"] = "DejaVu Sans"
    plt.rcParams["pdf.fonttype"] = 42
    with PdfPages(out, metadata={"Title": board.title, "Author": None,
                                 "Creator": "tools/board/build.py",
                                 "Producer": None,
                                 "CreationDate": None}) as pdf:
        doc = Document(pdf)
        summary_page(doc, board)
        placement_page(doc, board)
        wiring_page(doc, board)
        connections_page(doc, board)
        parts_page(doc, board)
        runs_pages(doc, board)
        schematic_pages(doc, board)
        reasons_page(doc, board)
    print("wrote", out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
