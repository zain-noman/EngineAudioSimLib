#include "esp_types.h"
#include "stdio.h"
#include "math.h"

#define MAXRPM 120
#define MINRPM 25

extern const uint8_t alto_25Hz_start[] asm("_binary_alto_25Hz_wav_start");
extern const uint8_t alto_25Hz_end[] asm("_binary_alto_25Hz_wav_end");

extern const uint8_t alto_40Hz_start[] asm("_binary_alto_40Hz_wav_start");
extern const uint8_t alto_40Hz_end[] asm("_binary_alto_40Hz_wav_end");

extern const uint8_t alto_65Hz_start[] asm("_binary_alto_65Hz_wav_start");
extern const uint8_t alto_65Hz_end[] asm("_binary_alto_65Hz_wav_end");

struct engineAudio
{
    int16_t *raw_audio;
    float rpm;
    int numSamples;
    float currentIdx;
    float gain;
};

struct engineAudio audios[3];
void initEngineAudio()
{
    audios[0] = (struct engineAudio){
        .raw_audio = (int16_t *)(alto_25Hz_start + 44),
        .rpm = 25,
        .numSamples = (alto_25Hz_end - alto_25Hz_start - 44) / sizeof(int16_t),
        .currentIdx = 0,
        .gain = 0.5,
    };
    audios[1] = (struct engineAudio){
        .raw_audio = (int16_t *)(alto_40Hz_start + 44),
        .rpm = 40,
        .numSamples = (alto_40Hz_end - alto_40Hz_start - 44) / sizeof(int16_t),
        .currentIdx = 0,
        .gain = 0.6,
    };
    audios[2] = (struct engineAudio){
        .raw_audio = (int16_t *)(alto_65Hz_start + 44),
        .rpm = 65,
        .numSamples = (alto_65Hz_end - alto_65Hz_start - 44) / sizeof(int16_t),
        .currentIdx = 0,
        .gain = 1,
    };
}

#define SAMPLE_RATE 16000

float getInterpolatedSample(const struct engineAudio *audio)
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

void fillBufEngineSound(int16_t *buf, size_t size, bool revUp)
{
    static float current_rpm = MINRPM;
    static int audioIdx = 0;

    float rpmToApproach = revUp ? MAXRPM : MINRPM;

    for (size_t i = 0; i < size; i++)
    {
#define STEP_SPEED 0.3 / SAMPLE_RATE

        current_rpm += (rpmToApproach - current_rpm) * (STEP_SPEED);

        // audio index
        if (current_rpm > audios[audioIdx + 1].rpm && audioIdx < 2)
        {
            audioIdx++;
            printf("audioIdx %d \n", audioIdx);
        }
        else if (current_rpm < audios[audioIdx].rpm && audioIdx != 0)
        {
            audioIdx--;
            printf("audioIdx %d \n", audioIdx);
        }

        float lerp_factor = (current_rpm - audios[audioIdx].rpm) / (audios[audioIdx + 1].rpm - audios[audioIdx].rpm);

        // adjust plaback speed based on rpm
        audios[audioIdx].currentIdx += 1 + (current_rpm - audios[audioIdx].rpm) / audios[audioIdx].rpm;
        audios[audioIdx + 1].currentIdx += 1 + (current_rpm - audios[audioIdx + 1].rpm) / audios[audioIdx + 1].rpm;

        float v = getInterpolatedSample(&audios[audioIdx]) * audios[audioIdx].gain;
        if (audioIdx < 2)
        {
            v *= (1 - lerp_factor);
            v += getInterpolatedSample(&audios[audioIdx + 1]) * audios[audioIdx + 1].gain * lerp_factor;
        }
        // buf[i] += (randn()*1000);
        buf[i] = v;
    }
}