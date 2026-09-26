lp_add_module(neopixel
    SOURCES neopixel.c
    DEPS swapchain pico_stdlib pico_mem_ops hardware_pio hardware_dma)

pico_generate_pio_header(neopixel ${CMAKE_CURRENT_SOURCE_DIR}/neopixel.pio)
