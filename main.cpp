#include <stdio.h>
#include <stdint.h>
#include <fstream>
#include <cassert>
#include <stack>
#include <limits>
#include <vector>

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

#define WIDTH 64
#define HEIGHT 32

#define DEBUG 1

#define DEBUG_PRINT(format, ...) \
  if(DEBUG) {fprintf (stderr, format __VA_OPT__(,) __VA_ARGS__);}



void print_registers() {
	for(int i = 0; i <= 0xF; i++) {
		printf("Register V%X: %d\n", i, REG(i));
	}
}


void ReadRom(const char* filename, uint8_t* buffer, int& out_size)
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
	DEBUG_PRINT("FETCH[%d,%d]: %04X\n", PC, PC+1, (memory[PC+1] | memory[PC] << 8));
	return Instruction {memory[PC], memory[PC+1]};
}

enum OpCode {
OP_0NNN,	/* Execute machine language subroutine at address NNN */
OP_00E0,	/* Clear the screen */
OP_00EE,	/* Return from a subroutine */
OP_1NNN,	/* Jump to address NNN */
OP_2NNN,	/* Execute subroutine starting at address NNN */
OP_3XNN,	/* Skip the following instruction if the value of register VX equals NN */
OP_4XNN,	/* Skip the following instruction if the value of register VX is not equal to NN */
OP_5XY0,	/* Skip the following instruction if the value of register VX is equal to the value of register VY */
OP_6XNN,	/* Store number NN in register VX */
OP_7XNN,	/* Add the value NN to register VX */
OP_8XY0,	/* Store the value of register VY in register VX */
OP_8XY1,	/* Set VX to VX OR VY */
OP_8XY2,	/* Set VX to VX AND VY */
OP_8XY3,	/* Set VX to VX XOR VY */
OP_8XY4,	/* Add the value of register VY to register VX
   	      	   Set VF to 01 if a carry occurs
   	      	   Set VF to 00 if a carry does not occur */
OP_8XY5,	/* Subtract the value of register VY from register VX
               Set VF to 00 if a borrow occurs
               Set VF to 01 if a borrow does not occur */
OP_8XY6,	/* Store the value of register VY shifted right one bit in register VX¹
               Set register VF to the least significant bit prior to the shift
               VY is unchanged */
OP_8XY7,	/* Set register VX to the value of VY minus VX
               Set VF to 00 if a borrow occurs
               Set VF to 01 if a borrow does not occur */
OP_8XYE,	/* Store the value of register VY shifted left one bit in register VX¹
               Set register VF to the most significant bit prior to the shift
               VY is unchanged */
OP_9XY0,	/* Skip the following instruction if the value of register VX is not equal to the value of register VY */
OP_ANNN,	/* Store memory address NNN in register I */
OP_BNNN,	/* Jump to address NNN + V0 */
OP_CXNN,	/* Set VX to a random number with a mask of NN*/
OP_DXYN,	/* Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
   	    	   Set VF to 01 if any set pixels are changed to unset, and 00 otherwise */
OP_EX9E,	/* Skip the following instruction if the key corresponding to the hex value currently stored in register VX is pressed */
OP_EXA1,	/* Skip the following instruction if the key corresponding to the hex value currently stored in register VX is not pressed */
OP_FX07,	/* Store the current value of the delay timer in register VX */
OP_FX0A,	/* Wait for a keypress and store the result in register VX */
OP_FX15,	/* Set the delay timer to the value of register VX */
OP_FX18,	/* Set the sound timer to the value of register VX */

OP_FX1E,	/* Add the value stored in register VX to register I */
OP_FX29,	/* Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX */
OP_FX33,	/* Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2 */

OP_FX55,	/* Store the values of registers V0 to VX inclusive in memory starting at address I
   	       	   I is set to I + X + 1 after operation */

OP_FX65,	/* Fill registers V0 to VX inclusive with the values stored in memory starting at address I
		       I is set to I + X + 1 after operation*/
};

OpCode decode(Instruction inst)
{
	uint8_t high = inst.a & 0xF0;
	uint8_t low = inst.a & 0xF;

	switch(high) {
		case 0x0: {
			if (low != 0x0) {
				return OP_0NNN;
			} else if (inst.b == 0xE0) { /* 00E0 Clear the screen */
				return OP_00E0;
			} else if (inst.b == 0xEE) { /* 00EE Return from a subroutine */
				return OP_00EE;
			} else {
				assert(false);
			}
		}
		break;
		case 0x10: /* 1NNN Jump to address NNN */
			return OP_1NNN;
		case 0x20: /* 2NNN Execute subroutine starting at address NNN */
			return OP_2NNN;
		break;
		case 0x30: /* 3XNN Skip the following instruction if the value of register VX equals NN */
			return OP_3XNN;
		case 0x40: /* 4XNN Skip the following instruction if the value of register VX is not equal to NN */
			return OP_4XNN;
		case 0x50:
			return OP_5XY0; /* Skip the following instruction if the value of register VX is equal to the value of register VY */
		case 0x60: /* 6XNN Store number NN in register VX */
			return OP_6XNN;
		case 0x70: /* 7XNN Add the value NN to register VX */
			return OP_7XNN;
		case 0x80: {
 			if((inst.b & 0xF) == 0x0) { /* 8XY0 Store the value of register VY in register VX */
				return OP_8XY0;
			} else if((inst.b & 0xF) == 0x1) { /*8XY1 Set VX to VX OR VY  */
				return OP_8XY1;
			} else if((inst.b & 0xF) == 0x2) { /*8XY2 Set VX to VX AND VY  */
				return OP_8XY2;
			} else if((inst.b & 0xF) == 0x3) { /*8XY3 Set VX to VX XOR VY  */
				return OP_8XY3;
			} else if((inst.b & 0xF) == 0x4) {
				return OP_8XY4;
			} else if((inst.b & 0xF) == 0x5) {
				return OP_8XY5;
			} else if((inst.b & 0xF) == 0x6) {
				return OP_8XY6;
			} else if((inst.b & 0xF) == 0x7) {
				return OP_8XY7;
			} else if((inst.b & 0xF) == 0xE) {
				return OP_8XYE;
			} else {
				assert(false);
			}
		}
		break;
		case 0x90:
			return OP_9XY0;
		case 0xA0: /* ANNN Store memory address NNN in register I */
			return OP_ANNN;
		case 0xD0:
			return OP_DXYN;
		case 0xF0: {
			if(inst.b == 0x07) { 		/* FX07 Store the current value of the delay timer in register VX */
				return OP_FX07;
			} else if(inst.b == 0x0A) { /* FX0A Wait for a keypress and store the result in register VX */
				return OP_FX0A;
			} else if(inst.b == 0x15) { /* FX15 Set the delay timer to the value of register VX */
				return OP_FX15;
			} else if(inst.b == 0x18) { /* FX18 Set the sound timer to the value of register VX */
				return OP_FX18;
			} else if(inst.b == 0x1E) { /* FX1E Add the value stored in register VX to register I */
				return OP_FX1E;
			} else if(inst.b == 0x29) { /* FX29 Set I to the memory address of the sprite data corresponding to the hexadecimal digit stored in register VX */
				return OP_FX29;
			} else if(inst.b == 0x33) { /* FX33 Store the binary-coded decimal equivalent of the value stored in register VX at addresses I, I + 1, and I + 2 */
				return OP_FX33;
			} else if(inst.b == 0x55) { /* FX55 Store the values of registers V0 to VX inclusive in memory starting at address I. I is set to I + X + 1 after operation² */
				return OP_FX55;
			} else if(inst.b == 0x65) { /* FX65 Fill registers V0 to VX inclusive with the values stored in memory starting at address I. I is set to I + X + 1 after operation²  */
				return OP_FX65;
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

void execute(OpCode op, Instruction inst, uint32_t* pixels)
{
	uint8_t high = inst.a & 0xF0;
	uint8_t low = inst.a & 0xF;

	DEBUG_PRINT("PC 0x%04X: %04X high: %X low: %X\n", PC, (inst.b | inst.a << 8), high, low);

	switch(op) {
		case OP_00E0: {
			DEBUG_PRINT("CLEAR SCREEN\n");
			memset(pixels, 0x0, WIDTH*HEIGHT*sizeof(uint32_t));
		}
		break;
		case OP_0NNN: {
			DEBUG_PRINT("Looks like this program uses an annoying instruction.\n");
			assert(false);
		}
		break;
		case OP_00EE: {
			DEBUG_PRINT("returning from subroutine at %04X to %04X\n", PC, sub_stack.top());
			PC = sub_stack.top();
			sub_stack.pop();
		}
		break;
		case OP_1NNN: { /* 1NNN Jump to address NNN */
			int address = (inst.b | low << 8);
			DEBUG_PRINT("jumping to address %03X\n", address);
			PC=address - 2; //minus two because PC gets incremented after
		}
		break;
		case OP_2NNN: { /* 2NNN Execute subroutine starting at address NNN */
			int address = (inst.b | low << 8);
			DEBUG_PRINT("execute subroutine at address %03X\n", address);
			DEBUG_PRINT("instruction at address %03X: %04X\n", address, (memory[address+1] | memory[address] << 8));
			sub_stack.push(PC);
			PC=address - 2; //minus two because PC gets incremented after
		}
		break;
		case OP_3XNN: { /* 3XNN Skip the following instruction if the value of register VX equals NN */
			DEBUG_PRINT("skip following instruction if value of V%X is equal to %02X\n", low, inst.b);
			DEBUG_PRINT("V%X is %02X\n", low, REG(low));
			if(REG(low) == inst.b) {
				PC+=2;
				DEBUG_PRINT("Incrementing PC by two.\n");
			}
		}
		break;
		case OP_4XNN: { /* 4XNN Skip the following instruction if the value of register VX is not equal to NN */
			DEBUG_PRINT("skip following instruction if value of V%X is not equal to %02X\n", low, inst.b);
			DEBUG_PRINT("V%X is %02X\n", low, REG(low));
			if(REG(low) != inst.b) {
				PC+=2;
				DEBUG_PRINT("Incrementing PC by two.\n");
			}
		}
		break;
		case OP_5XY0: { /* 5XY0 Skip the following instruction if the value of register VX is equal to the value of register VY */
			uint8_t X = REG(low);
			uint8_t Y = REG(((inst.b & 0xF0) >> 4) & 0xF);

			DEBUG_PRINT("skip following instruction if value of V%X (%02X) is not equal to V%X(%02X) \n", low, X, ((inst.b & 0xF0) >> 4) & 0xF, Y);

			if(X == Y) {
				PC+=2;
				DEBUG_PRINT("Incrementing PC by two.\n");
			}
		}
		break;
		case OP_6XNN: { /* 6XNN Store number NN in register VX */
			DEBUG_PRINT("store %02X in V%X\n", inst.b, low);
			DEBUG_PRINT("V%X before: %02X\n", low, REG(low));
			REG(low) = inst.b;
			DEBUG_PRINT("V%X after: %02X\n", low, REG(low));
		} break;
		case OP_7XNN: { /* 7XNN Add the value NN to register VX */
			DEBUG_PRINT("add %02X to V%X\n", inst.b, low);
			DEBUG_PRINT("V%X before: %02X\n", low, REG(low));
			REG(low) += inst.b;
			DEBUG_PRINT("V%X after: %02X\n", low, REG(low));
		} break;
		case OP_8XY0: { /* 8XY0 Store the value of register VY in register VX */
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

			DEBUG_PRINT("store value of V%X (%02X) in register V%X (%02X)\n", from_reg, from_val, to_reg, to_val);
			DEBUG_PRINT("V%X before: %02X\n", to_reg, REG(to_reg));
			REG(to_reg) = REG(from_reg);
			DEBUG_PRINT("V%X after: %02X\n", to_reg, REG(to_reg));
		} break;
		case OP_8XY1: { /*8XY1 Set VX to VX OR VY  */
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

			DEBUG_PRINT("set  V%X (%02X) to V%X | V%X (%02X)\n", to_reg, to_val, to_reg, from_reg, from_val);
			REG(to_reg) = to_val | from_val;
		} break;
		case OP_8XY2: {
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

			DEBUG_PRINT("set  V%X (%02X) to V%X & V%X (%02X)\n", to_reg, to_val, to_reg, from_reg, from_val);
			REG(to_reg) = to_val & from_val;
		} break;
		case OP_8XY3: {
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

			DEBUG_PRINT("set  V%X (%02X) to V%X ^ V%X (%02X)\n", to_reg, to_val, to_reg, from_reg, from_val);
			REG(to_reg) = to_val ^ from_val;
		} break;
		case OP_8XY4: {
			uint8_t from_reg = (inst.b & 0xF0) >> 4;
			uint8_t to_reg = low;
			uint8_t from_val = REG(from_reg);
			uint8_t to_val = REG(to_reg);

			uint32_t sum = from_val + to_val;
			DEBUG_PRINT("add value of V%X (%02X) to register V%X (%02X)\n", from_reg, from_val, to_reg, to_val);

			if(sum > static_cast<int>(std::numeric_limits<uint8_t>::max())) {
				DEBUG_PRINT("Carry occured %d + %d = %d\n", from_val, to_val, sum);
				VF = 0x01;
				sum -= 256;
			} else {
				VF = 0x0;
				DEBUG_PRINT("Carry didnt occur %d + %d = %d\n", from_val, to_val, sum);
			}
			
			REG(to_reg) = sum;
		} break;
		case OP_8XY5: {
			uint8_t y_reg = (inst.b & 0xF0) >> 4; //Y
			uint8_t y_val = REG(y_reg);

			uint8_t x_reg = low; //X
			uint8_t x_val = REG(x_reg);

			int32_t difference = x_val - y_val;

			VF = difference < 0 ? 0 : 0x01;

			REG(x_reg) -= REG(y_reg);
		} break;
		case OP_8XY6: {
			uint8_t y_reg = (inst.b & 0xF0) >> 4; //Y

			uint8_t x_reg = low; //X

			VF = REG(y_reg) & 0x1;

			REG(x_reg) = REG(y_reg) >> 1;
		} break;
		case OP_8XY7: {
			uint8_t y_reg = (inst.b & 0xF0) >> 4; //Y
			uint8_t y_val = REG(y_reg);

			uint8_t x_reg = low; //X
			uint8_t x_val = REG(x_reg);

			int32_t difference = x_val - y_val;

			VF = difference < 0 ? 0 : 0x01;

			REG(x_reg) = REG(y_reg) - REG(x_reg);
		} break;
		case OP_8XYE: {
			uint8_t y_reg = (inst.b & 0xF0) >> 4; //Y
			uint8_t x_reg = low; //X

			VF = REG(y_reg) & 0xFF;

			REG(x_reg) = REG(y_reg) << 1;
		} break;
		case OP_9XY0: { /* 9XY0 Skip the following instruction if the value of register VX is not equal to the value of register VY */
			uint8_t X = REG(low);
			uint8_t Y = REG((inst.b & 0xF0) >> 4);

			if(REG(X) != REG(Y)) {
				PC+=2;
				DEBUG_PRINT("Incrementing PC by two.\n");
			}
		} break;
		case OP_ANNN: {
			ADDR=(inst.b | low << 8);
			DEBUG_PRINT("store address %03X in register I\n", ADDR);
		} break;
		/* Draw a sprite at position VX, VY with N bytes of sprite data starting at the address stored in I
		Set VF to 01 if any set pixels are changed to unset, and 00 otherwise */
		case OP_DXYN: {
			uint8_t X = REG(low);
			uint8_t Y = REG((inst.b & 0xF0) >> 4);
			uint8_t N = inst.b & 0xF;
			VF = 0;
			DEBUG_PRINT("Drawing sprite 8x%d at %02Xx%02X\n", N, X, Y);

			for (int y = 0; y < N; ++y)
			{
				uint8_t pixel_bits = memory[ADDR+y];

				std::vector<int> was_set(0);

				for(int x = 0; x < 8; x++) {
					if(pixels[X+x + Y+y * WIDTH] > 0) {
						was_set.emplace_back(X+x + Y+y * WIDTH);
					}
				}

				pixels[((X+0)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+0 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x80) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+1)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+1 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x40) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+2)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+2 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x20) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+3)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+3 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x10) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+4)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+4 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x8) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+5)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+5 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x4) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+6)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+6 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x2) > 0 ? 0xFFFFFFFF : 0x0));
				pixels[((X+7)%WIDTH) + (Y+y) * WIDTH] = (pixels[X+7 + (Y+y) * WIDTH] ^ ((pixel_bits & 0x1) > 0 ? 0xFFFFFFFF : 0x0));

				for(size_t i = 0; i < was_set.size(); i++) {
					if(pixels[was_set.at(i)] == 0) {
						VF=1;
						break;
					}
				}
			}
		} break;
		case OP_FX07: {
			assert(false);
		} break;
		case OP_FX0A: {
			assert(false);
		} break;
		case OP_FX15: {
			assert(false);
		} break;
		case OP_FX18: {
			assert(false);
		} break;
		case OP_FX1E: {
			ADDR += REG(low);
		} break;
		case OP_FX29: {
			assert(false);
		} break;
		case OP_FX33: {
			uint8_t val_in_reg = REG(low);

			memory[ADDR] = (uint8_t) ((uint8_t) val_in_reg / 100);
			memory[ADDR + 1] = (uint8_t) ((uint8_t) (val_in_reg / 10) % 10);
			memory[ADDR + 2] = (uint8_t) ((uint8_t) (val_in_reg % 100) % 10);
		} break;
		case OP_FX55: {
			for(int i = 0; i <= low; i++) {
				memory[ADDR + i] = registers[i];
			}
			ADDR += low + 1;
		} break;
		case OP_FX65: {
			for(int i = 0; i <= low; i++) {
				registers[i] = memory[ADDR + i];
			}
			ADDR += low + 1;
		} break;
	default:
		printf("Instruction isnt implemented yet.\n");
		assert(false);
		break;
	}
}
uint8_t font[80] =
{
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

int main(void) {
	int rom_size;
	memset(memory, 0, sizeof(memory));

	memcpy(memory, font, sizeof(font));

	//ReadRom("roms/test_opcode.ch8", &memory[0x200], rom_size);
	//ReadRom("roms/GUESS", &memory[0x200], rom_size);
	//ReadRom("roms/BLINKY", &memory[0x200], rom_size);
	//ReadRom("roms/IBM.ch8", &memory[0x200], rom_size);
	//ReadRom("roms/trip8.ch8", &memory[0x200], rom_size);
	//ReadRom("roms/particle.ch8", &memory[0x200], rom_size);
	ReadRom("roms/picture.ch8", &memory[0x200], rom_size);
	PC=0x200;

	//assert(rom_size % 2 == 0);

	printf("rom size: %d\n", rom_size);

	SDL_Init(SDL_INIT_VIDEO);
	int windowWidth = 800, windowHeight = 600;

	SDL_Window* window = SDL_CreateWindow("CHIP-8", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowWidth, windowHeight, 0);

	SDL_Renderer* renderer = SDL_CreateRenderer(window,
		-1, SDL_RENDERER_PRESENTVSYNC);

	SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
    SDL_RenderSetIntegerScale(renderer, (SDL_bool)1);

	SDL_Texture* screen_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);

	uint32_t* pixels = (uint32_t*)malloc(WIDTH*HEIGHT*4);

	while(1) {

		Instruction inst = fetch(PC);

		OpCode op = decode(inst);

		execute(op, inst, pixels);

		PC += 2;
		getchar();

		SDL_Event event;

		while(SDL_PollEvent(&event)) {
			switch(event.type) {
				case SDL_QUIT:
					exit(0);
				break;
			}
		}

        SDL_RenderClear(renderer);
        SDL_UpdateTexture(screen_texture, NULL, pixels, WIDTH * 4);
        SDL_RenderCopy(renderer, screen_texture, NULL, NULL);
        SDL_RenderPresent(renderer);
	}

    return 0;
}
