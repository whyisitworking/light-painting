# -fcx-limited-range: plain complex multiplications instead of __mulsc3 calls
# that handle NaN/Inf operands, which audio samples never are
lp_add_module(fft
    SOURCES fft.c
    OPTIONS -fcx-limited-range
    MATH)
