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
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include "chip-8.h"

#define NANOSEC 1000000000L

enum Mode {
	Chip8,
};

struct opts {
	bool show_help;
	bool dump_rom;
	enum Mode mode;
	char* file_name;
};

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

bool parse_opts(int argc, char** argv, struct opts* opts);
void show_help(char* file_name);

int rom_size = 0;

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

	struct vm_state vm_state = {
		.delay = 0,
		.I = 0,
		.pc = 0,
		.sound = 0,
		.stack = {0},
		.stack_pointer = 0,
		.v = {0},
		.frame_buf = {0},
	};

	// Copy the font data into memory starting at `memory + FONT_START`.
	memcpy(vm_state.memory + FONT_START, font, sizeof(font));
	if ((rom_size = load_rom(argv[argc - 1], vm_state.memory + ROM_START)) == 0) {
		printf("Failed to load ROM.\n");
		exit(1);
	}

	if (opts.dump_rom) {
		dump_rom(vm_state.memory + ROM_START, rom_size);
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
	struct input_state input_state = {
		.kb_state = {0},
		.last_key = -1
	};

	// Move the program counter to the start of the ROM
	vm_state.pc = ROM_START;
	while (!quit) {
		display_op = NOOP;
		// Most recently pressed key; -1 indicates no key pressed
		input_state.last_key = -1;
		t0 = SDL_GetTicksNS();

		if (vm_state.delay > 0) {
			--vm_state.delay;
		}
		if (vm_state.sound > 0) {
			--vm_state.sound;
			generate_samples();
		}

		quit = read_input(&input_state);

		// The emulator loop runs faster than the frame rate; we can fit
		// `cycles_ps / frames_ps` emulator cycles for each rendered frame.
		for (int inst = 0; inst < cycles_ps / TARGET_FPS; ++inst) {
			uint16_t opcode = fetch(&vm_state);
			struct instruction inst = decode(opcode);
			execute(&inst, &vm_state, &display_op, &input_state);
		}

		if (display_op == DRAW) {
			draw(vm_state.frame_buf, FRAME_BUF_LEN);
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
		"\t%s [ --mode { ch8 } ] <rom_file_path>\n"
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

