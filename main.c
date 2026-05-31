#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// `clock_gettime` requires _POSIX_C_SOURCE >= 199309L
#include <time.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>

#define MEM_SIZE 4096
#define FONT_START 0x50	// 80
#define ROM_START 0x200	// 512
#define ROM_MAX 3894

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define NANOSEC 1000000000L

uint8_t memory[MEM_SIZE];
uint16_t pc = 0;
uint16_t I = 0;
uint16_t stack[16];
uint8_t delay = 0;
uint8_t sound = 0;
uint8_t v[16];

uint8_t const font[] = {
	0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
	0x20, 0x60, 0x20, 0x20, 0x70, // 1
	0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
	0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
	0x90, 0x90, 0xF0, 0x10, 0x10, // 4
	0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
	0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
	0xF0, 0x10, 0x20, 0x40, 0x40, // 7
	0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
	0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
	0xF0, 0x90, 0xF0, 0x90, 0x90, // A
	0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
	0xF0, 0x80, 0x80, 0x80, 0xF0, // C
	0xE0, 0x90, 0x90, 0x90, 0xE0, // D
	0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
	0xF0, 0x80, 0xF0, 0x80, 0x80  // F
};

uint8_t quit = 0;

uint8_t frame_buf[DISPLAY_HEIGHT * DISPLAY_WIDTH];
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;

void draw();

int rom_size = 0;

void dump_rom(uint8_t* rom, int len);
int load_rom(char* path, uint8_t* buffer);

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Usage:\n\t%s { rom_file_path }\n\t%s --dump-rom { rom_file_path }\n", argv[0], argv[0]);
		exit(0);
	}

	// Copy the font data into memory starting at `memory + FONT_START`.
	memcpy(memory + FONT_START, font, sizeof(font));
	rom_size = load_rom(argv[argc - 1], memory + ROM_START);

	if (strcmp(argv[1], "--dump-rom") == 0) {
		dump_rom(memory + ROM_START, rom_size);
		exit(0);
	}

	SDL_SetAppMetadata("CHIP-8", "0.0.0", "com.llebj.chip-8");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		SDL_Log("Failed to init: %s\n", SDL_GetError());
		exit(1);
	}
	if (!(window = SDL_CreateWindow("CHIP-8", DISPLAY_WIDTH * 10, DISPLAY_HEIGHT * 10, 0))) {
		SDL_Log("Failed to crate window: %s\n", SDL_GetError());
		exit(1);
	}
	if (!(renderer = SDL_CreateRenderer(window, NULL))) {
		SDL_Log("Failed to create renderer: %s\n", SDL_GetError());
		exit(1);
	}
	if (!SDL_SetRenderLogicalPresentation(
					renderer,
					DISPLAY_WIDTH,
					DISPLAY_HEIGHT,
					SDL_LOGICAL_PRESENTATION_LETTERBOX)) {
		SDL_Log("Failed to set render logical presentation: %s\n", SDL_GetError());
		exit(1);
	}
	if (!(texture = SDL_CreateTexture(renderer,
			SDL_PIXELFORMAT_ABGR32, SDL_TEXTUREACCESS_STREAMING,
			DISPLAY_WIDTH, DISPLAY_HEIGHT))) {
		SDL_Log("Failed to create texture: %s\n", SDL_GetError());
		exit(1);
	}
	if (!SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST)) {
		SDL_Log("Failed to set texture scale mode: %s\n", SDL_GetError());
		exit(1);
	}

	// Move the program counter to the start of the ROM
	pc = ROM_START;

	// Storage for the current opcode read during the fetch loop.
	uint16_t opcode = 0;

	const double period = 1.0 / 700;	// The target seconds per instruction
	double diff = 0,
	       remaining = 0;
	struct timespec t0,
			t1,
			wait;

	SDL_Event e;

	while (!quit) {
		while (SDL_PollEvent(&e)) {
			switch (e.type) {
			case SDL_EVENT_QUIT:
				quit = 1;
				break;
			case SDL_EVENT_KEY_UP:
				if (e.key.key == SDLK_ESCAPE) {
					quit = 1;
				}
				break;
			default:
				break;
			}
		}

		clock_gettime(CLOCK_MONOTONIC, &t0);

		// fetch
		// CHIP-8 instructions are two bytes. Therefore we are reading the
		// higher order byte first.
		opcode = (memory[pc++] & 0xFF) << 8;
		opcode |= (memory[pc++] & 0xFF);

		// decode
		// execute
		switch (opcode & 0xF000) {
		case 0x0000:
			switch (opcode) {
			case 0x00E0:	// O0E0: Clear screen
				for (uint8_t y = 0; y < DISPLAY_HEIGHT; ++y) {
					for (uint8_t x = 0; x < DISPLAY_WIDTH; ++x) {
						frame_buf[y * DISPLAY_WIDTH + x] = 0;
					}
				}
				draw();
				break;
			}
			break;
		case 0x1000:		// 1000: Jump
			pc = memory[opcode & 0x0FFF];
			break;
		case 0x6000:		// 6XNN: Set
			// Set VX to NN
			v[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
			break;
		case 0x7000:		// 7XNN: Add
			// Add NN to VX
			v[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
			break;
		case 0xA000:		// ANNN: Set index
			// Set index to NNN
			I = opcode & 0x0FFF; 
			break;
		case 0xD000:		// DXYN: Display
		{
			// Variables used to store values from the display instruction.
			uint8_t sx = v[(opcode & 0x0F00) >> 8] % DISPLAY_WIDTH,
				sy = v[(opcode & 0x00F0) >> 4] % DISPLAY_HEIGHT,
				height = opcode & 0x000F,
				line = 0;
			v[0xF] = 0;

			// We want to write the sprite data to N lines, starting at
			// Y. We need to ensure that we clip any lines that exceed
			// the height of the raster.
			for (uint8_t y = 0; y < height && sy + y < DISPLAY_HEIGHT; ++y) {
				line = memory[I + y];
				// We want to write the sprite data at `I + y` onto the
				// current line, starting at X. We need to clip
				// the data if it exceeds the width of the raster.
				// Sprites are always 8 pixels wide.
				for (uint8_t x = 0; x < 8 && sx + x < DISPLAY_WIDTH; ++x) {
					// `0x80 >> x` is used to mask off the x-th bit in
					// `line`, from most-significant to least-significant
					// bit.
					if ((line & (0x80 >> x)) == 0) {
						continue;
					}
					if (frame_buf[(sy + y) * DISPLAY_WIDTH + (sx + x)] == 1) {
						v[0xF] = 1;
					}
					frame_buf[(sy + y) * DISPLAY_WIDTH + (sx + x)] ^= 1;
				}
			}
			draw();
			break;
		}
		default:
			break;
		}

		clock_gettime(CLOCK_MONOTONIC, &t1);

		// Throttle execution time to `period`
		diff = (t1.tv_sec - t0.tv_sec) + 1.0e-9 * (t1.tv_nsec - t0.tv_nsec);
		remaining = period - diff;
		printf("time elapsed: %fs; waiting: %fs\n", diff, remaining > 0 ? remaining : 0);
		if (remaining > 0) {
			wait.tv_sec = (long)remaining;
			wait.tv_nsec = (long)(remaining * NANOSEC) % NANOSEC;
			nanosleep(&wait, NULL);
		}
	}

	SDL_DestroyTexture(texture);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void dump_rom(uint8_t* rom, int len)
{
	for (int i = 0; i < len; i++) {
		if (i % 16 == 0) {
			printf("%s%08x", i == 0 ? "" : "\n", i);
		}
		printf("%s%02x", i % 2 == 0 ? "    " : "", rom[i]);
	}
	printf("\n%08x\n", len);
}

int load_rom(char* path, uint8_t* buffer)
{
	unsigned int len = 0;
	FILE *fp;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Error: could not open %s\n", path);
		exit(1);
	}

	int c = '\0';
	uint8_t* ram_ptr = buffer;
	for ( ; (c = getc(fp)) != EOF && len < ROM_MAX; ++len) {
		*(ram_ptr++) = c;
	}
	if (c != EOF) {
		fprintf(stderr, "Error: rom file must be less than %d bytes\n", ROM_MAX);
		exit(1);
	}

	fclose(fp);
	return len;
}

// This function re-draws the entire screen each time. Can this be improved
// be only drawing the individual sprites?
void draw()
{
	uint32_t* pixels;
	int pitch, qpitch;

	if (!SDL_LockTexture(texture, NULL, (void**)&pixels, &pitch)) {
		SDL_Log("Failed to lock texture: %s\n", SDL_GetError());
		exit(1);
	}
	// pitch refers to the size in bytes so we divide by 4 to get the number
	// of 4-byte (32-bit) chuncks.
	qpitch = pitch / 4;
	for (int i = 0; i < DISPLAY_HEIGHT; i++) {
		for (int j = 0; j < DISPLAY_WIDTH; j++) {
			// The pitch of the texture is not guaranteed to be
			// equal to the display width; we are mapping the frame
			// buffer onto the pitched texture.
			*(pixels + i * qpitch + j) = frame_buf[i * DISPLAY_WIDTH + j] ? 0xffffffff : 0xff000000;
		}
	}

	SDL_UnlockTexture(texture);
	SDL_RenderTexture(renderer, texture, NULL, NULL);
	/* put the newly-cleared rendering on the screen. */
	SDL_RenderPresent(renderer);
}
