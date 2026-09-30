lp_add_module(show
    SOURCES
        show.c
        scene.c
        blocks.c
        rules.c
        look_pulse.c
        look_flow.c
        look_stage.c
        look_sweep.c
        look_storm.c
    DEPS features color
    MATH)
