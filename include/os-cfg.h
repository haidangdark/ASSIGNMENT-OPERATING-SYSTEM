/*
 * Copyright (C) 2026 pdnguyen of HCMC University of Technology VNU-HCM
 */

/* LamiaAtrium release
 * Source Code License Grant: The authors hereby grant to Licensee
 * personal permission to use and modify the Licensed Source Code
 * for the sole purpose of studying while attending the course CO2018.
 */

#ifndef OSCFG_H
#define OSCFG_H

#define MLQ_SCHED 1
#define MAX_PRIO 140

#define MM_PAGING
#define MM_FIXED_MEMSZ // comment dong nay de chay 
/*./os os_syscall
./os os_syscall_list
./os os_sc

./os os_0_mlq_paging
./os os_1_mlq_paging
./os os_1_mlq_paging_small_1K
./os os_1_mlq_paging_small_4K
./os os_1_singleCPU_mlq_paging*/

//con uncomment thi chay 
/*./os sched
./os sched_0
./os sched_1
./os os_1_singleCPU_mlq*/
//#define VMDBG 1
//#define MMDBG 1
#define IODUMP 1
#define PAGETBL_DUMP 1

/* 
 * @bksysnet:
 *    The address mode must be explicitly define in MM64 or no-MM64
 *    by commenting one of these following lines and uncommenting the other
 *      
 */
#define MM64 1
//#undef MM64

#endif
