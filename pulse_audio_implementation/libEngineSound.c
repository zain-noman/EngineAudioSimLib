#include "wave.h"
#include "libEngineSound.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define SAMPLE_RATE 16000

int EngineAudioLoadData(struct EngineAudio *out, char *filename)
{
    WaveFile *f = wave_open(filename, WAVE_OPEN_READ);
    if (f == NULL)
    {
        printf("cant open file %s \n", filename);
        return 1;
    }

    int sampleRate = wave_get_sample_rate(f);
    if (sampleRate != SAMPLE_RATE)
    {
        printf("SAMPLERATE %d, %s \n", sampleRate, filename);
        wave_close(f);
        return 1;
    }

    int numSamples = wave_get_length(f);
    if (wave_get_sample_size(f) != sizeof(int16_t))
    {
        printf("samplesize %ld \n", wave_get_sample_size(f));
        wave_close(f);
        return 1;
    }

    int16_t *raw_audio = malloc(sizeof(int16_t) * numSamples);
    wave_read(f, raw_audio, numSamples);
    wave_close(f);

    out->numSamples = numSamples;
    out->raw_audio = raw_audio;
    return 0;
}

static float getInterpolatedSample(const struct EngineAudio *audio)
{
#define WhittakerShannonInterpolation 0
#define LinearInterpolation 1
#define ZeroOrderHold 2

#define interpolationMethod LinearInterpolation

#if (interpolationMethod == WhittakerShannonInterpolation)
    float retval = 0;
    for (int i = -10; i <= 10; i++)
    {
        int idx = (i + (int)audio->currentIdx) % audio->numSamples;
        if (idx < 0)
            idx += audio->numSamples;
        if (i != 0)
            retval += (float)(audio->raw_audio[idx]) * sin(audio->currentIdx + i) / (float)(audio->currentIdx + i);
        else
            retval += (float)(audio->raw_audio[idx]);
    }
#elif (interpolationMethod == LinearInterpolation)
    // linear interpolation
    float lerpVal = audio->currentIdx - floor(audio->currentIdx);
    float retval = audio->raw_audio[(int)(audio->currentIdx) % audio->numSamples] * (1 - lerpVal) +
                   audio->raw_audio[((int)(audio->currentIdx) + 1) % audio->numSamples] * (lerpVal);
#else
    float retval = audio->raw_audio[(int)(audio->currentIdx) % audio->numSamples];
#endif
    return retval;
}

void fillBufferEngineSound(struct EngineSimulator *sim, int16_t *buf, int bufSize, bool revUp)
{
    struct EngineAudio* firstAudio = &sim->audios[sim->audioIdx];
    struct EngineAudio* secondAudio;
    if (sim->audioIdx + 1 < sim->numAudios)
        secondAudio = &sim->audios[sim->audioIdx+1];
    else
        secondAudio = NULL;

#define STEP_SPEED 0.3 / SAMPLE_RATE
    for (size_t i = 0; i < 1024; i++)
    {
        float target_rpm;
        if (revUp)
            target_rpm = MAXRPM;
        else
            target_rpm = MINRPM;
        sim->current_rpm = STEP_SPEED * target_rpm + (1 - STEP_SPEED) * sim->current_rpm;

        // audio index
        if (secondAudio != NULL && sim->current_rpm > secondAudio->rpm)
        {
            sim->audioIdx++;
            firstAudio = &sim->audios[sim->audioIdx];
            if (sim->audioIdx + 1 < sim->numAudios)
                secondAudio = &sim->audios[sim->audioIdx+1];
            else
                secondAudio = NULL;
            printf("audioIdx %d \n", sim->audioIdx);
        }
        else if (sim->audioIdx != 0 && sim->current_rpm < firstAudio->rpm)
        {
            sim->audioIdx--;
            firstAudio = &sim->audios[sim->audioIdx];
            if (sim->audioIdx + 1 < sim->numAudios)
                secondAudio = &sim->audios[sim->audioIdx+1];
            else
                secondAudio = NULL;
            printf("audioIdx %d \n", sim->audioIdx);
        }

        float lerp_factor;
        if (secondAudio != NULL) 
            lerp_factor = (sim->current_rpm - firstAudio->rpm) / 
                            (secondAudio->rpm - firstAudio->rpm);
        else 
            lerp_factor = 0;
        
        if (lerp_factor > 1)
            lerp_factor = 1;
        else if (lerp_factor < 0)
            lerp_factor = 0;

        
        float v = getInterpolatedSample(firstAudio) * firstAudio->gain;
        if (secondAudio != NULL)
        {
            v *= (1 - lerp_factor);
            v += getInterpolatedSample(secondAudio) * secondAudio->gain * lerp_factor;
        }
        
        // adjust plaback speed based on rpm
        firstAudio->currentIdx += 1 + (sim->current_rpm - firstAudio->rpm) / firstAudio->rpm;
        if (secondAudio != NULL)
            secondAudio->currentIdx += 1 + (sim->current_rpm - secondAudio->rpm) / secondAudio->rpm;
        
        buf[i] = v;
    }
}