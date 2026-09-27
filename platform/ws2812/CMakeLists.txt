lp_add_module(ws2812
    SOURCES ws2812.c
    DEPS swapchain pico_stdlib pico_mem_ops hardware_pio hardware_dma)

pico_generate_pio_header(ws2812 ${CMAKE_CURRENT_SOURCE_DIR}/ws2812.pio)
