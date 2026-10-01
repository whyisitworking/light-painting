lp_add_module(encoder
    SOURCES encoder.c
    DEPS pico_stdlib hardware_pio)

pico_generate_pio_header(encoder ${CMAKE_CURRENT_SOURCE_DIR}/encoder.pio)
