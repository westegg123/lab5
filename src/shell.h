/***************************************************************/
/*                                                             */
/*   ARM Instruction Level Simulator                       */
/*                                                             */
/*   CMSC-22200 Computer Architecture                                            */
/*   University of Chicago                                */
/*                                                             */
/***************************************************************/

/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
/*          DO NOT MODIFY THIS FILE!                            */
/* !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */

#ifndef _SIM_SHELL_H_
#define _SIM_SHELL_H_

#include "pipe.h"
#include <stdint.h>


#define FALSE 0
#define TRUE  1


typedef struct processor_t {
	CPU_State CPU0, CPU1, CPU2, CPU3;
} processor_t;

/* only the cache touches these functions */
uint32_t mem_read_32(uint64_t address);
void     mem_write_32(uint64_t address, uint32_t value);

/* statistics */
extern uint32_t stat_cycles, stat_inst_retire, stat_inst_fetch, stat_squash;

#endif
