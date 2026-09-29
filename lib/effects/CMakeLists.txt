lp_add_module(effects
    SOURCES
        effects.c
        palette.c
        sparkles.c
        layers.c
        helpers.c
        mode_spectrum.c
        mode_river.c
        mode_ripples.c
        mode_vu.c
        mode_glow.c
    DEPS features color
    MATH)
