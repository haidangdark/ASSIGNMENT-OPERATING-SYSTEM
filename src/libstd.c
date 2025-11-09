/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#include "common.h"
#include "syscall.h"

int libsyscall (struct pcb_t *caller,
             uint32_t syscall_idx,
             arg_t a1,
             arg_t a2,
             arg_t a3)
{
   struct sc_regs regs;

	/*
	 * @bksysnet: Please note that the architecture design of
	 *            dual spaces does not allow direct access 
	 *            between kernelspace and userspace
	 *            libstd routines are  in userspace with 
	 *            their own PCB struct, vals can be passed
	 *            through syscall argument but not the PCB struct.
	 *            This design follows centralized registry as in 
	 *            ntkernel but it keeps remain in userspace only.
	 */
   regs.a1 = a1;
   regs.a2 = a2;
   regs.a3 = a3;

   return syscall(caller->krnl, caller->pid, syscall_idx, &regs);
}
