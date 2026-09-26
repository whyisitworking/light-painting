lp_add_module(i2s
    SOURCES i2s.c
    DEPS swapchain pico_stdlib pico_mem_ops hardware_pio hardware_dma)

pico_generate_pio_header(i2s ${CMAKE_CURRENT_SOURCE_DIR}/i2s.pio)
