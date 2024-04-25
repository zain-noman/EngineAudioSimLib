#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <time.h>

#include <pulse/simple.h>
#include <pulse/error.h>
#include <stdlib.h>
#include "wave.h"
#include "libEngineSound.h"

#define SAMPLE_RATE 16000

int main(int argc, char *argv[])
{
    char *fileNames [] = {
        "../../alto_25Hz.wav",
        "../../alto_40Hz.wav",
        "../../alto_60Hz.wav",
        "../../alto_65Hz.wav",
    };
    struct EngineAudio audios[] = {
        {.rpm = 25,
         .gain = 0.5},
        {.rpm = 40,
         .gain = 0.5},
        {.rpm = 55,
         .gain = 0.8},
        {.rpm = 67,
         .gain = 1},
    };
    for (size_t i = 0; i < 4; i++)
    {
        if (EngineAudioLoadData(&audios[i],fileNames[i]) != 0)
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
    WaveFile *of = wave_open("out.wav", WAVE_OPEN_WRITE);
    if (of == NULL)
    {
        printf("cant create out file \n");
        return 1;
    }
    wave_set_format(of, WAVE_FORMAT_PCM);
    wave_set_num_channels(of, 1);
    wave_set_sample_rate(of, SAMPLE_RATE);
    wave_set_sample_size(of, sizeof(int16_t));

    struct EngineSimulator sim = {
        .audios = audios,
        .numAudios = 4,
        .audioIdx = 0,
        .current_rpm = MINRPM,
        .sin_phase = 0,
    };
    for (float t = 0; t < 30; t+= 1024.0/SAMPLE_RATE)
    {
        int16_t buf[1024];
        bool revup = fmod(t,10) > 5;
        float target_rpm = (revup) ? MAXRPM : MINRPM;

        fillBufferEngineSound(&sim,buf,1024,target_rpm);

        if (pa_simple_write(s, buf, sizeof(buf), &error) < 0)
        {
            fprintf(stderr, __FILE__ ": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        // write to file
        wave_write(of, buf, 1024);
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