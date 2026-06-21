#include <stdint.h>
#include <stdlib.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_render.h>

#include "chip-8.h"

SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

// Initialises the display resources. Returns a bit flag indicating success.
uint8_t init_display()
{
	if (!(window = SDL_CreateWindow("CHIP-8", DISPLAY_WIDTH * 10, DISPLAY_HEIGHT * 10, 0))) {
		SDL_Log("Failed to crate window: %s\n", SDL_GetError());
		return 0;
	}
	if (!(renderer = SDL_CreateRenderer(window, NULL))) {
		SDL_Log("Failed to create renderer: %s\n", SDL_GetError());
		return 0;
	}
	if (!SDL_SetRenderVSync(renderer, 1)) {
		SDL_Log("Failed to configure render vsync: %s\n", SDL_GetError());
		return 0;
	}
	if (!SDL_SetRenderLogicalPresentation(
					renderer,
					DISPLAY_WIDTH,
					DISPLAY_HEIGHT,
					SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
		SDL_Log("Failed to set render logical presentation: %s\n", SDL_GetError());
		return 0;
	}
	if (!(texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ABGR32, SDL_TEXTUREACCESS_STREAMING,
			DISPLAY_WIDTH, DISPLAY_HEIGHT))) {
		SDL_Log("Failed to create texture: %s\n", SDL_GetError());
		return 0;
	}
	if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
		SDL_Log("Failed to set texture scale mode: %s\n", SDL_GetError());
		return 0;
	}
	return 1;
}

void destroy_display()
{
	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
}

void clear_screen()
{
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	SDL_RenderClear(renderer);
}

// This function re-draws the entire screen each time. Can this be improved
// be only drawing the individual sprites?
void draw(uint8_t frame_buf[], size_t len)
{
	clear_screen();

	// SDL_PIXELFORMAT_ABGR32 uses 32-bit pixels.
	uint32_t* pixels;
	int pitch, qpitch;

	if (!SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch)) {
		SDL_Log("Failed to lock texture: %s\n", SDL_GetError());
		exit(1);
	}
	// pitch refers to the size in bytes so we divide by 4 to get the number
	// of 32-bit chunks i.e., a single pixel.
	qpitch = pitch / 4;
	for (uint16_t i = 0; i < DISPLAY_HEIGHT; i++) {
		for (uint16_t j = 0, p = 0; j < DISPLAY_WIDTH; j++) {
			p = i * DISPLAY_WIDTH + j;
			if (p >= len) {
				break;
			}
			// The pitch of the texture is not guaranteed to be
			// equal to the display width; we are mapping the frame
			// buffer onto the pitched texture.
			*(pixels + i * qpitch + j) = frame_buf[p] ? 0xffffffff : 0xff000000;
		}
	}

	SDL_UnlockTexture(texture);
	SDL_RenderTexture(renderer, texture, NULL, NULL);
	/* put the newly-cleared rendering on the screen. */
	SDL_RenderPresent(renderer);
}

