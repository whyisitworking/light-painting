# pico_multicore too, so that pico_flash compiles here as it does elsewhere
lp_add_module(storage
    SOURCES storage.c
    DEPS pico_stdlib hardware_flash pico_flash pico_multicore)
