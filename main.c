#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>
#include <math.h>

#include <SDL2/SDL.h>

#define WIDTH 224
#define HEIGHT 256



uint8_t *framebuffer;

// I/O Ports
uint8_t port[4];
// 0000 000x = Coin
// 0000 00x0 = P2 Start Button
// 0000 0x00 = P1 Start Button
// 0000 x000 = ?
// 000x 0000 = P1 Shoot Button
// 00x0 0000 = P1 Joystick Left
// 0x00 0000 = P1 Joystick Right
// x000 0000 = ?

typedef struct Flags {
    uint8_t z:1; // Zero Flag
    uint8_t s:1; // Sign Flag
    uint8_t p:1; // Parity Flag
    uint8_t cy:1; // Carry Flag
    uint8_t ac:1; // Auxiliary Carry Flag
    uint8_t pad:3;
} Flags;

typedef struct ProcessorState {
    uint8_t a; // Accumulator
    
    union {
        uint16_t bc_pair; // Register Pair B/C
        uint8_t bc[2]; // Registers B and C
    };
    
    union {
        uint16_t de_pair; // Register Pair D/E
        uint8_t de[2]; // Registers D and E
    };
    
    union {
        uint16_t hl_pair; // Register Pair H/L
        uint8_t hl[2]; // Registers H and L
    };

    union {
        uint16_t sp; // Stack Pointer
        uint8_t sp_hl[2]; // Lets you set the high and low bytes seperately
    };
    
    union {
        uint16_t combined; // Extra union for easy combining of bytes.
        uint8_t seperate[2];
    };
    
    union {
        uint16_t pc; // Program Counter
        uint8_t pc_hl[2]; // Lets you set the high low bytes seperately
    };
    
    uint8_t *memory;
    struct Flags flags;
    uint8_t int_enable;
} ProcessorState;

void LoadRom(ProcessorState *emulator){
    FILE *file_array[4];
    int file_sizes[4];
    // Open file.
    file_array[0]= fopen("invaders.h", "rb"); 
    file_array[1]= fopen("invaders.g", "rb"); 
    file_array[2]= fopen("invaders.f", "rb"); 
    file_array[3]= fopen("invaders.e", "rb"); 
    if (file_array[0] == NULL || file_array[1] == NULL || file_array[2] == NULL || file_array[3] == NULL) {    
        printf("error: Couldn't open file\n");
    }

    // Go through file and count the size.
    int invaders_size = 0;
    for(int i=0; i < 4; ++i){
        fseek(file_array[i], 0U, SEEK_END);    
        invaders_size += ftell(file_array[i]);   
        file_sizes[i] =  ftell(file_array[i]); 
        // Go back to beginning of file.
        fseek(file_array[i], 0U, SEEK_SET);    
    }
    emulator->memory = malloc(0x5000);
    int pc = 0;
    for(int i=0; i < 4; ++i){
        fread(&emulator->memory[pc], 2, file_sizes[i]/2, file_array[i]);    
        fclose(file_array[i]);
        pc +=  file_sizes[i];
    }
    framebuffer = &emulator->memory[0x2400];
}

const unsigned char* HexToAssembly(unsigned char *hex) {
    switch (hex[0]){
        case 0x00: return ("NOP");
        case 0x01: return ("LXI B");
        case 0x02: return ("STAX B");
        case 0x03: return ("INX B");
        case 0x04: return ("INR B");
        case 0x05: return ("DCR B");
        case 0x06: return ("MVI B");
        case 0x07: return ("RLC");
        case 0x09: return ("DAD B");
        case 0x0a: return ("LDAX B");
        case 0x0b: return("DCX B");
        case 0x0c: return("INR C");
        case 0x0d: return("DCR C");
        case 0x0e: return("MVI C");
        case 0x0f: return("RRC");
        case 0x11: return("LXI D");
        case 0x12: return("STAX D");
        case 0x13: return("INX D");
        case 0x14: return("INR D");
        case 0x15: return("DCR D");
        case 0x16: return("MVI D");
        case 0x17: return("RAL");
        case 0x19: return("DAD D");
        case 0x1a: return("LDAX D");
        case 0x1b: return("DCX D");
        case 0x1c: return("INR E");
        case 0x1d: return("DCR E");
        case 0x1e: return("MVI E");
        case 0x1f: return("RAR");
        case 0x20: return("RIM");
        case 0x21: return("LXI H");
        case 0x22: return("SHLD");
        case 0x23: return("INX H");
        case 0x24: return("INR H");
        case 0x25: return("DCR H");
        case 0x26: return("MVI H");
        case 0x27: return("DAA");
        case 0x29: return("DAD H");
        case 0x2a: return("LHLD");
        case 0x2b: return("DCX H");
        case 0x2c: return("INR L");
        case 0x2d: return("DCR L");
        case 0x2e: return("MVI L");
        case 0x2f: return("CMA");
        case 0x30: return("SIM");
        case 0x31: return("LXI SP");
        case 0x32: return("STA");
        case 0x33: return("INX SP");
        case 0x34: return("INR M");
        case 0x35: return("DCR M");
        case 0x36: return("MVI M");
        case 0x37: return("STC");
        case 0x39: return("DAD SP");
        case 0x3a: return("LDA");
        case 0x3b: return("DCX SP");
        case 0x3c: return("INR A");
        case 0x3d: return("DCR A");
        case 0x3e: return("MVI A");
        case 0x3f: return("CMC");
        case 0x40: return("MOV B,B"); break;
        case 0x41: return("MOV B,C"); break;
        case 0x42: return("MOV B,D"); break;
        case 0x43: return("MOV B,E"); break;
        case 0x44: return("MOV B,H"); break;
        case 0x45: return("MOV B,L"); break;
        case 0x46: return("MOV B,M"); break;
        case 0x47: return("MOV B,A"); break;
        case 0x48: return("MOV C,B"); break;
        case 0x49: return("MOV C,C"); break;
        case 0x4a: return("MOV C,D"); break;
        case 0x4b: return("MOV C,E"); break;
        case 0x4c: return("MOV C,H"); break;
        case 0x4d: return("MOV C,L"); break;
        case 0x4e: return("MOV C,M"); break;
        case 0x4f: return("MOV C,A"); break;
        case 0x50: return("MOV D,B"); break;
        case 0x51: return("MOV D,C"); break;
        case 0x52: return("MOV D,D"); break;
        case 0x53: return("MOV D,E"); break;
        case 0x54: return("MOV D,H"); break;
        case 0x55: return("MOV D,L"); break;
        case 0x56: return("MOV D,M"); break;
        case 0x57: return("MOV D,A"); break;
        case 0x58: return("MOV E,B"); break;
        case 0x59: return("MOV E,C"); break;
        case 0x5a: return("MOV E,D"); break;
        case 0x5b: return("MOV E,E"); break;
        case 0x5c: return("MOV E,H"); break;
        case 0x5d: return("MOV E,L"); break;
        case 0x5e: return("MOV E,M"); break;
        case 0x5f: return("MOV E,A"); break;
        case 0x60: return("MOV H,B"); break;
        case 0x61: return("MOV H,C"); break;
        case 0x62: return("MOV H,D"); break;
        case 0x63: return("MOV H,E"); break;
        case 0x64: return("MOV H,H"); break;
        case 0x65: return("MOV H,L"); break;
        case 0x66: return("MOV H,M"); break;
        case 0x67: return("MOV H,A"); break;
        case 0x68: return("MOV L,B"); break;
        case 0x69: return("MOV L,C"); break;
        case 0x6a: return("MOV L,D"); break;
        case 0x6b: return("MOV L,E"); break;
        case 0x6c: return("MOV L,H"); break;
        case 0x6d: return("MOV L,L"); break;
        case 0x6e: return("MOV L,M"); break;
        case 0x6f: return("MOV L,A"); break;
        case 0x70: return("MOV M,B"); break;
        case 0x71: return("MOV M,C"); break;
        case 0x72: return("MOV M,D"); break;
        case 0x73: return("MOV M,E"); break;
        case 0x74: return("MOV M,H"); break;
        case 0x75: return("MOV M,L"); break;
        case 0x76: return("HLT"); break;
        case 0x77: return("MOV M,A"); break;
        case 0x78: return("MOV A,B"); break;
        case 0x79: return("MOV A,C"); break;
        case 0x7a: return("MOV A,D"); break;
        case 0x7b: return("MOV A,E"); break;
        case 0x7c: return("MOV A,H"); break;
        case 0x7d: return("MOV A,L"); break;
        case 0x7e: return("MOV A,M"); break;
        case 0x7f: return("MOV A,A"); break;
        case 0x80: return("ADD B"); break;
        case 0x81: return("ADD C"); break;
        case 0x82: return("ADD D"); break;
        case 0x83: return("ADD E"); break;
        case 0x84: return("ADD H"); break;
        case 0x85: return("ADD L"); break;
        case 0x86: return("ADD M"); break;
        case 0x87: return("ADD A"); break;
        case 0x88: return("ADC B"); break;
        case 0x89: return("ADC C"); break;
        case 0x8a: return("ADC D"); break;
        case 0x8b: return("ADC E"); break;
        case 0x8c: return("ADC H"); break;
        case 0x8d: return("ADC L"); break;
        case 0x8e: return("ADC M"); break;
        case 0x8f: return("ADC A"); break;
        case 0x90: return("SUB B"); break;
        case 0x91: return("SUB C"); break;
        case 0x92: return("SUB D"); break;
        case 0x93: return("SUB E"); break;
        case 0x94: return("SUB H"); break;
        case 0x95: return("SUB L"); break;
        case 0x96: return("SUB M"); break;
        case 0x97: return("SUB A"); break;
        case 0x98: return("SBB B"); break;
        case 0x99: return("SBB C"); break;
        case 0x9a: return("SBB D"); break;
        case 0x9b: return("SBB E"); break;
        case 0x9c: return("SBB H"); break;
        case 0x9d: return("SBB L"); break;
        case 0x9e: return("SBB M"); break;
        case 0x9f: return("SBB A"); break;
        case 0xa0: return("ANA B"); break;
        case 0xa1: return("ANA C"); break;
        case 0xa2: return("ANA D"); break;
        case 0xa3: return("ANA E"); break;
        case 0xa4: return("ANA H"); break;
        case 0xa5: return("ANA L"); break;
        case 0xa6: return("ANA M"); break;
        case 0xa7: return("ANA A"); break;
        case 0xa8: return("XRA B"); break;
        case 0xa9: return("XRA C"); break;
        case 0xaa: return("XRA D"); break;
        case 0xab: return("XRA E"); break;
        case 0xac: return("XRA H"); break;
        case 0xad: return("XRA L"); break;
        case 0xae: return("XRA M"); break;
        case 0xaf: return("XRA A"); break;
        case 0xb0: return("ORA B"); break;
        case 0xb1: return("ORA C"); break;
        case 0xb2: return("ORA D"); break;
        case 0xb3: return("ORA E"); break;
        case 0xb4: return("ORA H"); break;
        case 0xb5: return("ORA L"); break;
        case 0xb6: return("ORA M"); break;
        case 0xb7: return ("ORA A"); break;
        case 0xb8: return ("CMP B"); break;
        case 0xb9: return ("CMP C"); break;
        case 0xba: return ("CMP D"); break;
        case 0xbb: return ("CMP E"); break;
        case 0xbc: return ("CMP H"); break;
        case 0xbd: return ("CMP L"); break;
        case 0xbe: return ("CMP M"); break;
        case 0xbf: return ("CMP A"); break;
        case 0xc0: return ("RNZ"); break;
        case 0xc1: return ("POP B"); break;
        case 0xc2: return ("JNZ");
        case 0xc3: return ("JMP");
        case 0xc4: return ("CNZ");
        case 0xc5: return ("PUSH B"); break;
        case 0xc6: return ("ADI");
        case 0xc7: return ("RST 0"); break;
        case 0xc8: return ("RZ"); break;
        case 0xc9: return ("RET"); break;
        case 0xca: return ("JZ");
        case 0xcc: return ("CZ");
        case 0xcd: return ("CALL");
        case 0xce: return ("ACI"); break;
        case 0xcf: return ("RST 1"); break;
        case 0xd0: return ("RNC"); break;
        case 0xd1: return ("POP D"); break;
        case 0xd2: return ("JNC");
        case 0xd3: return ("OUT");
        case 0xd4: return ("CNC");
        case 0xd5: return ("PUSH D"); break;
        case 0xd6: return ("SUI");
        case 0xd7: return ("RST"); break;
        case 0xd8: return ("RC"); break;
        case 0xda: return ("JC");
        case 0xdb: return ("IN");
        case 0xdc: return ("CC");
        case 0xde: return ("SBI");
        case 0xdf: return ("RST 3"); break;
        case 0xe0: return ("RPO"); break;
        case 0xe1: return ("POP H"); break;
        case 0xe2: return ("JPO");
        case 0xe3: return ("XTHL"); break;
        case 0xe4: return ("CPO");
        case 0xe5: return ("PUSH H"); break;
        case 0xe6: return ("ANI");
        case 0xe7: return ("RST 4"); break;
        case 0xe8: return ("RPE"); break;
        case 0xe9: return ("PCHL"); break;
        case 0xea: return ("JPE");
        case 0xeb: return ("XCHG"); break;
        case 0xec: return ("CPE");
        case 0xee: return ("XRI");
        case 0xef: return ("RST 5"); break;
        case 0xf0: return ("RP"); break;
        case 0xf1: return ("POP PSW"); break;
        case 0xf2: return ("JP");
        case 0xf3: return ("DI"); break;
        case 0xf4: return ("CP");
        case 0xf5: return ("PUSH PSW"); break;
        case 0xf6: return ("ORI");
        case 0xf7: return ("RST 6"); break;
        case 0xf8: return ("RM"); break;
        case 0xf9: return ("SPHL"); break;
        case 0xfa: return ("JM");
        case 0xfb: return ("EI"); break;
        case 0xfc: return ("CM");
        case 0xfe: return ("CPI");
        case 0xff: return ("RST 7");
        default: return("");
    }
}

void OpcodeNotWritten(unsigned char *opcode){
    unsigned char *instruction = opcode;
    printf("%s not defined", HexToAssembly(instruction));
    printf("\n");
}

int Parity(uint16_t x)
{
    int size = 8;
	int i;
	int p = 0;
	x = (x & ((1<<size)-1));
	for (i=0; i<size; i++)
	{
		if (x & 0x1) p++;
		x = x >> 1;
	}
	return (0 == (p & 0x1));
}

void HandleFlags(ProcessorState *emulator, uint16_t result, bool carry){
    
    // Handle Zero
    emulator->flags.z = result == 0 ? 1 : 0; 
    // Handle Sign
    emulator->flags.s = (result & 0x80) ? 1 : 0;
    
    // Handle Parity
    emulator->flags.p = Parity(result) ? 1 : 0;
    
    // Handle Carry
    emulator->flags.cy = carry ? 1 : 0;
    
    // Handle Auxillary Carry
}

uint8_t LoadInputPort(uint8_t index) {
    return port[index];
}

void ProcessOpcode(unsigned char *memory, int pc, ProcessorState *emulator){
    unsigned char *instruction = &memory[emulator->pc];
    
    if(*instruction == 0xdb) { // IN
        emulator->a = LoadInputPort(instruction[1]);
        emulator->pc += 1;
    }
    else if (*instruction == 0xd3) { // OUT
        //OutputToPort(instruction[1]);
        emulator->pc += 1;
    }
    
    switch (*instruction){
        case 0x00: // NOP 
            break;
        case 0x01: // LXI B xxxx
        {
            emulator->bc[1] = instruction[2];
            emulator->bc[0] = instruction[1];
            emulator->pc += 2;
            break;
        }
        case 0x02: // STAX B
        {
            emulator->memory[emulator->bc_pair] = emulator->a;
            break;
        }
        case 0x03: OpcodeNotWritten(&memory[pc]); break;
        case 0x04: OpcodeNotWritten(&memory[pc]); break;
        case 0x05: // DCR B
        {
            uint8_t result = emulator->bc[1] - 1;
            HandleFlags(emulator, result, false);
            emulator->bc[1] = result;
            break;
        }
        case 0x06: // MVI B
        {
            emulator->bc[1] = instruction[1];
            emulator->pc += 1;
            break;
        }
        case 0x07: OpcodeNotWritten(&memory[pc]); break;
        case 0x09: // DAD B
        {
            uint32_t result = emulator->hl_pair + emulator->bc_pair;
            HandleFlags(emulator, result, result>0xffff);
            emulator->hl_pair = result;
            break;
        }
        case 0x0a: OpcodeNotWritten(&memory[pc]); break;
        case 0x0b: OpcodeNotWritten(&memory[pc]); break;
        case 0x0c: OpcodeNotWritten(&memory[pc]); break;
        case 0x0d: // DCR C
        {
            uint8_t result = emulator->bc[0] - 1;
            HandleFlags(emulator,result,false);
            emulator->bc[0] = result;
            break;
        }
        case 0x0e: // MVI C
        {
            emulator->bc[0] = instruction[1];
            emulator->pc += 1;
            break;
        }
        case 0x0f: // RRC
        {
            emulator->flags.cy = (emulator->a & 0x01);
            emulator->a >>= 1;
            
            if(emulator->flags.cy == 1){
                emulator->a |= 0x80;
            }
            break;
        }
        case 0x11: // LXI D
        {
            emulator->de[0] = instruction[1];
            emulator->de[1] = instruction[2];
            emulator->pc += 2;
            break;
        }
        case 0x12: // STAX D
        {
            emulator->memory[emulator->de_pair] = emulator->a;
            break;
        }
        case 0x13: // INX D
        {
            emulator->de_pair += 1;
            break;
        }
        case 0x14: 
        {
            uint8_t result = emulator->de[1] +1;
            emulator->de[1] = result;
            HandleFlags(emulator,result,false);
            break;
        }
        case 0x15: OpcodeNotWritten(&memory[pc]); break;
        case 0x16: OpcodeNotWritten(&memory[pc]); break;
        case 0x17: OpcodeNotWritten(&memory[pc]); break;
        case 0x19: // DAD D
        {
            uint32_t result = emulator->hl_pair + emulator->de_pair;
            emulator->flags.cy = result>0xfff ? 1 : 0;
            emulator->hl_pair = result;
            break;
        }
        case 0x1a: // LDAX D
        {
            uint8_t result = emulator->memory[emulator->de_pair];
            emulator->a = result;
            break;
        }
        case 0x1b: OpcodeNotWritten(&memory[pc]); break;
        case 0x1c: OpcodeNotWritten(&memory[pc]); break;
        case 0x1d: OpcodeNotWritten(&memory[pc]); break;
        case 0x1e: OpcodeNotWritten(&memory[pc]); break;
        case 0x1f: OpcodeNotWritten(&memory[pc]); break;
        case 0x20: OpcodeNotWritten(&memory[pc]); break;
        case 0x21: // LXI H
        {
            emulator->hl[0] = instruction[1];
            emulator->hl[1] = instruction[2];
            emulator->pc += 2;
            break;
        }
        case 0x22: // SHLD
        {
            emulator->memory[instruction[1]] = emulator->hl[0];
            emulator->memory[instruction[2]] = emulator->hl[1];
        }
        case 0x23: // INX H
        {
            emulator->hl_pair += 1;
            break;
        }
        case 0x24: OpcodeNotWritten(&memory[pc]); break;
        case 0x25: OpcodeNotWritten(&memory[pc]); break;
        case 0x26: // MVI H
        {
            emulator->hl[1] = instruction[1];
            emulator->pc += 1;
            break;
        }
        case 0x27: OpcodeNotWritten(&memory[pc]); break;
        case 0x29: // DAD H
        {
            uint32_t result = emulator->hl_pair + emulator->hl_pair;
            emulator->flags.cy = result>0xfff ? 1 : 0;
            emulator->hl_pair = result;
            break;
        }
        case 0x2a: OpcodeNotWritten(&memory[pc]); break;
        case 0x2b: OpcodeNotWritten(&memory[pc]); break;
        case 0x2c: OpcodeNotWritten(&memory[pc]); break;
        case 0x2d: OpcodeNotWritten(&memory[pc]); break;
        case 0x2e: OpcodeNotWritten(&memory[pc]); break;
        case 0x2f: OpcodeNotWritten(&memory[pc]); break;
        case 0x30: OpcodeNotWritten(&memory[pc]); break;
        case 0x31: // LXI SP
        {
            emulator->sp_hl[0] = instruction[1];
            emulator->sp_hl[1] = instruction[2];
            emulator->pc += 2;
            break;
        }
        case 0x32: // STA
        {
            emulator->seperate[0] = instruction[1];
            emulator->seperate[1] = instruction[2];
            memory[emulator->combined] = emulator->a;
            emulator->pc += 2;
            break;
        }
        case 0x33: OpcodeNotWritten(&memory[pc]); break;
        case 0x34: OpcodeNotWritten(&memory[pc]); break;
        case 0x35: 
        {
            emulator->memory[emulator->hl_pair] -= 1;
            uint8_t result = emulator->memory[emulator->hl_pair];
            HandleFlags(emulator,result,false);
            break;
        }
        case 0x36: // MVI M
        {
            emulator->memory[emulator->hl_pair] = instruction[1];
            emulator->pc += 1;
            break;
        }
        case 0x37: OpcodeNotWritten(&memory[pc]); break;
        case 0x39: OpcodeNotWritten(&memory[pc]); break;
        case 0x3a: // LDA
        {
            emulator->seperate[0] = instruction[1];
            emulator->seperate[1] = instruction[2];
            emulator->a = memory[emulator->combined];
            emulator->pc += 2;
            break;
        }
        case 0x3b: OpcodeNotWritten(&memory[pc]); break;
        case 0x3c: OpcodeNotWritten(&memory[pc]); break;
        case 0x3d: OpcodeNotWritten(&memory[pc]); break;
        case 0x3e: // MVI A
        {
            emulator->a = instruction[1];
            emulator->pc += 1;
            break;
        }
        case 0x3f: OpcodeNotWritten(&memory[pc]); break;
        case 0x40: OpcodeNotWritten(&memory[pc]); break;
        case 0x41: // MOV B,C 
        {
            emulator->bc[1] = emulator->bc[0];
            break;
        }
        case 0x42: OpcodeNotWritten(&memory[pc]); break;
        case 0x43: OpcodeNotWritten(&memory[pc]); break;
        case 0x44: OpcodeNotWritten(&memory[pc]); break;
        case 0x45: OpcodeNotWritten(&memory[pc]); break;
        case 0x46: OpcodeNotWritten(&memory[pc]); break;
        case 0x47: OpcodeNotWritten(&memory[pc]); break;
        case 0x48: OpcodeNotWritten(&memory[pc]); break;
        case 0x49: OpcodeNotWritten(&memory[pc]); break;
        case 0x4a: OpcodeNotWritten(&memory[pc]); break;
        case 0x4b: OpcodeNotWritten(&memory[pc]); break;
        case 0x4c: OpcodeNotWritten(&memory[pc]); break;
        case 0x4d: OpcodeNotWritten(&memory[pc]); break;
        case 0x4e: OpcodeNotWritten(&memory[pc]); break;
        case 0x4f: OpcodeNotWritten(&memory[pc]); break;
        case 0x50: OpcodeNotWritten(&memory[pc]); break;
        case 0x51: OpcodeNotWritten(&memory[pc]); break;
        case 0x52: OpcodeNotWritten(&memory[pc]); break;
        case 0x53: OpcodeNotWritten(&memory[pc]); break;
        case 0x54: OpcodeNotWritten(&memory[pc]); break;
        case 0x55: OpcodeNotWritten(&memory[pc]); break;
        case 0x56: // MOV D,M
        {
            emulator->de[1] = emulator->memory[emulator->hl_pair];
            break;
        }
        case 0x57: OpcodeNotWritten(&memory[pc]); break;
        case 0x58: OpcodeNotWritten(&memory[pc]); break;
        case 0x59: OpcodeNotWritten(&memory[pc]); break;
        case 0x5a: OpcodeNotWritten(&memory[pc]); break;
        case 0x5b: OpcodeNotWritten(&memory[pc]); break;
        case 0x5c: OpcodeNotWritten(&memory[pc]); break;
        case 0x5d: OpcodeNotWritten(&memory[pc]); break;
        case 0x5e: // MOV E,M
        {
            emulator->de[0] = emulator->memory[emulator->hl_pair];
            break;
        }
        case 0x5f: OpcodeNotWritten(&memory[pc]); break;
        case 0x60: OpcodeNotWritten(&memory[pc]); break;
        case 0x61: OpcodeNotWritten(&memory[pc]); break;
        case 0x62: OpcodeNotWritten(&memory[pc]); break;
        case 0x63: OpcodeNotWritten(&memory[pc]); break;
        case 0x64: OpcodeNotWritten(&memory[pc]); break;
        case 0x65: OpcodeNotWritten(&memory[pc]); break;
        case 0x66: // MOV H,M
        {
            emulator->hl[1] = emulator->memory[emulator->hl_pair];
            break;
        }
        case 0x67: // MOV H,A
        {
            emulator->hl[1] = emulator->a;
            break;
        }
        case 0x68: OpcodeNotWritten(&memory[pc]); break;
        case 0x69: OpcodeNotWritten(&memory[pc]); break;
        case 0x6a: OpcodeNotWritten(&memory[pc]); break;
        case 0x6b: OpcodeNotWritten(&memory[pc]); break;
        case 0x6c: OpcodeNotWritten(&memory[pc]); break;
        case 0x6d: OpcodeNotWritten(&memory[pc]); break;
        case 0x6e: OpcodeNotWritten(&memory[pc]); break;
        case 0x6f: // MOV L,A
        {
            emulator->hl[0] = emulator->a;
            break;
        }
        case 0x70: OpcodeNotWritten(&memory[pc]); break;
        case 0x71: OpcodeNotWritten(&memory[pc]); break;
        case 0x72: OpcodeNotWritten(&memory[pc]); break;
        case 0x73: OpcodeNotWritten(&memory[pc]); break;
        case 0x74: OpcodeNotWritten(&memory[pc]); break;
        case 0x75: OpcodeNotWritten(&memory[pc]); break;
        case 0x76: OpcodeNotWritten(&memory[pc]); break;
        case 0x77: // MOV M,A
        {
            emulator->memory[emulator->hl_pair] = emulator->a;
            break;
        }
        case 0x78: OpcodeNotWritten(&memory[pc]); break;
        case 0x79: OpcodeNotWritten(&memory[pc]); break;
        case 0x7a: // MOV A,D
        {
            emulator->a = emulator->de[1];
            break;
        }
        case 0x7b: // MOV A,E
        {
            emulator->a = emulator->de[0];
            break;
        }
        case 0x7c: // MOV A,H
        {
            emulator->a = emulator->hl[1];
            break;
        }
        case 0x7d: OpcodeNotWritten(&memory[pc]); break;
        case 0x7e: // MOV A,M
        {
            emulator->a = emulator->memory[emulator->hl_pair];
            break;
        }
        case 0x7f: OpcodeNotWritten(&memory[pc]); break;
        case 0x80: OpcodeNotWritten(&memory[pc]); break;
        case 0x81: OpcodeNotWritten(&memory[pc]); break;
        case 0x82: // ADD D
        {
            // not sure.
            uint16_t result = emulator->a + emulator->de[1];
            
            // Handle AC.??
            HandleFlags(emulator,result,true);
            break;
        }
        case 0x83: OpcodeNotWritten(&memory[pc]); break;
        case 0x84: OpcodeNotWritten(&memory[pc]); break;
        case 0x85: OpcodeNotWritten(&memory[pc]); break;
        case 0x86: OpcodeNotWritten(&memory[pc]); break;
        case 0x87: OpcodeNotWritten(&memory[pc]); break;
        case 0x88: OpcodeNotWritten(&memory[pc]); break;
        case 0x89: OpcodeNotWritten(&memory[pc]); break;
        case 0x8a: OpcodeNotWritten(&memory[pc]); break;
        case 0x8b: OpcodeNotWritten(&memory[pc]); break;
        case 0x8c: OpcodeNotWritten(&memory[pc]); break;
        case 0x8d: OpcodeNotWritten(&memory[pc]); break;
        case 0x8e: OpcodeNotWritten(&memory[pc]); break;
        case 0x8f: OpcodeNotWritten(&memory[pc]); break;
        case 0x90: OpcodeNotWritten(&memory[pc]); break;
        case 0x91: OpcodeNotWritten(&memory[pc]); break;
        case 0x92: OpcodeNotWritten(&memory[pc]); break;
        case 0x93: OpcodeNotWritten(&memory[pc]); break;
        case 0x94: OpcodeNotWritten(&memory[pc]); break;
        case 0x95: OpcodeNotWritten(&memory[pc]); break;
        case 0x96: OpcodeNotWritten(&memory[pc]); break;
        case 0x97: OpcodeNotWritten(&memory[pc]); break;
        case 0x98: OpcodeNotWritten(&memory[pc]); break;
        case 0x99: OpcodeNotWritten(&memory[pc]); break;
        case 0x9a: OpcodeNotWritten(&memory[pc]); break;
        case 0x9b: OpcodeNotWritten(&memory[pc]); break;
        case 0x9c: OpcodeNotWritten(&memory[pc]); break;
        case 0x9d: OpcodeNotWritten(&memory[pc]); break;
        case 0x9e: OpcodeNotWritten(&memory[pc]); break;
        case 0x9f: OpcodeNotWritten(&memory[pc]); break;
        case 0xa0: OpcodeNotWritten(&memory[pc]); break;
        case 0xa1: OpcodeNotWritten(&memory[pc]); break;
        case 0xa2: OpcodeNotWritten(&memory[pc]); break;
        case 0xa3: OpcodeNotWritten(&memory[pc]); break;
        case 0xa4: OpcodeNotWritten(&memory[pc]); break;
        case 0xa5: OpcodeNotWritten(&memory[pc]); break;
        case 0xa6: OpcodeNotWritten(&memory[pc]); break;
        case 0xa7: // ANA A
        {
            uint16_t result = emulator->a & emulator->a;
            HandleFlags(emulator,result, result>0xff);
            emulator->a = result;
            break;
        }
        case 0xa8: OpcodeNotWritten(&memory[pc]); break;
        case 0xa9: OpcodeNotWritten(&memory[pc]); break;
        case 0xaa: OpcodeNotWritten(&memory[pc]); break;
        case 0xab: OpcodeNotWritten(&memory[pc]); break;
        case 0xac: OpcodeNotWritten(&memory[pc]); break;
        case 0xad: OpcodeNotWritten(&memory[pc]); break;
        case 0xae: OpcodeNotWritten(&memory[pc]); break;
        case 0xaf: // XRA A
        {
            uint16_t result = emulator->a ^ emulator->a;
            HandleFlags(emulator,result, result>0xff);
            emulator->a = result;
            break;
        }
        case 0xb0: OpcodeNotWritten(&memory[pc]); break;
        case 0xb1: OpcodeNotWritten(&memory[pc]); break;
        case 0xb2: OpcodeNotWritten(&memory[pc]); break;
        case 0xb3: OpcodeNotWritten(&memory[pc]); break;
        case 0xb4: OpcodeNotWritten(&memory[pc]); break;
        case 0xb5: OpcodeNotWritten(&memory[pc]); break;
        case 0xb6: OpcodeNotWritten(&memory[pc]); break;
        case 0xb7: OpcodeNotWritten(&memory[pc]); break;
        case 0xb8: OpcodeNotWritten(&memory[pc]); break;
        case 0xb9: OpcodeNotWritten(&memory[pc]); break;
        case 0xba: OpcodeNotWritten(&memory[pc]); break;
        case 0xbb: OpcodeNotWritten(&memory[pc]); break;
        case 0xbc: OpcodeNotWritten(&memory[pc]); break;
        case 0xbd: OpcodeNotWritten(&memory[pc]); break;
        case 0xbe: OpcodeNotWritten(&memory[pc]); break;
        case 0xbf: OpcodeNotWritten(&memory[pc]); break;
        case 0xc0: OpcodeNotWritten(&memory[pc]); break;
        case 0xc1: // POP B
        {
            emulator->bc[0] = emulator->memory[emulator->sp];
            emulator->bc[1] = emulator->memory[emulator->sp+1];
            emulator->sp += 2;
            break;
        }
        case 0xc2: // JNZ
        {
            emulator->seperate[0] = instruction[1];
            emulator->seperate[1] = instruction[2];
            emulator->pc = emulator->flags.z ? emulator->pc+2 : emulator->combined-1;
            break;
        }
        case 0xc3: // JMP
        {
            emulator->seperate[0] = instruction[1];
            emulator->seperate[1] = instruction[2];
            emulator->pc = emulator->combined - 1;
            break;
        }
        case 0xc4: OpcodeNotWritten(&memory[pc]); break;
        case 0xc5: // PUSH B
        {
            emulator->memory[emulator->sp-2] = emulator->bc[0];
            emulator->memory[emulator->sp-1] = emulator->bc[1];
            emulator->sp -=2;
            break;
        }
        case 0xc6: // ADI D8
        {
            uint16_t result = emulator->a + instruction[1];
            HandleFlags(emulator,result, result>0xff);
            emulator->a = result;
            emulator->pc += 1;
            break;
        }
        case 0xc7: OpcodeNotWritten(&memory[pc]); break;
        case 0xc8: // RZ
        {
            if(emulator->flags.z == true) {
                emulator->pc_hl[0] = emulator->memory[emulator->sp];
                emulator->pc_hl[1] = emulator->memory[emulator->sp+1];
                emulator->sp += 2;
            }
            break;
        }
        case 0xc9: // RET
        {
            emulator->pc_hl[0] = emulator->memory[emulator->sp]; 
            emulator->pc_hl[1] = emulator->memory[emulator->sp+1];
            emulator->sp += 2;
            break;
        }
        case 0xca: // JZ
        {
            if(emulator->flags.z == true) {
                emulator->pc_hl[0] = instruction[1];
                emulator->pc_hl[1] = instruction[2];
            }
            break;
        }
        case 0xcc: OpcodeNotWritten(&memory[pc]); break;
        case 0xcd: // CALL
        {
            emulator->memory[emulator->sp-1] = emulator->pc_hl[1];
            emulator->memory[emulator->sp-2] = emulator->pc_hl[0]+2;
            emulator->sp -= 2;
            emulator->seperate[0] = instruction[1];
            emulator->seperate[1] = instruction[2];
            emulator->pc = emulator->combined - 1;
            break;
        }
        case 0xce: OpcodeNotWritten(&memory[pc]); break;
        case 0xcf: OpcodeNotWritten(&memory[pc]); break;
        case 0xd0: OpcodeNotWritten(&memory[pc]); break;
        case 0xd1: // POP D
        {
            emulator->de[0] = emulator->memory[emulator->sp];
            emulator->de[1] = emulator->memory[emulator->sp+1];
            emulator->sp += 2;
            break;
        }
        case 0xd2: OpcodeNotWritten(&memory[pc]); break;
        case 0xd3: // OUT
        {
            // Handled as an interupt at top.
            break;
        }
        case 0xd4: OpcodeNotWritten(&memory[pc]); break;
        case 0xd5: // PUSH D
        {
            emulator->memory[emulator->sp-2] = emulator->de[0];
            emulator->memory[emulator->sp-1] = emulator->de[1];
            emulator->sp -= 2;
            break;
        }
        case 0xd6: OpcodeNotWritten(&memory[pc]); break;
        case 0xd7: OpcodeNotWritten(&memory[pc]); break;
        case 0xd8: OpcodeNotWritten(&memory[pc]); break;
        case 0xda: // JC
        {
            if(emulator->flags.cy == true) {
                emulator->pc_hl[0] = instruction[1];
                emulator->pc_hl[1] = instruction[2];
            }
            break;
        }
        case 0xdb: break; // IN - Implemented above.
        case 0xdc: OpcodeNotWritten(&memory[pc]); break;
        case 0xde: OpcodeNotWritten(&memory[pc]); break;
        case 0xdf: OpcodeNotWritten(&memory[pc]); break;
        case 0xe0: OpcodeNotWritten(&memory[pc]); break;
        case 0xe1: // POP H
        {
            emulator->hl[0] = emulator->memory[emulator->sp];
            emulator->hl[1] = emulator->memory[emulator->sp+1];
            emulator->sp += 2;
            break;
        }
        case 0xe2: OpcodeNotWritten(&memory[pc]); break;
        case 0xe3: OpcodeNotWritten(&memory[pc]); break;
        case 0xe4: OpcodeNotWritten(&memory[pc]); break;
        case 0xe5: // PUSH H
        {
            emulator->memory[emulator->sp-2] = emulator->hl[0];
            emulator->memory[emulator->sp-1] = emulator->hl[1];
            emulator->sp -= 2;
            break;
        }
        case 0xe6: // ANI
        {
            uint16_t result = emulator->a & instruction[1];
            HandleFlags(emulator,result, result>0xff);
            emulator->pc += 1;
            break;
        }
        case 0xe7: OpcodeNotWritten(&memory[pc]); break;
        case 0xe8: OpcodeNotWritten(&memory[pc]); break;
        case 0xe9: // PCHL
        {
            emulator->pc_hl[0] = emulator->hl[0];
            emulator->pc_hl[1] = emulator->hl[1];
            break;
        }
        case 0xea: OpcodeNotWritten(&memory[pc]); break;
        case 0xeb: // XCHG
        {
            uint16_t hl = emulator->hl_pair;
            emulator->hl_pair = emulator->de_pair;
            emulator->de_pair = hl;
            break;
        }
        case 0xec: OpcodeNotWritten(&memory[pc]); break;
        case 0xee: OpcodeNotWritten(&memory[pc]); break;
        case 0xef: OpcodeNotWritten(&memory[pc]); break;
        case 0xf0: // RP
        {
            if(emulator->flags.z == false) {
                emulator->pc_hl[0] = emulator->memory[emulator->sp];
                emulator->pc_hl[1] = emulator->memory[emulator->sp+1];
                emulator->sp += 2;
            }
            break;
        }
        case 0xf1: // POP PSW
        {
            emulator->flags.z = (emulator->memory[emulator->sp] & 0x01);
            emulator->flags.s = (emulator->memory[emulator->sp] & 0x02);
            emulator->flags.p = (emulator->memory[emulator->sp] & 0x04);
            emulator->flags.cy = (emulator->memory[emulator->sp] & 0x08);
            emulator->flags.ac= (emulator->memory[emulator->sp] & 0x010);

            emulator->a = emulator->memory[emulator->sp+1];
            emulator->sp +=2;
            break;
        }
        case 0xf2: OpcodeNotWritten(&memory[pc]); break;
        case 0xf3: OpcodeNotWritten(&memory[pc]); break;
        case 0xf4: OpcodeNotWritten(&memory[pc]); break;
        case 0xf5: // PUSH PSW
        {
            uint8_t flag_info = (
                emulator->flags.z |
                emulator->flags.s << 1 |
                emulator->flags.p << 2 |
                emulator->flags.cy << 3 |
                emulator->flags.ac << 4
            );
            
            emulator->memory[emulator->sp-2] = flag_info;
            emulator->memory[emulator->sp-1] = emulator->a;
            emulator->sp -= 2;
            break;
        }
        case 0xf6: OpcodeNotWritten(&memory[pc]); break;
        case 0xf7: OpcodeNotWritten(&memory[pc]); break;
        case 0xf8: OpcodeNotWritten(&memory[pc]); break;
        case 0xf9: OpcodeNotWritten(&memory[pc]); break;
        case 0xfa: OpcodeNotWritten(&memory[pc]); break;
        case 0xfb: // EI
        {
            break;
        }
        case 0xfc: OpcodeNotWritten(&memory[pc]); break;
        case 0xfe: // CPI
        {
            uint8_t result = emulator->a - instruction[1];
            
            emulator->flags.z = (result == 0);
            emulator->flags.s = (0x80 == (result & 0x80));
            emulator->flags.p = Parity(result);
            emulator->flags.cy = (emulator->a < instruction[1]);

            emulator->pc += 1;
            break;
        }
        case 0xff: OpcodeNotWritten(&memory[pc]); break;
        default: (OpcodeNotWritten(&memory[pc]));
    }
    
    emulator->pc += 1;
}

      ProcessorState emulator = { 
        .a = 0, 
        .bc = 0, 
        .de = 0, 
        .hl = 0, 
        .sp = 0xf000, 
        .pc = 0, 
        .flags.z = 0,
        .flags.s = 0,
        .flags.p = 0,
        .flags.cy = 0,
        .int_enable = 0
        };


SDL_Renderer *renderer;

void RenderTop() {    
    int y = 0;
    
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for(int i=32; i > 16; --i) {
        for(int bit=7; bit >=0; --bit) {
            int x = 0;
            
            for(int j=0; j <= 223; ++j) {
                int index = i + (j * 32-1);
                
                if(framebuffer[index] & (1 << bit)) {
                    SDL_RenderDrawPoint(renderer, x, y);
                }
                
                x++;
            }
            
            y++;
        }
    }
    SDL_RenderPresent(renderer);
}

void RenderBottom() {    
    int y = HEIGHT/2;
    
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    for(int i=16; i >= 0; --i) {
        for(int bit=7; bit >=0; --bit) {
            int x = 0;
            
            for(int j=0; j <= 223; ++j) {
                int index = i + (j * 32-1);
                
                if(framebuffer[index] & (1 << bit)) {
                    SDL_RenderDrawPoint(renderer, x, y);
                }
                
                x++;
            }
            
            y++;
        }
    }
    SDL_RenderPresent(renderer);
}

void GenerateInterrupt(ProcessorState* emulator, int interrupt_num)    
   {    
    //perform "PUSH PC"
    emulator->memory[emulator->sp-2] = emulator->pc_hl[0];
    emulator->memory[emulator->sp-1] = emulator->pc_hl[1];

    //Set the PC to the low memory vector.    
    //This is identical to an "RST interrupt_num" instruction.    
    emulator->pc = 8 * interrupt_num;    
}   


void *OutputBitMap(void *p){
    clock_t last_top = clock();
    clock_t last_bottom = clock();
    
    while(true){
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
        SDL_RenderClear(renderer);

        clock_t top = clock() - last_top;
        double top_sec = (top * 1000 / CLOCKS_PER_SEC);
        if(top_sec >= 16.6667) {
            RenderTop();
            GenerateInterrupt(&emulator,1);
        }
        clock_t bottom = clock() - last_bottom;
        double bottom_sec = (bottom * 1000 / CLOCKS_PER_SEC);
        if(bottom_sec >= 16.6667){
            RenderBottom();
            GenerateInterrupt(&emulator,2);
        }
    }
}

int main (int argc, char** argv) {
    SDL_Event event;
    SDL_Window *window;
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(WIDTH*3, HEIGHT*3, 0, &window, &renderer);
    SDL_RenderSetLogicalSize(renderer, WIDTH, HEIGHT);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
    SDL_RenderPresent(renderer);
    
        
    LoadRom(&emulator);
    pthread_t tid;
    pthread_create(&tid, NULL, OutputBitMap, &emulator);
    int count = 0;
    clock_t start = clock();
    while(true){
        if (SDL_PollEvent(&event)) {
            if(event.type == SDL_QUIT) {
                break;
            }
            if(event.type == SDL_KEYDOWN) {
                switch( event.key.keysym.sym) {
                    case SDLK_LEFT:
                        printf("LEFT PRESSED\n");
                        break;
                    case SDLK_RIGHT:
                        printf("RIGHT PRESSED\n");
                        break;
                    case SDLK_UP:
                        printf("UP PRESSED\n");
                        break;
                    case SDLK_DOWN:
                        printf("DOWN PRESSED\n");
                        break;
                    case SDLK_z:
                        printf("FIRE PRESSED\n");
                        break;
                    case SDLK_1:
                        port[1] |= 0x01;
                        break;
                    default:
                        printf("Generic pressed\n");
                        break;
                }
            }
            if(event.type == SDL_KEYUP) {
                switch( event.key.keysym.sym) {
                    case SDLK_LEFT:
                        printf("LEFT RELEASED\n");
                        break;
                    case SDLK_RIGHT:
                        printf("RIGHT RELEASED\n");
                        break;
                    case SDLK_UP:
                        printf("UP RELEASED\n");
                        break;
                    case SDLK_DOWN:
                        printf("DOWN RELEASED\n");
                        break;
                    case SDLK_z:
                        printf("FIRE RELEASED\n");
                        break;
                    case SDLK_1:
                        port[1] &= ~0x01;
                        break;
                    default:
                        printf("Generic released\n");
                        break;
                }
            }
        }
        
        clock_t difference = clock() - start;
        int seconds = (difference * 1000 / CLOCKS_PER_SEC) / 1000;
        if(seconds % 1 == 0){
                    ProcessOpcode(emulator.memory,emulator.pc, &emulator);
        }
        
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    
    return 0;    
}  