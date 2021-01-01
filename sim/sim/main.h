//main header

#ifndef __MAIN_H__
#define __MAIN_H__

//Includes:
#include <stdio.h> 
#include <stdlib.h> 
#include <stdbool.h>
#include <string.h>


//Defines:
#define REQUIERED_ARGS_COUNT 28
#define NUM_OF_CORES 4
#define NUM_OF_REGS 16
#define PIPE_LEN 5
#define MEM_SIZE 1<<20
#define CACHE_SIZE 256
#define IMEM_SIZE 1024


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
    ALLOC_FAILED,
} Status;

typedef enum _core {
    CORE0,
    CORE1,
    CORE2,
    CORE3
} Core;

typedef enum _bus_origid {
    CORE0 = CORE0,
    CORE1 = CORE1,
    CORE2 = CORE2,
    CORE3 = CORE3,
    MAIN_MEMORY
} Bus_origid;

typedef enum _bus_cmd {
    NO_COMMAND,
    BusRd,
    BusRdX,
    Flush,
} Bus_cmd;

typedef struct _tsram {
    int MSI;
    int tag;
}TSRAM;

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


//Global variables:
int cycle = 0;
int pc[NUM_OF_CORES] = { 0 };
int dsram[NUM_OF_CORES][CACHE_SIZE] = { 0 };
TSRAM tsram[NUM_OF_CORES][CACHE_SIZE] = { 0 };
int main_memory[MEM_SIZE] = { 0 };
int regs[NUM_OF_REGS] = { 0 };
BUS bus = { 0 };
Instruction Pipe[NUM_OF_CORES][PIPE_LEN] = { 0 };
int imem[NUM_OF_CORES][IMEM_SIZE] = { 0 };




#endif // __MAIN_H__
