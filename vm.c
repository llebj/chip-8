#include <inttypes.h>

#include <SDL3/SDL_stdinc.h>
#include <stdint.h>

#include "chip-8.h"

// TODO: Implement SUPER-CHIP.

uint16_t fetch(struct vm_state* vm_state)
{
	// Storage for the current opcode read during the fetch loop.
	uint16_t opcode = 0;

	// fetch
	// CHIP-8 instructions are two bytes. Therefore we are reading the
	// higher order byte first.
	opcode = (vm_state->memory[vm_state->pc++] & 0xFF) << 8;
	opcode |= (vm_state->memory[vm_state->pc++] & 0xFF);

	return opcode;
}

struct instruction decode(uint16_t opcode)
{
	struct instruction result = {
		.opcode = OP_UNKNOWN,
		.opcode_raw = opcode,
		.X = (opcode & 0x0f00) >> 8,
		.Y = (opcode & 0x00f0) >> 4,
		.N = (opcode & 0x000f),
		.NN = (opcode & 0x00ff),
		.NNN = (opcode & 0x0fff),
	};

	// Nested switch is required as opcodes cannot be identified using the
	// first nibble exclusively.
	switch (opcode & 0xF000) {
	case 0x0000:
		switch (opcode) {
		case 0x00E0:	// O0E0: Clear screen
			result.opcode = OP_00E0;
			break;
		case 0x00EE:	// OOEE: Subroutines
			result.opcode = OP_00EE;
			break;

		default:
			break;
		}
		break;
	case 0x1000:		// 1000: Jump
		result.opcode = OP_1NNN;
		break;
	case 0x2000:		// 2000: Subroutines
		result.opcode = OP_2NNN;
		break;
	case 0x3000:		// 3XNN: Skip conditionally
		result.opcode = OP_3XNN;
		break;
	case 0x4000:		// 4XNN: Skip conditionally
		result.opcode = OP_4XNN;
		break;
	case 0x5000:		// 5XY0: Skip conditionally
		result.opcode = OP_5XY0;
		break;
	case 0x6000:		// 6XNN: Set
		result.opcode = OP_6XNN;
		break;
	case 0x7000:		// 7XNN: Add
		result.opcode = OP_7XNN;
		break;
	case 0x8000:
		switch (opcode & 0x000F) {
		case 0x0000:	// 8XY0: Set
			result.opcode = OP_8XY0;
			break;
		case 0x0001:	// 8XY1: Binary OR
			result.opcode = OP_8XY1;
			break;
		case 0x0002:	// 8XY2: Binary AND
			result.opcode = OP_8XY2;
			break;
		case 0x0003:	// 8XY3: Binary XOR
			result.opcode = OP_8XY3;
			break;
		case 0x0004:	// 8XY4: Add
			result.opcode = OP_8XY4;
			break;
		case 0x0005:	// 8XY5: Subtract
			result.opcode = OP_8XY5;
			break;
		case 0x0006:	// 8XY6: Shift
			result.opcode = OP_8XY6;
			break;
		case 0x0007:	// 8XY7: Subtract
			result.opcode = OP_8XY7;
			break;
		case 0x000E:	// 8XYE: Shift
			result.opcode = OP_8XYE;
			break;
		default:
			break;
		}
		break;
	case 0x9000:		// 9XY0: Skip conditionally
		result.opcode = OP_9XY0;
		break;
	case 0xA000:		// ANNN: Set index
		// Set index to NNN
		result.opcode = OP_ANNN; 
		break;
	case 0xB000:		// BNNN: Jump with offset
		result.opcode = OP_BNNN;
		break;
	case 0xC000:		// CXNN: Random
		result.opcode = OP_CXNN;
		break;
	case 0xD000:		// DXYN: Display
		result.opcode = OP_DXYN;
		break;
	case 0xE000:
		switch (opcode & 0x0FF) {
		case 0x009E:	// 0xEX9E: Skip if key
			result.opcode = OP_EX9E;
			break;
		case 0x00A1:	// 0xEXA1: Skip if key
			result.opcode = OP_EXA1;
			break;
		default:
			break;
		}
		break;
	case 0xF000:
		switch (opcode & 0x00FF) {
		case 0x0007:	// 0xFX07: Store delay
			result.opcode = OP_FX07;
			break;
		case 0x000A:	// 0xFX0A: Get key
			result.opcode = OP_FX0A;
			break;
		case 0x0015:	// 0xFX15: Read delay
			result.opcode = OP_FX15;
			break;
		case 0x0018:	// 0xFX18: Store sound
			result.opcode = OP_FX18;
			break;
		case 0x001E:	// 0xFX1E: Add to index
			result.opcode = OP_FX1E;
			break;
		case 0x0029:	// 0xFX29: Font character
			result.opcode = OP_FX29;
			break;
		case 0x0033:	// 0xFX33: Binary-coded decimal conversion
			result.opcode = OP_FX33;
			break;
		case 0x0055:	// 0xFX55: Store memory
			result.opcode = OP_FX55;
			break;
		case 0x0065:	// 0xFX65: Load memory
			result.opcode = OP_FX65;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return result;
}

void execute(struct instruction *inst, struct vm_state *vm_state,
		enum DisplayOp *display_op, struct input_state *input_state)
{
	switch (inst->opcode) {
	case OP_00E0:	// Clear screen
		for (uint8_t y = 0; y < DISPLAY_HEIGHT; ++y) {
			for (uint8_t x = 0; x < DISPLAY_WIDTH; ++x) {
				vm_state->frame_buf[y * DISPLAY_WIDTH + x] = 0;
			}
		}
		*display_op = CLEAR;
		break;
	case OP_00EE:	// Subroutines (return)
		// WARN: Potential bounds bug
		vm_state->pc = vm_state->stack[--vm_state->stack_pointer];
		break;
	case OP_1NNN:	// Jump
		vm_state->pc = inst->NNN;
		break;
	case OP_2NNN:	// Subroutines (call)
		// WARN: Potential bounds bug
		vm_state->stack[vm_state->stack_pointer++] = vm_state->pc;
		vm_state->pc = inst->NNN;
		break;
	case OP_3XNN:	// Skip conditionally
		if (vm_state->v[inst->X] == inst->NN) {
			vm_state->pc += 2;
		}
		break;
	case OP_4XNN:	// Skip conditionally
		if (vm_state->v[inst->X] != inst->NN) {
			vm_state->pc += 2;
		}
		break;
	case OP_5XY0:	// Skip conditionally
		if (vm_state->v[inst->X] == vm_state->v[inst->Y]) {
			vm_state->pc += 2;
		}
		break;
	case OP_6XNN:	// Set
		vm_state->v[inst->X] = inst->NN;
		break;
	case OP_7XNN:	// Add
		vm_state->v[inst->X] += inst->NN;
		break;
	case OP_8XY0:	// Set
		vm_state->v[inst->X] = vm_state->v[inst->Y];
		break;
	case OP_8XY1:	// Binary OR
		vm_state->v[inst->X] |= vm_state->v[inst->Y];
		vm_state->v[0xF] = 0;
		break;
	case OP_8XY2:	// Binary AND
		vm_state->v[inst->X] &= vm_state->v[inst->Y];
		vm_state->v[0xF] = 0;
		break;
	case OP_8XY3:	// Binary XOR
		vm_state->v[inst->X] ^= vm_state->v[inst->Y];
		vm_state->v[0xF] = 0;
		break;
	case OP_8XY4:	// Add
	{
		// Set the carry flag if VX + VY overflows VX
		uint8_t carry = (0xFF - vm_state->v[inst->X]) < vm_state->v[inst->Y];
		vm_state->v[inst->X] += vm_state->v[inst->Y];
		vm_state->v[0xF] = carry;
		break;
	}
	case OP_8XY5:	// Subtract
	{
		// Set the carry flag to 0 if we underflow and 1 otherwise
		uint8_t carry = vm_state->v[inst->X] >= vm_state->v[inst->Y];
		vm_state->v[inst->X] -= vm_state->v[inst->Y];
		vm_state->v[0xF] = carry;
		break;
	}
	case OP_8XY6:	// Shift
	{
		// WARN: Ambiguous instruction; implemented COSMAC VIP behaviour.
		uint8_t carry;
		vm_state->v[inst->X] = vm_state->v[inst->Y];
		// Set the flag register to the value of the shifted-out bit.
		carry = vm_state->v[inst->X] & 1;
		vm_state->v[inst->X] >>= 1;
		vm_state->v[0xF] = carry;
		break;
	}
	case OP_8XY7:	// Subtract
	{
		// Set the carry flag to 0 if we underflow and 1 otherwise
		uint8_t carry = vm_state->v[inst->Y] >= vm_state->v[inst->X];
		vm_state->v[inst->X] = vm_state->v[inst->Y] - vm_state->v[inst->X];
		vm_state->v[0xF] = carry;
		break;
	}
	case OP_8XYE:	// Shift
	{
		// WARN: Ambiguous instruction; implemented COSMAC VIP behaviour.
		// Set the flag register to the value of the shifted-out bit.
		uint8_t carry = (vm_state->v[inst->Y] & 0x80) >> 7;
		vm_state->v[inst->X] = vm_state->v[inst->Y] << 1;
		vm_state->v[0xF] = carry;
		break;
	}
	case OP_9XY0:	// Skip conditionally
		if (vm_state->v[inst->X] != vm_state->v[inst->Y]) {
			vm_state->pc += 2;
		}
		break;
	case OP_ANNN:	// Set index
		vm_state->I = inst->NNN;
		break;
	case OP_BNNN:	// Jump with offset
		// WARN: Ambiguous instruction; implemented COSMAC VIP behaviour.
		vm_state->pc = inst->NNN + vm_state->v[0];
		break;
	case OP_CXNN:	// Random
		vm_state->v[inst->X] = SDL_rand(UINT16_MAX) & inst->NN;
		break;
	case OP_DXYN:	// Display
	{
		uint8_t sx = vm_state->v[inst->X] % DISPLAY_WIDTH,
			sy = vm_state->v[inst->Y] % DISPLAY_HEIGHT,
			line = 0;
		vm_state->v[0xF] = 0;

		// We want to write the sprite data to N lines, starting at
		// Y. We need to ensure that we clip any lines that exceed
		// the height of the raster.
		for (uint8_t y = 0; y < inst->N && sy + y < DISPLAY_HEIGHT; ++y) {
			line = vm_state->memory[vm_state->I + y];
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
				if (vm_state->frame_buf[(sy + y) * DISPLAY_WIDTH + (sx + x)] == 1) {
					vm_state->v[0xF] = 1;
				}
				vm_state->frame_buf[(sy + y) * DISPLAY_WIDTH + (sx + x)] ^= 1;
			}
		}
		*display_op = DRAW;
		break;
	}
	case OP_EX9E:	// Skip if key
		if (input_state->kb_state[vm_state->v[inst->X]]) {
			vm_state->pc += 2;
		}
		break;
	case OP_EXA1:	// Skip if key
		if (!input_state->kb_state[vm_state->v[inst->X]]) {
			vm_state->pc += 2;
		}
		break;
	case OP_FX07:	// Store delay
		vm_state->v[inst->X] = vm_state->delay;
		break;
	case OP_FX0A:	// Get key
		if (input_state->last_key == -1) {
			// Halt execution
			vm_state->pc -= 2;
		}
		else {
			vm_state->v[inst->X] = input_state->last_key;
		}
		break;
	case OP_FX15:	// Read delay
		vm_state->delay = vm_state->v[inst->X];
		break;
	case OP_FX18:	// Store sound
		vm_state->sound = vm_state->v[inst->X];
		break;
	case OP_FX1E:	// Add to index
		vm_state->I += vm_state->v[inst->X];
		break;
	case OP_FX29:	// Font character
		// Font characters are 5 bytes long.
		vm_state->I = FONT_START + 5 * (vm_state->v[inst->X] & 0x0F);
		break;
	case OP_FX33:	// Binary-coded decimal conversion
	{
		// uint8_t max is 255 so we are always going to be storing
		// 3 digits.
		for (uint8_t i = 3, vx = vm_state->v[inst->X]; i > 0; --i, vx /= 10) {
			vm_state->memory[vm_state->I + i - 1] = vx % 10;
		}
		break;
	}
	case OP_FX55:	// Store memory
	{
		// WARN: Ambiguous instruction; implemented COSMAC VIP behaviour.
		uint8_t registers = inst->X + 1;
		for (uint8_t i = 0; i < registers; ++i) {
			vm_state->memory[vm_state->I + i] = vm_state->v[i];
		}
		vm_state->I += registers;
		break;
	}
	case OP_FX65:	// Load memory
	{
		// WARN: Ambiguous instruction; implemented COSMAC VIP behaviour.
		uint8_t registers = inst->X + 1;
		for (uint8_t i = 0; i < registers; ++i) {
			vm_state->v[i] = vm_state->memory[vm_state->I + i];
		}
		vm_state->I += registers;
		break;
	}
	case OP_UNKNOWN:
	default:
		break;
	}
}

