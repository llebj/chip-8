#include <inttypes.h>
#include <stdio.h>

#include "chip-8.h"

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

// TODO: Refactor this API to explicitly return an error condition, rather
//	 than implicitly using the length.
int load_rom(char* path, uint8_t* buffer)
{
	unsigned int len = 0;
	FILE *fp;

	if ((fp = fopen(path, "r")) == NULL) {
		fprintf(stderr, "Error: could not open %s\n", path);
		return 0;
	}

	int c = '\0';
	uint8_t* ram_ptr = buffer;
	for ( ; (c = getc(fp)) != EOF && len < ROM_MAX; ++len) {
		*(ram_ptr++) = c;
	}
	if (c != EOF) {
		fprintf(stderr, "Error: rom file must be less than %d bytes\n", ROM_MAX);
		return 0;
	}

	fclose(fp);
	return len;
}
