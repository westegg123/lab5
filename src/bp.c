/*
 * ARM pipeline timing simulator
 *
 * CMSC 22200, Fall 2016
 * Gushu Li and Reza Jokar
 */
#include "helper.h"
#include "bp.h"
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>

#define BTB_SIZE 1024
#define PHT_SIZE 256

uint32_t get_8_pc_bits(uint32_t aPC) {
	return get_instruction_segment(2, 9, aPC);
}

uint32_t get_BTB_index(uint32_t aPC) {
	return get_instruction_segment(2, 11, aPC);
}

void update_GHR(bp_t *BP, int aIsConditional, int aBranchTaken) {
	if (aIsConditional == 0) {
		return;
	}

	// printf("UPDATE GHR\n");
	if (aBranchTaken == 1) /*taken*/ {
		(BP->gshare).GHR = (((BP->gshare).GHR << 1) | 1);
	} else if (aBranchTaken == 0) /*not taken*/ {
		(BP->gshare).GHR = (((BP->gshare).GHR << 1));
	}
	// printf("GHR: %x, TAKEN: %d\n", BP.gshare.GHR, aBranchTaken);
}

void update_PHT(bp_t *BP, uint32_t aPC, int aIsConditional, int aBranchTaken) {
	if (aIsConditional == 0) {
		return;
	}

	// printf("UPDATE PHT\n");
	uint32_t myPHTIndex = (get_8_pc_bits(aPC) ^ (BP->gshare).GHR); 
	// printf("PHT INDEX: %d\n", myPHTIndex);
	if (aBranchTaken == 1) {
		if ((BP->gshare).PHT[myPHTIndex] < 3) {
			(BP->gshare).PHT[myPHTIndex] += 1;
		}
	} else if (aBranchTaken == 0) {
		// printf("NOT TAKEN\n");
		if ((BP->gshare).PHT[myPHTIndex] > 0 ) {
			(BP->gshare).PHT[myPHTIndex] -= 1;
		}
	}
}

void update_BTB(bp_t *BP, uint64_t aAddressTag, uint64_t aTargetBranch, int aIsConditional, int aIsValid, int aBranchTaken) {
	// printf("UPDATE BTB\n");
	if (aBranchTaken == 1) {
		uint32_t myBTBIndex = get_BTB_index(aAddressTag);
		// printf("BTB INDEX: %d:", myBTBIndex);
		// printf("TARGET BRANCH: %d\n", aTargetBranch);
		(BP->BTB)[myBTBIndex].branch_target = aTargetBranch;
		(BP->BTB)[myBTBIndex].address_tag = aAddressTag;
		(BP->BTB)[myBTBIndex].conditional = aIsConditional;
		(BP->BTB)[myBTBIndex].valid = aIsValid;
	}
}

int should_take_branch(int aSaturatingCounter) {
	if (aSaturatingCounter == 3 || aSaturatingCounter == 2) {
		return 1;
	}
	return 0;
}

void evaluate_prediction(CPU_State *CURRENT_STATE, uint32_t aExecuteInstructionPC,
	uint32_t aActualNextInstructionPC, 
	uint32_t aPredictedNextInstructionPC, 
	BTB_entry_t aBTBEntry, 
	int aIsConditional, 
	int aBranchTaken,
	int aBranchTakenPrediction) {
	// printf("ACTUAL NEXT INSTRUCTION PC: %x, PREDICTED NEXT INSTRUCTION PC: %x\n", aActualNextInstructionPC, aPredictedNextInstructionPC);
	// printf("PREDICTION : %d, ACTUAL : %d\n", aBranchTakenPrediction, aBranchTaken);
	// printf("BTB ADDRESS TAG: %lx, EXECUTE INSTRUCTION PC: %x\n", aBTBEntry.address_tag, aExecuteInstructionPC);
	if ((aActualNextInstructionPC != aPredictedNextInstructionPC) || (aBranchTakenPrediction != aBranchTaken)
		|| (aBranchTaken && (aBTBEntry.valid != 1 || aBTBEntry.address_tag != aExecuteInstructionPC))) {
		// printf("PREDICTION MISS\n");
		set_settings_pred_miss(CURRENT_STATE, aActualNextInstructionPC);
	}
	// print_bp(BP);
	// printf("------------------\n");
	bp_update(&(CURRENT_STATE->BP), aExecuteInstructionPC, aActualNextInstructionPC, aIsConditional, VALID, aBranchTaken);
}

void bp_predict(CPU_State *CURRENT_STATE) {
    uint64_t myPCPrediction = CURRENT_STATE->PC + 4;
    gshare_t myGshare = (CURRENT_STATE->BP).gshare;
    BTB_entry_t myBTB_entry = (CURRENT_STATE->BP).BTB[get_BTB_index(CURRENT_STATE->PC)];

    //printf("This is my index: %d\n", get_BTB_index(CURRENT_STATE.PC));
    if (myBTB_entry.valid == 1 && myBTB_entry.address_tag == CURRENT_STATE->PC) {
		if (myBTB_entry.conditional == 0) { // If Unconditional Branch
			// printf("YOOHOO!-----------------\n");
			//printf("BTB HIT! (UNCONDITIONAL BRANCH)\n");
			myPCPrediction = myBTB_entry.branch_target;
		} else { // If Conditional Branch
			if (should_take_branch(myGshare.PHT[(myGshare.GHR ^ get_8_pc_bits(CURRENT_STATE->PC))])) {
				//printf("BTB HIT! (CONDITIONAL BRANCH)\n");
				myPCPrediction = myBTB_entry.branch_target;
			}
		}
    }

    // printf("PREDICTING: %lx\n", myPCPrediction);
    CURRENT_STATE->PC = myPCPrediction;
    // print_bp(BP);
}

void print_BTB(bp_t BP) {
	for (int i = 0; i < BTB_SIZE; i++) {
		if (BP.BTB[i].valid) {
			printf("Index %d:\n 	Address_tag: %d \n", i, BP.BTB[i].address_tag);
			printf("	Valid bit: %d, Conditional Bit; %d\n", BP.BTB[i].valid, BP.BTB[i].conditional);
			printf("	Branch Target: %d\n", BP.BTB[i].branch_target);
		}
	}
}

void print_Gshare(bp_t BP) {
	printf("GSHARE\n");
	printf("GHR: %x\n", BP.gshare.GHR);
	for (int i = 0; i < PHT_SIZE; i++) {
		if (BP.gshare.PHT[i]) {
			printf("Index %d: %d\n", i, BP.gshare.PHT[i]);
		}
	}
}

void print_bp(bp_t BP) {
	print_Gshare(BP);
	print_BTB(BP);
}

void bp_update(bp_t *BP, uint64_t aAddressTag, uint64_t aTargetBranch, int aIsConditional, int aIsValid, int aBranchTaken) {
    /* Update BTB */
    // printf("A BRANCH TAKEN: %d\n", aBranchTaken);
	update_BTB(BP, aAddressTag, aTargetBranch, aIsConditional, aIsValid, aBranchTaken);

    /* Update gshare directional predictor */
    update_PHT(BP, aAddressTag, aIsConditional, aBranchTaken);

    /* Update global history register */
    update_GHR(BP, aIsConditional, aBranchTaken);
}
