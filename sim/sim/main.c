
#include "main.h"


Status main(int argc, char* argv[]) {
	Status status = INVALID_STATUS_CODE;
    FILE* trace_files[NUM_OF_CORES] = { NULL };
    FILE* bus_trace = NULL;


    //Check for correct input argument count (or no arguments)
    if (argc != ARG_COUNT) {
        if (argc != 1) {
            printf("Error - got %d arguments, expecting %d", argc, ARG_COUNT);
            return WRONG_ARGUMENT_COUNT;
        }
    }
    //If correct argument count, update file names
    else update_args(argv);
    
    //Read imem files and stores them in global imem array
    status = init_imems();
    if (status) goto EXIT;

    //Reads memin file and store it in global main_memory array
    status = init_main_memory();
    if (status) goto EXIT;

    //Initiate global pipeline array (and watch for sc/ll) with invalid (-1) values
    init_pipeline();

    //Open bustrace and coreXtrace for writing
    fopen_s(&bus_trace, args[BUSTRACE], "w");
    if (bus_trace == NULL) {
        status = FOPEN_FAIL;
        goto EXIT;
    }
    for (int i = 0; i < NUM_OF_CORES; i++) {
        fopen_s(&trace_files[i], args[TRACE0 + i], "w");
        if (trace_files[i] == NULL) {
            status = FOPEN_FAIL;
            goto EXIT;
        }
    }
    
    //Main loop 
    //foreach clock cycle until all cores are done
    while ((core_done[CORE0] & core_done[CORE1] & core_done[CORE2] & core_done[CORE3]) == false) {
        //Do CORE0 operations
        if (core_done[CORE0] == false) {
            status = core(CORE0, trace_files[CORE0]);
            if (status) goto EXIT;
        }
        //Do CORE1 operations
        if (core_done[CORE1] == false) {
            status = core(CORE1, trace_files[CORE1]);
            if (status) goto EXIT;
        }
        //Do CORE2 operations
        if (core_done[CORE2] == false) {
            status = core(CORE2, trace_files[CORE2]);
            if (status) goto EXIT;
        }
        //Do CORE3 operations
        if (core_done[CORE3] == false) {
            status = core(CORE3, trace_files[CORE3]);
            if (status) goto EXIT;
        }

        //Simulating bus as a flipflop advencing D->Q
        advance_bus();

        //On every cycle we need to update bus trace
        if (print_bus_trace == true)
        {
            //print current bus to bustrace
            print_bus(bus_trace);
            //acts as a bus snoop to identify block in caches and cleans bus after flush
            bus_response();
            //Simulating bus as a flipflop advencing D->Q
            advance_bus();
            print_bus_trace = false;
        }

        cycle++;
    }

    //Close bustrace and coreXtrace
    fclose(bus_trace);
    bus_trace = NULL;

    for (int i = 0; i < NUM_OF_CORES; i++) {
        fclose(trace_files[i]);
        trace_files[i] = NULL;
    }

    //print memout
    status = print_file(MEMOUT);
    if (status) goto EXIT;

    for (int i = 0; i < NUM_OF_CORES; i++) {
        //print regoutX
        status = print_file(REGOUT0 + i);
        if (status) goto EXIT;

        //print dsramX
        status = print_file(DSRAM0 + i);
        if (status) goto EXIT;

        //print tsramX
        status = print_file(TSRAM0 + i);
        if (status) goto EXIT;

        //print statsX
        status = print_file(STATS0 + i);
        if (status) goto EXIT;
    }

	return SUCCESS;

    //Emergency EXIT
EXIT:
    for (int i = 0; i < NUM_OF_CORES; i++)
        if (trace_files[i] != NULL) {
            fclose(trace_files[i]);
        }
    if (bus_trace != NULL) 
        fclose(bus_trace);
    return status;

}

void update_args(char* argv[]) {
    for (int i = 1; i < ARG_COUNT; i++) {
        strcpy_s(args[i], _MAX_PATH, argv[i]);
    }
}

Status print_file(Arg file_enum) {
    Status status = INVALID_STATUS_CODE;
    char* file_name = args[file_enum];
    FILE* file = NULL;
    Core core;


    fopen_s(&file, file_name, "w");
    if (file == NULL)
        return FOPEN_FAIL;

    if (file_enum == MEMOUT) {
        for (int i = 0; i < MEM_SIZE; i++) {
            fprintf(file, "%08X\n", main_memory[i]);
        }
    }
    if (file_enum >= REGOUT0 && file_enum <= REGOUT3) {
        core = file_enum - REGOUT0;
        for (int i = 2; i < NUM_OF_REGS; i++) {
            fprintf(file, "%08X\n", cur_regs[core][i]);
        }
    }
    if (file_enum >= DSRAM0 && file_enum <= DSRAM3) {
        core = file_enum - DSRAM0;
        for (int i = 0; i < CACHE_SIZE; i++) {
            fprintf(file, "%08X\n", dsram[core][i]);
        }
    }
    if (file_enum >= TSRAM0 && file_enum <= TSRAM3) {
        core = file_enum - TSRAM0;
        for (int i = 0; i < CACHE_SIZE; i++) {
            fprintf(file, "%08X\n", ((tsram[core][i].MSI << 12) + tsram[core][i].tag));
        }
    }
    if (file_enum >= STATS0 && file_enum <= STATS3) {
        core = file_enum - STATS0;
        fprintf(file, "cycles %d\n", cycles_count[core]);
        fprintf(file, "instructions %d\n", instructions_count[core]);
        fprintf(file, "read_hit %d\n", read_hit_count[core]);
        fprintf(file, "write_hit %d\n", write_hit_count[core]);
        fprintf(file, "read_miss %d\n", read_miss_count[core]);
        fprintf(file, "write_miss %d\n", write_miss_count[core]);
        fprintf(file, "decode_stall %d\n", decode_stall_count[core]);
        fprintf(file, "mem_stall %d\n", mem_stall_count[core]);
    }

    fclose(file);

    return SUCCESS;
}

Status init_imems() {
    Status status = INVALID_STATUS_CODE;
    FILE* imem_files[NUM_OF_CORES] = { NULL };


    for (int core = 0; core < NUM_OF_CORES; core++) {
        fopen_s(&imem_files[core], args[IMEM0 + core], "r");
        if (imem_files[core] == NULL)
            return FOPEN_FAIL;

        for (int line = 0; line < IMEM_SIZE; line++) {
            fscanf_s(imem_files[core], "%08X", &imem[core][line]);
            if (feof(imem_files[core])) break;
        }

        fclose(imem_files[core]);
    }
    
    return SUCCESS;
}

Status init_main_memory() {
    Status status = INVALID_STATUS_CODE;
    FILE* memin_file = NULL;

    fopen_s(&memin_file, args[MEMIN], "r");
    if (memin_file == NULL)
        return FOPEN_FAIL;

    for (int line = 0; line < MEM_SIZE; line++) {
        fscanf_s(memin_file, "%08X", &main_memory[line]);
        if (feof(memin_file)) break;
    }

    fclose(memin_file);

    return SUCCESS;
}

void init_pipeline() {
    for (int core = CORE0; core < NUM_OF_CORES; core++) {
        for (int i = 0; i < PIPE_LEN; i++) {
            pipeline[core][i].pc = -1;
            pipeline[core][i].opcode = -1;
            pipeline[core][i].rd = -1;
            pipeline[core][i].rs = -1;
            pipeline[core][i].rt = -1;
            pipeline[core][i].imm = -1;
        }
        //initialize watch for sc/ll
        watch[core].address = -1;
    }
}

Status core(Core core_num, FILE* trace_file) {
    Status status = INVALID_STATUS_CODE;
    
    //No more imem left, terminate core
    if (pc[core_num] == 1024) {
        core_done[core_num] = true;
        return SUCCESS;
    }

    //If there're no stalls
    if (decode_stall[core_num] == 0 && mem_stall[core_num] == 0) {
        //FETCH
        status = fetch(core_num);
        if (status) return status;
    }

    //If there're no mem stalls
    if (mem_stall[core_num] == 0) {
        //DECODE
        status = decode(core_num);
        if (status) return status;

        //EXECUTE
        status = execute(core_num);
        if (status) return status;
    }

    //MEM
    status = mem(core_num);
    if (status) return status;

    //print trace line, before WB (WB updates registers)
    print_trace(core_num, trace_file);

    //WRITE_BACK
    status = write_back(core_num);
    if (status) return status;

    //Advance pipeline stages for which there were no stalls
    status = advance_pipeline(core_num);
    if (status) return status;

    return SUCCESS;
}

Status fetch(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    //Halt called, termination proccess
    if (pc[core_num] == -1) {
        pipeline[core_num][FETCH].pc = -1;
        return SUCCESS;
    }

    //Update global pipeline array with the new instruction from imem
    pipeline[core_num][FETCH].pc     = pc[core_num];
    pipeline[core_num][FETCH].opcode = (imem[core_num][pc[core_num]] & 0xff000000) >> 24;
    pipeline[core_num][FETCH].rd     = (imem[core_num][pc[core_num]] & 0x00f00000) >> 20;
    pipeline[core_num][FETCH].rs     = (imem[core_num][pc[core_num]] & 0x000f0000) >> 16;
    pipeline[core_num][FETCH].rt     = (imem[core_num][pc[core_num]] & 0x0000f000) >> 12;
    pipeline[core_num][FETCH].imm    = imem[core_num][pc[core_num]] & 0x00000800 ? 
                                        (-1 * ((~imem[core_num][pc[core_num]] + 1) & 0x00000fff)) : (imem[core_num][pc[core_num]] & 0x00000fff);

    //Error handling
    if ((pipeline[core_num][FETCH].opcode < ADD) || (pipeline[core_num][FETCH].opcode > HALT))
        return WRONG_OPCODE;
    if (pipeline[core_num][FETCH].rd > (NUM_OF_REGS-1))
        return WRONG_RD;
    if (pipeline[core_num][FETCH].rs > (NUM_OF_REGS - 1))
        return WRONG_RS;
    if (pipeline[core_num][FETCH].rt > (NUM_OF_REGS - 1))
        return WRONG_RT;

    return SUCCESS;
}

Status decode(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    //Halt called or instructions didn't reach decode yet
    if ((pipeline[core_num][DECODE].opcode == HALT) || (pipeline[core_num][DECODE].pc == -1)) {
        return SUCCESS;
    }

    //Searches foe RAW hazards and updates decode stall
    decode_stall[core_num] = detect_hazards(core_num, DECODE);

    //No stalls -> do branch resolusion for branch opcodes
    if (decode_stall[core_num] == 0) {
        if ((pipeline[core_num][DECODE].opcode >= BEQ) && (pipeline[core_num][DECODE].opcode <= JAL))
            //update global branch array with true/false for taken/not-taken
            branch[core_num] = branch_resolution(core_num, pipeline[core_num][DECODE]);
    }
    //else count it
    else decode_stall_count[core_num]++;


    return SUCCESS;
}

int detect_hazards(Core core_num, Pipe stage) {
    int hazard_potential[3] = { 0 };

    //look for registers with hazard potential for the current instruction
    if (pipeline[core_num][stage].opcode == JAL) {
        hazard_potential[0] = pipeline[core_num][stage].rd;
    }
    else if ((pipeline[core_num][stage].opcode < BEQ) || (pipeline[core_num][stage].opcode == LW) || (pipeline[core_num][stage].opcode == LL)) {
        hazard_potential[0] = pipeline[core_num][stage].rs;
        hazard_potential[1] = pipeline[core_num][stage].rt;
    }
    else {
        hazard_potential[0] = pipeline[core_num][stage].rd;
        hazard_potential[1] = pipeline[core_num][stage].rs;
        hazard_potential[2] = pipeline[core_num][stage].rt;
    }

    //foreach hazard potential
    for (int i = 0; i < 3; i++) {
        //if reg is $imm or $zero, ignore
        if ((hazard_potential[i] == 0) || (hazard_potential[i] == 1))
            continue;

        //look in the next pipeline stages, return the neccessary decode stall
        for (int j = stage + 1; j < PIPE_LEN; j++) {
            //ignore '---' of any kind
            if (pipeline[core_num][j].pc == -1) continue;

            //for JAL hazard is only $r15
            if ((pipeline[core_num][j].opcode == JAL) && (hazard_potential[i] == 15))
                return (WRITE_BACK - j + 1);

            //for rd writing operations
            else if ((pipeline[core_num][j].opcode < BEQ) || (pipeline[core_num][j].opcode == LW) || (pipeline[core_num][j].opcode == LL) || (pipeline[core_num][j].opcode == SC)) {
                //if rd is the same as the hazard potential
                if (hazard_potential[i] == pipeline[core_num][j].rd)
                    return (WRITE_BACK - j + 1);
            }
        }
    }

    return 0;
}

bool branch_resolution(Core core_num, Instruction inst) {
    int rd_value;
    int rs_value;
    int rt_value;

    //get reg values for the instruction
    get_reg_values(core_num, inst, &rd_value, &rs_value, &rt_value);

    //for branch opcode, if branch taken, update pc and return branch taken
    switch (inst.opcode) {
    case BEQ: {
        if (rs_value == rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case BNE: {
        if (rs_value != rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case BLT: {
        if (rs_value < rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case BGT: {
        if (rs_value > rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case BLE: {
        if (rs_value <= rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case BGE: {
        if (rs_value >= rt_value) {
            pc[core_num] = rd_value & 0x3ff;
            return true;
        }
        break;
    }
    case JAL: {
        pc[core_num] = rd_value & 0x3ff;
        return true;
    }
    }

    //return branch not taken
    return false;
}

void get_reg_values(Core core_num, Instruction inst, int* rd_value, int* rs_value, int* rt_value) {
    //If reg is $imm reg value is the immediate
    //otherwise get the actual reg value

    if (inst.rd == 1) *rd_value = inst.imm;
    else *rd_value = cur_regs[core_num][inst.rd];

    if (inst.rs == 1) *rs_value = inst.imm;
    else *rs_value = cur_regs[core_num][inst.rs];

    if (inst.rt == 1) *rt_value = inst.imm;
    else *rt_value = cur_regs[core_num][inst.rt];

    return;
}

Status execute(Core core_num) {
    Status status = INVALID_STATUS_CODE;
    int rd_value;
    int rs_value;
    int rt_value;

    //if halt called or instruction didn't reach execute yet
    if ((pipeline[core_num][EXECUTE].opcode == HALT) || (pipeline[core_num][EXECUTE].pc == -1)) {
        return SUCCESS;
    }

    //get reg values for the instruction
    get_reg_values(core_num, pipeline[core_num][EXECUTE], &rd_value, &rs_value, &rt_value);


    //calculate the result value the instruction and update updated_regs
    switch (pipeline[core_num][EXECUTE].opcode) {
    case ADD: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value + rt_value;
        break;
    }
    case SUB: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value - rt_value;
        break;
    }
    case AND: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value & rt_value;
        break;
    }
    case OR: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value | rt_value;
        break;
    }
    case XOR: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value ^ rt_value;
        break;
    }
    case MUL: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value * rt_value;
        break;
    }
    case SLL: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value << rt_value;
        break;
    }
    case SRA: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = rs_value >> rt_value;
        break;
    }
    case SRL: {
        updated_regs[core_num][pipeline[core_num][EXECUTE].rd] = (unsigned int)rs_value >> rt_value;
        break;
    }
    case JAL: {
        updated_regs[core_num][15] = pipeline[core_num][EXECUTE].pc + 1;
        break;
    }
    }
    return SUCCESS;
}

Status mem(Core core_num)
{
    Status status = INVALID_STATUS_CODE;
    int rd_value;
    int rs_value;
    int rt_value;
    int address;

    //if halt called or instruction didn't reach mem yet
    if ((pipeline[core_num][MEM].opcode == HALT) || (pipeline[core_num][MEM].pc == -1)) {
        return SUCCESS;
    }

    //decrement mem stall if exists
    if (mem_stall[core_num] > 0) {
        mem_stall[core_num]--;
    }
    
    //continue if not a mem operation
    if ((pipeline[core_num][MEM].opcode != SC) && (pipeline[core_num][MEM].opcode != SW) && (pipeline[core_num][MEM].opcode != LL) && (pipeline[core_num][MEM].opcode != LW))//check if the opcode isn't load or store opcode
        return SUCCESS;

    //get reg values for the instruction
    get_reg_values(core_num, pipeline[core_num][MEM], &rd_value, &rs_value, &rt_value);

    //calculate address
    address = rs_value + rt_value;

    //error handling
    if (address < 0 || address >= MEM_SIZE)
        return WRONG_ADDRESS;


    //Start of State machine
    if ((pipeline[core_num][MEM].opcode == LL) || (pipeline[core_num][MEM].opcode == LW))//PrRd opcode
    {
        if (mem_stage[core_num] == CACHE_ACCESS)//  search in cache
        {
            if ((pipeline[core_num][MEM].opcode == LL))//opcode is load linked
            {
                watch[core_num].address = address;
                watch[core_num].sc_dirty = 0;
            }
            if (tsram[core_num][(address) & 0xFF].tag == (address) / 256 && tsram[core_num][(address) & 0xFF].MSI != INVALID) // read hit
            {
                read_hit_count[core_num] += 1;
                updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][(address) & 0xFF];
                return SUCCESS;
            }
            if (free_access_bus(core_num))//  it is read miss so check if the bus is accessible 
            {
                mem_stage[core_num] = BUS_ACCESS;
                read_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
            else// the bus not accessible and it is read miss the core needs to wait
            {
                read_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stage[core_num] = WAIT_FOR_BUS;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
        }
        if (mem_stage[core_num] == WAIT_FOR_BUS && free_access_bus(core_num))//checks if the bus is accessible this turn after read miss
        {
            mem_stage[core_num] = BUS_ACCESS;
        }
        else if (mem_stage[core_num] == WAIT_FOR_BUS && !free_access_bus(core_num))// the bus not accessible  the core needs to wait
        {
            mem_stall[core_num] = 1;
            mem_stall_count[core_num]++;
            return SUCCESS;
        }
        if (mem_stage[core_num] == BUS_ACCESS)//  search in bus
        {
            if (cur_bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI != MODIFIED)// the block in the cache of the core that owns the bus is INVALIDATE or SHARE
            {
                updated_bus.cmd = BusRd;
                updated_bus.data = 0;
                updated_bus.addr = address;
                updated_bus.origid = core_num;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }

            else if (cur_bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI == MODIFIED)// the block in the cache of the core that owns the bus is MODIFIED
            {
                updated_bus.cmd = Flush;
                updated_bus.addr = (tsram[core_num][address & 0xff].tag << 8) + (address & 0xff);
                updated_bus.data = 0;
                updated_bus.origid = core_num;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }

            if (cur_bus.cmd == BusRd)// check if the cmd is BusRd
            {
                if (search_address_cache(&cur_bus) != NOT_FOUND && tsram[search_address_cache(&cur_bus)][cur_bus.addr & 0xff].MSI == MODIFIED)// checks if the block available in another modified core 
                {              
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    print_bus_trace = true;
                    return SUCCESS;
                }
                else// block in memory
                {
                    mem_stall[core_num]++;
                    mem_stall_count[core_num]++;
                    flush_cycles = FLUSH_TIME;
                    print_bus_trace = true;
                    return SUCCESS;
                }
            }

            if (cur_bus.cmd == Flush && cur_bus.origid == MAIN_MEMORY)// flush from main memory to the core who owns the bus
            {
                flush_cycles -= 1;
                if (flush_cycles == 0)// check if next command on the bus is flush
                {
                    print_bus_trace = true;
                    updated_bus.data = main_memory[cur_bus.addr];
                    tsram[core_num][cur_bus.addr & 0xff].MSI = SHARE;
                    tsram[core_num][cur_bus.addr & 0xff].tag = cur_bus.addr / 256;
                    dsram[core_num][cur_bus.addr & 0xff] = updated_bus.data;
                    updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][cur_bus.addr & 0xff];
                    mem_stage[core_num] = CACHE_ACCESS;
                    return SUCCESS;
                }
                else
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
            else if (cur_bus.cmd == Flush && cur_bus.origid != MAIN_MEMORY && tsram[core_num][cur_bus.addr & 0xff].MSI != MODIFIED)
            {
               
                    updated_bus.data = dsram[cur_bus.origid][cur_bus.addr];
                    dsram[core_num][cur_bus.addr & 0xff] = updated_bus.data;
                    updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][cur_bus.addr & 0xff];
                    tsram[core_num][cur_bus.addr & 0xff].MSI = SHARE;
                    tsram[core_num][cur_bus.addr & 0xff].tag = cur_bus.addr / 256;
                    tsram[cur_bus.origid][cur_bus.addr].MSI = SHARE;
                    print_bus_trace = true;
                    main_memory[cur_bus.addr] = updated_bus.data;
                    mem_stage[core_num] = CACHE_ACCESS;
                    return SUCCESS;
            }
            else if (cur_bus.cmd == Flush && cur_bus.origid != MAIN_MEMORY && tsram[core_num][cur_bus.addr & 0xff].MSI == MODIFIED)
            {

                    updated_bus.data = dsram[core_num][cur_bus.addr & 0xff];
                    print_bus_trace = true;
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;   
                    main_memory[cur_bus.addr] = updated_bus.data;
                    tsram[core_num][cur_bus.addr & 0xff].MSI = SHARE;
                    return SUCCESS;
            }
        }
    }


    else if ((pipeline[core_num][MEM].opcode == SC) || (pipeline[core_num][MEM].opcode == SW))//PrWr opcode
    {
        if (mem_stage[core_num] == CACHE_ACCESS)//  search in cache
        {
            
            if (tsram[core_num][(address) & 0xFF].tag == (address) / 256 && tsram[core_num][(address) & 0xFF].MSI == MODIFIED) // write hit
            {
                if (pipeline[core_num][MEM].opcode == SW)
                {
                    dsram[core_num][(address) & 0xFF] = rd_value;
                    write_hit_count[core_num] += 1;
                    return SUCCESS;
                }
                else if (pipeline[core_num][MEM].opcode == SC )
                {                   
                    if(sc_func(address, core_num))
                    {
                        write_hit_count[core_num] += 1;
                        dsram[core_num][address] = rd_value;
                    }
                    return SUCCESS;
                }       
            }
            if (free_access_bus(core_num) && ((pipeline[core_num][MEM].opcode == SW) || (pipeline[core_num][MEM].opcode == SC && sc_func(address, core_num))))// write miss so check if the bus is accessible 
            {
                mem_stage[core_num] = BUS_ACCESS;
                write_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
            else if ((pipeline[core_num][MEM].opcode == SW) || (pipeline[core_num][MEM].opcode == SC && sc_func(address, core_num)))// the bus not accessible  the core needs to wait
            {
                write_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stage[core_num] = WAIT_FOR_BUS;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
            else
                return SUCCESS;

        }
        if (mem_stage[core_num] == WAIT_FOR_BUS && free_access_bus(core_num))//checks if the bus is accessible
        {
            mem_stage[core_num] = BUS_ACCESS;
        }
        else if (mem_stage[core_num] == WAIT_FOR_BUS && !free_access_bus(core_num))// the bus not accessible  the core needs to wait
        {
            mem_stall[core_num] = 1;
            mem_stall_count[core_num]++;
            return SUCCESS;
        }
        if (mem_stage[core_num] == BUS_ACCESS)//  search in bus
        {
            if (cur_bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI != MODIFIED)// the block in the cache of the core that owns the bus is INVALIDATE or SHARE
            {
                updated_bus.cmd = BusRdX;
                updated_bus.data = 0;
                updated_bus.addr = address;
                updated_bus.origid = core_num;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }

            else if (cur_bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI == MODIFIED)// the block in the cache of the core that owns the bus is MODIFIED
            {
                updated_bus.cmd = Flush;
                updated_bus.addr = (tsram[core_num][address & 0xff].tag << 8) + address & 0xff;
                updated_bus.data = 0;
                updated_bus.origid = core_num;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }

            if (cur_bus.cmd == BusRdX)// check if the cmd is BusRd
            {
                for (int i = 0; i < NUM_OF_CORES; i++)
                    if (tsram[i][cur_bus.addr & 0xff].MSI == SHARE && i != core_num)
                        tsram[i][cur_bus.addr & 0xff].MSI = INVALID;
                if (search_address_cache(&cur_bus) != NOT_FOUND && tsram[search_address_cache(&cur_bus)][cur_bus.addr & 0xff].MSI == MODIFIED)// checks if the data available in another core 
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    print_bus_trace = true;
                    return SUCCESS;
                }
                else// the wanted block is in the main memory
                {
                    mem_stall[core_num]++;
                    mem_stall_count[core_num]++;
                    flush_cycles = FLUSH_TIME;
                    print_bus_trace = true;
                    return SUCCESS;
                }
            }

            if (cur_bus.cmd == Flush && cur_bus.origid == MAIN_MEMORY)// flush from main memory to the core who owns the bus
            {
                flush_cycles -= 1;
                if (flush_cycles == 0)// check if next command on the bus is flush
                {
                    print_bus_trace = true;
                    updated_bus.data = main_memory[cur_bus.addr];
                    tsram[core_num][cur_bus.addr & 0xff].MSI = MODIFIED;
                    tsram[core_num][cur_bus.addr & 0xff].tag = cur_bus.addr / 256;
                    dsram[core_num][cur_bus.addr & 0xff] = rd_value;
                    mem_stage[core_num] = CACHE_ACCESS;
                    if (pipeline[core_num][MEM].opcode == SC)
                    {
                        updated_regs[core_num][pipeline[core_num][MEM].rd] = 1;
                        watch[core_num].sc_dirty = 0;
                    }
                    return SUCCESS;
                }
                else
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
           
            else if (cur_bus.cmd == Flush && cur_bus.origid != MAIN_MEMORY && cur_bus.origid != core_num)
            {

                updated_bus.data = dsram[cur_bus.origid][cur_bus.addr & 0xff];
                dsram[core_num][cur_bus.addr & 0xff] = rd_value;
                tsram[core_num][cur_bus.addr & 0xff].MSI = MODIFIED;
                tsram[core_num][cur_bus.addr & 0xff].tag = cur_bus.addr / 256;
                tsram[cur_bus.origid][cur_bus.addr].MSI = INVALID;
                print_bus_trace = true;
                main_memory[cur_bus.addr] = cur_bus.data;

                if (pipeline[core_num][MEM].opcode == SC)
                {
                    updated_regs[core_num][pipeline[core_num][MEM].rd] = 1;
                    watch[core_num].sc_dirty = 0;
                }

                mem_stage[core_num] = CACHE_ACCESS;
                return SUCCESS;
            }
            else if (cur_bus.cmd == Flush && cur_bus.origid == core_num)
            {
                updated_bus.data = dsram[core_num][cur_bus.addr & 0xff];
                print_bus_trace = true;
                main_memory[cur_bus.addr] = updated_bus.data;
                tsram[core_num][cur_bus.addr & 0xff].MSI = SHARE;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
        }
    }
    return status;
}


Status write_back(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    //if halt, terminate core
    if (pipeline[core_num][WRITE_BACK].opcode == HALT) {
        core_done[core_num] = true;
        cycles_count[core_num] = cycle + 1;
        instructions_count[core_num]++;
        return SUCCESS;
    }
    //instruction didn't reach yet
    else if (pipeline[core_num][WRITE_BACK].pc == -1)
        return SUCCESS;

    //Updates cur_regs with updated_regs
    if (pipeline[core_num][WRITE_BACK].opcode == JAL)
        cur_regs[core_num][15] = updated_regs[core_num][15];

    else if ((pipeline[core_num][WRITE_BACK].opcode <= SRL) || (pipeline[core_num][WRITE_BACK].opcode == LW) || (pipeline[core_num][WRITE_BACK].opcode == LL) || (pipeline[core_num][WRITE_BACK].opcode == SC)) {
        cur_regs[core_num][pipeline[core_num][WRITE_BACK].rd] = updated_regs[core_num][pipeline[core_num][WRITE_BACK].rd];
    }
    instructions_count[core_num]++;

    return SUCCESS;
}

Status advance_pipeline(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    //if halt reached decode, terminate instruction in fetch and signal fetch stage with pc = -1
    if (pipeline[core_num][DECODE].opcode == HALT) {
        pc[core_num] = -1;
        pipeline[core_num][FETCH].pc = -1;
    }

    //advance pipeline stages and insert NOP's on stalls
    pipeline[core_num][WRITE_BACK].pc = -1;
    if (mem_stall[core_num] == 0) {
        advance_stage(core_num, MEM, WRITE_BACK);
        advance_stage(core_num, EXECUTE, MEM);
        if (decode_stall[core_num] == 0) {
            advance_stage(core_num, DECODE, EXECUTE);
            advance_stage(core_num, FETCH, DECODE);
            if (pc[core_num] != -1 && branch[core_num] == false) 
                pc[core_num]++;
        }
        else {
            pipeline[core_num][EXECUTE].pc = -1;
        }
    }
    
    //reset branch signal
    if (branch[core_num])
        branch[core_num] = false;

    return SUCCESS;
}


void advance_bus() {
    cur_bus.addr = updated_bus.addr;
    cur_bus.cmd = updated_bus.cmd;
    cur_bus.data = updated_bus.data;
    cur_bus.origid = updated_bus.origid;
}


void advance_stage(Core core_num, Pipe from, Pipe to) {
    pipeline[core_num][to].opcode   = pipeline[core_num][from].opcode;
    pipeline[core_num][to].rd       = pipeline[core_num][from].rd;
    pipeline[core_num][to].rs       = pipeline[core_num][from].rs;
    pipeline[core_num][to].rt       = pipeline[core_num][from].rt;
    pipeline[core_num][to].imm      = pipeline[core_num][from].imm;
    pipeline[core_num][to].pc       = pipeline[core_num][from].pc;
}

void print_trace(Core core_num, FILE* file) {

    fprintf(file, "%d ", cycle);
    for (int i = 0; i < PIPE_LEN; i++) {
        if (pipeline[core_num][i].pc == -1)
            fprintf(file, "--- ");
        else
            fprintf(file, "%03X ", pipeline[core_num][i].pc);
    }
    for (int i = 2; i < NUM_OF_REGS; i++) {
        fprintf(file, "%08X ", cur_regs[core_num][i]);
    }
    fprintf(file, "\n");

}


int search_address_cache(BUS* bus)// returns core num or not found
{
    int i;
    for (i = 0; i < NUM_OF_CORES; i++)
    {
        if (tsram[i][(*bus).addr & 0xff].tag == (*bus).addr / 256 && tsram[i][(*bus).addr & 0xff].MSI == MODIFIED)
            return i;
    }
    return NOT_FOUND;
}

bool free_access_bus(int core_num)
{
    int i;
    for (i = 0; i < NUM_OF_CORES; i++)
    {
        if (mem_stage[i] == BUS_ACCESS && i!=core_num)
            return false;
    }
    return true;
}

void print_bus(FILE* bus_trace)
{
        fprintf(bus_trace, "%d %d %d %05X %08X\n", cycle, cur_bus.origid, cur_bus.cmd, cur_bus.addr, cur_bus.data);
}

void bus_response()
{
        if (cur_bus.cmd == Flush)// finish flush by main memory so empty the updated_bus
            updated_bus.cmd = NO_COMMAND;
        if (cur_bus.cmd == BusRd || cur_bus.cmd == BusRdX)// update bus_origid to the the core that is going to make flush or the main memory that will do the flush
        {
            updated_bus.cmd = Flush;
            if (search_address_cache(&cur_bus) != NOT_FOUND && tsram[search_address_cache(&cur_bus)][cur_bus.addr & 0xff].MSI == MODIFIED)
                updated_bus.origid = search_address_cache(&cur_bus);
            else
                updated_bus.origid = MAIN_MEMORY;
        }
}



bool sc_func(int address, int core_num)
{
   
    for (int i = 0; i < NUM_OF_CORES; i++) {
        if (watch[i].address == watch[core_num].address && i != core_num && watch[i].sc_dirty == 1)
        {
            updated_regs[core_num][pipeline[core_num][MEM].rd] = 0;
            watch[core_num].address = -1;
            return false;
        }
    }
    watch[core_num].sc_dirty = 1;
    return true;
}

