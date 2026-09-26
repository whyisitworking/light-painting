lp_add_module(audio
    SOURCES audio.c
    DEPS fft
    MATH)
# Changes the layout of audio_t: every user must see the same definition
target_compile_definitions(audio PUBLIC AUDIO_ENVELOPE)
