"""The board: where every part sits, and what each pin carries.

Centred on column I: the module's body, the two microphones and the
encoder's knob under the screen. The LED output beside the encoder on the
right, D1 on the left; the two bars run under the encoder, D1 and C1
standing across the GND bar.
"""

from model import COLS, Board, Part, Pin, Wire, hole

# The module's header: two rows of nine (Waveshare RP2350-LCD-1.47-A,
# connector U4), USB to the left
TOP_ROW = ["VSYS", "GND", "3V3", "GP0", "GP1", "GP2", "GP3", "GP4", "GP5"]
BOTTOM_ROW = ["GP28", "GP29", "GP27", "GP26", "GP25", "GP9", "GP8", "GP7",
              "GP6"]
POWER = {"VSYS": "VSYS", "GND": "GND", "3V3": "3V3"}


def module(gpio):
    pins = []
    for i, name in enumerate(TOP_ROW):
        pins.append(Pin(name, (8 + i, 9), POWER.get(name) or gpio.get(name)))
    for i, name in enumerate(BOTTOM_ROW):
        pins.append(Pin(name, (8 + i, 16), gpio.get(name)))
    return Part("MOD", "RP2350-LCD-1.47-A", pins,
                body=("rect", 3.5, 8.5, 17.9, 16.5), label="RP2350 + LCD")


def mic(ref, centre, lr_net, turned=False):
    """An INMP441 module, 14 mm round: pins in two rows 3 apart, SCK, WS,
    L/R over SD, VDD, GND; turned half round, the other way up. As seen
    looking down on the board with the module's chip side down (its sound
    hole up): the chip side's silkscreen reads L/R, WS, SCK over GND, VDD,
    SD, mirrored (components101's pinout photo)"""
    c, r = centre
    grid = [("SCK", "SCK"), ("WS", "WS"), ("L/R", lr_net),
            ("SD", "SD"), ("VDD", "3V3"), ("GND", "GND")]
    places = [(c - 1, r - 1), (c, r - 1), (c + 1, r - 1),
              (c - 1, r + 2), (c, r + 2), (c + 1, r + 2)]
    if turned:
        places = list(reversed(places))
    pins = [Pin(name, h, net) for (name, net), h in zip(grid, places)]
    return Part(ref, "INMP441", pins, body=("circle", c, r + 0.5, 2.75),
                label=ref)


def encoder(left, row):
    """A KY-040: its five header pins in a row, the board below them"""
    names = [("GND", "GND"), ("+", "3V3"), ("SW", "ENC_SW"), ("DT", "ENC_B"),
             ("CLK", "ENC_A")]
    pins = [Pin(n, (left + i, row), net) for i, (n, net) in enumerate(names)]
    return Part("ENC", "KY-040", pins,
                body=("rect", left - 1.5, row - 0.4, left + 5.5, row + 11.8),
                label="KY-040")


def dip14(ref, value, left, top, nets):
    """Notch up: pins 1 to 7 down the left column, 8 to 14 up the right"""
    pins = []
    for i in range(7):
        pins.append(Pin(str(i + 1), (left, top + i), nets.get(i + 1)))
        pins.append(Pin(str(14 - i), (left + 3, top + i), nets.get(14 - i)))
    return Part(ref, value, pins,
                body=("rect", left + 0.3, top - 0.4, left + 2.7, top + 6.4),
                label=ref)


def two_lead(ref, value, a, b, net_a, net_b, names=("1", "2"), kind="axial"):
    a, b = hole(a), hole(b)
    if a[0] == b[0]:
        body = ("rect", a[0] - 0.3, min(a[1], b[1]) + 0.5, a[0] + 0.3,
                max(a[1], b[1]) - 0.5)
    else:
        body = ("rect", min(a[0], b[0]) + 0.5, a[1] - 0.3,
                max(a[0], b[0]) - 0.5, a[1] + 0.3)
    return Part(ref, value, [Pin(names[0], a, net_a), Pin(names[1], b, net_b)],
                body=body, label=ref, kind=kind)


# What the layout was chosen with (explore.py tried every combination, with
# bends and jumpers dear): the left microphone turned half round and the
# right channel (L/R to 3V3), the right one the left channel; SCK, WS and SD
# on GP1, GP2, GP3; the GND bar above the 5 V one. The firmware sums the two,
# so which is which does not matter
DEFAULTS = {"ml_turned": True, "mr_turned": False, "ml_lr": "3V3",
            "sck": 1, "sd": 3, "gnd_row": 28}

# 74HCT125, notch up, channel 1 used: its enable to GND, the LED data in and
# out on the left column (facing the module); each unused channel's enable
# and input side by side, to GND; unused outputs open
U1_NETS = {1: "GND", 2: "LED_IN", 3: "LED_OUT", 4: "GND", 5: "GND", 7: "GND",
           9: "GND", 10: "GND", 12: "GND", 13: "GND", 14: "5V"}


def w(net, face, *names):
    return Wire(net, face, [hole(n) for n in names])


# Hand-laid wires, the router adds the rest
FIXED = [
    # The encoder straight under its GPIOs, its GND straight down to the bar
    w("ENC_SW", "B", "K16", "K18"),
    w("ENC_B", "B", "L16", "L18"),
    w("ENC_A", "B", "M16", "M18"),
    w("GND", "B", "I18", "I28"),
    # Its 3V3 around the socket's end
    w("3V3", "B", "J9", "J10", "G10", "G17", "J17", "J18"),
    # VSYS straight down to D1, as GP6 to R1 on the right
    w("VSYS", "B", "H9", "F9", "F17"),
    # 5 V from D1 and C1 down the left edge and over the GND bar on a
    # jumper, as U1's on the right: wire round the bar's end would come
    # within a pitch of it
    w("5V", "B", "F21", "A21", "A26"),
    w("5V", "T", "A26", "A30"),
    w("5V", "B", "A30", "A31", "B31"),
    w("5V", "B", "D22", "D21"),
    w("GND", "B", "D24", "D28"),
    # The LED output: GPIO straight down to R1, R1 and R2 to U1's input, R3
    # from its output
    w("LED_GPIO", "B", "P16", "P17"),
    w("LED_IN", "B", "O21", "Q21"),
    w("GND", "B", "O25", "O28"),
    w("LED_OUT", "B", "Q22", "P22"),
    # U1's supply down the right edge and over the GND bar, its GND pins
    # onto a trunk to the bar
    w("5V", "B", "T20", "U20", "U26"),
    w("5V", "T", "U26", "U30"),
    w("5V", "B", "U30", "U31", "T31"),
    w("GND", "B", "R20", "R28"),
    w("GND", "B", "Q20", "R20"),
    w("GND", "B", "Q23", "R23"),
    w("GND", "B", "Q24", "R24"),
    w("GND", "B", "Q26", "R26"),
    w("GND", "B", "T21", "T22", "S22", "R22"),
    w("GND", "B", "T24", "T25", "S25", "R25"),
]


# The microphones' wires, as explore.py routed them for DEFAULTS: SCK and WS
# up from their GPIOs, SD round the top with one short straight jumper, each
# mic's GND down a column either side of the module
ROUTED = [
    w("SCK", "B", "L9", "L6", "H6"),
    w("SCK", "B", "L6", "L3", "N3"),
    w("3V3", "B", "J9", "J7", "F7", "F6"),
    w("3V3", "B", "F6", "F4", "G4", "G3"),
    w("3V3", "B", "J10", "Q10", "Q7", "O7", "O6"),
    w("GND", "B", "I18", "E18", "E8", "I8", "I9"),
    w("GND", "B", "E8", "E3", "F3"),
    w("GND", "B", "R20", "R6", "P6"),
    w("GND", "B", "P6", "P3"),
    w("WS", "B", "M9", "M4", "O4", "O3"),
    w("WS", "B", "O3", "O2", "I2", "I5", "G5", "G6"),
    w("SD", "B", "N9", "N6"),
    w("SD", "B", "N8", "P8"),
    w("SD", "T", "P8", "S8"),
    w("SD", "B", "S8", "S1", "H1", "H3"),
]


def board(routed=True, **options):
    """The board; without routed, only the hand-laid wires (for explore.py
    to route the rest, e.g. with other options)"""
    o = dict(DEFAULTS, **options)
    gpio = {f"GP{o['sck']}": "SCK", f"GP{o['sck'] + 1}": "WS",
            f"GP{o['sd']}": "SD", "GP9": "ENC_A", "GP25": "ENC_B",
            "GP26": "ENC_SW", "GP6": "LED_GPIO"}
    mr_lr = "3V3" if o["ml_lr"] == "GND" else "GND"
    parts = [
        module(gpio),
        mic("ML", (7, 4), o["ml_lr"], o["ml_turned"]),
        mic("MR", (15, 4), mr_lr, o["mr_turned"]),
        encoder(9, 18),
        dip14("U1", "74HCT125", 17, 20, U1_NETS),
        two_lead("R1", "1k", "P17", "P21", "LED_GPIO", "LED_IN"),
        two_lead("R2", "10k", "O21", "O25", "LED_IN", "GND"),
        two_lead("R3", "330R", "P22", "P26", "LED_OUT", "DATA"),
        two_lead("D1", "1N5819", "F21", "F17", "5V", "VSYS", ("A", "K")),
        Part("C1", "2200uF 10V",
             [Pin("+", hole("D22"), "5V"), Pin("-", hole("D24"), "GND")],
             body=("circle", 4, 23, 2.5), label="C1", kind="radial"),
    ]
    five_row = o["gnd_row"] + 3
    return Board(parts=parts, rails={"GND": (o["gnd_row"], 2, COLS - 1),
                                     "5V": (five_row, 2, COLS - 1)},
                 wires=[Wire(x.net, x.face, list(x.points))
                        for x in FIXED + (ROUTED if routed else [])],
                 title="RP2350 music visualiser, perfboard wiring")
