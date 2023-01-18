#include <stdio.h>
#include <stdint.h>
#include <fstream>
#include <cassert>
#include <stack>
#include <limits>

#include <SDL2/SDL.h>

uint8_t registers[16] = {0};
uint8_t memory[0x1000] = {0};
int PC = 0;
uint16_t ADDR = 0;
std::stack<int> sub_stack;


#define REG(x) registers[x]
#define V0 REG(0x1)
#define V1 REG(0x2)
#define V2 REG(0x3)
#define V3 REG(0x4)
#define V4 REG(0x5)
#define V5 REG(0x6)
#define V6 REG(0x7)
#define V8 REG(0x8)
#define V9 REG(0x9)
#define VA REG(0xA)
#define VB REG(0xB)
#define VC REG(0xC)
#define VD REG(0xD)
#define VE REG(0xE)
#define VF REG(0xF)

#define WIDTH 800
#define HEIGHT 600

void print_registers() {
	for(int i = 0; i <= 0xF; i++) {
		printf("Register V%X: %d\n", i, REG(i));
	}
}

//0NNN 	/* Execute machine language subroutine at address NNN */
//00E0 	/* Clear the screen */
//00EE 	/* Return from a subroutine */
//1NNN 	/* Jump to address NNN */
//2NNN 	/* Execute subroutine starting at address NNN */
//3XNN 	/* Skip the following instruction if the value of register VX equals NN */
//4XNN 	/* Skip the following instruction if the value of register VX is not equal to NN */
//5XY0 	/* Skip the following instruction if the value of register VX is equal to the value of register VY */
//6XNN 	/* Store number NN in register VX */
//7XNN 	/* Add the value NN to register VX */
//8XY0 	/* Store the value of register VY in register VX */
//8XY1 	/* Set VX to VX OR VY */
//8XY2 	/* Set VX to VX AND VY */
//8XY3 	/* Set VX to VX XOR VY */
//8XY4 	/* Add the value of register VY to register VX
//		   Set VF to 01 if a carry occurs
//		   Set VF to 00 if a carry does not occur */
//8XY5 	/* Subtract the value of register VY from register VX
//           Set VF to 00 if a borrow occurs
//           Set VF to 01 if a borrow does not occur */
//
//8XY6 	/* Store the value of register VY shifted right one bit in register VX¹
//           Set register VF to the least significant bit prior to the shift
//           VY is unchanged */
//8XY7 	/* Set register VX to the value of VY minus VX
//           Set VF to 00 if a borrow occurs
//           Set VF to 01 if a borrow does not occur */
//8XYE 	/* Store the value of register VY shifted left one bit in register VX¹
//           Set register VF to the most significant bit prior to the shift
//           VY is unchanged */
//9XY0 	/* Skip the following instruction if the value of register VX is not equal to the value of register VY */
//ANNN 	/* Store memory address NNN in register I */
//BNNN 	/* Jump to address NNN + V0 */
//CXNN 	/* Set VX to a random number with a mask of NN*/
//DXYN 	/* Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
//		Set VF to 01 if any set pixels are changed to unset, and 00 otherwise */
//EX9E 	/* Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed */
//EXA1 	/* Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed */
//FX07 	/* Store the current value of the delay timer in register VX */
//FX0A 	/* Wait for a keypress and store the result in register VX */
//FX15 	/* Set the delay timer to the value of register VX */
//FX18 	/* Set the sound timer to the value of register VX */
//
//FX1E 	/* Add the value stored in register VX to register I */
//FX29 	/* Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX */
//FX33 	/* Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2 */
//
//FX55 	/* Store the values of registers V0 to VX inclusive in memory starting at address I
//		   I is set to I + X + 1 after operation */
//
//FX65 	/* Fill registers V0 to VX inclusive with the values stored in memory starting at address I
		   //I is set to I + X + 1 after operation*/

void ReadBinFile(const char* filename, uint8_t* buffer, int& out_size)
{
  std::ifstream ifs(filename, std::ifstream::binary);

  if(ifs) {
    ifs.seekg (0, ifs.end);
    out_size = ifs.tellg();
    ifs.seekg (0, ifs.beg);

    ifs.read((char*)buffer, out_size);

    ifs.close();
  }
}

struct Instruction {
	uint8_t a, b;
};

Instruction fetch(int PC) {
	//printf("ROM[%d,%d]: %04X\n", PC, PC+1, (rom[PC+1] | rom[PC] << 8));
	return Instruction {memory[PC], memory[PC+1]};
}

void decode(Instruction inst) {
	uint8_t high = inst.a & 0xF0;
	uint8_t low = inst.a & 0xF;

	printf("Instruction @ %04X: %04X high: %X low: %X\n", PC, (inst.b | inst.a << 8), high, low);

	switch(high) {
		case 0x0: {
			if (low != 0x0) {
				printf("Looks like this program uses an annoying instruction.\n");
				assert(false);
			} else if (inst.b == 0xE0) { /* 00E0 Clear the screen */
				printf("SHOULD CLEAR SCREEN\n");
			}
			else if (inst.b == 0xEE) { /* 00EE Return from a subroutine */
				printf("returning from subroutine at %04X to %04X\n", PC, sub_stack.top());
				PC = sub_stack.top() - 2;
				sub_stack.pop();
			} else {
				assert(false);
			}
		}
		break;
		case 0x10: { /* 1NNN Jump to address NNN */
			int address = (inst.b | low << 8);
			printf("jumping to address %03X\n", address);
			PC=address - 2; //minus two because PC gets incremented after
		}
		break;
		case 0x20: { /* 2NNN Execute subroutine starting at address NNN */
			int address = (inst.b | low << 8);
			printf("execute subroutine at address %03X\n", address);
			printf("instruction at address %03X: %04X\n", address, (memory[address+1] | memory[address] << 8));
			sub_stack.push(PC);
			PC=address - 2; //minus two because PC gets incremented after
		}
		break;
		case 0x30: { /* 3XNN Skip the following instruction if the value of register VX equals NN */
			printf("skip following instruction if value of V%X is equal to %02X\n", low, inst.b);
			printf("V%X is %02X\n", low, REG(low));
			if(REG(low) == inst.b) {
				PC+=2;
				printf("Incrementing PC by two.\n");
			}
		}
		break;
		case 0x40: { /* 4XNN Skip the following instruction if the value of register VX is not equal to NN */
			printf("skip following instruction if value of V%X is not equal to %02X\n", low, inst.b);
			printf("V%X is %02X\n", low, REG(low));
			if(REG(low) != inst.b) {
				PC+=2;
				printf("Incrementing PC by two.\n");
			}
		}
		break;
		case 0x60: /* 6XNN Store number NN in register VX */
			printf("store %02X in V%X\n", inst.b, low);
			printf("V%X before: %02X\n", low, REG(low));
			REG(low) = inst.b;
			printf("V%X after: %02X\n", low, REG(low));
		break;
		case 0x70: /* 7XNN Add the value NN to register VX */
			printf("add %02X to V%X\n", inst.b, low);
			printf("V%X before: %02X\n", low, REG(low));
			REG(low) += inst.b;
			printf("V%X after: %02X\n", low, REG(low));
		break;
		case 0x80: {
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

 			if((inst.b & 0xF) == 0x0) { /* 8XY0 Store the value of register VY in register VX */
				printf("store value of V%X (%02X) in register V%X (%02X)\n", from_reg, from_val, to_reg, to_val);
				printf("V%X before: %02X\n", to_reg, REG(to_reg));
				REG(to_reg) = REG(from_reg);
				printf("V%X after: %02X\n", to_reg, REG(to_reg));
			} else if((inst.b & 0xF) == 0x4) { /* 8XY4 Add the value of register VY to register VX
											          Set VF to 01 if a carry occurs
											          Set VF to 00 if a carry does not occur */

				uint32_t sum = from_val + to_val;
				printf("add value of V%X (%02X) to register V%X (%02X)\n", from_reg, from_val, to_reg, to_val);

				if(sum > static_cast<int>(std::numeric_limits<uint8_t>::max())) {
					printf("Carry occured %d + %d = %d\n", from_val, to_val, sum);
					VF = 0x01;
					sum -= 256;
				} else {
					VF = 0x0;
					printf("Carry didnt occur %d + %d = %d\n", from_val, to_val, sum);
				}
				
				REG(to_reg) = sum;
			} else if((inst.b & 0xF) == 0x2) { /*8XY2 Set VX to VX AND VY  */
				printf("set  V%X (%02X) to V%X & V%X (%02X)\n", to_reg, to_val, to_reg, from_reg, from_val);
				REG(to_reg) = to_val & from_val;
			} else if((inst.b & 0xF) == 0x3) { /*8XY3 Set VX to VX OR VY  */
				printf("set  V%X (%02X) to V%X | V%X (%02X)\n", to_reg, to_val, to_reg, from_reg, from_val);
				REG(to_reg) = to_val | from_val;
			} else {
				assert(false);
			}
		}
		break;
		case 0xA0: { /* ANNN Store memory address NNN in register I */
			int address = (inst.b | low << 8);
			printf("store address %03X in register I\n", address);
			ADDR=address; //minus two because PC gets incremented after
		}
		break;
		case 0xF0: {
			if(inst.b == 0x07) { 		/* FX07 Store the current value of the delay timer in register VX */
				assert(false);
			} else if(inst.b == 0x0A) { /* FX0A Wait for a keypress and store the result in register VX */
				assert(false);
			} else if(inst.b == 0x15) { /* FX15 Set the delay timer to the value of register VX */
				assert(false);
			} else if(inst.b == 0x18) { /* FX18 Set the sound timer to the value of register VX */
				assert(false);
			} else if(inst.b == 0x1E) { /* FX1E Add the value stored in register VX to register I */
				ADDR += REG(low);
			} else if(inst.b == 0x29) { /* FX29 Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX */
				assert(false);
			} else if(inst.b == 0x33) { /* FX33 Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2 */
				uint8_t val_in_reg = REG(low);

				for(int i = 3; i > 0; --i)
				{
					memory[ADDR + i - 1] = REG(low) % 10;
					val_in_reg /= 10;
				}
			} else if(inst.b == 0x55) { /* FX55 Store the values of registers V0 to VX inclusive in memory starting at address I. I is set to I + X + 1 after operation² */
				for(int i = 0; i <= low; i++) {
					memory[ADDR + i] = registers[i];
				}
				ADDR += low + 1;
			} else if(inst.b == 0x65) { /* FX65 Fill registers V0 to VX inclusive with the values stored in memory starting at address I. I is set to I + X + 1 after operation²  */
				for(int i = 0; i <= low; i++) {
					registers[i] = memory[ADDR + i];
				}
				ADDR += low + 1;
			} else {
				assert(false);
			}
		}
		break;
	default:
		printf("No such opcode implemented yet.\n");
		assert(false);
		break;
	}
}

int main(void) {
	int rom_size;
	memset(memory, sizeof(memory), 0);

	ReadBinFile("BLINKY", &memory[0x200], rom_size);
	PC=0x200;

	assert(rom_size % 2 == 0);

	printf("rom size: %d\n", rom_size);
	int instructions_ran = 0;

	while(true) {
		Instruction inst = fetch(PC);

		decode(inst);
		instructions_ran++;

		PC += 2;
		printf("\n%d\n", instructions_ran);
	}

	//for(int i = 0; i < rom_size+1; i+=2) {
	//	printf("%d-%d: %02X%02X\n", i, i+1, rom[i], rom[i+1]);
	//}

	//registers[0xE] = 0xFF;

	//SDL_Init(SDL_INIT_VIDEO);

	//SDL_Window* window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);

	//SDL_Surface* surface = SDL_GetWindowSurface(window);
	//
	//uint32_t* pixels = (uint32_t*)surface->pixels;

	//while(1) {
	//	SDL_Event event;

	//	while(SDL_PollEvent(&event)) {
	//		switch(event.type) {
	//			case SDL_QUIT:
	//				exit(0); break;
	//			case SDL_WINDOWEVENT: {
	//				if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
	//				{
	//					surface = SDL_GetWindowSurface(window);
	//					pixels = (uint32_t*)surface->pixels;
	//				}
	//			}; break;
	//		}
	//	}

	//	SDL_UpdateWindowSurface(window);
	//}

    return 0;
}
