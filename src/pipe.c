/*
 * CMSC 22200
 *
 * ARM pipeline timing simulator
 */
//#include "sim.c"
#include "cache.h"
#include "pipe.h"
#include "shell.h"
#include "helper.h"
#include "cuRrent_state.bp.h"

// Public libs
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>



/************************ TURN ON VERBOSE MODE IF 1 ******************************/
int VERBOSE = 0;
int CACHE_VERBOSE = 0;

/************************************ CONSTANTS ************************************/
/* 
 * NOT INCLUDED: CBNZ, CBZ, MOVZ, B, B.COND
 * 
 */
#define HLT 0xd4400000 // entire instr, not code
#define ADD 0x458 //need to add one to handle both cases
#define ADDI 0x488 //need to add one to handle both cases
#define ADDS 0x558 //need to add one to handle both cases
#define ADDIS 0x588 //need to add one to handle both cases
#define AND 0x450
#define ANDS 0x750
#define EOR 0x650
#define ORR 0x550
#define LDUR_64 0x7C2
#define LDUR_32 0x5C2
#define LDURB 0x1C2
#define LDURH 0x3C2
#define LSL	0x69B //IMMEDIATE VERSION
#define LSR 0x69A //IMMEDIATE VERSION
#define STUR 0x7C0
#define STURB 0x1C0
#define STURH 0x3C0
#define STURW 0x5C0
#define SUB 0x658 //need to add one to handle both cases
#define SUBI 0x688 //need to add one to handle both cases
#define SUBS 0x758 //need to add one to handle both cases
#define SUBIS 0x788
#define MUL 0x4D8
#define BR 0x6B0

#define CONDITIONAL 1
#define UNCONDITIONAL 0

#define VALID 1
#define INVALID 0
/************************************ END OF CONSTANTS ************************************/

/************************************ HELPERS ************************************/

// This function prints the cache behavior
// flag = 1 (instruction phase), flag = 2 (memmory phase)
void print_cache_behavior(int flag) {
	if (CACHE_VERBOSE == 0) {
		return;
	}

	if (flag == 1) {
		if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 50) {
			printf("icache miss (0x%lx) at cycle %d\n", CURRENT_STATE.PC, stat_cycles + 1);	
		} else if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE != 0) {
			printf("icache current_state.bubble(%d)\n", CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE);
			if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 1) {
				printf("icache fill at cycle %d\n", stat_cycles + 1);
			}
		} else {
			if (CURRENT_STATE.FETCH_MORE != 0 ) {
				printf("icache hit (0x%lx) at cycle %d\n", CURRENT_STATE.PC, stat_cycles + 1);
			}
		}
	} else if (flag == 2) {
		//printf("(Cycle: %d) -> ", CURRENT_STATE.PC, stat_cycles + 1);
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 50) {
			printf("dcache miss (0x%lx) at cycle %d\n", CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result, stat_cycles + 1);	
		} else if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
			printf("dcache stall(%d)\n", CURRENT_STATE.CYCLE_STALL_DATA_CACHE);
			if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 1) {
				printf("dcache fill at cycle %d\n", stat_cycles + 1);
			}
		} else {
			printf("dcache hit (0x%lx) at cycle %d\n", CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result, stat_cycles + 1);
		}
	}
}


void reset_current_state.bubble() {
	CURRENT_STATE.BUBBLE = 0;
}

void manage_cache_stall() {
	if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE != 0) {
		CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE--;
	}


	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
		CURRENT_STATE.CYCLE_STALL_DATA_CACHE--;
	}
}

void clear_IF_ID_REGS() {
	CURRENT_STATE.CURRENT_REGS.IF_ID.PC = 0;
	CURRENT_STATE.CURRENT_REGS.IF_ID.instruction = 0;
	CURRENT_STATE.CURRENT_REGS.IF_ID.PHT_result = 0; 
}

void clear_IF_ID_RESERVOIR_REGS() {
	CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PC = 0;
	CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.instruction = 0;
	CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PHT_result = 0; 
}

void clear_ID_EX_REGS() {
	CURRENT_STATE.CURRENT_REGS.ID_EX.PC = 0;
	CURRENT_STATE.CURRENT_REGS.ID_EX.instruction = 0;
	CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = 0;
	CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = 0;
	CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = 0;
	CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result = 0;
}

void clear_EX_MEM_REGS() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.PC = 0;
	CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction = 0;
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = 0;
	CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write = 0;
}

void clear_MEM_WB_REGS() {
	CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction = 0;
	CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data = 0;
	CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result = 0;
}

// Equation for regWrite flag
int get_regWrite(uint32_t opcode) {
	return (opcode < 0x0A0 || opcode > 0x0BF) && (opcode != STUR) && 
		(opcode != STURB) && (opcode != STURH) && (opcode != STURW);
}

int get_memRead(uint32_t opcode) {
	return (opcode == LDUR_64) || (opcode == LDUR_32) ||
		(opcode == LDURB) || (opcode == LDURH);
}

int hazard_detection_unit(uint32_t depend_instruct, uint32_t ind_instruct) {
	if (ind_instruct == 0) {
		return 0;
	}

	parsed_instruction_holder depend_holder = get_holder(depend_instruct);
	parsed_instruction_holder ind_holder = get_holder(ind_instruct);

	if (depend_holder.opcode >= 0x694 && depend_holder.opcode <= 0x697) {
		return 0;
	}

	if (get_memRead(ind_holder.opcode)) {
		if (ind_holder.Rt == depend_holder.Rn) {
			if (depend_holder.format != 4 && depend_holder.format != 5 &&
				depend_holder.format != 6) {
				return 1;
			}
		}

		if (depend_holder.format == 1) {	
			if (depend_holder.opcode == LSL || depend_holder.opcode == LSR 
				|| depend_holder.opcode == BR) {
				return 0;
			}
			if (ind_holder.Rt == depend_holder.Rm) {
				return 2;
			}
		// SPECIAL FOR STUR, CBZ, CBNZ
		} else if (depend_holder.opcode == STUR || depend_holder.opcode == STURH ||
			depend_holder.opcode == STURB || depend_holder.opcode == STURW ||
			(depend_holder.opcode >= 0x5A0 && depend_holder.opcode <= 0x5AF)) {

			if (ind_holder.Rt == depend_holder.Rt) {
				return 2;
			}
		}
	}
	return 0;
}

int forward(uint32_t depend_instruct, uint32_t ind_instruct) {
	if (depend_instruct == 0 || ind_instruct == 0) {
		return 0;
	}


	parsed_instruction_holder depend_holder = get_holder(depend_instruct);
	parsed_instruction_holder ind_holder = get_holder(ind_instruct);

	if (get_memRead(ind_instruct)) {
		return 0;
	}

	if (depend_holder.opcode >= 0x694 && depend_holder.opcode <= 0x697) {
		return 0;
	}

	if ((ind_holder.Rd != 31) && get_regWrite(ind_holder.opcode)) {
		uint32_t ind_target = ind_holder.Rd;
		if (ind_holder.format == 3) {
			ind_target = ind_holder.Rt;
		} else if (ind_holder.format == 4 || ind_holder.format == 5) {
			return 0;
		}

		if (ind_target == depend_holder.Rn) {
			if (depend_holder.format != 4 && depend_holder.format != 5 &&
				depend_holder.format != 6) {
				return 1;
			}
		}

		if (depend_holder.format == 1) {	
			if (depend_holder.opcode == LSL || depend_holder.opcode == LSR ||
				depend_holder.opcode == BR) {
				return 0;
			} 

			if (ind_target == depend_holder.Rm) {
				return 2;
			}
		} else if (depend_holder.opcode == STUR || depend_holder.opcode == STURH ||
			depend_holder.opcode == STURB || depend_holder.opcode == STURW ||
			(depend_holder.opcode >= 0x5A0 && depend_holder.opcode <= 0x5AF)) {
			
			if (ind_target == depend_holder.Rt) {
				return 2;
			}
		}
	}
	return 0;
}

void set_settings_pred_miss (uint32_t aActualNextInstructionPC) {
	CURRENT_STATE.BUBBLE = 1;
	if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE != 0) {
		if ((get_instruction_cache_tag(CURRENT_STATE.PC) != 
				get_instruction_cache_tag(aActualNextInstructionPC)) || 
			(get_instruction_cache_set_index(CURRENT_STATE.PC) 
				!= get_instruction_cache_set_index(aActualNextInstructionPC))) {
			CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE = 0;
		} 
	}

	CURRENT_STATE.PC = aActualNextInstructionPC;
	clear_IF_ID_REGS();
	clear_ID_EX_REGS();
}
/******************************* R EXECUTION INSTRUCTIONS HANLDERS *******************************/

void handle_add() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder + CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;
}

void handle_adds() {
	handle_add();
}

void handle_and() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder & CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;	
}

void handle_ands() {
	handle_and();
}

void handle_eor() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder ^ CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;	
}

void handle_orr() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder | CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;
}

void handle_lsl() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = ((uint64_t)CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder) << (0x3F - CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
}

void handle_lsr() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder >> CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;
}

void handle_sub() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder - CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;		
}

void handle_subs() {
	handle_sub();
}

void handle_br(uint32_t aExecuteInstructionPC, uint32_t aPredictedNextInstructionPC) {
	uint32_t myActualNextInstructionPC = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder;

	evaluate_prediction(&CURRENT_STATE,aExecuteInstructionPC,
		myActualNextInstructionPC, 
		aPredictedNextInstructionPC, 
		CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry,
		UNCONDITIONAL,
		1,
		1);
}

void handle_mul() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder * CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;		
}


/******************************* I EXECUTION INSTRUCTIONS HANLDERS *******************************/
void handle_addi() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate; 
}	

void handle_addis() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
}

void handle_subi() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder - CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
}

void handle_subis() {
	CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder - CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
}


/******************************* CB EXECUTION INSTRUCTIONS HANLDERS *******************************/
void handle_bcond(parsed_instruction_holder HOLDER, uint32_t aExecuteInstructionPC, uint32_t aPredictedNextInstructionPC) {
	uint32_t cond = (HOLDER.Rt & 14) >> 1;
	int flag_N = CURRENT_STATE.FLAG_N;
	int flag_Z = CURRENT_STATE.FLAG_Z;
	int result = 0; 

	parsed_instruction_holder MEM_instruct = get_holder(CURRENT_STATE.START_REGS.EX_MEM.instruction);
	if (MEM_instruct.opcode == ADDS || MEM_instruct.opcode == (ADDS + 1) ||
		MEM_instruct.opcode == ANDS || MEM_instruct.opcode == SUBS || 
		MEM_instruct.opcode == (SUBS + 1) || MEM_instruct.opcode == ADDIS || 
		MEM_instruct.opcode == (ADDIS + 1) || MEM_instruct.opcode == SUBIS || 
		MEM_instruct.opcode == (SUBIS + 1)) {

		flag_Z = (CURRENT_STATE.START_REGS.EX_MEM.ALU_result == 0) ? 1 : 0;
		flag_N = ((long)CURRENT_STATE.START_REGS.EX_MEM.ALU_result < 0) ? 1 : 0;
	}

	if (cond == 0) {
		// EQ or NE
		//printf("HANDLING BEQ or BNE\n");
		if (flag_Z == 1) {
			result = 1;	
		}
	} else if (cond == 5) {
		// BGE or BLT
		// printf("HANDLING BGE or BLT\n");
		if (flag_N == 0) {
			result = 1;
		}
	} else if (cond == 6) {
		// BGT or BLE
		// printf("HANDLING BGT or BLE\n");
		if ((flag_N == 0) && (flag_Z == 0)) {
			result = 1;
		}
	}

	if ((HOLDER.Rt & 1) == 1 && ((HOLDER.Rt & 15) != 15)) {
		result = !result;
	}

	uint32_t myActualNextInstructionPC;
	int myBranchTaken;
	if (result == 1) {
		myActualNextInstructionPC = aExecuteInstructionPC + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
		myBranchTaken = 1;
	} else {
		myActualNextInstructionPC = aExecuteInstructionPC + 4;
		myBranchTaken = 0;
	}

	evaluate_prediction(&CURRENT_STATE,aExecuteInstructionPC,
		myActualNextInstructionPC, 
		aPredictedNextInstructionPC, 
		CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry,
		CONDITIONAL,
		myBranchTaken,
		CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result);
}

void handle_cbnz(uint32_t aExecuteInstructionPC, uint32_t aPredictedNextInstructionPC) {
	int myBranchTaken;
	uint32_t myActualNextInstructionPC;

	if (CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder != 0) {
		myBranchTaken = 1;
		myActualNextInstructionPC = aExecuteInstructionPC + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
	} else {
		myBranchTaken = 0;
		myActualNextInstructionPC = aExecuteInstructionPC + 4;
	}

	evaluate_prediction(&CURRENT_STATE,aExecuteInstructionPC,
		myActualNextInstructionPC, 
		aPredictedNextInstructionPC, 
		CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry,
		CONDITIONAL,
		myBranchTaken,
		CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result);
}


void handle_cbz(uint32_t aExecuteInstructionPC, uint32_t aPredictedNextInstructionPC) {
	int myBranchTaken;
	int myTakenPrediction = CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result;
	uint32_t myActualNextInstructionPC;
	if (CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder == 0) {
		myBranchTaken = 1;
		myActualNextInstructionPC = CURRENT_STATE.CURRENT_REGS.ID_EX.PC + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
	} else {
		myBranchTaken = 0;
		myActualNextInstructionPC = CURRENT_STATE.CURRENT_REGS.ID_EX.PC + 4;
	}

	evaluate_prediction(&CURRENT_STATE,aExecuteInstructionPC,
		myActualNextInstructionPC, 
		aPredictedNextInstructionPC, 
		CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry,
		CONDITIONAL,
		myBranchTaken,
		CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result);
}

void forward_data (parsed_instruction_holder HOLDER, int result, uint64_t data) {
	if (result == 1) {
		CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = data;
		if (HOLDER.format == 1 && HOLDER.opcode != BR) {
			if (HOLDER.Rm == HOLDER.Rn) {
				CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = data;	
			}
		}
	} else if (result == 2) {
		CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = data;
	}
}


/************************************ END OF HELPERS ************************************/

void pipe_init(CPU_State CPU, PC) {
    memset(&CURRENT_STATE, 0, sizeof(CPU_State));
    CURRENT_STATE.PC = 0x00400000;
    CURRENT_STATE.RUN_BIT = 1;

    // Initiating Cache
    CURRENT_STATE.TheinstructionCache = instruction_cache_new();
	CURRENT_STATE.theDataCache = data_cache_new();
}

void pipe_cycle(CPU_State CPU) {
	if (VERBOSE) {
		printf("--------CYCLE START -----\n");
	}

	START_REGS = CURRENT_REGS;
	pipe_stage_wb();
	pipe_stage_mem();
	pipe_stage_execute();
	pipe_stage_decode();
	pipe_stage_fetch();
	reset_current_state.bubble();
	manage_cache_stall();

	if (VERBOSE) {
		printf("-------- CYCLE END (%d, %lx) -------\n\n", (stat_cycles + 1), CURRENT_STATE.PC);
	}

}

uint64_t mem_read_64_DC(uint64_t aAddr) {
	uint64_t bit31_0 = read_cache(CURRENT_STATE.theDataCache, aAddr);
	uint64_t bit63_32 = read_cache(CURRENT_STATE.theDataCache, aAddr + 4);
	return ((bit63_32 << 32) | bit31_0);
}

void mem_write_64_DC(uint64_t aAddr, uint64_t aValue) {
	uint64_t bit31_0 = aValue & 0xFFFFFFFF;
	uint64_t bit63_32 = (aValue & 0xFFFFFFFF00000000) >> 32;
	uint32_t b1 = bit31_0;
	uint32_t b2 = bit63_32;

	write_to_cache(CURRENT_STATE.theDataCache, aAddr, b1);
	write_to_cache(CURRENT_STATE.theDataCache, (aAddr + 4), b2);
}

void handle_load_stur(parsed_instruction_holder INSTRUCTION_HOLDER, uint64_t aMemoryAddr, uint32_t aDataToWrite) {
	int myDataCacheMissed = 0;
	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 0) {
		myDataCacheMissed = check_data_in_cache(CURRENT_STATE.theDataCache, aMemoryAddr);
	}

	if (myDataCacheMissed != 0) {
		CURRENT_STATE.CYCLE_STALL_DATA_CACHE = 50;
		// TEMPORARY, LOOK FOR WORK AROUND!
	}
	print_cache_behavior(2);

	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 0) {
		//printf("THis is the cycle: %d\n", stat_cycles +1);
		if (INSTRUCTION_HOLDER.opcode == 0x7C2) {
			CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data = 
				mem_read_64_DC(aMemoryAddr);
		} else if (INSTRUCTION_HOLDER.opcode == 0x5C2) {
			CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data = read_cache(CURRENT_STATE.theDataCache, aMemoryAddr);
		} else if (INSTRUCTION_HOLDER.opcode == 0x1C2 ) {
			CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data = 
				get_memory_segment(0,7,read_cache(CURRENT_STATE.theDataCache, aMemoryAddr));
		

		} else if (INSTRUCTION_HOLDER.opcode == 0x3C2) {
			CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data = 
				get_memory_segment(0,15, read_cache(CURRENT_STATE.theDataCache, aMemoryAddr));
		} else /* store */{
			if (INSTRUCTION_HOLDER.opcode != STUR) {
				write_to_cache(CURRENT_STATE.theDataCache, aMemoryAddr, aDataToWrite);
			} else {
				mem_write_64_DC(aMemoryAddr, aDataToWrite);
			}
		}
	} else {
		//printf("THIS IS THE CYCLE STALL: %d\n", CURRENT_STATE.CYCLE_STALL_DATA_CACHE);
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 1) {
			//printf("This is the cycle: %d\n", stat_cycles +1);
			cache_update(CURRENT_STATE.theDataCache, aMemoryAddr);
		}
	}
}

void pipe_stage_wb() {
	if (VERBOSE) {
		printf("\nWrite BACK -----------> ");
		print_operation(CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction);
	}
	
	if (CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction == 0 || CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
		return;
	} else if (CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction == HLT) {
		stat_inst_retire++;
		// printf("RETIRE INSTR 1\n");
		CURRENT_STATE.RUN_BIT = 0;
		return;
	}
	parsed_instruction_holder INSTRUCTION_HOLDER = get_holder(CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction);
	int WRITE_TO = -1;

	if (INSTRUCTION_HOLDER.format == 1) {
		WRITE_TO = INSTRUCTION_HOLDER.Rd;
		if (INSTRUCTION_HOLDER.opcode == ADDS || INSTRUCTION_HOLDER.opcode == (ADDS + 1) ||
			INSTRUCTION_HOLDER.opcode == ANDS || 
			INSTRUCTION_HOLDER.opcode == SUBS || INSTRUCTION_HOLDER.opcode == (SUBS + 1)) {
			CURRENT_STATE.FLAG_N = ((long)CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result < 0) ? 1 : 0;
			CURRENT_STATE.FLAG_Z = (CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result == 0) ? 1 : 0;
		}

		if (INSTRUCTION_HOLDER.opcode == BR) {
			WRITE_TO = -1;
		}

	} else if (INSTRUCTION_HOLDER.format == 2) {
		WRITE_TO = INSTRUCTION_HOLDER.Rd;
		if (INSTRUCTION_HOLDER.opcode == ADDIS || INSTRUCTION_HOLDER.opcode == (ADDIS + 1) ||
			INSTRUCTION_HOLDER.opcode == SUBIS || INSTRUCTION_HOLDER.opcode == (SUBIS + 1)) {
			CURRENT_STATE.FLAG_N = ((long)CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result < 0) ? 1 : 0;
			CURRENT_STATE.FLAG_Z = (CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result == 0) ? 1 : 0;
		}

	} else if (INSTRUCTION_HOLDER.format == 3) {
		if ((INSTRUCTION_HOLDER.opcode == LDUR_64) || (INSTRUCTION_HOLDER.opcode == LDUR_32) ||
			(INSTRUCTION_HOLDER.opcode == LDURH) || (INSTRUCTION_HOLDER.opcode == LDURB)) {
			WRITE_TO = INSTRUCTION_HOLDER.Rt;
		}
	} else if (INSTRUCTION_HOLDER.format == 4 || INSTRUCTION_HOLDER.format == 5) {              
	} else if (INSTRUCTION_HOLDER.format == 6) {
		WRITE_TO = INSTRUCTION_HOLDER.Rd;
	}


	if ((WRITE_TO != -1) && (WRITE_TO != 31)) {
		if (INSTRUCTION_HOLDER.format != 3) {
			CURRENT_STATE.REGS[WRITE_TO] = CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result;
		} else {
			CURRENT_STATE.REGS[WRITE_TO] = CURRENT_STATE.CURRENT_REGS.MEM_WB.fetched_data;
		}
	}
	stat_inst_retire++;
	// printf("RETIRE INSTR 2\n");
}

void pipe_stage_mem() {
	if (VERBOSE) {
		printf("Memory -----------> ");
		print_operation(CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction);
	}

	if (CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction == 0) {
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 0) {
			clear_MEM_WB_REGS();
		}
		return;
	} else if (CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction == HLT) {
		clear_MEM_WB_REGS();
		CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction = CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction;
		return;
	}

	clear_MEM_WB_REGS();
	parsed_instruction_holder INSTRUCTION_HOLDER = get_holder(CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction);
	
	if (INSTRUCTION_HOLDER.format == 1) {
		//printf ("SOMETHING WEIRD HAPPENING - R INSTR SHOULDNT GO TO MEM\n");
	} else if (INSTRUCTION_HOLDER.format == 2) {
		//printf ("SOMETHING WEIRD HAPPENING - I INSTR SHOULDNT GO TO MEM\n");
	} else if (INSTRUCTION_HOLDER.format == 3) {
		// load
		handle_load_stur(INSTRUCTION_HOLDER, CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result, CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write);
	} else if ((INSTRUCTION_HOLDER.format == 4) || (INSTRUCTION_HOLDER.format == 5) || 
		(INSTRUCTION_HOLDER.format == 6)) {

	}
	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
		return;
	}
	CURRENT_STATE.CURRENT_REGS.MEM_WB.data_to_write = CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write;
	CURRENT_STATE.CURRENT_REGS.MEM_WB.ALU_result = CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result;
	CURRENT_STATE.CURRENT_REGS.MEM_WB.instruction = CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction;
}

// R INSTR EXECUTE STAGE
void pipe_stage_execute() {
	if (VERBOSE) {	
		printf("Execute -----------> ");
		print_operation(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);
	}
	
	// printf("PC OF INSTRUCTION TO EXECUTE: %lx. PC OF NEXT INSTRUCTION: %lx\n", CURRENT_STATE.CURRENT_REGS.ID_EX.PC, CURRENT_STATE.CURRENT_REGS.IF_ID.PC);

	if (CURRENT_STATE.CURRENT_REGS.ID_EX.instruction == 0) {
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 0) {
			clear_EX_MEM_REGS();	
		}
		return;
	} else if (CURRENT_STATE.CURRENT_REGS.ID_EX.instruction == HLT) {
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
			return;
		}
		clear_EX_MEM_REGS();
		CURRENT_STATE.FETCH_MORE = 0;
		// CURRENT_STATE.PC = CURRENT_STATE.CURRENT_REGS.ID_EX.PC + 8; NO NEED BRUH
		CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction = CURRENT_STATE.CURRENT_REGS.ID_EX.instruction;
		return;
	}

	CURRENT_STATE.BUBBLE = (hazard_detection_unit(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction, CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction) != 0) ? 1 : 0;
	if (CURRENT_STATE.BUBBLE != 0) {
		if (VERBOSE) {
			printf("BUBBLING!\n");
		}
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 0) {
			clear_EX_MEM_REGS();
		}
		return;
	}

	parsed_instruction_holder HOLDER = get_holder(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);	

	int MEM_forward = forward(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction, CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction);
	int WB_forward = forward(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction, CURRENT_STATE.START_REGS.MEM_WB.instruction);

	//printf("This is the Mem Forward Result: %d, WB Forward: %d\n", MEM_forward, WB_forward);

	forward_data(HOLDER, MEM_forward, CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result);

	if ((WB_forward != 0) && (MEM_forward != WB_forward)) {
		forward_data(HOLDER, WB_forward, CURRENT_STATE.START_REGS.MEM_WB.ALU_result);
	}

	if (get_memRead(get_holder(CURRENT_STATE.START_REGS.MEM_WB.instruction).opcode)) {
		int current_state.bubble_result = hazard_detection_unit(CURRENT_STATE.CURRENT_REGS.ID_EX.instruction, CURRENT_STATE.START_REGS.MEM_WB.instruction);
		forward_data(HOLDER, current_state.bubble_result, CURRENT_STATE.START_REGS.MEM_WB.fetched_data);

		if (current_state.bubble_result == 2) {
			//printf("This is RN: %d and Primary: %d\n", CURRENT_STATE.REGS[HOLDER.Rn], CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder);
			if (CURRENT_STATE.REGS[HOLDER.Rn] != CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder) {
				CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = CURRENT_STATE.REGS[HOLDER.Rn];
			}
		} else if (current_state.bubble_result == 1 && (HOLDER.Rm != HOLDER.Rn)) {
			//printf("This is Rm: %d and second: %d\n", CURRENT_STATE.REGS[HOLDER.Rm], CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
			if (HOLDER.format == 1) {	
				if (HOLDER.opcode != LSL && HOLDER.opcode != LSR && HOLDER.opcode != BR) { 
					if (CURRENT_STATE.REGS[HOLDER.Rm] != CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder) {
						CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = CURRENT_STATE.REGS[HOLDER.Rm];
					}
				}
			}
		}

	}

	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0) {
		return;
	}

	clear_EX_MEM_REGS();
	
	CURRENT_STATE.CURRENT_REGS.EX_MEM.instruction = CURRENT_STATE.CURRENT_REGS.ID_EX.instruction;
	uint32_t myExecuteInstructionPC = CURRENT_STATE.CURRENT_REGS.ID_EX.PC;
	uint32_t myPredictedNextInstructionPC = CURRENT_STATE.CURRENT_REGS.IF_ID.PC;
	
	if (HOLDER.format == 1) {
		// printf("THIS is primary: %d, this is secondary_data_holder: %d\n", CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder,
		// 	CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
		if (HOLDER.opcode == 0x458 || HOLDER.opcode == 0x459) {
			handle_add();
		} else if (HOLDER.opcode == 0x558 || HOLDER.opcode == 0x559) {
			handle_adds();
		} else if (HOLDER.opcode == 0x450) {
			handle_and();
		} else if (HOLDER.opcode == 0x750) {
			handle_ands();
		} else if (HOLDER.opcode == 0x650) {
			handle_eor();
		} else if (HOLDER.opcode == 0x550) {
			handle_orr();
		} else if (HOLDER.opcode == 0x69B) {
			if (get_instruction_segment(10,15, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction) == 0x3F) {
				CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = get_instruction_segment(16,21, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);
				handle_lsr();
		} 	else {
				CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = get_instruction_segment(10,15, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);
				handle_lsl();
			}
		} else if (HOLDER.opcode == 0x69A) {
			if (get_instruction_segment(10,15, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction) != 0x3F) {
				CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = get_instruction_segment(10,15, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);
				handle_lsl();
		} 	else {
				CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = get_instruction_segment(16,21, CURRENT_STATE.CURRENT_REGS.ID_EX.instruction);
				handle_lsr();
			}	
		} else if (HOLDER.opcode == 0x658 || HOLDER.opcode == 0x659) {
			handle_sub();
		} else if (HOLDER.opcode == 0x758 || HOLDER.opcode == 0x759) {
			handle_subs();
		} else if (HOLDER.opcode == 0x6B0) {
			handle_br(myExecuteInstructionPC, myPredictedNextInstructionPC);
		} else if (HOLDER.opcode == 0x4D8) {
			handle_mul();
		}
	} else if (HOLDER.format == 2) {
		if (HOLDER.opcode == ADDI || HOLDER.opcode == (ADDI + 1)) {
			handle_addi();
		} else if (HOLDER.opcode == ADDIS || HOLDER.opcode == (ADDIS + 1)) {
			handle_addis();
		} else if (HOLDER.opcode == SUBI || HOLDER.opcode == (SUBI + 1)) {
			handle_subi();
		} else if (HOLDER.opcode == SUBIS || HOLDER.opcode == (SUBIS + 1)) {
			handle_subis();
		}
	} else if (HOLDER.format == 3) {

		//printf("THIS IS THE PRIMARY DATA HOLDER FROM EXE: %lx\n", CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder);
		CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;

		if (HOLDER.opcode == 0x7C2) {
		} else if (HOLDER.opcode == 0x1C2) {
		} else if (HOLDER.opcode == 0x3C2) {
		} else if (HOLDER.opcode == 0x7C0) {
			CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write = CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder;
		} else if (HOLDER.opcode == 0x1C0) {
			CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write = get_memory_segment(0,7, CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
		} else if (HOLDER.opcode == 0x3C0) {
			CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write = get_memory_segment(0,15, CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
		} else if (HOLDER.opcode == 0x5C0) {
			CURRENT_STATE.CURRENT_REGS.EX_MEM.data_to_write = get_memory_segment(0,31, CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder);
		}
		//printf("THIS IS THE ALU RESULT IN EXE: %lx\n", CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result);
	} else if (HOLDER.format == 4) {
		uint32_t myActualNextInstructionPC = myExecuteInstructionPC + CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
		evaluate_prediction(&CURRENT_STATE,myExecuteInstructionPC,
			myActualNextInstructionPC, 
			myPredictedNextInstructionPC, 
			CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry,
			UNCONDITIONAL,
			1,
			1);
			
	} else if (HOLDER.format == 5) {
		if (HOLDER.opcode >= 0x5A8 && HOLDER.opcode <= 0x5AF) {
			handle_cbnz(myExecuteInstructionPC, myPredictedNextInstructionPC);
		} else if (HOLDER.opcode >= 0x5A0 && HOLDER.opcode <= 0x5A7) {
			handle_cbz(myExecuteInstructionPC, myPredictedNextInstructionPC);
		} else if (HOLDER.opcode >= 0x2A0 && HOLDER.opcode <= 0x2A7) {
			handle_bcond(HOLDER,myExecuteInstructionPC, myPredictedNextInstructionPC);
		}
	} else if (HOLDER.format == 6) {
		CURRENT_STATE.CURRENT_REGS.EX_MEM.ALU_result = CURRENT_STATE.CURRENT_REGS.ID_EX.immediate;
	}
}


void pipe_stage_decode() {
	if (VERBOSE) {
		printf("Decode -----------> ");
		print_operation(CURRENT_STATE.CURRENT_REGS.IF_ID.instruction);
	}

	
	if (CURRENT_STATE.BUBBLE != 0) {
		return;
	}

	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0  && CURRENT_STATE.CURRENT_REGS.ID_EX.instruction != 0) {
		return;
	}
	if (CURRENT_STATE.CURRENT_REGS.IF_ID.instruction == 0) {
		//printf("SKIPPING DECODE\n");
		clear_ID_EX_REGS();
		return;
	} else if (CURRENT_STATE.CURRENT_REGS.IF_ID.instruction == HLT) {
		CURRENT_STATE.CURRENT_REGS.ID_EX.instruction = CURRENT_STATE.CURRENT_REGS.IF_ID.instruction;
		CURRENT_STATE.CURRENT_REGS.ID_EX.PC = CURRENT_STATE.CURRENT_REGS.IF_ID.PC;
		clear_IF_ID_REGS();
		return;
	}

	parsed_instruction_holder INSTRUCTION_HOLDER = get_holder(CURRENT_STATE.CURRENT_REGS.IF_ID.instruction);

	clear_ID_EX_REGS();
	if (INSTRUCTION_HOLDER.format == 1) { // R
		CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rn];
		CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rm];
		if (INSTRUCTION_HOLDER.opcode == 0x69B) {
			CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = INSTRUCTION_HOLDER.shamt;
		} else if (INSTRUCTION_HOLDER.opcode == 0x69A) {
			CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = 
				get_instruction_segment(16,21, CURRENT_STATE.CURRENT_REGS.IF_ID.instruction);
		}
	} else if (INSTRUCTION_HOLDER.format == 2) { // I
	 	CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rn];
	 	CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = INSTRUCTION_HOLDER.ALU_immediate;
		
	} else if (INSTRUCTION_HOLDER.format == 3) { // D
		CURRENT_STATE.CURRENT_REGS.ID_EX.primary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rn];
		CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = INSTRUCTION_HOLDER.DT_address;

		if (INSTRUCTION_HOLDER.opcode == STUR || INSTRUCTION_HOLDER.opcode == STURH ||
			INSTRUCTION_HOLDER.opcode == STURW || INSTRUCTION_HOLDER.opcode == STURB) {
			CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rt];
		}

	} else if (INSTRUCTION_HOLDER.format == 4) { // B
		CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = sign_extend(INSTRUCTION_HOLDER.BR_address, 26, 2);
	} else if (INSTRUCTION_HOLDER.format == 5) { // CB
		if ((INSTRUCTION_HOLDER.opcode >= 0x5A8 && INSTRUCTION_HOLDER.opcode <= 0x5AF) || 
			(INSTRUCTION_HOLDER.opcode >= 0x5A0 && INSTRUCTION_HOLDER.opcode <= 0x5A7)) {
			CURRENT_STATE.CURRENT_REGS.ID_EX.secondary_data_holder = CURRENT_STATE.REGS[INSTRUCTION_HOLDER.Rt];
		}

 		CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = sign_extend(INSTRUCTION_HOLDER.COND_BR_address, 19, 2);
	} else if (INSTRUCTION_HOLDER.format == 6) { // IM/IW
		CURRENT_STATE.CURRENT_REGS.ID_EX.immediate = INSTRUCTION_HOLDER.MOV_immediate;
	}
	
	CURRENT_STATE.CURRENT_REGS.ID_EX.instruction = CURRENT_STATE.CURRENT_REGS.IF_ID.instruction;
	CURRENT_STATE.CURRENT_REGS.ID_EX.PC = CURRENT_STATE.CURRENT_REGS.IF_ID.PC;
	CURRENT_STATE.CURRENT_REGS.ID_EX.accessed_entry = CURRENT_STATE.CURRENT_REGS.IF_ID.accessed_entry;
	CURRENT_STATE.CURRENT_REGS.ID_EX.PHT_result = CURRENT_STATE.CURRENT_REGS.IF_ID.PHT_result;
	clear_IF_ID_REGS();
}

void pipe_stage_fetch() {
	if (VERBOSE) {
		printf("Fetch -----------> ");
	}
	//printf("CYCLE STALL INSTRUCT CACHE %d\n", CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE);
	print_cache_behavior(1);

	if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 0 && CURRENT_STATE.CYCLE_STALL_DATA_CACHE != 50 && CURRENT_STATE.CURRENT_REGS.IF_ID.instruction != 0) {
		if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 1) {
			cache_update(CURRENT_STATE.TheinstructionCache, CURRENT_STATE.PC);
		}
		// print_operation(CURRENT_STATE.CURRENT_REGS.IF_ID.instruction);
		return;
	}
	
	if (CURRENT_STATE.BUBBLE != 0) {
		if (VERBOSE) {
			printf("CURRENT_STATE.BUBBLE\n");
		}
		return;
	}


	int myInstructionCacheMissed = 0;
	if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 0 && CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.instruction == 0) {
		myInstructionCacheMissed = check_data_in_cache(CURRENT_STATE.TheinstructionCache, CURRENT_STATE.PC);
	}
	// print_cache(curRent_state.TheinstructionCache);
	if (myInstructionCacheMissed == 1) {
		// stall 50 cycles
		CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE = 50;
	}

	if ((CURRENT_STATE.FETCH_MORE != 0) && (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 0)) {
		if (CURRENT_STATE.CYCLE_STALL_DATA_CACHE == 50) {
			CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.instruction = read_cache(CURRENT_STATE.TheinstructionCache, CURRENT_STATE.PC);
			CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PC = CURRENT_STATE.PC;
			CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.accessed_entry = CURRENT_STATE.BP.BTB[get_BTB_index(CURRENT_STATE.PC)];
			CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PHT_result = should_take_branch(CURRENT_STATE.BP.gshare.PHT[(CURRENT_STATE.BP.gshare.GHR ^ get_8_pc_bits(CURRENT_STATE.PC))]);
			
			bp_predict(&CURRENT_STATE);
		} else {
			if (CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PC == 0) /*reservoir empty*/{
				//printf("NOT USING RESERVOIR REGS!\n");
				clear_IF_ID_REGS();
				CURRENT_STATE.CURRENT_REGS.IF_ID.instruction = read_cache(CURRENT_STATE.TheinstructionCache, CURRENT_STATE.PC);
				CURRENT_STATE.CURRENT_REGS.IF_ID.PC = CURRENT_STATE.PC;
				CURRENT_STATE.CURRENT_REGS.IF_ID.accessed_entry = CURRENT_STATE.BP.BTB[get_BTB_index(CURRENT_STATE.PC)];
				CURRENT_STATE.CURRENT_REGS.IF_ID.PHT_result = should_take_branch(CURRENT_STATE.BP.gshare.PHT[(CURRENT_STATE.BP.gshare.GHR ^ get_8_pc_bits(CURRENT_STATE.PC))]);

				bp_predict(&CURRENT_STATE);
			} else /*reservoir full */{
				//printf("USING RESERVOIR REGS\n");
				CURRENT_STATE.CURRENT_REGS.IF_ID.instruction = CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.instruction;
				CURRENT_STATE.CURRENT_REGS.IF_ID.PC = CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PC;
				CURRENT_STATE.CURRENT_REGS.IF_ID.accessed_entry = CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.accessed_entry;
				CURRENT_STATE.CURRENT_REGS.IF_ID.PHT_result = CURRENT_STATE.CURRENT_REGS.IF_ID_RESERVOIR.PHT_result;
				clear_IF_ID_RESERVOIR_REGS();
			}
		}
		if (VERBOSE) {
			print_operation(CURRENT_STATE.CURRENT_REGS.IF_ID.instruction);
		}


	} else if ((CURRENT_STATE.FETCH_MORE != 0) && (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE != 0)) {
		if (!CURRENT_STATE.CYCLE_STALL_DATA_CACHE) {
			clear_IF_ID_REGS();
		}
		if (CURRENT_STATE.CYCLE_STALL_INSTRUCT_CACHE == 1) {
			cache_update(CURRENT_STATE.TheinstructionCache, CURRENT_STATE.PC);
		}
		if (VERBOSE) {
			printf("\n");
		}
	} else {
		if (VERBOSE) {
			printf("\n");
		}
	}
}