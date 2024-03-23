/***
  This file is part of PulseAudio.

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, see <http://www.gnu.org/licenses/>.
***/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#include <pulse/simple.h>
#include <pulse/error.h>

#include "wave.h"

struct engineAudio
{
    int16_t *raw_audio;
    float rpm;
    int numSamples;
    float currentIdx;
    char *filename;
    float gain;
};

#define SAMPLE_RATE 16000

int EngineAudioLoadData(struct engineAudio *out)
{
    WaveFile *f = wave_open(out->filename, WAVE_OPEN_READ);
    if (f == NULL)
    {
        printf("cant open file %s \n", out->filename);
        return 1;
    }

    int sampleRate = wave_get_sample_rate(f);
    if (sampleRate != SAMPLE_RATE)
    {
        printf("SAMPLERATE %d, %s \n", sampleRate, out->filename);
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

float randn()
{
    float retval = -6;
    for (int i = 0; i < 12; i++)
    {
        retval += ((float)rand()) / RAND_MAX;
    }
    return retval;
}

float getInterpolatedSample(const struct engineAudio* audio){
    #define WhittakerShannonInterpolation 1
    #if (WhittakerShannonInterpolation)
    float retval = 0;
    for (int i = -10; i <= 10; i++)
    {
        int idx = (i + (int)audio->currentIdx) % audio->numSamples;
        if (idx < 0)
            idx += audio->numSamples;
        if (i != 0)
            retval += (float) (audio->raw_audio[idx])*sin(audio->currentIdx +i)/(float)(audio->currentIdx +i);
        else
            retval += (float) (audio->raw_audio[idx]);
    }
    #else
    //linear interpolation
    float lerpVal = audio->currentIdx - floor(audio->currentIdx);
    float retval = audio->raw_audio[(int)(audio->currentIdx) % audio->numSamples] * (1-lerpVal) +
                   audio->raw_audio[((int)(audio->currentIdx)+1) % audio->numSamples] * (lerpVal);
    #endif
    return retval;
}

int main(int argc, char *argv[])
{

    struct engineAudio audios[] = {
        {.rpm = 25,
         .filename = "../../alto_25Hz.wav",
         .gain = 0.5},
        {.rpm = 40,
         .filename = "../../alto_40Hz.wav",
         .gain = 0.5},
        {.rpm = 55,
         .filename = "../../alto_60Hz.wav",
         .gain = 0.8},
        {.rpm = 67,
         .filename = "../../alto_65Hz.wav",
         .gain = 1},
    };
    for (size_t i = 0; i < 4; i++)
    {
        if (EngineAudioLoadData(&audios[i]) != 0)
        {
            return 1;
        }
    }

    /* The Sample format to use */
    pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = SAMPLE_RATE,
        .channels = 1};

    pa_simple *s = NULL;
    int ret = 1;
    int error;

    /* Create a new playback stream */
    if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_PLAYBACK, NULL, "playback", &ss, NULL, NULL, &error)))
    {
        fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    // crate a file to write audio to 
    WaveFile* of = wave_open("out.wav",WAVE_OPEN_WRITE);
    if (of == NULL){
        printf("cant create out file \n");
        return 1;
    }
    wave_set_format(of, WAVE_FORMAT_PCM);
    wave_set_num_channels(of,1);
    wave_set_sample_rate(of,SAMPLE_RATE);
    wave_set_sample_size(of,sizeof(int16_t));

#define MAXRPM 120
#define MINRPM 25
#define SIN_AMMOUNT 0.0
    float current_rpm = MINRPM;
    float sin_phase = 0;
    int audioIdx = 0;
    for (float t = 0; t < 30;)
    {
        int16_t buf[1024];
        for (size_t i = 0; i < 1024; i++)
        {
#define STEP_SPEED 0.3 / SAMPLE_RATE
            t += 1.0 / SAMPLE_RATE;
            if (fmod(t, 10) > 5)
                current_rpm += (MAXRPM - current_rpm) * (STEP_SPEED);
            else
                current_rpm += (MINRPM - current_rpm) * (STEP_SPEED);

            // audio index
            if (current_rpm > audios[audioIdx + 1].rpm && audioIdx<3)
            {
                audioIdx++;
                printf("audioIdx %d \n", audioIdx);
            }
            else if (current_rpm < audios[audioIdx].rpm)
            {
                audioIdx--;
                printf("audioIdx %d \n", audioIdx);
            }
            
            float lerp_factor = (current_rpm - audios[audioIdx].rpm) / (audios[audioIdx + 1].rpm - audios[audioIdx].rpm);
            
            //adjust plaback speed based on rpm
            audios[audioIdx].currentIdx += 1 + (current_rpm - audios[audioIdx].rpm)/audios[audioIdx].rpm;
            audios[audioIdx+1].currentIdx += 1 + (current_rpm - audios[audioIdx+1].rpm)/audios[audioIdx+1].rpm;

            float v = getInterpolatedSample(&audios[audioIdx])* audios[audioIdx].gain;
            if (audioIdx < 3){
                v *= (1-lerp_factor);
                v += getInterpolatedSample(&audios[audioIdx+1])* audios[audioIdx+1].gain * lerp_factor;
            }
            // buf[i] += (randn()*1000);
            buf[i] = v;
        }

        /* ... and play it */
        if (pa_simple_write(s, buf, sizeof(buf), &error) < 0)
        {
            fprintf(stderr, __FILE__ ": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
    
        // write to file
        wave_write(of,buf,1024);
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(s, &error) < 0)
    {
        fprintf(stderr, __FILE__ ": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }

    ret = 0;

finish:

    if (s)
        pa_simple_free(s);
    for (size_t i = 0; i < 4; i++)
    {
        free(audios[i].raw_audio);
    }

    return ret;
}