#pragma once
#include <stdint.h>
#include <stdbool.h>

#define MAXRPM 120
#define MINRPM 25

struct EngineAudio
{
    int16_t *raw_audio;
    float rpm;
    int numSamples;
    float currentIdx;
    float gain;
};
int EngineAudioLoadData(struct EngineAudio *out, char* filename);

struct EngineSimulator{
    struct EngineAudio* audios;
    int numAudios;
    int audioIdx;
    float current_rpm;
    float sin_phase;
};


void fillBufferEngineSound(struct EngineSimulator *sim, int16_t* buf, int bufSize, float target_rpm);