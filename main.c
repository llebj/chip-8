#define _POSIX_C_SOURCE 199309L

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// `clock_gettime` requires _POSIX_C_SOURCE >= 199309L
#include <time.h>

#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL.h>

#define MEM_SIZE 4096
#define FONT_START 0x50	// 80
#define ROM_START 0x200	// 512
#define ROM_MAX 3894

#define NANOSEC 1000000000L

uint8_t memory[MEM_SIZE];
uint8_t *pc = NULL;
uint16_t i = 0;
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

	// Move the program counter to the start of the ROM
	pc = memory + ROM_START;

	// Storage for the current opcode read during the fetch loop.
	uint16_t opcode = 0;

	const double period = 1.0 / 700;	// The target seconds per instruction
	double diff = 0,
	       remaining = 0;
	struct timespec t0,
			t1,
			wait;

	while (1) {
		clock_gettime(CLOCK_MONOTONIC, &t0);

		// fetch
		// CHIP-8 instructions are two bytes. Therefore we are reading the
		// higher order byte first.
		opcode = (*pc++ & 0xFF) << 8;
		opcode |= (*pc++ & 0xFF);

		// decode
		// execute

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


SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;

void sdl_init()
{
	SDL_SetAppMetadata("CHIP-8", "0.0.0", "com.llebj.chip-8");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer("CHIP-8", 640, 320, SDL_WINDOW_MAXIMIZED, &window, &renderer);
	SDL_SetRenderLogicalPresentation(renderer, 64, 32, SDL_LOGICAL_PRESENTATION_LETTERBOX);
}

void sdl_destroy()
{
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void draw()
{
	SDL_SetRenderDrawColorFloat(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE_FLOAT);
	/* clear the window to the draw color. */
	SDL_RenderClear(renderer);
	/* put the newly-cleared rendering on the screen. */
	SDL_RenderPresent(renderer);
}
