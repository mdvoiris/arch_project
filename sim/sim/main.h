//main header

#ifndef __MAIN_H__
#define __MAIN_H__

//Includes:
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h>
#include <string.h>


//Defines:
#define NUM_OF_CORES 4
#define NUM_OF_REGS 16
#define PIPE_LEN 5
#define MEM_SIZE 1<<20
#define CACHE_SIZE 256
#define IMEM_SIZE 1024
#define FLUSH_TIME 64
#define NOT_FOUND -1


//Typedefs:
//Enum to organize the input argv
typedef enum _arg {
	IMEM0 = 1,
	IMEM1,
	IMEM2,
	IMEM3,
	MEMIN,
	MEMOUT,
	REGOUT0,
	REGOUT1,
	REGOUT2,
	REGOUT3,
	TRACE0,
	TRACE1,
	TRACE2,
	TRACE3,
	BUSTRACE,
	DSRAM0,
	DSRAM1,
	DSRAM2,
	DSRAM3,
	TSRAM0,
	TSRAM1,
	TSRAM2,
	TSRAM3,
	STATS0,
	STATS1,
	STATS2,
	STATS3,
	ARG_COUNT
} Arg;

//All of the main functions return a status enum for debugging
typedef enum _status {
	INVALID_STATUS_CODE = -1,
	SUCCESS = 0,
	WRONG_ARGUMENT_COUNT,
	FOPEN_FAIL,
	WRONG_OPCODE,
	WRONG_RD,
	WRONG_RS,
	WRONG_RT,
	WRONG_ADDRESS
} Status;

typedef enum _core {
	CORE0,
	CORE1,
	CORE2,
	CORE3
} Core;

typedef enum _pipe {
	FETCH,
	DECODE,
	EXECUTE,
	MEM,
	WRITE_BACK
} Pipe;

typedef enum _opcode {
	ADD,
	SUB,
	AND,
	OR,
	XOR,
	MUL,
	SLL,
	SRA,
	SRL,
	BEQ,
	BNE,
	BLT,
	BGT,
	BLE,
	BGE,
	JAL,
	LW,
	SW,
	LL,
	SC,
	HALT
} Opcode;

typedef enum _bus_origid {
	//CORE0,
	//CORE1,
	//CORE2,
	//CORE3,
	MAIN_MEMORY=4
} Bus_origid;

typedef enum _bus_cmd {
	NO_COMMAND,
	BusRd,
	BusRdX,
	Flush,
} Bus_cmd;

typedef enum _mem_stage {
	CACHE_ACCESS,
	WAIT_FOR_BUS,
	BUS_ACCESS
} Mem_stage;

typedef struct _tsram {
	int MSI;
	int tag;
}TSRAM;

typedef enum _msi {
	INVALID,
	SHARE,
	MODIFIED
}MSI;


typedef struct _bus {
	Bus_origid origid;
	Bus_cmd cmd;
	int addr;
	int data;
}BUS;

typedef struct _instruction {
	int opcode;
	int rd;
	int rs;
	int rt;
	int imm;
	int pc;
}Instruction;

typedef struct _load_store_conditional {
	int address;
	bool sc_dirty;
}LOAD_STORE_CONDITIONAL;




//Global variables:
//Input arguments
char args[ARG_COUNT][_MAX_PATH] = { "sim.exe","imem0.txt","imem1.txt","imem2.txt","imem3.txt",
		"memin.txt","memout.txt","regout0.txt","regout1.txt","regout2.txt","regout3.txt",
		"core0trace.txt","core1trace.txt","core2trace.txt","core3trace.txt","bustrace.txt",
		"dsram0.txt","dsram1.txt","dsram2.txt","dsram3.txt","tsram0.txt","tsram1.txt","tsram2.txt","tsram3.txt",
		"stats0.txt","stats1.txt","stats2.txt","stats3.txt" };

//Clock cycles
int cycle = 0;

//Main memory
int main_memory[MEM_SIZE] = { 0 };

//Bus
BUS cur_bus = { 0 };
BUS updated_bus = { 0 };
int flush_cycles = 0;
bool print_bus_trace = false;

//Core status
int pc[NUM_OF_CORES] = { 0 };
int cycles_count[NUM_OF_CORES] = { 0 };
bool core_done[NUM_OF_CORES] = { false };
int decode_stall[NUM_OF_CORES] = { 0 };
int mem_stall[NUM_OF_CORES] = { 0 };
int decode_stall_count[NUM_OF_CORES] = { 0 };
int mem_stall_count[NUM_OF_CORES] = { 0 };
Mem_stage mem_stage[NUM_OF_CORES] = { CACHE_ACCESS };
bool branch[NUM_OF_CORES] = { false };

//Core regs
int cur_regs[NUM_OF_CORES][NUM_OF_REGS] = { 0 };
int updated_regs[NUM_OF_CORES][NUM_OF_REGS] = { 0 };

//Core instructions
int imem[NUM_OF_CORES][IMEM_SIZE] = { 0 };
Instruction pipeline[NUM_OF_CORES][PIPE_LEN] = { 0 };
int instructions_count[NUM_OF_CORES] = { 0 };
LOAD_STORE_CONDITIONAL watch[NUM_OF_CORES] = { 0 };

//Core cache
int dsram[NUM_OF_CORES][CACHE_SIZE] = { 0 };
TSRAM tsram[NUM_OF_CORES][CACHE_SIZE] = { INVALID };
int read_hit_count[NUM_OF_CORES] = { 0 };
int write_hit_count[NUM_OF_CORES] = { 0 };
int read_miss_count[NUM_OF_CORES] = { 0 };
int write_miss_count[NUM_OF_CORES] = { 0 };
int abort_cache=-1;


//Function Handles:
void update_args(char* argv[]);

Status init_imems();

Status init_main_memory();

void init_pipeline();

Status core(Core core_num, FILE* trace_file);

Status fetch(Core core_num);

Status decode(Core core_num);

Status execute(Core core_num);

Status mem(Core core_num);

Status write_back(Core core_num);

Status advance_pipeline(Core core_num);

int detect_hazards(Core core_num, Pipe stage);

bool branch_resolution(Core core_num, Instruction inst);

void get_reg_values(Core core_num, Instruction inst, int* rd_value, int* rs_value, int* rt_value);

void advance_stage(Core core_num, Pipe from, Pipe to);

void print_trace(Core core_num, FILE* file);

Status print_file(Arg file_enum);

int search_address_cache(BUS* bus);

bool free_access_bus(int core_num);

void bus_response();

void print_bus(FILE* bus_trace);

//void arrange_bus_load();

//void arrange_bus_store();

bool sc_func(int address, int core_num, int rd_value);

void advance_bus();

#endif // __MAIN_H__
