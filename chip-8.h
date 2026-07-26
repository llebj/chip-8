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

void dump_rom(uint8_t* rom, int len);
int load_rom(char* path, uint8_t* buffer);

// ---------
// Display -
// ---------

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

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
