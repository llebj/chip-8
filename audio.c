#include "chip-8.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_stdinc.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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

void generate_samples(bool silence)
{
	if (silence) {
		return;
	}

	// We may be generating samples unneccessarily.
	// TODO: Check in the audio stream queue to determine how many samples
	// to generate.
	for (int i = 0; i < SAMPLES_BUF_SIZE; i++) {
		int tone_freq = 440;
		// We use a shared `current_sine_sample` so that the phase is
		// consistent from one frame to the next. If we simply used `i`,
		// then there would be a phase discontinuity as `i` is reset
		// each time this function is called.
		float phase = (float) current_sine_sample * tone_freq / SAMPLE_RATE;
		samples[i] = 0.5f * SDL_sinf(phase * 2 * SDL_PI_F);
		++current_sine_sample;
	}
	// Wrap around to avoid floating point inaccuracies.
	current_sine_sample %= SAMPLE_RATE;

	SDL_PutAudioStreamData(stream, samples, sizeof (samples));
}
