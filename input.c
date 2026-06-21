#include <stdlib.h>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_log.h>

#include "chip-8.h"

bool read_input(struct input_state *state)
{
	if (state == NULL) {
		SDL_Log("Failed to read input: state is null\n");
		exit(1);
	}

	bool quit = false;
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		switch (e.type) {
		case SDL_EVENT_QUIT:
			quit = true;
			break;
		// We use the event queue to determine which keys have
		// been released since we last polled for input.
		case SDL_EVENT_KEY_UP:
			switch (e.key.scancode) {
			case SDL_SCANCODE_ESCAPE:
				if (!e.key.down) {
					quit = true;
				}
				break;
			// The following cases map scan codes to hex values
			// corresponding to the CHIP-8 keyboard.
			case SDL_SCANCODE_1:
				state->last_key = 0x1;
				state->kb_state[0x1] = false;
				break;
			case SDL_SCANCODE_2:
				state->last_key = 0x2;
				state->kb_state[0x2] = false;
				break;
			case SDL_SCANCODE_3:
				state->last_key = 0x3;
				state->kb_state[0x3] = false;
				break;
			case SDL_SCANCODE_4:
				state->last_key = 0xC;
				state->kb_state[0xC] = false;
				break;
			case SDL_SCANCODE_Q:
				state->last_key = 0x4;
				state->kb_state[0x4] = false;
				break;
			case SDL_SCANCODE_W:
				state->last_key = 0x5;
				state->kb_state[0x5] = false;
				break;
			case SDL_SCANCODE_E:
				state->last_key = 0x6;
				state->kb_state[0x6] = false;
				break;
			case SDL_SCANCODE_R:
				state->last_key = 0xD;
				state->kb_state[0xD] = false;
				break;
			case SDL_SCANCODE_A:
				state->last_key = 0x7;
				state->kb_state[0x7] = false;
				break;
			case SDL_SCANCODE_S:
				state->last_key = 0x8;
				state->kb_state[0x8] = false;
				break;
			case SDL_SCANCODE_D:
				state->last_key = 0x9;
				state->kb_state[0x9] = false;
				break;
			case SDL_SCANCODE_F:
				state->last_key = 0xE;
				state->kb_state[0xE] = false;
				break;
			case SDL_SCANCODE_Z:
				state->last_key = 0xA;
				state->kb_state[0xA] = false;
				break;
			case SDL_SCANCODE_X:
				state->last_key = 0x0;
				state->kb_state[0x0] = false;
				break;
			case SDL_SCANCODE_C:
				state->last_key = 0xB;
				state->kb_state[0xB] = false;
				break;
			case SDL_SCANCODE_V:
				state->last_key = 0xF;
				state->kb_state[0xF] = false;
				break;
			default:
				break;
			}
			break;
		case SDL_EVENT_KEY_DOWN:
			switch (e.key.scancode) {
			case SDL_SCANCODE_1:
				state->kb_state[0x1] = true;
				break;
			case SDL_SCANCODE_2:
				state->kb_state[0x2] = true;
				break;
			case SDL_SCANCODE_3:
				state->kb_state[0x3] = true;
				break;
			case SDL_SCANCODE_4:
				state->kb_state[0xC] = true;
				break;
			case SDL_SCANCODE_Q:
				state->kb_state[0x4] = true;
				break;
			case SDL_SCANCODE_W:
				state->kb_state[0x5] = true;
				break;
			case SDL_SCANCODE_E:
				state->kb_state[0x6] = true;
				break;
			case SDL_SCANCODE_R:
				state->kb_state[0xD] = true;
				break;
			case SDL_SCANCODE_A:
				state->kb_state[0x7] = true;
				break;
			case SDL_SCANCODE_S:
				state->kb_state[0x8] = true;
				break;
			case SDL_SCANCODE_D:
				state->kb_state[0x9] = true;
				break;
			case SDL_SCANCODE_F:
				state->kb_state[0xE] = true;
				break;
			case SDL_SCANCODE_Z:
				state->kb_state[0xA] = true;
				break;
			case SDL_SCANCODE_X:
				state->kb_state[0x0] = true;
				break;
			case SDL_SCANCODE_C:
				state->kb_state[0xB] = true;
				break;
			case SDL_SCANCODE_V:
				state->kb_state[0xF] = true;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}
	return quit;
}

