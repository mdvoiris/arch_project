
#include "main.h"


Status main(int argc, char* argv[]) {
	Status status = INVALID_STATUS_CODE;
    FILE* trace_files[NUM_OF_CORES] = { NULL };
    FILE* bus_trace = {NULL};

    //initialize watch
    watch[CORE0].address = -1;
    watch[CORE1].address = -1;
    watch[CORE2].address = -1;
    watch[CORE3].address = -1;


    //Check for correct input argument count
    //if (argc != 1 || argc != ARG_COUNT) {
    if (argc != ARG_COUNT) {
        printf("Error - got %d arguments, expecting %d", argc, ARG_COUNT);
        return WRONG_ARGUMENT_COUNT;
    }
    else if (argc == ARG_COUNT) update_args(argv);

    status = init_imems();
    if (status) goto EXIT;

    status = init_main_memory();
    if (status) goto EXIT;

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
    
    while ((core_done[CORE0] & core_done[CORE1] & core_done[CORE2] & core_done[CORE3]) == false) {
        if (core_done[CORE0] == false) {
            status = core(CORE0, trace_files[CORE0], bus_trace);
            if (status) goto EXIT;
        }
        if (core_done[CORE1] == false) {
            status = core(CORE1, trace_files[CORE1], bus_trace);
            if (status) goto EXIT;
        }
        if (core_done[CORE2] == false) {
            status = core(CORE2, trace_files[CORE2], bus_trace);
            if (status) goto EXIT;
        }
        if (core_done[CORE3] == false) {
            status = core(CORE3, trace_files[CORE3], bus_trace);
            if (status) goto EXIT;
        }
        cycle++;

        status = cache_update();
      
        if (status) goto EXIT;
    }
    fclose(bus_trace);
    for (int i = 0; i < NUM_OF_CORES; i++) {
        fclose(trace_files[i]);
        trace_files[i] = NULL;
    }


    status = print_file(MEMOUT);
    if (status) goto EXIT;

    for (int i = 0; i < NUM_OF_CORES; i++) {
        status = print_file(REGOUT0 + i);
        if (status) goto EXIT;

        status = print_file(DSRAM0 + i);
        if (status) goto EXIT;

        status = print_file(TSRAM0 + i);
        if (status) goto EXIT;

        status = print_file(STATS0 + i);
        if (status) goto EXIT;
    }

	return SUCCESS;


EXIT:
    for (int i = 0; i < NUM_OF_CORES; i++)
        if (trace_files[i] != NULL) {
            fclose(trace_files[i]);
        }
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
            fprintf(file, "%04X\n", tsram[core][i]);
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
            fscanf_s(imem_files[core], "%08X", imem[core][line]);
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
        fscanf_s(memin_file, "%08X", main_memory[line]);
        if (feof(memin_file)) break;
    }

    fclose(memin_file);

    return SUCCESS;
}

Status core(Core core_num, FILE* trace_file, FILE* bus_trace) {
    Status status = INVALID_STATUS_CODE;
    bool print_bus_trace = false;

    if (decode_stall[core_num] == 0 && mem_stall[core_num] == 0) {
        status = fetch(core_num);
        if (status) return status;

        status = decode(core_num);
        if (status) return status;
    }
    else if (mem_stall[core_num] == 0) {
        status = execute(core_num);
        if (status) return status;
        status = mem(core_num, &print_bus_trace);
        if (status) return status;
    }
    print_trace(core_num, trace_file);
    if (print_bus_trace == true)
    {
        print_bus(bus_trace);
        if ((pipeline[core_num][MEM].opcode == LL) || (pipeline[core_num][MEM].opcode == LW))//PrRd opcode
            arrange_bus_load();
        if ((pipeline[core_num][MEM].opcode == SC) || (pipeline[core_num][MEM].opcode == SW))//PrRd opcode
            arrange_bus_store();
    }
    status = write_back(core_num);
    if (status) return status;

    status = advance_pipeline(core_num);
    if (status) return status;

    return SUCCESS;
}

Status fetch(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    if (pc[core_num] == -1) {
        pipeline[core_num][FETCH].pc = -1;
        return SUCCESS;
    }

    pipeline[core_num][FETCH].opcode = imem[core_num][pc[core_num]] & 0xff000000;
    pipeline[core_num][FETCH].rd     = imem[core_num][pc[core_num]] & 0x00f00000;
    pipeline[core_num][FETCH].rs     = imem[core_num][pc[core_num]] & 0x000f0000;
    pipeline[core_num][FETCH].rt     = imem[core_num][pc[core_num]] & 0x0000f000;
    pipeline[core_num][FETCH].imm    = imem[core_num][pc[core_num]] & 0x00000fff;
    pipeline[core_num][FETCH].pc     = pc[core_num];

    if ((pipeline[core_num][FETCH].opcode < ADD) || (pipeline[core_num][FETCH].opcode > HALT))
        return WRONG_OPCODE;
    if ((pipeline[core_num][FETCH].rd < 0) || (pipeline[core_num][FETCH].rd > (NUM_OF_REGS-1)))
        return WRONG_RD;
    if ((pipeline[core_num][FETCH].rs < 0) || (pipeline[core_num][FETCH].rs > (NUM_OF_REGS - 1)))
        return WRONG_RS;
    if ((pipeline[core_num][FETCH].rt < 0) || (pipeline[core_num][FETCH].rt > (NUM_OF_REGS - 1)))
        return WRONG_RT;


    return SUCCESS;
}

Status decode(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    if ((pipeline[core_num][DECODE].opcode == HALT) || (pipeline[core_num][DECODE].pc == -1)) {
        return SUCCESS;
    }

    decode_stall[core_num] = detect_hazards(core_num, DECODE);

    if (decode_stall[core_num] == 0) {
        if ((pipeline[core_num][DECODE].opcode >= BEQ) && (pipeline[core_num][DECODE].opcode <= JAL))
            branch_resolution(core_num, pipeline[core_num][DECODE]);
    }

    return SUCCESS;
}

int detect_hazards(Core core_num, Pipe stage) {
    int hazard_potential[3] = { 0 };

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

    for (int i = 0; i < 3; i++) {
        if ((hazard_potential[i] == 0) || (hazard_potential[i] == 1))
            continue;
        for (int j = stage + 1; j < PIPE_LEN; j++) {
            if ((pipeline[core_num][j].opcode == JAL) && (hazard_potential[i] == 15))
                return (WRITE_BACK - j + 1);

            else if ((pipeline[core_num][stage].opcode < BEQ) || (pipeline[core_num][stage].opcode == LW) || (pipeline[core_num][stage].opcode == LL) || (pipeline[core_num][stage].opcode == SC)) {
                if (hazard_potential[i] == pipeline[core_num][j].rd)
                    return (WRITE_BACK - j + 1);
            }
        }
    }

    return 0;
}

void branch_resolution(Core core_num, Instruction inst) {
    int rd_value;
    int rs_value;
    int rt_value;


    get_reg_values(core_num, inst, &rd_value, &rs_value, &rt_value);

    switch (inst.opcode) {
    case BEQ: {
        if (rs_value == rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case BNE: {
        if (rs_value != rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case BLT: {
        if (rs_value < rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case BGT: {
        if (rs_value > rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case BLE: {
        if (rs_value <= rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case BGE: {
        if (rs_value >= rt_value)
            pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    case JAL: {
        pc[core_num] = (rd_value & 0x3ff) - 1;
        break;
    }
    }
    return;
}

void get_reg_values(Core core_num, Instruction inst, int* rd_value, int* rs_value, int* rt_value) {
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


    if ((pipeline[core_num][EXECUTE].opcode == HALT) || (pipeline[core_num][EXECUTE].pc == -1)) {
        return SUCCESS;
    }

    get_reg_values(core_num, pipeline[core_num][EXECUTE], &rd_value, &rs_value, &rt_value);

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

Status mem(Core core_num, bool print_bus_trace)
{
    Status status = INVALID_STATUS_CODE;
    int rd_value;
    int rs_value;
    int rt_value;
    int address;
    if ((pipeline[core_num][MEM].opcode == HALT) || (pipeline[core_num][MEM].pc == -1)) {
        return SUCCESS;
    }
    get_reg_values(core_num, pipeline[core_num][MEM], &rd_value, &rs_value, &rt_value);
    address = rs_value + rt_value;
    if (address < 0 || address >= IMEM_SIZE)
        return WRONG_ADDRESS;
    if ((pipeline[core_num][MEM].opcode != SC) && (pipeline[core_num][MEM].opcode != SW) && (pipeline[core_num][MEM].opcode != LL) && (pipeline[core_num][MEM].opcode != LW))//check if the opcode isn't load or store opcode
        return SUCCESS;
    if ((pipeline[core_num][MEM].opcode == LL) || (pipeline[core_num][MEM].opcode == LW))//PrRd opcode
    {
        if (mem_stage[core_num] == CACHE_ACCESS)//  search in cache
        {
            if (tsram[core_num][(address) & 0xFF].tag == (address) / 256) // read hit
            {
                read_hit_count[core_num] += 1;
                updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][(address) & 0xFF];
                if ((pipeline[core_num][MEM].opcode == LL))//opcode is load linked
                {
                    watch[core_num].address = address;
                    watch[core_num].sc_dirty = 0;
                }
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
            if (bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI != MODIFIED)// the block in the cache of the core that owns the bus is INVALIDATE or SHARE
            {
                bus.cmd = BusRd;
                bus.data = 0;
                bus.addr = address;
                bus.origid = core_num;
            }

            else if (bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI == MODIFIED)// the block in the cache of the core that owns the bus is MODIFIED
            {
                bus.cmd = Flush;
                bus.addr = address;
                bus.data = 0;
                bus.origid = core_num;
                flush_cycles = FLUSH_TIME;
            }

            if (bus.cmd == BusRd)// check if the cmd is BusRd
            {

                if (search_address_cache() != NOT_FOUND && tsram[search_address_cache()][bus.addr & 0xff].MSI == MODIFIED)// checks if the block available in another modified core 
                {
                    
                    flush_cycles = FLUSH_TIME;
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

            if (bus.cmd == Flush && bus.origid == MAIN_MEMORY)// flush from main memory to the core who owns the bus
            {
                flush_cycles -= 1;
                if (flush_cycles == 0)// check if next command on the bus is flush
                {
                    print_bus_trace = true;
                    bus.data = main_memory[bus.addr];
                    tsram[core_num][bus.addr & 0xff].MSI = SHARE;
                    tsram[core_num][bus.addr & 0xff].tag = bus.addr / 256;
                    dsram[core_num][bus.addr & 0xff] = bus.data;
                    updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][bus.addr & 0xff];
                    mem_stage[core_num] = CACHE_ACCESS;
                    if ((pipeline[core_num][MEM].opcode == LL))//opcode is load linked
                    {
                        watch[core_num].address = address;
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
            else if (bus.cmd == Flush && bus.origid != MAIN_MEMORY && tsram[core_num][bus.addr & 0xff].MSI != MODIFIED)
            {
               
                if (flush_cycles == FLUSH_TIME )//flush to the bus and update the calling core
                {
                    bus.data = dsram[bus.origid][bus.addr];
                    dsram[core_num][bus.addr & 0xff] = bus.data;
                    updated_regs[core_num][pipeline[core_num][MEM].rd] = dsram[core_num][bus.addr & 0xff];
                    tsram[core_num][bus.addr & 0xff].MSI = SHARE;
                    tsram[core_num][bus.addr & 0xff].tag = bus.addr / 256;
                    tsram[bus.origid][bus.addr].MSI = SHARE;
                    print_bus_trace = true;
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    flush_cycles -= 1;
                    if ((pipeline[core_num][MEM].opcode == LL))//opcode is load linked
                    {
                        watch[core_num].address = address;
                        watch[core_num].sc_dirty = 0;
                    }
                    return SUCCESS;
                }
                flush_cycles -= 1;
                if (flush_cycles == 0)//flush arrives to main memory and empty the bus
                {
                    main_memory[bus.addr] = bus.data;
                    mem_stage[core_num] = CACHE_ACCESS;
                    bus.cmd = NO_COMMAND;
                    return SUCCESS;
                }
                else//waiting for the flush to arrive to the main memory
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
            else if (bus.cmd == Flush && bus.origid != MAIN_MEMORY && tsram[core_num][bus.addr & 0xff].MSI == MODIFIED)
            {
                if (flush_cycles == FLUSH_TIME )//flush to the bus 
                {
                    bus.data = dsram[core_num][bus.addr & 0xff];
                    print_bus_trace = true;
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    flush_cycles--;
                    return SUCCESS;
                }
                flush_cycles--;
                if (flush_cycles == 0)//flush arrives to main memory and empty the bus
                {
                    main_memory[bus.addr] = bus.data;
                    bus.cmd = NO_COMMAND;
                    tsram[core_num][bus.addr & 0xff].MSI = SHARE;
                    return SUCCESS;
                }
                else//waiting for the flush to arrive to the main memory
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
        }
    }


    else if ((pipeline[core_num][MEM].opcode == SC) || (pipeline[core_num][MEM].opcode == SW))//PrWr opcode
    {
        /*  if (mem_stage[core_num] == CACHE_ACCESS && tsram[core_num][bus.addr*0xff].MSI == MODIFIED && core_num == abort_cache && address == bus.addr && (bus.cmd == BusRd || bus.cmd == Flush))// abort mem request because the block flush or BusRd
          {
              mem_stall[core_num]++;
              return SUCCESS;

          }*/
        if (mem_stage[core_num] == CACHE_ACCESS)//  search in cache
        {
            
            if (tsram[core_num][(address) & 0xFF].tag == (address) / 256) // write hit
            {
                write_hit_count[core_num] += 1;
                if (tsram[core_num][address & 0xff].MSI == SHARE)//opcode is load linked
                {
                    if (free_access_bus(core_num))
                        mem_stage[core_num] = BUS_ACCESS;
                    else// bus isn't available
                        mem_stage[core_num] = WAIT_FOR_BUS;
                    return SUCCESS;
                }
                else
                {
                    if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SC)
                        sc_func(address, core_num, rd_value);
                    else if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SW)
                        dsram[core_num][address & 0xff] = rd_value;
                    return SUCCESS;
                }
            }
            if (free_access_bus(core_num))// write miss so check if the bus is accessible 
            {
                mem_stage[core_num] = BUS_ACCESS;
                write_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
            else// the bus not accessible  the core needs to wait
            {
                read_miss_count[core_num]++;
                mem_stall[core_num] = 1;
                mem_stage[core_num] = WAIT_FOR_BUS;
                mem_stall_count[core_num]++;
                return SUCCESS;
            }
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
            if (bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI != MODIFIED)// the block in the cache of the core that owns the bus is INVALIDATE or SHARE
            {
                bus.cmd = BusRdX;
                bus.data = 0;
                bus.addr = address;
                bus.origid = core_num;
            }

            else if (bus.cmd == NO_COMMAND && tsram[core_num][address & 0xff].MSI == MODIFIED)// the block in the cache of the core that owns the bus is MODIFIED
            {
                bus.cmd = Flush;
                bus.addr = address;
                bus.data = 0;
                bus.origid = core_num;
                flush_cycles = FLUSH_TIME;
            }

            if (bus.cmd == BusRdX)// check if the cmd is BusRd
            {
                for (int i = 0; i < 4; i++)
                    if (tsram[i][bus.addr & 0xff].MSI == SHARE && i != core_num)
                        tsram[i][bus.addr & 0xff].MSI = INVALID;
                if (search_address_cache() != NOT_FOUND && tsram[search_address_cache()][bus.addr & 0xff].MSI == MODIFIED)// checks if the data available in another core 
                {

                    //abort_cache = search_address_cache();
                    flush_cycles = FLUSH_TIME;
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

            if (bus.cmd == Flush && bus.origid == MAIN_MEMORY)// flush from main memory to the core who owns the bus
            {
                flush_cycles -= 1;
                if (flush_cycles == 0)// check if next command on the bus is flush
                {
                    print_bus_trace = true;
                    bus.data = main_memory[bus.addr];
                    tsram[core_num][bus.addr & 0xff].MSI = MODIFIED;
                    tsram[core_num][bus.addr & 0xff].tag = bus.addr / 256;
                    dsram[core_num][bus.addr & 0xff] = bus.data;
                    mem_stage[core_num] = CACHE_ACCESS;
                    if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SC)
                        sc_func(address, core_num, rd_value);
                    if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SW)
                        dsram[core_num][bus.addr & 0xff] = rd_value;
                    return SUCCESS;
                }
                else
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
            else if (bus.cmd == Flush && bus.origid != MAIN_MEMORY && bus.origid != core_num)
            {

                if (flush_cycles == FLUSH_TIME)//flush to the bus and update the calling core
                {
                    bus.data = dsram[bus.origid][bus.addr];
                    dsram[core_num][bus.addr & 0xff] = bus.data;
                    tsram[core_num][bus.addr & 0xff].MSI = MODIFIED;
                    tsram[core_num][bus.addr & 0xff].tag = bus.addr / 256;
                    tsram[bus.origid][bus.addr].MSI = INVALID;
                    print_bus_trace = true;
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SC)
                        sc_func(address, core_num, rd_value);
                    if (tsram[core_num][address & 0xff].MSI == MODIFIED && pipeline[core_num][MEM].opcode == SW)
                        dsram[core_num][bus.addr & 0xff] = rd_value;
                    flush_cycles -= 1;
                    return SUCCESS;
                }
                flush_cycles -= 1;
                if (flush_cycles == 0)//flush arrives to main memory and empty the bus
                {
                    main_memory[bus.addr] = bus.data;
                    mem_stage[core_num] = CACHE_ACCESS;
                    bus.cmd = NO_COMMAND;
                    return SUCCESS;
                }
                else//waiting for the flush to arrive to the main memory
                {
                    //tsram[abort_cache][bus.addr].MSI = SHARE;//cancel abort to block who was modified and finished to flush to the cache who owns the bus
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
            else if (bus.cmd == Flush && bus.origid != MAIN_MEMORY && tsram[core_num][bus.addr & 0xff].MSI == MODIFIED)
            {
                if (flush_cycles == FLUSH_TIME)//flush to the bus 
                {
                    bus.data = dsram[core_num][bus.addr & 0xff];
                    print_bus_trace = true;
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    flush_cycles--;
                    return SUCCESS;
                }
                flush_cycles--;
                if (flush_cycles == 0)//flush arrives to main memory and empty the bus
                {
                    main_memory[bus.addr] = bus.data;
                    bus.cmd = BusRdX;
                    tsram[core_num][bus.addr & 0xff].MSI = MODIFIED;
                    return SUCCESS;
                }
                else//waiting for the flush to arrive to the main memory
                {
                    mem_stall[core_num] = 1;
                    mem_stall_count[core_num]++;
                    return SUCCESS;
                }
            }
        }
    }
}


Status write_back(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    if (pipeline[core_num][EXECUTE].opcode == HALT) {
        core_done[core_num] = true;
        cycles_count[core_num] = cycle;
        instructions_count[core_num]++;
        return SUCCESS;
    }
    else if (pipeline[core_num][EXECUTE].pc == -1)
        return SUCCESS;

    if (pipeline[core_num][WRITE_BACK].opcode == JAL)
        cur_regs[core_num][15] = updated_regs[core_num][15];

    else if ((pipeline[core_num][WRITE_BACK].opcode <= SRL) || (pipeline[core_num][WRITE_BACK].opcode == LW) || (pipeline[core_num][WRITE_BACK].opcode == LL)) {
        cur_regs[core_num][pipeline[core_num][WRITE_BACK].rd] = updated_regs[core_num][pipeline[core_num][WRITE_BACK].rd];
    }
    instructions_count[core_num]++;

    return SUCCESS;
}

Status advance_pipeline(Core core_num) {
    Status status = INVALID_STATUS_CODE;


    if (pipeline[core_num][DECODE].opcode == HALT) {
        pc[core_num] = -1;
    }
    else {
        pc[core_num]++;
    }

    pipeline[core_num][WRITE_BACK].pc = -1;
    if (mem_stall[core_num] == 0) {
        advance_stage(core_num, MEM, WRITE_BACK);
        advance_stage(core_num, EXECUTE, MEM);
        if (decode_stall[core_num] == 0) {
            advance_stage(core_num, DECODE, EXECUTE);
            advance_stage(core_num, FETCH, DECODE);
        }
        else {
            pipeline[core_num][EXECUTE].pc = -1;
        }
    }

    if (mem_stall[core_num] > 0) {
        mem_stall[core_num]--;
        mem_stall_count[core_num]++;
    }
    if (decode_stall[core_num] > 0) {
        decode_stall[core_num]--;
        decode_stall_count[core_num]++;
    }

    return SUCCESS;
}

void advance_stage(Core core_num, Pipe from, Pipe to) {
    pipeline[core_num][to].opcode   = pipeline[core_num][from].opcode;
    pipeline[core_num][to].rd       = pipeline[core_num][from].rd;
    pipeline[core_num][to].rs       = pipeline[core_num][from].rs;
    pipeline[core_num][to].rt       = pipeline[core_num][from].rt;
    pipeline[core_num][to].imm      = pipeline[core_num][from].imm;
    pipeline[core_num][to].pc       = pipeline[core_num][from].pc;

    return;
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

Status cache_update() {
    Status status = INVALID_STATUS_CODE;

    //HAIM

    return SUCCESS;
}

int search_address_cache()// returns core num or not found
{
    int i;
    for (i = 0; i < 4; i++)
    {
        if (tsram[i][bus.addr & 0xff].tag == bus.addr / 256)
            return i;
    }
    return NOT_FOUND;
}

bool free_access_bus(int core_num)
{
    int i;
    for (i = 0; i < 4; i++)
    {
        if (mem_stage[i] == BUS_ACCESS && i!=core_num)
            return false;
    }
    return true;
}

void print_bus(FILE* bus_trace)
{
    fprintf(bus_trace, "%d %d %d %05X %08X\n", cycle, bus.origid, bus.cmd, bus.addr, bus.data);
}

void arrange_bus_load()
{
    if (bus.cmd == Flush && bus.origid == MAIN_MEMORY)// finish flush by main memory so empty the bus
        bus.cmd = NO_COMMAND;
    if (bus.cmd == BusRd)// update bus_origid to the the core that is going to make flush or the main memory that will do the flush
    {
        bus.cmd = Flush;
        if (search_address_cache() != NOT_FOUND && tsram[search_address_cache()][bus.addr & 0xff].MSI == MODIFIED)
            bus.origid = search_address_cache();
        else
            bus.origid = MAIN_MEMORY;
    }
}

void arrange_bus_store()
{
    if (bus.cmd == Flush && bus.origid == MAIN_MEMORY)// finish flush by main memory so empty the bus
        bus.cmd = NO_COMMAND;
    if (bus.cmd == BusRdX)// update bus_origid to the the core that is going to make flush or the main memory that will do the flush
    {
        bus.cmd = Flush;
        if (search_address_cache() != NOT_FOUND && tsram[search_address_cache()][bus.addr & 0xff].MSI == MODIFIED)
            bus.origid = search_address_cache();
        else
            bus.origid = MAIN_MEMORY;
    }
}

void sc_func(int address, int core_num, int rd_value)
{
    if (watch[core_num].sc_dirty == 0)
    {
        for (int i = 0; i < 4; i++)
            if (watch[i].address == watch[core_num].address && i != core_num)
                watch[i].sc_dirty = 1;
        dsram[core_num][address & 0xff] = rd_value;
        updated_regs[core_num][pipeline[core_num][MEM].rd] = 1;
        watch[core_num].address = -1;
    }
    else if (watch[core_num].sc_dirty == 1)
    {
        watch[core_num].sc_dirty = 0;
        updated_regs[core_num][pipeline[core_num][MEM].rd] = 0;
        watch[core_num].address = -1;
    }
}