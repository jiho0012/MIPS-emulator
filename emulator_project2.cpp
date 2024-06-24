#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <map>
#include <iomanip>
#include <cstring>

#define DATA_START 0x10000000
#define INST_START 0x00400000
unsigned long long PC = INST_START;
std::array<unsigned int,32> registers = {0};
std::map<unsigned int, unsigned int> memory_text;
std::map<unsigned int, unsigned int> memory_data;

void loadMemory(const std::string& filename) {
    std::ifstream file(filename);
    if (!file) {
        std::cerr << "Error opening file\n";
        return;
    }

    std::string line;
    getline(file, line);
    unsigned int text_size = std::stoul(line, nullptr, 16) / 4;  // Convert hex string to decimal and get count
    getline(file, line);
    unsigned int data_size = std::stoul(line, nullptr, 16) / 4;

    unsigned int address = INST_START;
    for (unsigned int i = 0; i < text_size; ++i) {
        if (!getline(file, line)) {
            std::cerr << "Failed to read an instruction\n";
            return;
        }
        unsigned int instruction = std::stoul(line, nullptr, 16);
        memory_text[address] = instruction;
        address += 4;
    }

    address = DATA_START;
    for (unsigned int i = 0; i < data_size; ++i) {
        if (!getline(file, line)) {
            std::cerr << "Failed to read a value\n";
            return;
        }
        unsigned int value = std::stoul(line, nullptr, 16);
        memory_data[address] = value;
        address += 4;
    }
    file.close();
}

void printRegisters() {
    std::cout << "Current register values:\n";
    std::cout << "-------------------------------------\n";
    std::cout << "PC: 0x" << std::hex << PC << std::dec << "\n";  // PC 값을 16진수로 출력
    std::cout << "Registers :\n";
    for (int i = 0; i < 32; ++i) {
        std::cout << "R" << i << ": 0x" << std::hex << registers[i] << std::dec << "\n";  // 레지스터 값들을 16진수로 출력
    }
}

unsigned int read_memory_data(unsigned int address) {
    if (memory_data.find(address) != memory_data.end()) {
        return memory_data[address];
    }
    return 0; // Default to 0 if address is not in memory_data
}

void write_memory_data(unsigned int address, unsigned int value) {
    memory_data[address] = value;
}

void printMemory(const std::map<unsigned int, unsigned int>& memory, unsigned int start, unsigned int end) {
    std::cout << "\nMemory content [" << std::hex << start << ".." << end << "]:\n";
    std::cout << "-------------------------------------\n";
    for (unsigned int addr = start; addr <= end; addr += 4) {
        auto it = memory.find(addr);
        if (it != memory.end()) {
            std::cout << "0x" << std::hex << addr << ": 0x" << it->second << "\n";
        } else {
            std::cout << "0x" << std::hex << addr << ": 0x0\n";
        }
    }
}


void execute_inst(unsigned int addr) {
    unsigned int instruction = memory_text[addr];
    unsigned int opcode = (instruction >> 26) & 0x3F;
    unsigned int rs = (instruction >> 21) & 0x1F;
    unsigned int rt = (instruction >> 16) & 0x1F;
    unsigned int rd = (instruction >> 11) & 0x1F;
    unsigned int shamt = (instruction >> 6) & 0x1F;
    unsigned int func = instruction & 0x3F;
    unsigned int imm = instruction & 0xFFFF;       // Immediate value
    unsigned int target = instruction & 0x03FFFFFF; // Target address

    if (opcode == 0) { // R-type
	switch (func) {
            case 0x24: // AND
                registers[rd] = registers[rs] & registers[rt];
		PC+=4;
		break;
	    case 0x21: //addu
		registers[rd] = registers[rs]+registers[rt];
		PC+=4;
		break;
	    case 0x8: //jr
		PC=registers[rs];
		break;
	    case 0x27: //nor
		registers[rd]=~(registers[rs]|registers[rt]);
		PC+=4;
		break;
	    case 0x25: //or
		registers[rd]=registers[rs]|registers[rt];
		PC+=4;
		break;
	    case 0x2b: //sltu
		registers[rd]=(registers[rs]<registers[rt]) ? 1: 0;
		PC+=4;
		break;
	    case 0x0: //sll
		registers[rd]=registers[rt]<<shamt;
		PC+=4;
		break;
	    case 0x2: //srl
		registers[rd]=registers[rt]>>shamt;
                PC+=4;
                break;
	    case 0x23: //subu
		registers[rd]=registers[rs]-registers[rt];
		PC+=4;
		break;
        }
    } else if (opcode== 0x9) { //addiu
	registers[rt]=registers[rs]+static_cast<short>(imm);
	PC+=4;
    } else if (opcode== 0xc){ //andi
	registers[rt]=registers[rs] & (imm & 0xffff); //zero-extention
	PC+=4;
    
    } else if (opcode == 0x4){ //beq
	if (registers[rs] == registers[rt]) {
            PC += 4 + (static_cast<short>(imm) << 2); // Calculate branch address
        } else {
            PC += 4; // Move to next instruction
        }
    } else if (opcode== 0x5){ //bne
	if (registers[rs] != registers[rt]) {
	    PC +=4 + (static_cast<short>(imm) << 2);		
	} else	{
	    PC+=4;
	}
    } else if (opcode==0x2){ //j
    	PC=(PC&0xF0000000)|(target<<2);
    } else if (opcode==0x3){ //jal
        registers[31]= PC+4;
	PC=(PC&0xF0000000)|(target<<2);
    } else if (opcode==0xf) { //lui
    	registers[rt]=imm<<16;
	PC+=4;
    } else if (opcode==0x23){//lw
	int32_t offset = static_cast<int16_t>(imm); // Sign-extend
        uint32_t address = registers[rs] + offset;
        registers[rt] = read_memory_data(address);
	PC+=4;
    } else if (opcode==0x20) {//lb
    	int32_t offset = static_cast<int16_t>(imm);  // Sign-extend the immediate value
    	uint32_t address = registers[rs] + offset;  // Calculate the address
    	uint8_t byte = read_memory_data(address) & 0xFF;   // Read a byte from memory and apply correct mask
    	registers[rt] = static_cast<int8_t>(byte); // Correctly sign-extend the byte to 32 bits
	PC += 4;    
    } else if (opcode==0x2b){ //sw
        int32_t offset = static_cast<int16_t>(imm);  // Sign-extend the immediate value
        uint32_t address = registers[rs] + offset;  // Calculate the address
        write_memory_data(address, registers[rt]);
	PC+=4;
    } else if (opcode==0x28){ //sb
    	int32_t offset = static_cast<int16_t>(imm);  // Sign-extend the immediate value
        uint32_t address = registers[rs] + offset;  // Calculate the address
        write_memory_data(address, registers[rt] & 0xFF);
	PC+=4;
    } else if (opcode==0xd){//ori
        registers[rt]=registers[rs] | (imm & 0xffff); //zero-extention
	PC+=4;
    } else if (opcode==0xb){//sltiu
    	registers[rt] = (registers[rs] < static_cast<unsigned int>(static_cast<int16_t>(imm))) ? 1 : 0;
	PC+=4;
    }
    


    
    // I-type 및 J-type 명령어 처리 예시
    // 예: opcode == 0x2 (JUMP), opcode == 0x4 (BEQ) 등
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " [-m addr1:addr2] [-d] [-n num_instruction] <input file>\n";
        return 1;
    }

    std::string filename = argv[argc - 1];
    loadMemory(filename);

    bool debug_mode = false;
    bool mem_range_specified = false;
    unsigned int mem_start, mem_end;
    int num_instructions = -1; // -1 means unlimited


    for (int i = 1; i < argc - 1; ++i) {
        if (strcmp(argv[i], "-d") == 0) {
            debug_mode = true;
        } else if (strcmp(argv[i], "-m") == 0 && i + 1 < argc) {
            mem_range_specified = true;
            std::string range = argv[++i];
            size_t colon = range.find(':');
            std::stringstream ss;
            ss << std::hex << range.substr(0, colon) << " " << range.substr(colon + 1);
            ss >> mem_start >> mem_end;
        } else if (strcmp(argv[i], "-n") == 0 && i + 1 < argc) {
            num_instructions = std::stoi(argv[++i]); // Get the number of instructions to execute
        }
    }

    int executed_instructions = 0;
    while (memory_text[PC] != 0x00000000 && (num_instructions == -1 || executed_instructions < num_instructions)) {
        execute_inst(PC);
	executed_instructions++;

    	if (debug_mode){
	    printRegisters();
    	    if (mem_range_specified) {
        	if (mem_start < DATA_START) {
            	    printMemory(memory_text, mem_start, mem_end);
        	} else {
            	    printMemory(memory_data, mem_start, mem_end);
        	}
	    }
	}
    }
    if (debug_mode==false){
    printRegisters();
    if (mem_range_specified) {
        if (mem_start < DATA_START) {
            printMemory(memory_text, mem_start, mem_end);
        } else {
            printMemory(memory_data, mem_start, mem_end);
        }
    }
    }
    return 0;
}
