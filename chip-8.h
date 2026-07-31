#ifndef CHIP8
#define CHIP8

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEM_SIZE	4096
#define FONT_START	0x50	// 80
#define ROM_START	0x200	// 512
#define ROM_MAX		(MEM_SIZE - ROM_START)

#define TARGET_FPS	60

// Use the maximum possible frame buffer size (SuperChip)
#define DISPLAY_WIDTH 128
#define DISPLAY_HEIGHT 64
#define FRAME_BUF_MAX (DISPLAY_WIDTH * DISPLAY_HEIGHT)

void dump_rom(uint8_t* rom, int len);
int load_rom(char* path, uint8_t* buffer);

// --------
// Chip-8 -
// --------

enum Mode {
	Chip8,
	SuperChip
};
struct chip8 {
	enum Mode mode;
	uint8_t memory[MEM_SIZE];
	uint8_t v[16];
	uint16_t pc;
	uint16_t I;
	uint16_t stack[16];
	uint8_t stack_pointer;
	struct framebuffer* frame_buf;
	uint8_t delay;
	uint8_t sound;
};
struct framebuffer {
	uint8_t buffer[FRAME_BUF_MAX];
	uint8_t width;
	uint8_t height;
};

// ---------
// Display -
// ---------

enum DisplayOp {
	NOOP,
	DRAW,
	CLEAR
};

void clear_screen();
void draw(uint8_t frame_buf[], size_t len);
uint8_t init_display();
void destroy_display();

// -------
// Audio -
// -------

uint8_t init_audio();
void destroy_audio();
void generate_samples();

// -------
// Input -
// -------
struct input_state {
	bool kb_state[16];
	int8_t last_key;
};

bool read_input(struct input_state *state);

#endif
