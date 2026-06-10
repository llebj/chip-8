#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "chip-8.h"

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define NANOSEC 1000000000L

enum DisplayOp {
	NOOP,
	DRAW,
	CLEAR
};

uint8_t memory[MEM_SIZE];
uint16_t pc = 0;
uint16_t I = 0;
uint16_t stack[16];
uint8_t stack_pointer = 0;
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
void execute_loop(enum DisplayOp* display_op);

int rom_size = 0;

uint8_t frame_buf[DISPLAY_HEIGHT * DISPLAY_WIDTH];
SDL_Window *window = NULL;
SDL_Renderer *renderer = NULL;
SDL_Texture *texture = NULL;
void clear_screen();
void draw();
uint8_t init_display();
void destroy_display();

int main(int argc, char** argv)
{
	if (argc < 2) {
		printf("Usage:\n\t%s { rom_file_path }\n\t%s --dump-rom { rom_file_path }\n", argv[0], argv[0]);
		exit(0);
	}

	// Copy the font data into memory starting at `memory + FONT_START`.
	memcpy(memory + FONT_START, font, sizeof(font));
	if ((rom_size = load_rom(argv[argc - 1], memory + ROM_START)) == 0) {
		printf("Failed to load ROM.");
		exit(1);
	}

	if (strcmp(argv[1], "--dump-rom") == 0) {
		dump_rom(memory + ROM_START, rom_size);
		exit(0);
	}

	SDL_SetAppMetadata("CHIP-8", "0.0.0", "com.llebj.chip-8");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
		SDL_Log("Failed to init: %s\n", SDL_GetError());
		SDL_Quit();
		exit(1);
	}
	if (!init_display()) {
		SDL_Quit();
		exit(1);
	}

	const uint16_t frames_ps = 60,
		       cycles_ps = 720;
	const uint32_t ns_between_frames = (1.0 / frames_ps) * 1.0e9;

	uint64_t t0 = 0,
	       t1 = 0,
	       t_delta = 0;

	SDL_Event e;
	enum DisplayOp display_op = NOOP;

	// Move the program counter to the start of the ROM
	pc = ROM_START;
	while (!quit) {
		t0 = SDL_GetTicksNS();

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

		// The emulator loop runs faster than the frame rate; we can fit
		// `cycles_ps / frames_ps` emulator cycles for each rendered frame.
		for (int inst = 0; inst < cycles_ps / frames_ps; ++inst) {
			execute_loop(&display_op);
		}

		if (display_op == DRAW) {
			draw();
			display_op = NOOP;
		}
		else if (display_op == CLEAR) {
			clear_screen();
			display_op = NOOP;
		}

		t1 = SDL_GetTicksNS();

		// Throttle execution time to `ns_between_frames`
		t_delta = t1 - t0;
		if (ns_between_frames > t_delta) {
			SDL_DelayNS(ns_between_frames - t_delta);
		}
	}

	destroy_display();
	SDL_Quit();
}

/*
 * Executes one iteration of the fetch-decode-execute loop. Inputs:
 *	`display_op`: A pointer to a variable of type `DisplayOp` which controls
 *		      display output operations; this function mutates that
 *		      external state.
 */
void execute_loop(enum DisplayOp* display_op)
{
	// Storage for the current opcode read during the fetch loop.
	uint16_t opcode = 0;

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
			*display_op = CLEAR;
			break;
		case 0x00EE:	// OOEE: Subroutines
			// WARN: Potential bounds bug
			pc = stack[--stack_pointer];
			break;
		}
		break;
	case 0x1000:		// 1000: Jump
		pc = opcode & 0x0FFF;
		break;
	case 0x2000:		// 2000: Subroutines
		// WARN: Potential bounds bug
		stack[stack_pointer++] = pc;
		pc = opcode & 0x0FFF;
		break;
	case 0x3000:		// 3XNN: Skip conditionally
		if (v[(opcode & 0x0F00) >> 8] == (opcode & 0x00FF)) {
			pc += 2;
		}
		break;
	case 0x4000:		// 4XNN: Skip conditionally
		if (v[(opcode & 0x0F00) >> 8] != (opcode & 0x00FF)) {
			pc += 2;
		}
		break;
	case 0x5000:		// 5XY0: Skip conditionally
		if (v[(opcode & 0x0F00) >> 8] == v[(opcode & 0x00F0) >> 4]) {
			pc += 2;
		}
		break;
	case 0x6000:		// 6XNN: Set
		// Set VX to NN
		v[(opcode & 0x0F00) >> 8] = opcode & 0x00FF;
		break;
	case 0x7000:		// 7XNN: Add
		// Add NN to VX
		v[(opcode & 0x0F00) >> 8] += opcode & 0x00FF;
		break;
	case 0x8000:
		switch (opcode & 0x000F) {
		case 0x0000:	// 8XY0: Set
			v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0001:	// 8XY1: Binary OR
			v[(opcode & 0x0F00) >> 8] |= v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0002:	// 8XY2: Binary AND
			v[(opcode & 0x0F00) >> 8] &= v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0003:	// 8XY3: Binary XOR
			v[(opcode & 0x0F00) >> 8] ^= v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0004:	// 8XY4: Add
			// Set the carry flag if VX + VY overflows VX
			v[0xF] = (0xFF - v[(opcode & 0x0F00) >> 8]) < v[(opcode & 0x00F0) >> 4];
			v[(opcode & 0x0F00) >> 8] += v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0005:	// 8XY5: Subtract
			// Set the carry flag to 0 if we underflow and 1 otherwise
			v[0xF] = v[(opcode & 0x0F00) >> 8] >= v[(opcode & 0x00F0) >> 4];
			v[(opcode & 0x0F00) >> 8] -= v[(opcode & 0x00F0) >> 4];
			break;
		case 0x0006:	// 8XY6: Shift
			// WARN: Ambiguous isntruction; implemented COSMAC VIP behaviour.
			v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4];
			// Set the flag register to the value of the shifted-out bit.
			v[0xF] = v[(opcode & 0x0F00) >> 8] & 1;
			v[(opcode & 0x0F00) >> 8] >>= 1;
			break;
		case 0x0007:	// 8XY7: Subtract
			// Set the carry flag to 0 if we underflow and 1 otherwise
			v[0xF] = v[(opcode & 0x00F0) >> 4] >= v[(opcode & 0x0F00) >> 8];
			v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4] - v[(opcode & 0x0F00) >> 8];
			break;
		case 0x000E:	// 8XYE: Shift
			// WARN: Ambiguous isntruction; implemented COSMAC VIP behaviour.
			v[(opcode & 0x0F00) >> 8] = v[(opcode & 0x00F0) >> 4];
			// Set the flag register to the value of the shifted-out bit.
			v[0xF] = v[(opcode & 0x0F00) >> 8] & 0x80;
			v[(opcode & 0x0F00) >> 8] <<= 1;
			break;
		}
		break;
	case 0x9000:		// 9XY0: Skip conditionally
		if (v[(opcode & 0x0F00) >> 8] != v[(opcode & 0x00F0) >> 4]) {
			pc += 2;
		}
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
		*display_op = DRAW;
		break;
	}
	case 0xF000:
		switch (opcode & 0x00FF) {
		case 0x001E:	// 0xFX1E: Add to index
			I += v[(opcode & 0x0F00) >> 8];
			break;
		case 0x0033:	// 0xFX33: Binary-coded decimal conversion
		{
			// uint8_t max is 255 so we are always going to be storing
			// 3 digits.
			for (uint8_t i = 3, vx = v[(opcode & 0x0F00) >> 8]; i > 0; --i, vx /= 10) {
				memory[I + i - 1] = vx % 10;
			}
			break;
		}
		case 0x0055:	// 0xFX55: Store memory
		{
			// WARN: Ambiguous isntruction; implemented COSMAC VIP behaviour.
			uint8_t registers = ((opcode & 0x0F00) >> 8) + 1;
			for (uint8_t i = 0; i < registers; ++i) {
				memory[I + i] = v[i];
			}
			I += registers;
			break;
		}
		case 0x0065:	// 0xFX65: Load memory
		{
			// WARN: Ambiguous isntruction; implemented COSMAC VIP behaviour.
			uint8_t registers = ((opcode & 0x0F00) >> 8) + 1;
			for (uint8_t i = 0; i < registers; ++i) {
				v[i] = memory[I + i];
			}
			I += registers;
			break;
		}
		default:
			break;
		}
		break;
	default:
		break;
	}
}

void clear_screen()
{
	SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0xff);
	SDL_RenderClear(renderer);
}

// This function re-draws the entire screen each time. Can this be improved
// be only drawing the individual sprites?
void draw()
{
	clear_screen();

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
