
#include "main.h"

Status main(int argc, char* argv[]) {
	Status status = INVALID_STATUS_CODE;
    

    //Check for correct input argument count
    if (argc != 1 || argc != ARG_COUNT) {
        printf("Error - got %d arguments, expecting %d", argc, ARG_COUNT);
        return WRONG_ARGUMENT_COUNT;
    }
    else if (argc == ARG_COUNT) update_args(argv);

    status = init_imems();
    if (status) return status;

    status = init_main_memory();
    if (status) return status;


    while ((core_done[CORE0] & core_done[CORE1] & core_done[CORE2] & core_done[CORE3]) == false) {
        if (core_done[CORE0] == false) {
            status = core(CORE0);
            if (status) return status;
        }
        if (core_done[CORE1] == false) {
            status = core(CORE1);
            if (status) return status;
        }
        if (core_done[CORE2] == false) {
            status = core(CORE2);
            if (status) return status;
        }
        if (core_done[CORE3] == false) {
            status = core(CORE3);
            if (status) return status;
        }
        cycle++;

        status = cache_update();
        if (status) return status;

    }


	return SUCCESS;
}

void update_args(char* argv[]) {
    for (int i = 1; i < ARG_COUNT; i++) {
        strcpy_s(args[i], _MAX_PATH, argv[i]);
    }
}

Status init_imems() {
    Status status = INVALID_STATUS_CODE;
    FILE* imem_files[NUM_OF_CORES] = { NULL };


    for (int core = 0; core < NUM_OF_CORES; core++) {
        fopen_s(&imem_files[core], args[IMEM0 + core], "r");
        if (imem_files[core] == NULL)
            return FOPEN_FAIL;

        for (int line = 0; line < IMEM_SIZE; line++) {
            fscanf(imem_files[core], "%08X", imem[core][line]);
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
        fscanf(memin_file, "%08X", main_memory[line]);
        if (feof(memin_file)) break;
    }

    fclose(memin_file);

    return SUCCESS;
}

Status core(Core core_num) {
    Status status = INVALID_STATUS_CODE;

    if (decode_stall_c[core_num] == 0 && mem_stall_c[core_num] == 0) {
        status = fetch(core_num);
        status = decode(core_num);
    }
    else if (mem_stall_c[core_num] == 0) {
        status = execute(core_num);
        status = mem(core_num);
    }
    status = write_back(core_num);

    advance_pipeline(core_num);
    pc[core_num]++;

    return SUCCESS;
}

Status fetch(Core core_num) {
    Status status = INVALID_STATUS_CODE;

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

    if (pipeline[core_num][DECODE].opcode == HALT) {
        pipeline[core_num][FETCH].pc = -1;
        return SUCCESS;
    }

    decode_stall_c[core_num] = detect_hazards(core_num, DECODE);

    if (decode_stall_c[core_num] == 0) {
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


    if (inst.rd == 1) rd_value = inst.imm;
    else rd_value = cur_regs[inst.rd];

    if (inst.rs == 1) rs_value = inst.imm;
    else rs_value = cur_regs[inst.rs];

    if (inst.rt == 1) rt_value = inst.imm;
    else rt_value = cur_regs[inst.rt];


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

Status execute(Core core_num) {
    Status status = INVALID_STATUS_CODE;



    return SUCCESS;
}

Status mem(Core core_num) {
    Status status = INVALID_STATUS_CODE;



    return SUCCESS;
}

Status write_back(Core core_num) {
    Status status = INVALID_STATUS_CODE;



    return SUCCESS;
}

Status advance_pipeline(Core core_num) {
    Status status = INVALID_STATUS_CODE;



    return SUCCESS;
}

Status cache_update() {
    Status status = INVALID_STATUS_CODE;



    return SUCCESS;
}