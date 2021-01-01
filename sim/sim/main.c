
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

}

Status cache_update() {

}