#ifndef CHIP8
#define CHIP8

#include <inttypes.h>

#define MEM_SIZE	4096
#define FONT_START	0x50	// 80
#define ROM_START	0x200	// 512
#define ROM_MAX		(MEM_SIZE - ROM_START)

void dump_rom(uint8_t* rom, int len);
int load_rom(char* path, uint8_t* buffer);

#endif
