lp_add_module(show
    SOURCES
        show.c
        scene.c
        blocks.c
        rules.c
        look_pulse.c
        look_flow.c
        look_stage.c
    DEPS features color
    MATH)
