#ifndef AUDIO_H
#define AUDIO_H

#include "uefi.h"

#define AUDIO_SAMPLE_RATE 44100
#define AUDIOChannels 2
#define AUDIO_BITS_PER_SAMPLE 16

typedef struct {
    UINT8 *buffer;
    UINTN size;
    UINTN write_pos;
    BOOLEAN playing;
} audio_stream_t;

void audio_init(void);
BOOLEAN audio_init_hda(void);
BOOLEAN audio_init_speaker(void);
BOOLEAN audio_play(const UINT8 *data, UINTN size);
BOOLEAN audio_beep(UINT16 frequency_hz, UINT16 duration_ms);
void audio_stop(void);

#endif