/**
 * Prints a song's parts and hits over time, as the firmware's analysis hears
 * it: the song parts to compare with how the song really goes, and the hits
 * counted per 10 s. The song is never stored anywhere.
 *
 *   timeline song.wav [trim_db]
 *   timeline --song
 *
 * trim_db: how much quieter the microphones hear it than the file is, as
 * the preview's input level (default -18 dB). A WAV file from an MP3:
 *   afconvert -f WAVE -d LEI16 song.mp3 song.wav
 * --song: two loops of the synthetic song (lib/song) instead of a file, a
 * check of the tool itself (ctest runs it)
 */

#include "features.h"
#include "song.h"
#include "spectrum.h"
#include "ui_names.h"
#include "wav.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

constexpr size_t FFT_SIZE = 512;
constexpr size_t HOP_SIZE = 256;
constexpr float GAIN = 1.5f;
constexpr float DEFAULT_TRIM_DB = -18.f;
constexpr float COUNT_PERIOD_S = 10.f;

static uint8_t *read_file(const char *path, size_t *size) {
    FILE *file = fopen(path, "rb");
    uint8_t *bytes = nullptr;
    long length;

    if (file == nullptr)
        return nullptr;

    if (fseek(file, 0, SEEK_END) == 0 && (length = ftell(file)) > 0 &&
        fseek(file, 0, SEEK_SET) == 0 &&
        (bytes = (uint8_t *)malloc((size_t)length)) != nullptr &&
        fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        bytes = nullptr;
    }
    fclose(file);
    *size = bytes != nullptr ? (size_t)length : 0;

    return bytes;
}

// Two loops of the synthetic song as samples, -1 to 1 of 24 bits
static bool synthetic_song(wav_t *wav) {
    static int32_t frames[2 * HOP_SIZE];
    constexpr float RATE = 48828.125f;
    song_t song;
    size_t count = (size_t)(2.f * SONG_LENGTH_S * RATE) / HOP_SIZE * HOP_SIZE;

    *wav = (wav_t){.sample_rate = RATE, .count = count};
    wav->samples = (float *)malloc(count * sizeof(float));
    if (wav->samples == nullptr || !song_init(&song, RATE, 7))
        return false;

    for (size_t n = 0; n < count; n += HOP_SIZE) {
        song_fill(&song, frames, HOP_SIZE);
        for (size_t i = 0; i < HOP_SIZE; i++)
            wav->samples[n + i] = (float)(frames[2 * i] >> 7) / 8388607.f;
    }

    return true;
}

// m:ss.s
static void print_time(float seconds) {
    int minutes = (int)(seconds / 60.f);

    printf("%d:%04.1f", minutes, (double)(seconds - 60.f * (float)minutes));
}

int main(int argc, char **argv) {
    static int32_t frames[2 * HOP_SIZE];
    const char *error = "";
    spectrum_t spectrum;
    features_t features;
    wav_t wav;
    uint8_t *bytes;
    size_t size, hits[FEATURES_REGION_COUNT] = {0}, drops = 0, lifts = 0;
    float trim, hop_period_s, next_count_s = COUNT_PERIOD_S;
    parts_part_t part = PARTS_COUNT;

    if (argc < 2) {
        fprintf(stderr, "usage: %s song.wav [trim_db] | --song\n", argv[0]);
        return EXIT_FAILURE;
    }

    trim = powf(10.f, (argc > 2 ? strtof(argv[2], nullptr) : DEFAULT_TRIM_DB) /
                          20.f);

    if (strcmp(argv[1], "--song") == 0) {
        if (!synthetic_song(&wav)) {
            fprintf(stderr, "out of memory\n");
            return EXIT_FAILURE;
        }
        // Made at the microphones' level already
        trim = 1.f;
    } else if ((bytes = read_file(argv[1], &size)) == nullptr) {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return EXIT_FAILURE;
    } else if (!wav_read(&wav, bytes, size, &error)) {
        fprintf(stderr, "%s: %s\n", argv[1], error);
        free(bytes);
        return EXIT_FAILURE;
    } else {
        free(bytes);
    }

    hop_period_s = (float)HOP_SIZE / wav.sample_rate;
    if (!spectrum_init(&spectrum, FFT_SIZE, HOP_SIZE) ||
        !features_init(&features, spectrum_bin_count(&spectrum),
                       wav.sample_rate / (float)FFT_SIZE, hop_period_s)) {
        fprintf(stderr, "cannot start the analysis\n");
        wav_free(&wav);
        return EXIT_FAILURE;
    }
    // As the visualizer tells it: the song parts take their level before it
    features_set_gain(&features, GAIN);

    printf("%s: %.1f s at %.0f Hz, trim %.1f dB\n\n", argv[1],
           (double)((float)wav.count / wav.sample_rate),
           (double)wav.sample_rate, (double)(20.f * log10f(trim)));
    printf("  time    part   event    low mid high hits in the 10 s before\n");

    for (size_t hop = 0; (hop + 1) * HOP_SIZE <= wav.count; hop++) {
        float t = (float)(hop + 1) * hop_period_s;
        const sound_t *sound;

        // As the preview makes I2S words of its samples
        for (size_t i = 0; i < HOP_SIZE; i++) {
            float scaled = wav.samples[hop * HOP_SIZE + i] * trim * 8388607.f;
            int32_t word;

            scaled = fmaxf(fminf(scaled, 8388607.f), -8388608.f);
            word = (int32_t)((uint32_t)(int32_t)lroundf(scaled) << 7);
            frames[2 * i] = word;
            frames[2 * i + 1] = word;
        }

        spectrum_analyze(&spectrum, frames, GAIN);
        sound = features_update(&features, spectrum_bins(&spectrum));

        for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
            hits[r] += sound->hits[r].fired;
        drops += sound->event == PARTS_DROP;
        lifts += sound->event == PARTS_LIFT;

        if (sound->part != part || sound->event != PARTS_NONE) {
            printf("  ");
            print_time(t);
            printf("  %-6s %s\n", ui_names_part(sound->part),
                   sound->event == PARTS_DROP   ? "DROP"
                   : sound->event == PARTS_LIFT ? "lift"
                                                : "");
            part = sound->part;
        }

        if (t >= next_count_s) {
            printf("  ");
            print_time(t);
            printf("                  %3zu %3zu %4zu\n", hits[FEATURES_LOW],
                   hits[FEATURES_MID], hits[FEATURES_HIGH]);
            for (size_t r = 0; r < FEATURES_REGION_COUNT; r++)
                hits[r] = 0;
            next_count_s += COUNT_PERIOD_S;
        }
    }

    printf("\n%zu drops, %zu lifts\n", drops, lifts);

    features_deinit(&features);
    spectrum_deinit(&spectrum);
    wav_free(&wav);

    return EXIT_SUCCESS;
}
