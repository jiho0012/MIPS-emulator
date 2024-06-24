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
unsigned int PC = INST_START;
std::array<unsigned int,32> registers = {0};
std::map<unsigned int, unsigned int> memory_text;
std::map<unsigned int, unsigned int> memory_data;

struct stage {
    unsigned int pc;
    unsigned int instruction;
    unsigned int opcode;
    unsigned int rs;
    unsigned int rt;
    unsigned int rd;
    unsigned int shamt;
    unsigned int func;
    unsigned int imm;
    unsigned int target;
    unsigned int ALU_out;
    unsigned int MEM_out;
    unsigned int BR_target;
    unsigned int rs_value;
    unsigned int rt_value;
    unsigned int taken;

    stage() : pc(0), instruction(0), opcode(0), rs(0), rt(0), rd(0), shamt(0), func(0), imm(0),
              target(0), ALU_out(0), MEM_out(0), BR_target(0), rs_value(0), rt_value(0), taken(0) {}
};
stage PC_IF;
stage IF_ID;
stage ID_EX;
stage EX_MEM;
stage MEM_WB;
stage stall;
stage buffer;

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
    std::cout << "-------------------------------------------\n";
    std::cout << "PC: 0x" << std::hex << PC << std::dec << "\n";  // PC 값을 16진수로 출력
    std::cout << "Registers :\n";
    for (int i = 0; i < 32; ++i) {
        std::cout << "R" << i << ": 0x" << std::hex << registers[i] << std::dec << "\n";  // 레지스터 값들을 16진수로 출력
    }
}



void printPipelineState() {
    auto format_stage = [](unsigned int pc) -> std::string {
        if (memory_text[pc] == 0) return "";  // PC가 0이면 빈 문자열 반환
        std::stringstream ss;
        ss << "0x" << std::hex << pc;  // 필요한 경우에만 0x 접두사를 붙이고, 불필요한 0을 제거
        return ss.str();
    };

    std::cout << "Current pipeline PC state: {"
              << format_stage(PC_IF.pc) << "|"
              << format_stage(IF_ID.pc) << "|"
              << format_stage(ID_EX.pc) << "|"
              << format_stage(EX_MEM.pc) << "|"
              << format_stage(MEM_WB.pc) << "}\n\n";
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

#include <iostream>
#include <iomanip>
#include <map>

void printMemory(const std::map<unsigned int, unsigned int>& memory, unsigned int start, unsigned int end) {
    std::cout << "\nMemory content [0x" << std::hex << start << "..0x" << end << "]:\n";
    std::cout << "-------------------------------------------\n";
    
    if (start % 4 == 0) {  // mem_start가 4의 배수인 경우
         for (unsigned int addr = start; addr <= end; addr += 4) {
            auto it = memory.find(addr);
            if (it == memory.end()){
	        std::cout << "0x" << std::hex << addr << ": 0x0\n";
	    } else if (end%4==0){ //mem_end가 4의 배수
	   	std::cout << "0x" << std::hex << addr << ": 0x" << it->second << "\n";
            } else { //mem_end가 4의 배수가 아님
	    	if (addr<end-4){
	        	std::cout << "0x" << std::hex << addr << ": 0x" << it->second << "\n";		}
	    	else{
	            if (end%4==1){
		    	std::cout<<"0x"<<std::hex << addr<<": 0x"<< ((it->second>>16)&0xFFFF)<<"\n";
		    } else if (end%4==2){
	            	std::cout<<"0x"<<std::hex << addr<<": 0x"<<((it->second>>8)&0xFFFFFF)<<"\n";
	            } else if (end%4==3){
	            	std::cout<<"0x"<<std::hex << addr<<": 0x"<< it->second<<"\n";
		    }
	        }
	    }
	    }
    } else { //mem_start가 4의 배수가 아닌경우
    	for (unsigned int addr = start; addr <= end; addr += 4) {
            auto it = memory.find(addr-addr%4);
            if (it == memory.end()){
                std::cout << "0x" << std::hex << addr << ": 0x0\n";
            } else if (addr<end-4){ //end 전까지
                if(start%4==1){
		    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFFFFFF)<<8) << "\n";
		} else if (start%4==2){
		    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFFFF)<<16) << "\n";
		} else if (start%4==3){
                    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFF)<<24) << "\n";
                }
	    } else { //end 관리1
	     if (start%4>=end%4){
	     	if(start%4==1){
                    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFFFFFF)<<8) << "\n";
                } else if (start%4==2){
                    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFFFF)<<16) << "\n";
                } else if (start%4==3){
                    std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFF)<<24) << "\n";
                }
	     } else { //end 관리
		if(start%4==1){
			if (end%4==2){
			std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFFFF00)>>8) << "\n";
			} else {std::cout << "0x" << std::hex << addr << ": 0x" << (it->second&0xFFFFFF) << "\n";
			}
		} else if (start%4==2){
			if (end%4==2){
			std::cout << "0x" << std::hex << addr << ": 0x" << ((it->second&0xFF00)>>8) << "\n";
			} else {
			std::cout << "0x" << std::hex << addr << ": 0x" << (it->second&0xFFFF) << "\n";
			}
	    } else if (start%4==3){
			std::cout << "0x" << std::hex << addr << ": 0x" << (it->second&0xFF) << "\n";
		}
	     }
	    }
	}
	}
	}	



void IF(stage pc_if) {
    IF_ID.pc=pc_if.pc;
    IF_ID.instruction=memory_text[pc_if.pc];
    IF_ID.opcode = (IF_ID.instruction >> 26) & 0x3F;
    IF_ID.rs = (IF_ID.instruction >> 21) & 0x1F;
    IF_ID.rt = (IF_ID.instruction >> 16) & 0x1F;
    IF_ID.rd = (IF_ID.instruction >> 11) & 0x1F;
    IF_ID.shamt = (IF_ID.instruction >> 6) & 0x1F;
    IF_ID.func = IF_ID.instruction & 0x3F;
    IF_ID.imm = IF_ID.instruction & 0xFFFF;       // Immediate value
    IF_ID.target = IF_ID.instruction & 0x03FFFFFF; // Target address
}

void ID (stage if_id) {
    ID_EX=if_id;
    ID_EX.rs_value=registers[if_id.rs];
    ID_EX.rt_value=registers[if_id.rt];
    
    if (if_id.opcode == 0x0&&if_id.func==0x8) { //jr
	ID_EX.BR_target=registers[31];
    } else if (if_id.opcode==0x2){ //j
        ID_EX.BR_target=(if_id.pc&0xF0000000)|(if_id.target<<2);
    } else if (if_id.opcode==0x3){ //jal
        ID_EX.BR_target=(if_id.pc&0xF0000000)|(if_id.target<<2);
    } else if (if_id.opcode == 0x4){ //beq
        ID_EX.BR_target=if_id.pc +4 + (static_cast<short>(if_id.imm) << 2); // Calculate branch address
    } else if (if_id.opcode== 0x5){ //bne
        ID_EX.BR_target=if_id.pc +4 + (static_cast<short>(if_id.imm) << 2); // Calculate branch address
    }


}

void EX(stage id_ex){
	EX_MEM=id_ex;
	if (id_ex.opcode == 0) { // R-type
	switch (id_ex.func) {
            case 0x24: // AND
                EX_MEM.ALU_out = id_ex.rs_value & id_ex.rt_value;
                break;
            case 0x21: //addu
                EX_MEM.ALU_out = id_ex.rs_value + id_ex.rt_value;
                break;
            case 0x8: //jr
                EX_MEM.BR_target=id_ex.rs_value;
                break;
            case 0x27: //nor
                EX_MEM.ALU_out=~(id_ex.rs_value|id_ex.rt_value);
                break;
            case 0x25: //or
                EX_MEM.ALU_out=id_ex.rs_value|id_ex.rt_value;
                break;
            case 0x2b: //sltu
                EX_MEM.ALU_out=(id_ex.rs_value<id_ex.rt_value) ? 1: 0;
                break;
            case 0x0: //sll
                EX_MEM.ALU_out=id_ex.rt_value<<id_ex.shamt;
                break;
            case 0x2: //srl
                EX_MEM.ALU_out=id_ex.rt_value>>id_ex.shamt;
                break;
            case 0x23: //subu
                EX_MEM.ALU_out=id_ex.rs_value-id_ex.rt_value;
                break;
        }
    } else if (id_ex.opcode== 0x9) { //addiu
        EX_MEM.ALU_out=id_ex.rs_value+static_cast<short>(id_ex.imm);
    } else if (id_ex.opcode== 0xc){ //andi
        EX_MEM.ALU_out=id_ex.rs_value & (id_ex.imm & 0xffff); //zero-extention
    } else if (id_ex.opcode == 0x4){ //beq
        EX_MEM.taken=(id_ex.rs_value==id_ex.rt_value) ? 1: 0;
	EX_MEM.BR_target=id_ex.pc +4 + (static_cast<short>(id_ex.imm) << 2); // Calculate branch address
    } else if (id_ex.opcode== 0x5){ //bne
        EX_MEM.taken=(id_ex.rs_value!=id_ex.rt_value) ? 1: 0;
        EX_MEM.BR_target=id_ex.pc +4 + (static_cast<short>(id_ex.imm) << 2); // Calculate branch address

    } else if (id_ex.opcode==0x2){ //j
        EX_MEM.BR_target=(id_ex.pc&0xF0000000)|(id_ex.target<<2);
    } else if (id_ex.opcode==0x3){ //jal
        EX_MEM.BR_target=(id_ex.pc&0xF0000000)|(id_ex.target<<2);
    } else if (id_ex.opcode==0xf) { //lui
        EX_MEM.ALU_out=id_ex.imm<<16;
    } else if (id_ex.opcode==0x23){//lw
        int32_t offset = static_cast<int16_t>(id_ex.imm); // Sign-extend
        EX_MEM.ALU_out = id_ex.rs_value + offset;
    } else if (id_ex.opcode==0x20) {//lb
        int32_t offset = static_cast<int16_t>(id_ex.imm);  // Sign-extend the immediate value
        EX_MEM.ALU_out = id_ex.rs_value + offset;  // Calculate the address
    } else if (id_ex.opcode==0x2b){ //sw
        int32_t offset = static_cast<int16_t>(id_ex.imm);  // Sign-extend the immediate value
        EX_MEM.ALU_out = id_ex.rs_value + offset;  // Calculate the address
    } else if (id_ex.opcode==0x28){ //sb
        int32_t offset = static_cast<int16_t>(id_ex.imm);  // Sign-extend the immediate value
        EX_MEM.ALU_out = id_ex.rs_value + offset;  // Calculate the address
    } else if (id_ex.opcode==0xd){//ori
        EX_MEM.ALU_out=id_ex.rs_value | (id_ex.imm & 0xffff); //zero-extention
    } else if (id_ex.opcode==0xb){//sltiu
        EX_MEM.ALU_out = (id_ex.rs_value < static_cast<unsigned int>(static_cast<int16_t>(id_ex.imm))) ? 1 : 0;
    }
}

void MEM (stage ex_mem) {
    MEM_WB=ex_mem;
    if (ex_mem.opcode==0x23){//lw
        MEM_WB.MEM_out = read_memory_data(ex_mem.ALU_out);
    } else if (ex_mem.opcode==0x20) {//lb
        MEM_WB.MEM_out = read_memory_data(ex_mem.ALU_out) & 0xFF;   // Read a byte from memory and apply correct mask
    } else if (ex_mem.opcode==0x2b){ //sw
    	write_memory_data(ex_mem.ALU_out, ex_mem.rt_value);
    } else if (ex_mem.opcode==0x28){ //sb
        write_memory_data(ex_mem.ALU_out, ex_mem.rt_value & 0xFF);
    }
}
void WB (stage mem_wb) {
    if (mem_wb.opcode == 0) { // R-type
        if (mem_wb.func!=0x8) {
        registers[mem_wb.rd]=mem_wb.ALU_out;
	}
    } else if (mem_wb.opcode == 0x09 || mem_wb.opcode == 0x0C || mem_wb.opcode == 0x0F || mem_wb.opcode == 0x0B||mem_wb.opcode==0xd) { 
	    // addiu, andi, lui, sltiu,ori
        registers[mem_wb.rt] = mem_wb.ALU_out;
    } else if (mem_wb.opcode == 0x03) { // jal
        registers[31] = mem_wb.pc+4;
    } else if (mem_wb.opcode == 0x23 || mem_wb.opcode == 0x20) { // lw, lb
        registers[mem_wb.rt] = mem_wb.MEM_out;
    }
}


int main(int argc, char *argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << "<-atp or -antp> [-m addr1:addr2] [-d] [-p] [-n num_instruction] <input file>\n";
        return 1;
    }

    std::string filename = argv[argc - 1];
    loadMemory(filename);

    bool debug_mode = false;
    bool mem_range_specified = false;
    bool always_taken = false;
    bool print_pipeline = false;
    
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
        } else if (strcmp(argv[i], "-atp") == 0) {
            always_taken = true;
        } else if (strcmp(argv[i], "-antp") == 0) {
            always_taken = false;
        } else if (strcmp(argv[i], "-p") == 0) {
            print_pipeline = true;
        }
    }

    int executed_instructions = 0;
    int total_cycles=0;

 
    while ((MEM_WB.instruction!=0x0||EX_MEM.instruction!=0x0||ID_EX.instruction!=0x0||IF_ID.instruction!=0x0||memory_text[PC_IF.pc]!=0x0||PC==0x00400000)) {
      	//EX_MEM to EX
	if (EX_MEM.opcode==0&&EX_MEM.func!=8){ //R-type
		if (EX_MEM.rd==ID_EX.rs){
			ID_EX.rs_value=EX_MEM.ALU_out;
		} if(EX_MEM.rd==ID_EX.rt){
			ID_EX.rt_value=EX_MEM.ALU_out;
		}
	} else if (EX_MEM.opcode==0x9||EX_MEM.opcode==0xc||EX_MEM.opcode==0xf||EX_MEM.opcode==0xb||EX_MEM.opcode==0xd){//addiu,andi,lui,sltiu,ori
		if (EX_MEM.rt==ID_EX.rs){
			ID_EX.rs_value=EX_MEM.ALU_out;
		} if(EX_MEM.rt==ID_EX.rt){
			ID_EX.rt_value=EX_MEM.ALU_out;
		}
	} //MEM_WB to EX
        if (MEM_WB.opcode==0&&MEM_WB.func!=8){ //R-type
                if (MEM_WB.rd==ID_EX.rs){
			if (EX_MEM.opcode==0&&EX_MEM.func!=8&&EX_MEM.rd==ID_EX.rs){
			} else if ((EX_MEM.opcode==0x9||EX_MEM.opcode==0xc||EX_MEM.opcode==0xf||EX_MEM.opcode==0xb||EX_MEM.opcode==0xd)&&(EX_MEM.rt==ID_EX.rs)) {
			} else {		
                         ID_EX.rs_value=MEM_WB.ALU_out;
			}
		} if(MEM_WB.rd==ID_EX.rt){
			if (EX_MEM.opcode==0&&EX_MEM.func!=8&&EX_MEM.rd==ID_EX.rt){     
                        } else if ((EX_MEM.opcode==0x9||EX_MEM.opcode==0xc||EX_MEM.opcode==0xf||EX_MEM.opcode==0xb||EX_MEM.opcode==0xd)&&(EX_MEM.rt==ID_EX.rt)) {
                        } else {
                        ID_EX.rt_value=MEM_WB.ALU_out;
			}
                }
        } else if (MEM_WB.opcode==0x9||MEM_WB.opcode==0xc||MEM_WB.opcode==0xf||MEM_WB.opcode==0xb||MEM_WB.opcode==0xd){//addiu,andi,lui,sltiu,ori
                if (MEM_WB.rt==ID_EX.rs){
                        if (EX_MEM.opcode==0&&EX_MEM.func!=8&&EX_MEM.rd==ID_EX.rs){     
                        } else if ((EX_MEM.opcode==0x9||EX_MEM.opcode==0xc||EX_MEM.opcode==0xf||EX_MEM.opcode==0xb||EX_MEM.opcode==0xd)&&(EX_MEM.rt==ID_EX.rs)) {
                        } else {
			ID_EX.rs_value=MEM_WB.ALU_out;
			}
                } if(MEM_WB.rt==ID_EX.rt){
                        if (EX_MEM.opcode==0&&EX_MEM.func!=8&&EX_MEM.rd==ID_EX.rt){
                        } else if ((EX_MEM.opcode==0x9||EX_MEM.opcode==0xc||EX_MEM.opcode==0xf||EX_MEM.opcode==0xb||EX_MEM.opcode==0xd)&&(EX_MEM.rt==ID_EX.rt)) {
                        } else {
			ID_EX.rt_value=MEM_WB.ALU_out;
			}
		}
        }
	//load-use data hazard (MEM_WB to MEM)
	if ((ID_EX.opcode==0x23||ID_EX.opcode==0x20)&&(ID_EX.rt==IF_ID.rs)) {
		if (IF_ID.opcode==0x2||IF_ID.opcode==0x3)  {//j, jal skip
		} else {
		PC=PC_IF.pc;
		PC_IF=IF_ID;
		IF_ID=stall;
		}
	} if ((ID_EX.opcode==0x23||ID_EX.opcode==0x20)&&(ID_EX.rt==IF_ID.rt)) {
                if (IF_ID.opcode==0x2||IF_ID.opcode==0x3)  {//j, jal skip
                } else if (IF_ID.opcode==0x2b||IF_ID.opcode==0x28) { //sw, sb skip
		}else {
                PC=PC_IF.pc;
                PC_IF=IF_ID;
                IF_ID=stall;
                }

	} if ((MEM_WB.opcode==0x23||MEM_WB.opcode==0x20)&&(MEM_WB.rt==ID_EX.rs)){
                ID_EX.rs_value=MEM_WB.MEM_out;
	}if ((MEM_WB.opcode==0x23||MEM_WB.opcode==0x20)&&(MEM_WB.rt==ID_EX.rt)){
                ID_EX.rt_value=MEM_WB.MEM_out;
        } if ((MEM_WB.opcode==0x23||MEM_WB.opcode==0x20)&&(MEM_WB.rt==EX_MEM.rt)){
                if (EX_MEM.opcode==0x2b||EX_MEM.opcode==0x28)  { // save-load
                        EX_MEM.rt_value=MEM_WB.MEM_out;
		}	
	}

	
	WB(MEM_WB);
	MEM(EX_MEM);
	EX(ID_EX);
	ID(IF_ID);
	IF(PC_IF);
	
	if ((ID_EX.opcode == 0 && ID_EX.func == 0x8) || ID_EX.opcode == 0x2 || ID_EX.opcode == 0x3) { // jr, j, jal
            PC = ID_EX.BR_target;
	    IF(stall);
	}else if ((ID_EX.opcode == 0x4 || ID_EX.opcode == 0x5) && always_taken) { // beq, bne
            PC = ID_EX.BR_target;
            IF(stall); // flush the instruction
	}

	if ((MEM_WB.opcode==0x4||MEM_WB.opcode==0x5)&&(always_taken!=MEM_WB.taken)){ //wrong prediction
            if (MEM_WB.taken){
                PC=MEM_WB.BR_target;
                EX(stall);
                ID(stall);
                IF(stall);
            } else {
                PC=MEM_WB.pc+4;
                EX(stall);
                ID(stall);
                IF(stall);
            }
	}	
	if ((executed_instructions < num_instructions|| num_instructions==-1)&&(memory_text[PC]!=0x0)){
   	    PC_IF.pc=PC;
	    PC+=4;
        } else if (num_instructions==0){
	    PC_IF.pc=0x0;
	    total_cycles++;
	    break;    
	} else {
	    PC_IF.pc=0x0;
	}
	total_cycles++;
	executed_instructions++;
//	std::cout<<"MEM_WB.instruction:"<<std::hex <<MEM_WB.instruction << std::dec <<std::endl; // debug
	if (print_pipeline&&(MEM_WB.instruction!=0x0||EX_MEM.instruction!=0x0||ID_EX.instruction!=0x0||IF_ID.instruction!=0x0||memory_text[PC_IF.pc]!=0x0)) {
            std::cout << "==== Cycle " << total_cycles << " ====\n";
            printPipelineState();
        }
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

    std::cout << "==== Completion cycle : " << total_cycles-1 << " ====\n\n";
    printPipelineState();
    printRegisters();
    if (mem_range_specified) {
        if (mem_start < DATA_START) {
            printMemory(memory_text, mem_start, mem_end);
        } else {
            printMemory(memory_data, mem_start, mem_end);
        }
    }
    return 0;
}
