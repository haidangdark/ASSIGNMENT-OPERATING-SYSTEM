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
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "mm64.h"
#include "queue.h" 

/* --- [OVERRIDE] GHI ĐÈ CẤU HÌNH CHO CHẾ ĐỘ 64-BIT --- */
#ifdef MM64
  /* 1. Ghi đè kích thước trang (4096 thay vì 256) */
  #undef PAGING_PAGESZ
  #define PAGING_PAGESZ PAGING64_PAGESZ 

  /* 2. Ghi đè các macro bit để dùng chung logic */
  #undef PAGING_PTE_FPN
  #define PAGING_PTE_FPN(pte) PAGING64_PTE_FPN(pte)
  
  #undef PAGING_PTE_PRESENT
  #define PAGING_PTE_PRESENT(pte) PAGING64_PTE_PRESENT(pte)

  #undef PAGING_PTE_SWAPPED
  #define PAGING_PTE_SWAPPED(pte) PAGING64_PTE_SWAPPED(pte)
#endif
/* --------------------------------------------------- */

/* Macro tính địa chỉ vật lý: Dùng PAGING_PAGESZ (Lúc này đã là 4096 nhờ Override) */
#define PHY_ADDR(krnl, fpn) \
  ((uint64_t *)((char *)(krnl)->mram->storage + ((fpn) * PAGING_PAGESZ)))

/* * print_mm_stats - Thống kê chi tiết bộ nhớ của tiến trình
 * Duyệt cây bảng trang 5 cấp
 */
static void 
print_mm_stats (struct krnl_t *krnl, struct pcb_t *proc)
{
  struct mm_struct *mm = proc->krnl->mm;
  int i_pgd, i_p4d, i_pud, i_pmd;
  
  int cnt_pgd = 0, cnt_p4d = 0, cnt_pud = 0, cnt_pmd = 0, cnt_pt = 0;
  int pages_in_ram = 0;
  int pages_swapped = 0;

  printf ("[MMSTATS] === MEMORY STRUCTURE DEBUG ===\n");
  printf ("[MMSTATS] mm_struct: %p\n", (void *)mm);

#ifdef MM64
  printf ("[MMSTATS] pgd (Virtual): %p\n", (void *)mm->pgd);

  /* 1. Duyệt PGD (Level 5) */
  for (i_pgd = 0; i_pgd < PAGING64_PGD_CNT; i_pgd++)
    {
      if (PAGING_PTE_PRESENT (mm->pgd[i_pgd]))
        {
          cnt_pgd++;
          
          /* Lấy P4D từ RAM */
          addr_t fpn_p4d = PAGING_PTE_FPN (mm->pgd[i_pgd]);
          uint64_t *tbl_p4d = PHY_ADDR (krnl, fpn_p4d);
          
          /* 2. Duyệt P4D (Level 4) */
          for (i_p4d = 0; i_p4d < PAGING64_P4D_CNT; i_p4d++)
            {
              if (PAGING_PTE_PRESENT (tbl_p4d[i_p4d]))
                {
                  cnt_p4d++;
                  
                  /* Lấy PUD */
                  addr_t fpn_pud = PAGING_PTE_FPN (tbl_p4d[i_p4d]);
                  uint64_t *tbl_pud = PHY_ADDR (krnl, fpn_pud);

                  /* 3. Duyệt PUD (Level 3) */
                  for (i_pud = 0; i_pud < PAGING64_PUD_CNT; i_pud++)
                    {
                      if (PAGING_PTE_PRESENT (tbl_pud[i_pud]))
                        {
                          cnt_pud++;
                          
                          /* Lấy PMD */
                          addr_t fpn_pmd = PAGING_PTE_FPN (tbl_pud[i_pud]);
                          uint64_t *tbl_pmd = PHY_ADDR (krnl, fpn_pmd);

                          /* 4. Duyệt PMD (Level 2) */
                          for (i_pmd = 0; i_pmd < PAGING64_PMD_CNT; i_pmd++)
                            {
                              if (PAGING_PTE_PRESENT (tbl_pmd[i_pmd]))
                                {
                                  cnt_pmd++;
                                  
                                  /* Lấy PT */
                                  addr_t fpn_pt = PAGING_PTE_FPN (tbl_pmd[i_pmd]);
                                  uint64_t *tbl_pt = PHY_ADDR (krnl, fpn_pt);

                                  /* 5. Đếm số trang trong PT (Level 1) */
                                  int k;
                                  for (k = 0; k < PAGING64_PT_CNT; k++)
                                    {
                                      if (PAGING_PTE_PRESENT (tbl_pt[k]))
                                        {
                                          cnt_pt++;
                                          pages_in_ram++;
                                        }
                                      else if (PAGING_PTE_SWAPPED (tbl_pt[k]))
                                        {
                                          cnt_pt++; 
                                          pages_swapped++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

  printf ("[MMSTATS] 64-bit paging structures analysis:\n");
  printf ("[MMSTATS] Count PGD entries used: %d/%d\n", cnt_pgd, PAGING64_PGD_CNT);
  printf ("[MMSTATS] Count P4D entries used: %d\n", cnt_p4d);
  printf ("[MMSTATS] Count PUD entries used: %d\n", cnt_pud);
  printf ("[MMSTATS] Count PMD entries used: %d\n", cnt_pmd);
  printf ("[MMSTATS] Count PT (Data Pages):  %d\n", pages_in_ram + pages_swapped);

#endif /* MM64 */
  
  struct vm_area_struct *vma = mm->mmap;
  if (vma) 
    {
      printf ("[MMSTATS] VM Area 0: start=0x%lx - end=0x%lx (size: %lu bytes)\n", 
              (unsigned long)vma->vm_start, 
              (unsigned long)vma->vm_end, 
              (unsigned long)(vma->vm_end - vma->vm_start));
      printf ("[MMSTATS] VMA 0: vm_id=%lu, sbrk=0x%lx\n", vma->vm_id, (unsigned long)vma->sbrk);
    }

  printf ("[MMSTATS] === MEMORY STATISTICS ===\n");
  printf ("[MMSTATS] Process PID: %d\n", proc->pid);
  printf ("[MMSTATS] Total pages allocated: %d\n", pages_in_ram + pages_swapped);
  printf ("[MMSTATS] Pages in RAM: %d\n", pages_in_ram);
  printf ("[MMSTATS] Pages in SWAP: %d\n", pages_swapped);
  
  /* Tính toán dung lượng (PAGING_PAGESZ = 4096) */
  printf ("[MMSTATS] Memory usage: %d pages (%d KB)\n", 
          pages_in_ram + pages_swapped, 
          (pages_in_ram + pages_swapped) * (PAGING_PAGESZ / 1024));
}


/* * __sys_xxxhandler: System Call Handler (ID: 440)
 */
int 
__sys_xxxhandler (struct krnl_t *krnl, uint32_t pid, struct sc_regs *regs)
{
  struct pcb_t *caller = NULL;
  struct queue_t *running_list = krnl->running_list;
  int i;

  /* Tìm PCB */
  for (i = 0; i < running_list->size; i++) 
    {
      if (running_list->proc[i]->pid == pid) 
        {
          caller = running_list->proc[i];
          break;
        }
    }

  if (caller == NULL) 
    {
      printf ("[MMSTATS] Error: Process PID %d not found in running list.\n", pid);
      return -1;
    }

  printf ("[DEBUG] Syscall 440 called by PID %d, params: a1=" FORMAT_ARG ", a2=" FORMAT_ARG ", a3=" FORMAT_ARG "\n",
          pid, (arg_t)regs->a1, (arg_t)regs->a2, (arg_t)regs->a3);
  
  printf ("[MMSTATS] Looking for process with PID: %d\n", pid);
  printf ("[MMSTATS] Found process %d\n", pid);

  print_mm_stats (krnl, caller);

  return 0;
}