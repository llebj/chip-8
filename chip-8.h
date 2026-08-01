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

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32
#define FRAME_BUF_LEN (DISPLAY_HEIGHT * DISPLAY_WIDTH)

enum DisplayOp {
	NOOP,
	DRAW,
	CLEAR
};
enum OpCode {
	OP_00E0,  // Clear screen
	OP_00EE,  // Subroutines (return)

	OP_1NNN,  // Jump
	OP_2NNN,  // Subroutines (call)
	OP_3XNN,  // Skip conditionally
	OP_4XNN,  // Skip conditionally
	OP_5XY0,  // Skip conditionally
	OP_6XNN,  // Set
	OP_7XNN,  // Add

	OP_8XY0,  // Set
	OP_8XY1,  // Binary OR
	OP_8XY2,  // Binary AND
	OP_8XY3,  // Binary XOR
	OP_8XY4,  // Add
	OP_8XY5,  // Subtract
	OP_8XY6,  // Shift
	OP_8XY7,  // Subtract
	OP_8XYE,  // Shift

	OP_9XY0,  // Skip conditionally
	OP_ANNN,  // Set index
	OP_BNNN,  // Jump with offset
	OP_CXNN,  // Random
	OP_DXYN,  // Display

	OP_EX9E,  // Skip if key
	OP_EXA1,  // Skip if key

	OP_FX07,  // Store delay
	OP_FX0A,  // Get key
	OP_FX15,  // Read delay
	OP_FX18,  // Store sound
	OP_FX1E,  // Add to index
	OP_FX29,  // Font character
	OP_FX33,  // Binary-coded decimal conversion
	OP_FX55,  // Store memory
	OP_FX65,  // Load memory

	OP_UNKNOWN, // decode failed to match a known opcode
};

struct instruction {
	enum OpCode opcode;
	uint16_t opcode_raw;
	uint8_t X;
	uint8_t Y;
	uint8_t N;
	uint8_t NN;
	uint16_t NNN;
};
struct input_state {
	bool kb_state[16];
	int8_t last_key;
};
struct vm_state {
	uint8_t memory[MEM_SIZE];
	uint16_t pc;
	uint16_t I;
	uint8_t v[16];
	uint16_t stack[16];
	uint8_t stack_pointer;
	uint8_t delay;
	uint8_t sound;
	uint8_t frame_buf[FRAME_BUF_LEN];
};

void dump_rom(uint8_t* rom, int len);
int load_rom(char* path, uint8_t* buffer);

// ------
// - VM -
// ------

uint16_t fetch(struct vm_state* vm_state);
struct instruction decode(uint16_t opcode);
void execute(struct instruction *inst, struct vm_state *vm_state,
	enum DisplayOp *display_op, struct input_state *input_state);

// -----------
// - Display -
// -----------

void clear_screen();
void draw(uint8_t frame_buf[], size_t len);
uint8_t init_display();
void destroy_display();

// ---------
// - Audio -
// ---------

uint8_t init_audio();
void destroy_audio();
void generate_samples();

// ---------
// - Input -
// ---------

bool read_input(struct input_state *state);

#endif
