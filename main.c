#include <stdio.h>
#include <stdlib.h>

#define MEM_SIZE 4096

unsigned char mem[MEM_SIZE];
int rom_size = 0;

int main(int argc, char** argv)
{
	if (argc != 2) {
		printf("Usage:\n\t%s { rom_file }\n", argv[0]);
		exit(1);
	}

	FILE *fp;
	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		fprintf(stderr, "Error: could not open %s\n", argv[1]);
		exit(2);
	}

	int c = '\0';
	unsigned char* ram_ptr = mem;
	for ( ; (c = getc(fp)) != EOF && rom_size < MEM_SIZE; ++rom_size) {
		*(ram_ptr++) = c;
	}
	if (c != EOF) {
		fprintf(stderr, "Error: rom file must be less than %d bytes\n", MEM_SIZE);
		exit(3);
	}

	for (int i = 0; i < rom_size; i++) {
		if (i % 16 == 0) {
			printf("%s%08x", i == 0 ? "" : "\n", i);
		}
		printf("%s%02x", i % 2 == 0 ? "    " : "", mem[i]);
	}
	printf("\n%08x\n", rom_size);
}
