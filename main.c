#include <SDL3/SDL_init.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>

#define MEM_SIZE 4096
#define FONT_START 0x50	// 80
#define ROM_START 0x200	// 512
#define ROM_MAX 3894

unsigned char mem[MEM_SIZE];
unsigned char const font[] = {
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

void dump_rom(unsigned char* rom, int len);
int load_rom(char* path, unsigned char* buffer);

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Usage:\n\t%s { rom_file_path }\n", argv[0]);
		exit(0);
	}

	SDL_Window *window = NULL;
	SDL_Renderer *renderer = NULL;

	SDL_SetAppMetadata("CHIP-8", "0.0.0", "com.llebj.chip-8");
	SDL_Init(SDL_INIT_VIDEO);
	SDL_CreateWindowAndRenderer("CHIP-8", 640, 320, SDL_WINDOW_MAXIMIZED, &window, &renderer);
	SDL_SetRenderLogicalPresentation(renderer, 64, 32, SDL_LOGICAL_PRESENTATION_LETTERBOX);

	SDL_SetRenderDrawColorFloat(renderer, 255, 0, 0, SDL_ALPHA_OPAQUE_FLOAT);
	/* clear the window to the draw color. */
	SDL_RenderClear(renderer);
	/* put the newly-cleared rendering on the screen. */
	SDL_RenderPresent(renderer);

	while (1) {
		continue;
	}

	// Copy the font data into memory starting at `mem + FONT_START`.
	/*
	memcpy(mem + FONT_START, font, sizeof(font));
	rom_size = load_rom(argv[1], mem + ROM_START);
	dump_rom(mem + ROM_START, rom_size);
	*/

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();
}

void dump_rom(unsigned char* rom, int len)
{
	for (int i = 0; i < len; i++) {
		if (i % 16 == 0) {
			printf("%s%08x", i == 0 ? "" : "\n", i);
		}
		printf("%s%02x", i % 2 == 0 ? "    " : "", rom[i]);
	}
	printf("\n%08x\n", len);
}

int load_rom(char* path, unsigned char* buffer)
{
	unsigned int len = 0;
	FILE *fp;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Error: could not open %s\n", path);
		exit(1);
	}

	int c = '\0';
	unsigned char* ram_ptr = buffer;
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
