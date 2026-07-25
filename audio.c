#include "chip-8.h"
#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_log.h>
#include <stdint.h>
#include <stdlib.h>

SDL_AudioStream *stream = NULL;
SDL_AudioSpec spec;
uint16_t samples_per_frame = 0;
float *audio_buffer = NULL;

uint8_t init_audio(uint32_t ns_between_frames)
{
	if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL)) {
		SDL_Log("Failed to get audio device format: %s\n", SDL_GetError());
		return 0;
	}
	if (!(stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL))) {
		SDL_Log("Failed to open audio device stream: %s\n", SDL_GetError());
		return 0;
	}
	// 
	audio_buffer = malloc(spec.freq * (ns_between_frames / 1.0e9) * (sizeof(float) / sizeof(uint8_t)));
	return 1;
}

void destroy_audio()
{
	SDL_DestroyAudioStream(stream);
}
