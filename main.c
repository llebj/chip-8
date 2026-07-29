#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_log.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_scancode.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "chip-8.h"

#define NANOSEC 1000000000L
#define FRAME_BUF_LEN (DISPLAY_HEIGHT * DISPLAY_WIDTH)

enum Mode {
	Chip8,
	SuperChip
};

struct opts {
	bool show_help;
	bool dump_rom;
	enum Mode mode;
	char* file_name;
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

void execute_loop(enum DisplayOp* display_op, struct input_state* input_state, enum Mode mode);
bool parse_opts(int argc, char** argv, struct opts* opts);
void show_help(char* file_name);

int rom_size = 0;

uint8_t frame_buf[FRAME_BUF_LEN];

int main(int argc, char** argv)
{
	if (argc < 2) {
		show_help(argv[0]);
		exit(0);
	}
	struct opts opts;
	opts.show_help = false;
	opts.mode = Chip8;
	opts.dump_rom = false;
	if (!parse_opts(argc, argv, &opts)) {
		printf("Failed to parse program options.\n");
		show_help(argv[0]);
		exit(1);
	}

	if (opts.show_help) {
		show_help(argv[0]);
		exit(0);
	}

	// Copy the font data into memory starting at `memory + FONT_START`.
	memcpy(memory + FONT_START, font, sizeof(font));
	if ((rom_size = load_rom(opts.file_name, memory + ROM_START)) == 0) {
		printf("Failed to load ROM.\n");
		exit(1);
	}

	if (opts.dump_rom) {
		dump_rom(memory + ROM_START, rom_size);
		exit(0);
	}

	SDL_SetAppMetadata("CHIP-8", "0.0.0", "com.llebj.chip-8");
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO)) {
		SDL_Log("Failed to init: %s\n", SDL_GetError());
		SDL_Quit();
		exit(1);
	}
	if (!init_display()) {
		SDL_Quit();
		exit(1);
	}
	if (!init_audio()) {
		SDL_Quit();
		exit(1);
	}

	const uint16_t cycles_ps = 720;
	const uint32_t ns_between_frames = (1.0 / TARGET_FPS) * 1.0e9;

	uint64_t t0 = 0,
	       t1 = 0,
	       t_delta = 0;

	SDL_srand(SDL_GetTicksNS());
	enum DisplayOp display_op = NOOP;
	bool quit = false;
	struct input_state input_state;
	memset(input_state.kb_state, 0, sizeof(input_state.kb_state));
	input_state.last_key = -1;

	// Move the program counter to the start of the ROM
	pc = ROM_START;
	while (!quit) {
		display_op = NOOP;
		// Most recently pressed key; -1 indicates no key pressed
		input_state.last_key = -1;
		t0 = SDL_GetTicksNS();

		if (delay > 0) {
			--delay;
		}
		if (sound > 0) {
			--sound;
			generate_samples();
		}

		quit = read_input(&input_state);

		// The emulator loop runs faster than the frame rate; we can fit
		// `cycles_ps / frames_ps` emulator cycles for each rendered frame.
		for (int inst = 0; inst < cycles_ps / TARGET_FPS; ++inst) {
			execute_loop(&display_op, &input_state, opts.mode);
		}

		if (display_op == DRAW) {
			draw(frame_buf, FRAME_BUF_LEN);
		}
		else if (display_op == CLEAR) {
			clear_screen();
		};

		// This method of timing is inaccurate and will lead to drift
		// against the target FPS.
		// TODO: Implement a more accurate delay mechanism.
		t1 = SDL_GetTicksNS();
		// Throttle execution time to `ns_between_frames`
		t_delta = t1 - t0;
		if (ns_between_frames > t_delta) {
			SDL_DelayNS(ns_between_frames - t_delta);
		}
	}

	destroy_display();
	destroy_audio();
	SDL_Quit();
}

void show_help(char* file_name)
{
	printf("Usage:\n"
		"\t%s [ --mode { ch8 | sch } ] <rom_file_path>\n"
		"\t%s --dump-rom <rom_file_path>\n", file_name, file_name);
}

bool parse_opts(int argc, char** argv, struct opts* opts)
{
	bool result = true;
	if (opts == NULL) {
		return false;
	}

	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--help") == 0) {
			opts->show_help = true;
		}
		else if (strcmp(argv[i], "--dump-rom") == 0) {
			opts->dump_rom = true;
		}
		else if (strcmp(argv[i], "--mode") == 0) {
			if (i + 1 >= argc) {
				// The `--mode` flag must be followed by an argument.
				result = false;
				continue;
			}
			char* mode = argv[++i];
			if (strcmp(mode, "ch8") == 0) {
				opts->mode = Chip8;
			}
			else if (strcmp(mode, "sch") == 0) {
				opts->mode = SuperChip;
			}
			else {
				// We have an un-defined mode string.
				result = false;
			}
		}
		else if (i + 1 == argc) {
			// The ROM file name must be the final argument.
			opts->file_name = argv[i];
		}
		else {
			// We have encountered an unknown argument.
			result = false;
		}
	}

	return result;
}

/*
 * Executes one iteration of the fetch-decode-execute loop. Inputs:
 *	`display_op`: A pointer to a variable of type `DisplayOp` which controls
 *		      display output operations; this function mutates that
 *		      external state.
 */
void execute_loop(enum DisplayOp* display_op, struct input_state* input_state, enum Mode mode)
{
	// TODO: Implement SUPER-CHIP.
	
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
		case 0x00C0:	// 00CN: Scroll down (SuperChip)
			// TODO: Implement.
			break;
		case 0x00E0:	// 00E0: Clear screen
			for (uint8_t y = 0; y < DISPLAY_HEIGHT; ++y) {
				for (uint8_t x = 0; x < DISPLAY_WIDTH; ++x) {
					frame_buf[y * DISPLAY_WIDTH + x] = 0;
				}
			}
			*display_op = CLEAR;
			break;
		case 0x00EE:	// 00EE: Subroutines
			// WARN: Potential bounds bug
			pc = stack[--stack_pointer];
			break;
		}
		case 0x00FB:	// 00FB: Scroll right (SuperChip)
			// TODO: Implement.
			break;
		case 0x00FC:	// 00FC: Scroll left (SuperChip)
			// TODO: Implement.
			break;
		case 0x00FD:	// 00FD: Exit (SuperChip)
			// TODO: Implement.
			break;
		case 0x00FE:	// 00FE: Lores (SuperChip)
			// TODO: Implement.
			break;
		case 0x00FF:	// 00FF: Hires (SuperChip)
			// TODO: Implement.
			break;
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
			if (mode == Chip8) {
				v[0xF] = 0;
			}
			break;
		case 0x0002:	// 8XY2: Binary AND
			v[(opcode & 0x0F00) >> 8] &= v[(opcode & 0x00F0) >> 4];
			if (mode == Chip8) {
				v[0xF] = 0;
			}
			break;
		case 0x0003:	// 8XY3: Binary XOR
			v[(opcode & 0x0F00) >> 8] ^= v[(opcode & 0x00F0) >> 4];
			if (mode == Chip8) {
				v[0xF] = 0;
			}
			break;
		case 0x0004:	// 8XY4: Add
		{
			// Set the carry flag if VX + VY overflows VX
			uint8_t vx = (opcode & 0x0F00) >> 8,
				vy = (opcode & 0x00F0) >> 4,
				carry = (0xFF - v[vx]) < v[vy];
			v[vx] += v[vy];
			v[0xF] = carry;
			break;
		}
		case 0x0005:	// 8XY5: Subtract
		{
			// Set the carry flag to 0 if we underflow and 1 otherwise
			uint8_t vx = (opcode & 0x0F00) >> 8,
				vy = (opcode & 0x00F0) >> 4,
				carry = v[vx] >= v[vy];
			v[vx] -= v[vy];
			v[0xF] = carry;
			break;
		}
		case 0x0006:	// 8XY6: Shift
		{
			uint8_t vx = (opcode & 0x0F00) >> 8,
				vy = (opcode & 0x00F0) >> 4,
				carry = 0;
			if (mode == Chip8) {
				v[vx] = v[vy];
			}
			// Set the flag register to the value of the shifted-out bit.
			carry = v[vx] & 1;
			v[vx] >>= 1;
			v[0xF] = carry;
			break;
		}
		case 0x0007:	// 8XY7: Subtract
		{
			// Set the carry flag to 0 if we underflow and 1 otherwise
			uint8_t vx = (opcode & 0x0F00) >> 8,
				vy = (opcode & 0x00F0) >> 4,
				carry = v[vy] >= v[vx];
			v[vx] = v[vy] - v[vx];
			v[0xF] = carry;
			break;
		}
		case 0x000E:	// 8XYE: Shift
		{
			uint8_t vx = (opcode & 0x0F00) >> 8,
				vy = (opcode & 0x00F0) >> 4,
				carry = 0;
			// Set the flag register to the value of the shifted-out bit.
			if (mode == Chip8) {
				carry = (v[vy] & 0x80) >> 7;
				v[vx] = v[vy] << 1;
			}
			else {
				carry = (v[vx] & 0x80) >> 7;
				v[vx] <<= 1;
			}
			v[0xF] = carry;
			break;
		}
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
	case 0xB000:		// BNNN: Jump with offset
		pc = (opcode & 0x0FFF) + v[mode == Chip8 ? 0 : (opcode & 0x0F00) >> 8];
		break;
	case 0xC000:		// CXNN: Random
		v[(opcode & 0x0F00) >> 8] = SDL_rand(UINT16_MAX) & (opcode & 0x00FF);
		break;
	case 0xD000:
	{
		switch (opcode & 0x000F) {
		case 0x0000:	// DXY0: Draw 16x16 (SuperChip)
			if (mode == SuperChip) {
				// TODO: Implement.
				break;
			}
		default:	// DXYN: Draw
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
		}
	}
	case 0xE000:
		switch (opcode & 0x0FF) {
		case 0x009E:	// E09E: Skip if key
			if (input_state->kb_state[v[(opcode & 0x0F00) >> 8]]) {
				pc += 2;
			}
			break;
		case 0x00A1:	// E0A1: Skip if key
			if (!input_state->kb_state[v[(opcode & 0x0F00) >> 8]]) {
				pc += 2;
			}
			break;
		default:
			break;
		}
	case 0xF000:
		switch (opcode & 0x00FF) {
		case 0x0007:	// FX07: Store delay
			v[(opcode & 0x0F00) >> 8] = delay;
			break;
		case 0x000A:	// FX0A: Get key
			if (input_state->last_key == -1) {
				// Halt execution
				pc -= 2;
			}
			else {
				v[(opcode & 0x0F00) >> 8] = input_state->last_key;
			}
			break;
		case 0x0015:	// FX15: Read delay
			delay = v[(opcode & 0x0F00) >> 8];
			break;
		case 0x0018:	// FX18: Store sound
			sound = v[(opcode & 0x0F00) >> 8];
			break;
		case 0x001E:	// FX1E: Add to index
			I += v[(opcode & 0x0F00) >> 8];
			break;
		case 0x0029:	// FX29: Font character
			// The bit pattern `0x0300` masks off the first nibble
			// of X.
			// Font characters are 5 bytes long.
			I = FONT_START + 5 * ((opcode & 0x0300) >> 8);
			break;
		case 0x0030:	// FX30: Set large hex character (SuperChip)
			// TODO: Implement.
			break;
		case 0x0033:	// FX33: Binary-coded decimal conversion
		{
			// uint8_t max is 255 so we are always going to be storing
			// 3 digits.
			for (uint8_t i = 3, vx = v[(opcode & 0x0F00) >> 8]; i > 0; --i, vx /= 10) {
				memory[I + i - 1] = vx % 10;
			}
			break;
		}
		case 0x0055:	// FX55: Store memory
		{
			uint8_t registers = ((opcode & 0x0F00) >> 8) + 1;
			for (uint8_t i = 0; i < registers; ++i) {
				memory[I + i] = v[i];
			}
			if (mode == Chip8) {
				I += registers;
			}
			break;
		}
		case 0x0065:	// 0xFX65: Load memory
		{
			uint8_t registers = ((opcode & 0x0F00) >> 8) + 1;
			for (uint8_t i = 0; i < registers; ++i) {
				v[i] = memory[I + i];
			}
			if (mode == Chip8) {
				I += registers;
			}
			break;
		}
		case 0x0075:	// FX75: Save to flag register (SuperChip)
			// TODO: Implement.
			break;
		case 0x0085:	// FX85: Restore from flag register (SuperChip)
			// TODO: Implement.
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

