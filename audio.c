#include "chip-8.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define SAMPLE_RATE 15360
#define SAMPLES_PER_FRAME (SAMPLE_RATE / TARGET_FPS)
// Target value is greater than the samples per frame as the timing per frame
// is not exect, so we over-generate to provide a smooth sound.
#define TARGET_QUEUED_SAMPLES (2 * SAMPLES_PER_FRAME)
#define SAMPLES_BUF_SIZE TARGET_QUEUED_SAMPLES

SDL_AudioStream *stream = NULL;
SDL_AudioSpec spec = {
	.channels = 1,
	.freq = SAMPLE_RATE,
	.format = SDL_AUDIO_F32,
};
float samples[SAMPLES_BUF_SIZE] = {0};
uint32_t current_sine_sample = 0;

uint8_t init_audio()
{
	if (!(stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL))) {
		SDL_Log("Failed to open audio device stream: %s\n", SDL_GetError());
		return 0;
	}
	SDL_ResumeAudioStreamDevice(stream);
	return 1;
}

void destroy_audio()
{
	SDL_DestroyAudioStream(stream);
}

void generate_samples()
{
	int queued = SDL_GetAudioStreamQueued(stream);
	if (queued < 0) {
		return;
	}

	// sizeof returns uint so we cast to int to prevent arithmetic conversion.
	int needed = TARGET_QUEUED_SAMPLES - (queued / (int)sizeof(float));
	for (int i = 0; i < needed; ++i, ++current_sine_sample) {
		int tone_freq = 440;
		// We use a shared `current_sine_sample` so that the phase is
		// consistent from one frame to the next. If we simply used `i`,
		// then there would be a phase discontinuity as `i` is reset
		// each time this function is called.
		float phase = (float) current_sine_sample * tone_freq / SAMPLE_RATE;
		samples[i] = 0.5f * SDL_sinf(phase * 2 * SDL_PI_F);
	}
	// Wrap around to avoid floating point inaccuracies.
	current_sine_sample %= SAMPLE_RATE;

	SDL_PutAudioStreamData(stream, samples, needed * sizeof(float));
}
